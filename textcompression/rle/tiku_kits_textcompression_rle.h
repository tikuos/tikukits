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
 * @brief Compress a byte buffer using Run-Length Encoding
 *
 * Scans the input left-to-right and replaces every maximal run of
 * identical bytes with a 2-byte pair: [count][value], where count is
 * in the range 1..255.  Runs longer than 255 bytes are automatically
 * split into consecutive pairs so that no information is lost.
 *
 * The algorithm is O(n) in time and uses zero auxiliary RAM beyond the
 * caller-provided source and destination buffers.
 *
 * Worst-case output size is exactly 2 * @p src_len (when every byte
 * differs from its neighbours).  Best-case is 2 bytes (entire input is
 * a single repeated value, provided the run is <= 255 bytes).  Callers
 * should size @p dst accordingly, or handle ERR_SIZE gracefully.
 *
 * @param src     Input data to compress (must not be NULL)
 * @param src_len Length of input data in bytes; 0 is valid (produces
 *                zero output)
 * @param dst     Output buffer for compressed data (must not be NULL)
 * @param dst_cap Capacity of @p dst in bytes; must be >= 2 * src_len
 *                in the worst case to guarantee success
 * @param out_len Output: number of bytes actually written to @p dst
 *                (must not be NULL)
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if src, dst, or out_len
 *         is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if @p dst_cap is too
 *         small to hold the compressed output
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
 * @brief Decompress a Run-Length Encoded byte buffer
 *
 * Reads consecutive [count][value] pairs from the compressed input and
 * expands each one by writing @c value into the destination buffer
 * @c count times.  The process repeats until all input pairs are
 * consumed.
 *
 * Because the compressed format uses 2-byte pairs, @p src_len must be
 * even; an odd length indicates truncated or corrupt data.  A count
 * byte of zero is likewise invalid (every pair must represent at least
 * one output byte).
 *
 * The algorithm is O(n) in the size of the decompressed output and
 * uses zero auxiliary RAM beyond the caller-provided buffers.
 *
 * @param src     Compressed input data (must not be NULL)
 * @param src_len Length of compressed data in bytes; must be even
 * @param dst     Output buffer for decompressed data (must not be NULL)
 * @param dst_cap Capacity of @p dst in bytes; must be large enough
 *                to hold the fully expanded output
 * @param out_len Output: number of bytes actually written to @p dst
 *                (must not be NULL)
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if src, dst, or out_len
 *         is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT if @p src_len is odd
 *         or a count byte is zero,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if @p dst_cap is too
 *         small to hold the decompressed output
 */
int tiku_kits_textcompression_rle_decode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len);

#endif /* TIKU_KITS_TEXTCOMPRESSION_RLE_H_ */
