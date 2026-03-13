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
 * Maximum number of merge rules the encoder will produce.
 * Each merge replaces one frequent byte pair with a single symbol.
 * More merges yield better compression but increase header size
 * (3 bytes per rule) and encoding time.
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES
#define TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_MERGES 32
#endif

/**
 * Maximum input size in bytes. Determines the size of the internal
 * working buffer (stack-allocated). Also limits decompression output.
 * Set according to available RAM.
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT
#define TIKU_KITS_TEXTCOMPRESSION_BPE_MAX_INPUT 256
#endif

/*---------------------------------------------------------------------------*/
/* ENCODING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compress data using Byte Pair Encoding
 *
 * Iteratively finds the most frequent adjacent byte pair in the data,
 * assigns it an unused byte value as a merge symbol, and replaces all
 * occurrences. Stops when no pair occurs more than once or when
 * MAX_MERGES rules have been created or no unused byte values remain.
 *
 * @param src     Input data to compress (max BPE_MAX_INPUT bytes)
 * @param src_len Length of input data in bytes
 * @param dst     Output buffer for compressed data
 * @param dst_cap Capacity of output buffer in bytes
 * @param out_len Output: number of bytes written to dst
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW if src_len exceeds
 *         BPE_MAX_INPUT,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if dst is too small
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
 * Reads the merge table from the header, then applies merge rules in
 * reverse order to expand the payload back to the original data.
 *
 * @param src     Compressed input data
 * @param src_len Length of compressed data in bytes
 * @param dst     Output buffer for decompressed data
 * @param dst_cap Capacity of output buffer in bytes
 * @param out_len Output: number of bytes written to dst
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT if header is malformed,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if dst is too small,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW if expanded data
 *         exceeds BPE_MAX_INPUT
 */
int tiku_kits_textcompression_bpe_decode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len);

#endif /* TIKU_KITS_TEXTCOMPRESSION_BPE_H_ */
