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
 * @struct hs_bit_writer
 * @brief Bit-level output stream packed MSB-first into a byte buffer
 *
 * Allows the LZSS encoder to emit individual bits (flags, offsets,
 * lengths) into a contiguous byte buffer.  Bits are packed from the
 * most-significant bit toward the least-significant bit within each
 * byte.  When bit_pos reaches 8 the writer advances to the next byte
 * and pre-clears it so that subsequent OR operations start from zero.
 *
 * @note The writer does not own its buffer; the caller is responsible
 *       for providing a sufficiently large buffer via bw_init().
 */
struct hs_bit_writer {
    uint8_t *buf;       /**< Destination byte buffer (not owned) */
    uint16_t cap;       /**< Capacity of buf in bytes */
    uint16_t byte_pos;  /**< Current byte index in buf */
    uint8_t  bit_pos;   /**< Next bit index within current byte (0..7) */
};

/**
 * @brief Initialize a bit writer to the start of a byte buffer
 *
 * Resets all internal cursors and pre-clears the first byte so that
 * subsequent bw_write_bits() calls can OR bits into it directly.
 */
static void bw_init(struct hs_bit_writer *bw, uint8_t *buf, uint16_t cap)
{
    bw->buf = buf;
    bw->cap = cap;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    if (cap > 0) {
        buf[0] = 0;  /* Pre-clear so OR-ing individual bits works */
    }
}

/**
 * @brief Write @p nbits from @p val into the bit stream (MSB first)
 *
 * Iterates from the most-significant requested bit down to bit 0.
 * Each '1' bit is OR-ed into the current output byte at the correct
 * position; '0' bits are already zero thanks to the pre-clear in
 * bw_init / the rollover clear below.  When a byte is full the
 * writer advances and pre-clears the next byte.
 *
 * @param bw    Bit writer state
 * @param val   Value whose lower @p nbits bits are written
 * @param nbits Number of bits to write (1..16)
 * @return 0 on success, -1 if the output buffer is full
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
            /* Pre-clear next byte for subsequent OR operations */
            if (bw->byte_pos < bw->cap) {
                bw->buf[bw->byte_pos] = 0;
            }
        }
    }
    return 0;
}

/**
 * @brief Return total bytes consumed (including a partially-filled last byte)
 *
 * If any bits have been written into the current byte (bit_pos > 0),
 * that byte counts as used even though its remaining bits are zero-
 * padded.
 */
static uint16_t bw_size(const struct hs_bit_writer *bw)
{
    return bw->byte_pos + (bw->bit_pos > 0 ? 1 : 0);
}

/*---------------------------------------------------------------------------*/
/* BIT READER                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @struct hs_bit_reader
 * @brief Bit-level input stream, MSB-first
 *
 * Mirror image of hs_bit_writer for the decoder side.  Reads
 * individual bits from a contiguous byte buffer, tracking the current
 * byte and bit position.  Returns an error when the stream is
 * exhausted before the requested number of bits have been read.
 */
struct hs_bit_reader {
    const uint8_t *buf; /**< Source byte buffer (not owned) */
    uint16_t len;       /**< Length of buf in bytes */
    uint16_t byte_pos;  /**< Current byte index in buf */
    uint8_t  bit_pos;   /**< Next bit index within current byte (0..7) */
};

/**
 * @brief Initialize a bit reader to the start of a byte buffer
 *
 * Resets all cursors so that the first br_read_bits() call starts
 * reading from the MSB of buf[0].
 */
static void br_init(struct hs_bit_reader *br, const uint8_t *buf, uint16_t len)
{
    br->buf = buf;
    br->len = len;
    br->byte_pos = 0;
    br->bit_pos = 0;
}

