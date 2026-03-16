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
 *
 * Scans all adjacent pairs in [data, data+len) and returns the pair
 * (a, b) that occurs the most times.  A visited[] array prevents
 * double-counting: once a pair starting at position i has been tallied
 * as part of an earlier identical pair's scan, position i is skipped
 * in the outer loop.
 *
 * O(n^2) worst case, bounded by BPE_MAX_INPUT.  The visited buffer is
 * stack-allocated at BPE_MAX_INPUT bytes.
 *
 * @param data    Working buffer (not modified)
 * @param len     Length of data in bytes
 * @param best_a  Output: first byte of the most frequent pair
 * @param best_b  Output: second byte of the most frequent pair
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
        /* Skip positions already counted by an earlier identical pair */
        if (visited[i]) {
            continue;
        }

        a = data[i];
        b = data[i + 1];
        count = 1;
        visited[i] = 1;

        /* Count remaining occurrences of the same pair */
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
 *
 * Builds a 256-byte presence bitmap from the working buffer, then
 * scans for the first byte value (0..255) that does not appear.  The
 * encoder uses this free value as the merge symbol for the current
 * round.  Returns -1 when all 256 values are already in use, which
 * terminates the merge loop.
 *
 * @param data Working buffer (not modified)
 * @param len  Length of data in bytes
 * @param sym  Output: the first byte value not present in data
 * @return 0 if an unused symbol was found, -1 if all 256 values are
 *         in use
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

    /* Return the lowest-numbered free byte value */
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
 *
 * Uses a read-index / write-index approach so that the buffer shrinks
 * in place without a second temporary array.  When the pair (a, b) is
 * found at the read position, a single @p sym byte is written and the
 * read index advances by two.  Otherwise the byte is copied as-is.
 * Because wi <= ri at every step, earlier writes never clobber unread
 * data.
 *
 * @param data Working buffer (modified in place; buffer shrinks)
 * @param len  Current length of valid data in @p data
 * @param a    First byte of the pair to replace
 * @param b    Second byte of the pair to replace
 * @param sym  Replacement symbol written in place of (a, b)
 * @return New length of the data after all replacements
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
            /* Pair matched -- collapse two bytes into one symbol */
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

/**
 * @brief Compress a byte buffer using Byte Pair Encoding
 *
 * Copies the input into a stack-allocated working buffer, then runs
 * up to MAX_MERGES rounds.  Each round: (1) find the most frequent
 * adjacent pair, (2) pick an unused byte value as the merge symbol,
 * (3) replace every occurrence of the pair in-place.  The merge table
 * is recorded for later serialisation.
 *
 * Once no pair has frequency >= 2, or all symbols are exhausted, the
 * loop terminates and the output is assembled as:
 *   [num_merges (1 byte)] [merge_table (3 * N)] [compressed payload]
 */
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

    /* Work on a copy so the caller's source buffer is never modified */
    memcpy(work, src, src_len);
    work_len = src_len;
    num_merges = 0;

    /* Iteratively merge the most frequent pair until no pair repeats,
     * the merge budget is exhausted, or no free symbol remains. */
    while (num_merges < TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES) {
        freq = find_best_pair(work, work_len, &best_a, &best_b);
        if (freq < 2) {
            break;  /* No pair occurs more than once -- stop */
        }

        if (find_unused_symbol(work, work_len, &new_sym) < 0) {
            break;  /* All 256 byte values in use -- stop */
        }

        merge_a[num_merges] = best_a;
        merge_b[num_merges] = best_b;
        merge_sym[num_merges] = new_sym;
        num_merges++;

        work_len = replace_pair(work, work_len, best_a, best_b, new_sym);
    }

    /* Assemble output: [num_merges] [merge_table...] [compressed data] */
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

/**
 * @brief Decompress Byte Pair Encoded data
 *
 * Parses the header (merge count + merge table), copies the compressed
 * payload into a working buffer, then iterates the merge table in
 * reverse order.  Each pass expands one merge symbol back to its
 * original two-byte pair, using @p dst as scratch for the expanded
 * output and copying the result back to @p work for the next pass.
 *
 * Reverse-order iteration is required because later merges may have
 * consumed symbols created by earlier merges; undoing the newest merge
 * first peels away each layer correctly.
 */
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

    /* At minimum the stream must contain the 1-byte merge count */
    if (src_len < 1) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    /* Read merge count and validate against compile-time limit */
    num_merges = src[0];
    if (num_merges > TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    header_size = 1 + (uint16_t)num_merges * 3;
    if (header_size > src_len) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    /* Read merge table: each entry is (pair_a, pair_b, symbol) */
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

    /* Apply merges in reverse: expand each merge symbol back to its
     * original pair.  Newest merge undone first because later merges
     * may contain symbols introduced by earlier merges. */
    for (m = (int)num_merges - 1; m >= 0; m--) {
        new_len = 0;
        for (i = 0; i < work_len; i++) {
            if (work[i] == merge_sym[m]) {
                /* Expand symbol back to its two constituent bytes */
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

        /* Copy expanded result back to work for the next merge pass.
         * Skip the copy after the last pass (m == 0) since dst already
         * holds the final output. */
        if (m > 0) {
            if (work_len > TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT) {
                return TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW;
            }
            memcpy(work, dst, work_len);
        }
    }

    /* When there are no merges the payload IS the original data;
     * copy it directly to the output buffer. */
    if (num_merges == 0) {
        if (work_len > dst_cap) {
            return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
        }
        memcpy(dst, work, work_len);
    }

    *out_len = work_len;
    return TIKU_KITS_TEXTCOMPRESSION_OK;
}
