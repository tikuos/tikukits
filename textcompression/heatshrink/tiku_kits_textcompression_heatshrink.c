/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_textcompression_heatshrink.c - LZSS compression for embedded
 *
 * Heatshrink-style LZSS compression. Searches a sliding window for
 * repeated sequences, encoding matches as (offset, length) pairs and
 * unmatched bytes as literals. Bit-packed output with zero heap usage.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_textcompression_heatshrink.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL CONSTANTS                                                        */
/*---------------------------------------------------------------------------*/

#define HS_WINDOW_SIZE \
    (1U << TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_WINDOW_BITS)
#define HS_LOOKAHEAD_SIZE \
    (1U << TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_LOOKAHEAD_BITS)
#define HS_MIN_MATCH \
    TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_MIN_MATCH
#define HS_MAX_MATCH \
    (HS_LOOKAHEAD_SIZE - 1 + HS_MIN_MATCH)
#define HS_WINDOW_BITS \
    TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_WINDOW_BITS
#define HS_LOOKAHEAD_BITS \
    TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_LOOKAHEAD_BITS

/*---------------------------------------------------------------------------*/
/* BIT WRITER                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Bit-level output stream packed MSB-first into a byte buffer
 */
struct hs_bit_writer {
    uint8_t *buf;
    uint16_t cap;
    uint16_t byte_pos;
    uint8_t  bit_pos;   /**< Next bit index within current byte (0..7) */
};

/**
 * @brief Initialize a bit writer
 */
static void bw_init(struct hs_bit_writer *bw, uint8_t *buf, uint16_t cap)
{
    bw->buf = buf;
    bw->cap = cap;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    if (cap > 0) {
        buf[0] = 0;
    }
}

/**
 * @brief Write nbits from val (MSB first)
 * @return 0 on success, -1 if buffer full
 */
static int bw_write_bits(struct hs_bit_writer *bw, uint16_t val, uint8_t nbits)
{
    int i;

    for (i = nbits - 1; i >= 0; i--) {
        if (bw->byte_pos >= bw->cap) {
            return -1;
        }
        if (val & (1U << i)) {
            bw->buf[bw->byte_pos] |= (uint8_t)(1U << (7 - bw->bit_pos));
        }
        bw->bit_pos++;
        if (bw->bit_pos == 8) {
            bw->bit_pos = 0;
            bw->byte_pos++;
            if (bw->byte_pos < bw->cap) {
                bw->buf[bw->byte_pos] = 0;
            }
        }
    }
    return 0;
}

/**
 * @brief Total bytes used (including partial last byte)
 */
static uint16_t bw_size(const struct hs_bit_writer *bw)
{
    return bw->byte_pos + (bw->bit_pos > 0 ? 1 : 0);
}

/*---------------------------------------------------------------------------*/
/* BIT READER                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Bit-level input stream, MSB-first
 */
struct hs_bit_reader {
    const uint8_t *buf;
    uint16_t len;
    uint16_t byte_pos;
    uint8_t  bit_pos;
};

/**
 * @brief Initialize a bit reader
 */
static void br_init(struct hs_bit_reader *br, const uint8_t *buf, uint16_t len)
{
    br->buf = buf;
    br->len = len;
    br->byte_pos = 0;
    br->bit_pos = 0;
}

/**
 * @brief Read nbits into val (MSB first)
 * @return 0 on success, -1 if not enough data
 */
