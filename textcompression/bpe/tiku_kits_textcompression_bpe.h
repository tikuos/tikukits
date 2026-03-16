/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_textcompression_bpe.h - Byte Pair Encoding compression
 *
 * Iteratively replaces the most frequent adjacent byte pair with a
 * new symbol. Simple, good compression on text data, and the merge
 * table is small. Bounded memory usage via compile-time limits.
 *
 * Compressed format:
 *   Byte 0:            number of merge rules (0..MAX_MERGES)
 *   Bytes 1..3N:       merge table (N entries of 3 bytes: a, b, symbol)
 *   Remaining bytes:   compressed payload
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

#ifndef TIKU_KITS_TEXTCOMPRESSION_BPE_H_
#define TIKU_KITS_TEXTCOMPRESSION_BPE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_textcompression.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of merge rules the encoder will produce.
 *
 * Each merge replaces the most frequent adjacent byte pair with a
 * single unused symbol, shrinking the data by one byte per occurrence.
 * More merges yield better compression at the cost of a larger
 * compressed-data header (3 bytes per rule: pair_a, pair_b, symbol)
 * and quadratically increasing encoding time (the pair frequency scan
 * is O(n) per merge round).
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES 16
 *   #include "tiku_kits_textcompression_bpe.h"
 * @endcode
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES
#define TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES 32
#endif

/**
 * @brief Maximum input size in bytes for BPE encode/decode.
 *
 * Determines the size of the internal working buffer, which is
 * stack-allocated inside both the encoder and decoder.  Also serves as
 * an upper bound on decompressed output size.  Choose a value that
 * fits comfortably in the target's available stack space; on MSP430
 * with 2 KB RAM, 256 is a conservative default.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT 512
 *   #include "tiku_kits_textcompression_bpe.h"
 * @endcode
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT
#define TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT 256
#endif

/*---------------------------------------------------------------------------*/
/* ENCODING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compress a byte buffer using Byte Pair Encoding
 *
 * Iteratively finds the most frequent adjacent byte pair in a working
 * copy of the data, assigns it an unused byte value as a merge symbol,
 * and replaces every occurrence of that pair with the new symbol.  The
 * process repeats until:
 *   - No pair occurs more than once, or
 *   - MAX_MERGES rules have been created, or
 *   - All 256 byte values are in use (no unused symbol available).
 *
 * The compressed output is formatted as:
 *   [num_merges (1 byte)] [merge_table (num_merges * 3 bytes)]
 *   [compressed payload]
 *
 * Time complexity is O(M * N) where M is the number of merge rounds
 * and N is the current working-buffer length.  Space complexity is
 * O(BPE_MAX_INPUT) on the stack for the working buffer plus small
 * arrays for the merge table.
 *
 * @param src     Input data to compress (must not be NULL; length must
 *                not exceed BPE_MAX_INPUT)
 * @param src_len Length of input data in bytes
 * @param dst     Output buffer for compressed data (must not be NULL)
 * @param dst_cap Capacity of @p dst in bytes; must accommodate the
 *                1-byte header + 3 * num_merges + compressed payload
 * @param out_len Output: number of bytes actually written to @p dst
 *                (must not be NULL)
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if src, dst, or out_len
 *         is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW if @p src_len exceeds
 *         BPE_MAX_INPUT,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if @p dst_cap is too
 *         small to hold the compressed output
 *
 * @code
 *   uint8_t src[] = "ABABABABCD";
 *   uint8_t dst[64];
 *   uint16_t out_len;
 *   tiku_kits_textcompression_bpe_encode(src, 10, dst, 64, &out_len);
 * @endcode
 */
int tiku_kits_textcompression_bpe_encode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len);

/*---------------------------------------------------------------------------*/
/* DECODING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Decompress Byte Pair Encoded data
 *
 * Reads the merge-count byte and the merge table from the compressed
 * header, then applies the merge rules in reverse order (last merge
 * first) to expand each merge symbol back into its original byte
 * pair.  After all merges are undone the working buffer contains the
 * original uncompressed data, which is copied to @p dst.
 *
 * The reverse-order application is essential: the last merge created
 * during encoding may have replaced a pair that itself contained
 * symbols introduced by earlier merges, so undoing the newest merge
 * first ensures correct recursive expansion.
 *
 * @param src     Compressed input data (must not be NULL)
 * @param src_len Length of compressed data in bytes; must be at least
 *                1 (the merge-count byte)
 * @param dst     Output buffer for decompressed data (must not be
 *                NULL)
 * @param dst_cap Capacity of @p dst in bytes; must be large enough
 *                to hold the fully expanded original data
 * @param out_len Output: number of bytes actually written to @p dst
 *                (must not be NULL)
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if src, dst, or out_len
 *         is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT if the header is
 *         malformed (e.g. merge count exceeds MAX_MERGES or the
 *         header extends past src_len),
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if @p dst_cap is too
 *         small to hold the decompressed output,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW if the expanded data
 *         exceeds BPE_MAX_INPUT during an intermediate merge pass
 */
int tiku_kits_textcompression_bpe_decode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len);

#endif /* TIKU_KITS_TEXTCOMPRESSION_BPE_H_ */
