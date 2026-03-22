/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_hkdf.h - HKDF key derivation (RFC 5869)
 *
 * Implements HMAC-based Extract-and-Expand Key Derivation Function
 * using HMAC-SHA256 as the underlying PRF.  Includes the TLS 1.3
 * Expand-Label variant (RFC 8446 Section 7.1).  Designed for
 * ultra-low-power microcontrollers with zero heap allocation.
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

#ifndef TIKU_KITS_CRYPTO_HKDF_H_
#define TIKU_KITS_CRYPTO_HKDF_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_crypto.h"
#include "../hmac/tiku_kits_crypto_hmac.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** @brief HKDF-SHA256 PRK size in bytes (equals the hash length). */
#define TIKU_KITS_CRYPTO_HKDF_PRK_SIZE   32

/** @brief Maximum OKM length: 255 * HashLen (RFC 5869 Section 2.3). */
#define TIKU_KITS_CRYPTO_HKDF_MAX_OKM    (255U * 32U)

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief HKDF-Extract: derive a pseudorandom key from input keying
 *        material.
 *
 * Implements RFC 5869 Section 2.2:
 *   PRK = HMAC-SHA256(salt, IKM)
 *
 * If @p salt is NULL, a 32-byte all-zero salt is used as specified
 * by the RFC.
 *
 * @param[in]  salt      Optional salt value (NULL for default)
 * @param[in]  salt_len  Salt length in bytes (ignored when salt
 *                       is NULL)
 * @param[in]  ikm       Input keying material (must not be NULL)
 * @param[in]  ikm_len   IKM length in bytes
 * @param[out] prk       Output buffer for the 32-byte PRK
 *                       (must not be NULL)
 *
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if ikm or prk is NULL
 */
int tiku_kits_crypto_hkdf_extract(const uint8_t *salt,
                                  uint16_t salt_len,
                                  const uint8_t *ikm,
                                  uint16_t ikm_len,
                                  uint8_t *prk);

/**
 * @brief HKDF-Expand: expand a PRK into output keying material.
 *
 * Implements RFC 5869 Section 2.3:
 *   N = ceil(L / HashLen)
 *   T(0) = empty string
 *   T(i) = HMAC-SHA256(PRK, T(i-1) || info || i)  for i = 1..N
 *   OKM  = first L octets of T(1) || T(2) || ... || T(N)
 *
 * @param[in]  prk       Pseudorandom key, 32 bytes
 *                       (must not be NULL)
 * @param[in]  info      Optional context/application info
 *                       (NULL is treated as empty)
 * @param[in]  info_len  Info length in bytes (ignored when info
 *                       is NULL)
 * @param[out] okm       Output keying material buffer
 *                       (must not be NULL)
 * @param[in]  okm_len   Desired OKM length in bytes
 *                       (1..TIKU_KITS_CRYPTO_HKDF_MAX_OKM)
 *
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if prk or okm is NULL,
 *         TIKU_KITS_CRYPTO_ERR_PARAM if okm_len is 0 or
 *         exceeds 255 * 32
 */
int tiku_kits_crypto_hkdf_expand(const uint8_t *prk,
                                 const uint8_t *info,
                                 uint16_t info_len,
                                 uint8_t *okm,
                                 uint16_t okm_len);

/**
 * @brief TLS 1.3 HKDF-Expand-Label (RFC 8446 Section 7.1).
 *
 * Constructs an HkdfLabel structure and calls hkdf_expand():
 *
 *   struct {
 *       uint16 length = out_len;
 *       opaque label<7..255> = "tls13 " + label;
 *       opaque context<0..255> = context;
 *   } HkdfLabel;
 *
 * The caller supplies @p label WITHOUT the "tls13 " prefix; this
 * function prepends it automatically.
 *
 * @param[in]  secret       PRK / secret, 32 bytes
 *                          (must not be NULL)
 * @param[in]  label        Label string without "tls13 " prefix
 *                          (must not be NULL)
 * @param[in]  context      Transcript hash or empty context
 *                          (NULL is treated as zero-length)
 * @param[in]  context_len  Context length in bytes
 * @param[out] out          Output buffer (must not be NULL)
 * @param[in]  out_len      Desired output length in bytes
 *
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if secret, label, or out
 *         is NULL,
 *         TIKU_KITS_CRYPTO_ERR_PARAM if the constructed label
 *         exceeds the internal buffer or out_len is invalid
 */
int tiku_kits_crypto_hkdf_expand_label(const uint8_t *secret,
                                       const char *label,
                                       const uint8_t *context,
                                       uint16_t context_len,
                                       uint8_t *out,
                                       uint16_t out_len);

#endif /* TIKU_KITS_CRYPTO_HKDF_H_ */
