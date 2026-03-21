/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec_hex.h - Hex encoder/decoder (binary <-> hex string)
 *
 * Converts between raw byte arrays and lowercase hexadecimal ASCII
 * strings.  Designed for embedded systems with zero heap allocation:
 * the caller supplies all buffers.
 *
 * The encoder produces a null-terminated lowercase hex string (two
 * ASCII characters per input byte).  The decoder accepts both upper-
 * and lowercase hex digits and writes the resulting raw bytes into a
 * caller-provided buffer.
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

#ifndef TIKU_KITS_CODEC_HEX_H_
#define TIKU_KITS_CODEC_HEX_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_codec.h"

/*---------------------------------------------------------------------------*/
/* MACROS                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Buffer size needed to hex-encode @p n bytes.
 *
 * Each input byte produces two hex characters, plus one byte for the
 * null terminator.
 *
 * @param n Number of source bytes
 * @return  Required destination buffer size (including '\0')
 */
#define TIKU_KITS_CODEC_HEX_ENCODE_LEN(n)  ((n) * 2 + 1)

/**
 * @brief Number of decoded bytes produced from @p n hex characters.
 *
 * Two hex characters decode to one byte.  The caller must ensure that
 * @p n is even before calling the decode function.
 *
 * @param n Number of hex characters (must be even)
 * @return  Number of decoded bytes
 */
#define TIKU_KITS_CODEC_HEX_DECODE_LEN(n)  ((n) / 2)

/*---------------------------------------------------------------------------*/
/* ENCODER                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encode a byte array to a lowercase hex string.
 *
 * Writes two lowercase hex characters per input byte into @p dst,
 * followed by a null terminator.  The total number of characters
 * written (excluding the null) is stored in @p out_len if it is
 * not NULL.
 *
 * @param src      Source bytes to encode (must not be NULL when
 *                 src_len > 0)
 * @param src_len  Number of source bytes
 * @param dst      Destination buffer for hex string (must not be
 *                 NULL)
 * @param dst_size Size of destination buffer in bytes; must be at
 *                 least TIKU_KITS_CODEC_HEX_ENCODE_LEN(src_len)
 * @param out_len  Output: number of hex characters written
 *                 (excluding null); may be NULL
 *
 * @return TIKU_KITS_CODEC_OK on success
 * @return TIKU_KITS_CODEC_ERR_NULL if a required pointer is NULL
 * @return TIKU_KITS_CODEC_ERR_OVERFLOW if dst_size is too small
 */
int tiku_kits_codec_hex_encode(const uint8_t *src,
                               uint16_t src_len,
                               char *dst,
                               uint16_t dst_size,
                               uint16_t *out_len);

/*---------------------------------------------------------------------------*/
/* DECODER                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Decode a hex string to a byte array.
 *
 * Converts pairs of hexadecimal ASCII characters (uppercase or
 * lowercase) into raw bytes.  The number of bytes written is
 * stored in @p out_len if it is not NULL.
 *
 * @param src      Hex string to decode (must not be NULL when
 *                 src_len > 0)
 * @param src_len  Number of hex characters (must be even)
 * @param dst      Destination buffer for decoded bytes (must not
 *                 be NULL)
 * @param dst_size Size of destination buffer in bytes; must be at
 *                 least TIKU_KITS_CODEC_HEX_DECODE_LEN(src_len)
 * @param out_len  Output: number of bytes written; may be NULL
 *
 * @return TIKU_KITS_CODEC_OK on success
 * @return TIKU_KITS_CODEC_ERR_NULL if a required pointer is NULL
 * @return TIKU_KITS_CODEC_ERR_PARAM if src_len is odd
 * @return TIKU_KITS_CODEC_ERR_OVERFLOW if dst_size is too small
 * @return TIKU_KITS_CODEC_ERR_CORRUPT if an invalid hex character
 *         is encountered
 */
int tiku_kits_codec_hex_decode(const char *src,
                               uint16_t src_len,
                               uint8_t *dst,
                               uint16_t dst_size,
                               uint16_t *out_len);

#endif /* TIKU_KITS_CODEC_HEX_H_ */
