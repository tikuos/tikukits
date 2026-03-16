/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_textcompression_heatshrink.h - LZSS compression for embedded
 *
 * Heatshrink-style LZSS compression with a configurable sliding window
 * and lookahead. Both compressor and decompressor run in static memory
 * with no malloc. Suitable for general-purpose compression on MSP430.
 *
 * Compressed format:
 *   Bytes 0-1: original size (uint16_t, little-endian)
 *   Bytes 2+:  bit-packed LZSS stream
 *     Each item begins with a 1-bit flag:
 *       1 + 8-bit literal byte
 *       0 + WINDOW_BITS offset + LOOKAHEAD_BITS length
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

#ifndef TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_H_
#define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_textcompression.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Sliding window size expressed as a bit count.
 *
 * The actual window holds (1 << WINDOW_BITS) bytes.  A larger window
 * lets the compressor find matches further back in the input, which
 * improves the compression ratio, but linearly increases the time
 * spent searching for matches (the search is brute-force O(W * N)
 * where W is the window size).
 *
 * Default 8 gives a 256-byte window -- a good balance between
 * compression quality and CPU time on MSP430.  Valid range: 4..12.
 *
 * Override before including this header to change the window:
 * @code
 *   #define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_WINDOW_BITS 10
 *   #include "tiku_kits_textcompression_heatshrink.h"
 * @endcode
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_WINDOW_BITS
#define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_WINDOW_BITS 8
#endif

/**
 * @brief Lookahead size expressed as a bit count.
 *
 * Controls the maximum match length the compressor can encode:
 *   max_match = (1 << LOOKAHEAD_BITS) - 1 + MIN_MATCH
 * With the default value of 4, max_match = 15 + 2 = 17 bytes.
 *
 * Must be strictly less than WINDOW_BITS (a match length wider than
 * the window itself is nonsensical).
 *
 * Override before including this header to change the lookahead:
 * @code
 *   #define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_LOOKAHEAD_BITS 5
 *   #include "tiku_kits_textcompression_heatshrink.h"
 * @endcode
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_LOOKAHEAD_BITS
#define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_LOOKAHEAD_BITS 4
#endif

/**
 * @brief Minimum match length for a back-reference to be worthwhile.
 *
 * Matches shorter than this threshold are encoded as individual
 * literal bytes instead.  The default of 2 means a back-reference
 * must replace at least 2 bytes to justify the overhead of the
 * (offset, length) encoding (1 + WINDOW_BITS + LOOKAHEAD_BITS bits
 * for a reference vs. 1 + 8 bits per literal).
 *
 * Override before including this header to change the threshold:
 * @code
 *   #define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_MIN_MATCH 3
 *   #include "tiku_kits_textcompression_heatshrink.h"
 * @endcode
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_MIN_MATCH
#define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_MIN_MATCH 2
#endif

/*---------------------------------------------------------------------------*/
/* ENCODING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compress a byte buffer using LZSS (heatshrink-style)
 *
 * For each input position the compressor searches the preceding
 * sliding window (up to WINDOW_SIZE bytes back) for the longest byte
 * sequence that matches the data at the current position.  If a match
 * of at least MIN_MATCH bytes is found, a compact back-reference
 * (offset, length) is emitted; otherwise a literal byte is written.
 *
 * The output is a 2-byte little-endian header containing the original
 * size, followed by a bit-packed LZSS stream where each item starts
 * with a 1-bit flag:
 *   - Flag 1: literal (8 bits for the byte value)
 *   - Flag 0: back-reference (WINDOW_BITS offset + LOOKAHEAD_BITS length)
 *
 * All compressor state lives on the stack; no heap or static buffers
 * are used.  Time complexity is O(N * W) where N is the input length
 * and W is the window size.
 *
 * @param src     Input data to compress (must not be NULL)
 * @param src_len Length of input data in bytes; 0 is valid (produces
 *                a 2-byte header only)
 * @param dst     Output buffer for compressed data (must not be NULL);
 *                must have at least 2 bytes for the header
 * @param dst_cap Capacity of @p dst in bytes
 * @param out_len Output: number of bytes actually written to @p dst
 *                (must not be NULL)
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if src, dst, or out_len
 *         is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if @p dst_cap is too
 *         small to hold the compressed output (including the 2-byte
 *         header)
 *
 * @code
 *   uint8_t src[] = "ABCABCABC";
 *   uint8_t dst[32];
 *   uint16_t out_len;
 *   tiku_kits_textcompression_heatshrink_encode(src, 9, dst, 32, &out_len);
 * @endcode
 */
int tiku_kits_textcompression_heatshrink_encode(
    const uint8_t *src, uint16_t src_len,
    uint8_t *dst, uint16_t dst_cap,
    uint16_t *out_len);

/*---------------------------------------------------------------------------*/
/* DECODING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Decompress LZSS (heatshrink-style) compressed data
 *
 * Reads the 2-byte little-endian header to learn the original size,
 * then walks the bit-packed LZSS stream.  For each item the 1-bit
 * flag is read:
 *   - Flag 1: read an 8-bit literal and append it to the output.
 *   - Flag 0: read a WINDOW_BITS offset and LOOKAHEAD_BITS length,
 *     then copy @c (length + MIN_MATCH) bytes starting @c (offset + 1)
 *     bytes back in the already-decompressed output.
 *
 * The byte-by-byte copy in the back-reference path deliberately avoids
 * memcpy so that overlapping matches (where offset < length) produce
 * the correct repeating pattern.
 *
 * @param src     Compressed input data (must not be NULL); must start
 *                with the 2-byte original-size header
 * @param src_len Length of compressed data in bytes; must be >= 2
 * @param dst     Output buffer for decompressed data (must not be
 *                NULL)
 * @param dst_cap Capacity of @p dst in bytes; must be >= the original
 *                size encoded in the header
 * @param out_len Output: number of bytes actually written to @p dst
 *                (must not be NULL)
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if src, dst, or out_len
 *         is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT if @p src_len < 2 or
 *         the bit stream is truncated / malformed,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if @p dst_cap is too
 *         small to hold the decompressed output
 */
int tiku_kits_textcompression_heatshrink_decode(
    const uint8_t *src, uint16_t src_len,
    uint8_t *dst, uint16_t dst_cap,
    uint16_t *out_len);

#endif /* TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_H_ */
