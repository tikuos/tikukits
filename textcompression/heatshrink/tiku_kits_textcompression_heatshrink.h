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
 * Sliding window size in bits. Window holds 1 << WINDOW_BITS bytes.
 * Larger windows find better matches but use more search time.
 * Default 8 gives a 256-byte window (good balance for MSP430).
 * Valid range: 4..12.
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_WINDOW_BITS
#define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_WINDOW_BITS 8
#endif

/**
 * Lookahead size in bits. Maximum match length is
 * (1 << LOOKAHEAD_BITS) - 1 + MIN_MATCH.
 * Default 4 gives max match length of 17 bytes.
 * Must be less than WINDOW_BITS.
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_LOOKAHEAD_BITS
#define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_LOOKAHEAD_BITS 4
#endif

/**
 * Minimum match length. Matches shorter than this are encoded as
 * literals. Default 2 (a back-reference must save at least 1 bit
 * over two separate literals).
 */
#ifndef TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_MIN_MATCH
#define TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_MIN_MATCH 2
#endif

/*---------------------------------------------------------------------------*/
/* ENCODING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compress data using LZSS (heatshrink-style)
 *
 * Searches a sliding window for repeated byte sequences and encodes
 * them as compact back-references. Falls back to literals for
 * unmatched bytes. All state is on the stack.
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
 * Reads the 2-byte original-size header, then decodes the bit-packed
 * LZSS stream of literals and back-references.
 *
 * @param src     Compressed input data
 * @param src_len Length of compressed data in bytes
 * @param dst     Output buffer for decompressed data
 * @param dst_cap Capacity of output buffer in bytes
 * @param out_len Output: number of bytes written to dst
 * @return TIKU_KITS_TEXTCOMPRESSION_OK on success,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT if stream is malformed,
 *         TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE if dst is too small
 */
int tiku_kits_textcompression_heatshrink_decode(
    const uint8_t *src, uint16_t src_len,
    uint8_t *dst, uint16_t dst_cap,
    uint16_t *out_len);

#endif /* TIKU_KITS_TEXTCOMPRESSION_HEATSHRINK_H_ */
