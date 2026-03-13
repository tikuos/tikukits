/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_textcompression_rle.h - Run-Length Encoding compression
 *
 * Replaces runs of repeated bytes with (count, byte) pairs. Nearly zero
 * code size, zero extra RAM. Best suited for structured sensor logs with
 * long runs of identical values (e.g. zeros or stuck readings).
 *
 * Compressed format: sequence of 2-byte pairs [count][value], where
 * count is 1..255. Runs longer than 255 are split into multiple pairs.
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

#ifndef TIKU_KITS_TEXTCOMPRESSION_RLE_H_
#define TIKU_KITS_TEXTCOMPRESSION_RLE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_textcompression.h"

/*---------------------------------------------------------------------------*/
/* ENCODING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encode data using Run-Length Encoding
 *
 * Each run of identical bytes is replaced with a 2-byte pair:
 * (count, byte_value), where count is 1..255. Runs longer than
 * 255 bytes are split into multiple pairs.
 *
 * Worst case output size is 2 * src_len (no repeated bytes).
 *
 * @param src     Input data to compress
 * @param src_len Length of input data in bytes
 * @param dst     Output buffer for compressed data
 * @param dst_cap Capacity of output buffer in bytes
 * @param out_len Output: number of bytes written to dst
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if dst is too small
 *
 * @code
 *   uint8_t src[] = {0, 0, 0, 0, 1, 1, 2};
 *   uint8_t dst[14];
 *   uint16_t out_len;
 *   tiku_kits_textcompression_rle_encode(src, 7, dst, 14, &out_len);
 *   // dst = {4, 0, 2, 1, 1, 2}, out_len = 6
 * @endcode
 */
int tiku_kits_textcompression_rle_encode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len);

/*---------------------------------------------------------------------------*/
/* DECODING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Decode Run-Length Encoded data
 *
 * Expands (count, byte_value) pairs back into the original byte stream.
 *
 * @param src     Compressed input data
 * @param src_len Length of compressed data in bytes (must be even)
 * @param dst     Output buffer for decompressed data
 * @param dst_cap Capacity of output buffer in bytes
 * @param out_len Output: number of bytes written to dst
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT if src_len is odd or
 *         a count byte is zero,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if dst is too small
 */
int tiku_kits_textcompression_rle_decode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len);

#endif /* TIKU_KITS_TEXTCOMPRESSION_RLE_H_ */