/**
 * @brief Read @p nbits from the bit stream into @p val (MSB first)
 *
 * Shifts bits into @p val one at a time, MSB first, advancing the
 * internal cursor.  Returns -1 if the stream is exhausted before all
 * requested bits have been read, leaving @p val in a partially-
 * populated state (callers should treat this as a corrupt-data error).
 *
 * @param br    Bit reader state
 * @param nbits Number of bits to read (1..16)
 * @param val   Output: assembled value with the read bits right-aligned
 * @return 0 on success, -1 if not enough data remains
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

/**
 * @brief Compress a byte buffer using LZSS (heatshrink-style)
 *
 * Writes a 2-byte little-endian original-size header, then encodes
 * the input as a bit-packed LZSS stream.  For each input position the
 * algorithm performs a brute-force search over the sliding window
 * (up to HS_WINDOW_SIZE bytes back) to find the longest matching
 * byte sequence.  If the best match meets the MIN_MATCH threshold, a
 * back-reference is emitted; otherwise a literal byte is written.
 *
 * Time complexity: O(N * W * M) where N = input length, W = window
 * size, M = max match length.  All state is stack-local.
 */
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

    /* The output always begins with a 2-byte original-size header */
    if (dst_cap < 2) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
    }

    /* Store original size as little-endian uint16_t so the decoder
     * knows when to stop reading bits. */
    dst[0] = (uint8_t)(src_len & 0xFF);
    dst[1] = (uint8_t)((src_len >> 8) & 0xFF);

    if (src_len == 0) {
        *out_len = 2;
        return TIKU_KITS_TEXTCOMPRESSION_OK;
    }

    /* Bit stream starts immediately after the 2-byte header */
    bw_init(&bw, dst + 2, dst_cap - 2);

    i = 0;
    while (i < src_len) {
        /* Brute-force search for the longest match in the window */
        best_offset = 0;
        best_len = 0;
        search_start = (i > HS_WINDOW_SIZE) ? (i - HS_WINDOW_SIZE) : 0;

        /* Clamp max match to remaining input or HS_MAX_MATCH */
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
                    break;  /* Can't do better -- stop searching */
                }
            }
        }

        if (best_len >= HS_MIN_MATCH) {
            /* Emit back-reference: flag(0) + offset + length.
             * Offset is stored as (offset - 1) to use the full bit
             * range; length is stored as (length - MIN_MATCH). */
            if (bw_write_bits(&bw, 0, 1) < 0
                || bw_write_bits(&bw, best_offset - 1, HS_WINDOW_BITS) < 0
                || bw_write_bits(&bw, best_len - HS_MIN_MATCH,
                                 HS_LOOKAHEAD_BITS) < 0) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
            }
            i += best_len;
        } else {
            /* Emit literal: flag(1) + 8-bit byte value */
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

/**
 * @brief Decompress LZSS (heatshrink-style) compressed data
 *
 * Reads the 2-byte little-endian original-size header, then walks the
 * bit stream until @c orig_size output bytes have been produced.  Each
 * item is decoded by reading a 1-bit flag first:
 *   - Flag 1 (literal): read 8 bits and append to output.
 *   - Flag 0 (back-ref): read WINDOW_BITS offset and LOOKAHEAD_BITS
 *     length, then copy the referenced bytes from the already-decoded
 *     output.
 *
 * The back-reference copy is deliberately byte-by-byte (not memcpy)
 * so that overlapping matches, where the source and destination
 * regions overlap, produce the correct repeating pattern.
 */
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

    /* Minimum valid stream is the 2-byte header */
    if (src_len < 2) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    /* Reconstruct original size from little-endian header */
    orig_size = (uint16_t)src[0] | ((uint16_t)src[1] << 8);

    if (orig_size == 0) {
        *out_len = 0;
        return TIKU_KITS_TEXTCOMPRESSION_OK;
    }

    if (orig_size > dst_cap) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
    }

    /* Bit stream starts immediately after the 2-byte header */
    br_init(&br, src + 2, src_len - 2);
    di = 0;

    while (di < orig_size) {
        if (br_read_bits(&br, 1, &flag) < 0) {
            return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
        }

        if (flag) {
            /* Literal: next 8 bits are a raw byte value */
            if (br_read_bits(&br, 8, &literal) < 0) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
            }
            dst[di++] = (uint8_t)literal;
        } else {
            /* Back-reference: decode offset and length fields */
            if (br_read_bits(&br, HS_WINDOW_BITS, &offset) < 0
                || br_read_bits(&br, HS_LOOKAHEAD_BITS, &length) < 0) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
            }

            offset += 1;       /* Stored as offset-1 during encoding */
            length += HS_MIN_MATCH;

            /* Offset must not reach before the start of output */
            if (offset > di) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
            }

            ref_pos = di - offset;

            /* Byte-by-byte copy so overlapping matches (where
             * offset < length) correctly generate repeating patterns
             * rather than reading stale data via a bulk memcpy. */
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
