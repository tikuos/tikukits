/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_textcompression_bpe.c - Byte Pair Encoding compression
 *
 * Iterative byte-pair replacement with bounded memory. The encoder
 * scans for the most frequent adjacent pair, assigns an unused byte
 * value, and replaces all occurrences. The merge table is stored in
 * the compressed output header for the decoder.
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

#include "tiku_kits_textcompression_bpe.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Find the most frequent adjacent byte pair in data
 * @param data    Working buffer
 * @param len     Length of data
 * @param best_a  Output: first byte of most frequent pair
 * @param best_b  Output: second byte of most frequent pair
 * @return Frequency of the most frequent pair (0 if len < 2)
 */
static uint16_t find_best_pair(const uint8_t *data, uint16_t len,
                               uint8_t *best_a, uint8_t *best_b)
{
    uint16_t best_count;
    uint16_t i;
    uint16_t j;
    uint8_t a;
    uint8_t b;
    uint16_t count;
    uint8_t visited[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT];

    if (len < 2) {
        return 0;
    }

    memset(visited, 0, len);
    best_count = 0;

    for (i = 0; i + 1 < len; i++) {
        if (visited[i]) {
            continue;
        }

        a = data[i];
        b = data[i + 1];
        count = 1;
        visited[i] = 1;

        for (j = i + 1; j + 1 < len; j++) {
            if (data[j] == a && data[j + 1] == b) {
                count++;
                visited[j] = 1;
            }
        }

        if (count > best_count) {
            best_count = count;
            *best_a = a;
            *best_b = b;
        }
    }

    return best_count;
}

/**
 * @brief Find an unused byte value not present in data
 * @param data Working buffer
 * @param len  Length of data
 * @param sym  Output: unused byte value
 * @return 0 if found, -1 if all 256 values are in use
 */
static int find_unused_symbol(const uint8_t *data, uint16_t len, uint8_t *sym)
{
    uint8_t used[256];
    uint16_t i;
    uint16_t v;

    memset(used, 0, sizeof(used));
    for (i = 0; i < len; i++) {
        used[data[i]] = 1;
    }

    for (v = 0; v < 256; v++) {
        if (!used[v]) {
            *sym = (uint8_t)v;
            return 0;
        }
    }

    return -1;
}

/**
 * @brief Replace all occurrences of pair (a, b) with sym, in place
 * @param data Working buffer (modified in place, shrinks)
 * @param len  Current length
 * @param a    First byte of pair
 * @param b    Second byte of pair
 * @param sym  Replacement symbol
 * @return New length after replacements
 */
static uint16_t replace_pair(uint8_t *data, uint16_t len,
                             uint8_t a, uint8_t b, uint8_t sym)
{
    uint16_t ri;
    uint16_t wi;

    ri = 0;
    wi = 0;

    while (ri < len) {
        if (ri + 1 < len && data[ri] == a && data[ri + 1] == b) {
            data[wi++] = sym;
            ri += 2;
        } else {
            data[wi++] = data[ri++];
        }
    }

    return wi;
}

/*---------------------------------------------------------------------------*/
/* ENCODING                                                                  */
/*---------------------------------------------------------------------------*/

int tiku_kits_textcompression_bpe_encode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len)
{
    uint8_t work[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT];
    uint16_t work_len;
    uint8_t merge_a[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES];
    uint8_t merge_b[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES];
    uint8_t merge_sym[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES];
    uint8_t num_merges;
    uint8_t best_a;
    uint8_t best_b;
    uint8_t new_sym;
    uint16_t freq;
    uint16_t header_size;
    uint16_t di;
    uint16_t i;

    if (src == NULL || dst == NULL || out_len == NULL) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_NULL;
    }

    if (src_len > TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW;
    }

    /* Copy input to working buffer */
    memcpy(work, src, src_len);
    work_len = src_len;
    num_merges = 0;

    /* Iteratively merge the most frequent pair */
    while (num_merges < TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES) {
        freq = find_best_pair(work, work_len, &best_a, &best_b);
        if (freq < 2) {
            break;
        }

        if (find_unused_symbol(work, work_len, &new_sym) < 0) {
            break;  /* All 256 byte values in use */
        }

        merge_a[num_merges] = best_a;
        merge_b[num_merges] = best_b;
        merge_sym[num_merges] = new_sym;
        num_merges++;

        work_len = replace_pair(work, work_len, best_a, best_b, new_sym);
    }

    /* Build output: [num_merges] [merge_table...] [compressed data] */
    header_size = 1 + (uint16_t)num_merges * 3;
    if (header_size + work_len > dst_cap) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
    }

    di = 0;
    dst[di++] = num_merges;

    for (i = 0; i < num_merges; i++) {
        dst[di++] = merge_a[i];
        dst[di++] = merge_b[i];
        dst[di++] = merge_sym[i];
    }

    memcpy(dst + di, work, work_len);
    di += work_len;

    *out_len = di;
    return TIKU_KITS_TEXTCOMPRESSION_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODING                                                                  */
/*---------------------------------------------------------------------------*/

int tiku_kits_textcompression_bpe_decode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len)
{
    uint8_t work[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT];
    uint16_t work_len;
    uint8_t num_merges;
    uint8_t merge_a[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES];
    uint8_t merge_b[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES];
    uint8_t merge_sym[TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES];
    uint16_t header_size;
    uint16_t payload_len;
    uint16_t i;
    int m;
    uint16_t new_len;

    if (src == NULL || dst == NULL || out_len == NULL) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_NULL;
    }

    if (src_len < 1) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    /* Read merge count */
    num_merges = src[0];
    if (num_merges > TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    header_size = 1 + (uint16_t)num_merges * 3;
    if (header_size > src_len) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    /* Read merge table */
    for (i = 0; i < num_merges; i++) {
        merge_a[i]   = src[1 + i * 3];
        merge_b[i]   = src[1 + i * 3 + 1];
        merge_sym[i] = src[1 + i * 3 + 2];
    }

    /* Copy compressed payload to working buffer */
    payload_len = src_len - header_size;
    if (payload_len > TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW;
    }

    memcpy(work, src + header_size, payload_len);
    work_len = payload_len;

    /* Apply merges in reverse: expand each merge symbol back to its pair */
    for (m = (int)num_merges - 1; m >= 0; m--) {
        new_len = 0;
        for (i = 0; i < work_len; i++) {
            if (work[i] == merge_sym[m]) {
                if (new_len + 2 > dst_cap) {
                    return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
                }
                dst[new_len++] = merge_a[m];
                dst[new_len++] = merge_b[m];
            } else {
                if (new_len + 1 > dst_cap) {
                    return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
                }
                dst[new_len++] = work[i];
            }
        }

        work_len = new_len;

        /* Copy back to work for next iteration (if not last) */
        if (m > 0) {
            if (work_len > TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW;
            }
            memcpy(work, dst, work_len);
        }
    }

    /* Handle case with no merges: copy directly to dst */
    if (num_merges == 0) {
        if (work_len > dst_cap) {
            return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
        }
        memcpy(dst, work, work_len);
    }

    *out_len = work_len;
    return TIKU_KITS_TEXTCOMPRESSION_OK;
}