static int br_read_bits(struct hs_bit_reader *br, uint8_t nbits, uint16_t *val)
{
    uint8_t i;

    *val = 0;
    for (i = 0; i < nbits; i++) {
        if (br->byte_pos >= br->len) {
            return -1;
        }
        *val <<= 1;
        if (br->buf[br->byte_pos] & (1U << (7 - br->bit_pos))) {
            *val |= 1;
        }
        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
/* ENCODING                                                                  */
/*---------------------------------------------------------------------------*/

int tiku_kits_textcompression_heatshrink_encode(
    const uint8_t *src, uint16_t src_len,
    uint8_t *dst, uint16_t dst_cap,
    uint16_t *out_len)
{
    struct hs_bit_writer bw;
    uint16_t i;
    uint16_t search_start;
    uint16_t j;
    uint8_t match_len;
    uint8_t max_match;
    uint16_t best_offset;
    uint8_t best_len;

    if (src == NULL || dst == NULL || out_len == NULL) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_NULL;
    }

    /* Need at least 2 bytes for the original-size header */
    if (dst_cap < 2) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
    }

    /* Write original size as little-endian uint16_t header */
    dst[0] = (uint8_t)(src_len & 0xFF);
    dst[1] = (uint8_t)((src_len >> 8) & 0xFF);

    if (src_len == 0) {
        *out_len = 2;
        return TIKU_KITS_TEXTCOMPRESSION_OK;
    }

    bw_init(&bw, dst + 2, dst_cap - 2);

    i = 0;
    while (i < src_len) {
        /* Find longest match in the sliding window */
        best_offset = 0;
        best_len = 0;
        search_start = (i > HS_WINDOW_SIZE) ? (i - HS_WINDOW_SIZE) : 0;

        max_match = (uint8_t)((src_len - i < HS_MAX_MATCH)
                              ? (src_len - i) : HS_MAX_MATCH);

        for (j = search_start; j < i; j++) {
            match_len = 0;
            while (match_len < max_match
                   && src[j + match_len] == src[i + match_len]) {
                match_len++;
            }

            if (match_len > best_len) {
                best_len = match_len;
                best_offset = i - j;
                if (match_len == max_match) {
                    break;
                }
            }
        }

        if (best_len >= HS_MIN_MATCH) {
            /* Back-reference: flag(0) + offset + length */
            if (bw_write_bits(&bw, 0, 1) < 0
                || bw_write_bits(&bw, best_offset - 1, HS_WINDOW_BITS) < 0
                || bw_write_bits(&bw, best_len - HS_MIN_MATCH,
                                 HS_LOOKAHEAD_BITS) < 0) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
            }
            i += best_len;
        } else {
            /* Literal: flag(1) + byte */
            if (bw_write_bits(&bw, 1, 1) < 0
                || bw_write_bits(&bw, src[i], 8) < 0) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
            }
            i++;
        }
    }

    *out_len = 2 + bw_size(&bw);
    return TIKU_KITS_TEXTCOMPRESSION_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODING                                                                  */
/*---------------------------------------------------------------------------*/

int tiku_kits_textcompression_heatshrink_decode(
    const uint8_t *src, uint16_t src_len,
    uint8_t *dst, uint16_t dst_cap,
    uint16_t *out_len)
{
    struct hs_bit_reader br;
    uint16_t orig_size;
    uint16_t di;
    uint16_t flag;
    uint16_t literal;
    uint16_t offset;
    uint16_t length;
    uint16_t ref_pos;
    uint16_t k;

    if (src == NULL || dst == NULL || out_len == NULL) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_NULL;
    }

    if (src_len < 2) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    /* Read original size from little-endian header */
    orig_size = (uint16_t)src[0] | ((uint16_t)src[1] << 8);

    if (orig_size == 0) {
        *out_len = 0;
        return TIKU_KITS_TEXTCOMPRESSION_OK;
    }

    if (orig_size > dst_cap) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
    }

    br_init(&br, src + 2, src_len - 2);
    di = 0;

    while (di < orig_size) {
        if (br_read_bits(&br, 1, &flag) < 0) {
            return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
        }

        if (flag) {
            /* Literal byte */
            if (br_read_bits(&br, 8, &literal) < 0) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
            }
            dst[di++] = (uint8_t)literal;
        } else {
            /* Back-reference */
            if (br_read_bits(&br, HS_WINDOW_BITS, &offset) < 0
                || br_read_bits(&br, HS_LOOKAHEAD_BITS, &length) < 0) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
            }

            offset += 1;       /* stored as offset-1 */
            length += HS_MIN_MATCH;

            if (offset > di) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
            }

            ref_pos = di - offset;

            /* Copy byte-by-byte to support overlapping matches */
            for (k = 0; k < length; k++) {
                if (di >= dst_cap) {
                    return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
                }
                dst[di++] = dst[ref_pos + k];
            }
        }
    }

    *out_len = di;
    return TIKU_KITS_TEXTCOMPRESSION_OK;
}
