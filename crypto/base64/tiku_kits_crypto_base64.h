/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_base64.h - Base64 encoding/decoding for embedded systems
 *
 * Implements RFC 4648 standard Base64 encoding and decoding.  Designed
 * for ultra-low-power MCUs with zero heap allocation -- the caller
 * provides all buffers.  Encode produces a null-terminated string;
 * decode writes raw bytes.
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

#ifndef TIKU_KITS_CRYPTO_BASE64_H_
#define TIKU_KITS_CRYPTO_BASE64_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_crypto.h"

/*---------------------------------------------------------------------------*/
/* MACROS                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the Base64-encoded output size for @p n input bytes.
 *
 * The result includes space for the null terminator.
 * Formula: ceil(n / 3) * 4 + 1
 *
 * @param n  Number of input bytes
 */
#define TIKU_KITS_CRYPTO_BASE64_ENCODE_LEN(n) \
    ((((n) + 2) / 3) * 4 + 1)

/**
 * @brief Compute the maximum decoded size for @p n encoded characters.
 *
 * The actual decoded length may be 1-2 bytes shorter due to padding.
 * Formula: n / 4 * 3
 *
 * @param n  Number of Base64-encoded characters (excluding null)
 */
#define TIKU_KITS_CRYPTO_BASE64_DECODE_LEN(n) \
    (((n) / 4) * 3)

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encode raw bytes to a Base64 string (RFC 4648).
 *
 * Writes a null-terminated Base64 string into @p dst.  The output
 * length (excluding the null terminator) is written to @p out_len
 * if it is not NULL.
 *
 * @param[in]  src       Source data to encode
 * @param[in]  src_len   Number of source bytes
 * @param[out] dst       Destination buffer for the encoded string
 * @param[in]  dst_size  Size of destination buffer in bytes
 * @param[out] out_len   Receives the number of characters written
 *                       (excluding null terminator); may be NULL
 *
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if src or dst is NULL,
 *         TIKU_KITS_CRYPTO_ERR_OVERFLOW if dst_size is too small
 */
int tiku_kits_crypto_base64_encode(const uint8_t *src,
                                    uint16_t src_len,
                                    char *dst,
                                    uint16_t dst_size,
                                    uint16_t *out_len);

/**
 * @brief Decode a Base64 string to raw bytes (RFC 4648).
 *
 * Decodes @p src_len characters from @p src into @p dst.  The number
 * of decoded bytes is written to @p out_len if it is not NULL.
 * Padding characters ('=') are handled; the input length should be
 * a multiple of 4.
 *
 * @param[in]  src       Base64-encoded input string
 * @param[in]  src_len   Number of encoded characters (must be % 4 == 0)
 * @param[out] dst       Destination buffer for decoded bytes
 * @param[in]  dst_size  Size of destination buffer in bytes
 * @param[out] out_len   Receives the number of bytes written;
 *                       may be NULL
 *
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if src or dst is NULL,
 *         TIKU_KITS_CRYPTO_ERR_OVERFLOW if dst_size is too small,
 *         TIKU_KITS_CRYPTO_ERR_CORRUPT if src contains invalid
 *         Base64 characters or has incorrect length
 */
int tiku_kits_crypto_base64_decode(const char *src,
                                    uint16_t src_len,
                                    uint8_t *dst,
                                    uint16_t dst_size,
                                    uint16_t *out_len);

#endif /* TIKU_KITS_CRYPTO_BASE64_H_ */
