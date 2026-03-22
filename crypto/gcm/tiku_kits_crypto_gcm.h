/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_gcm.h - AES-128-GCM authenticated encryption
 *
 * Implements AES-128-GCM (NIST SP 800-38D) for embedded systems
 * using the existing AES-128-ECB block cipher as the underlying
 * primitive.  Provides authenticated encryption with associated
 * data (AEAD) suitable for TLS 1.3 record protection.  All
 * storage is statically allocated; no heap usage.
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

#ifndef TIKU_KITS_CRYPTO_GCM_H_
#define TIKU_KITS_CRYPTO_GCM_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_crypto.h"
#include "../aes128/tiku_kits_crypto_aes128.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** @brief GCM nonce size in bytes (96 bits, the only size TLS 1.3 uses). */
#define TIKU_KITS_CRYPTO_GCM_NONCE_SIZE  12

/** @brief GCM authentication tag size in bytes (128 bits). */
#define TIKU_KITS_CRYPTO_GCM_TAG_SIZE    16

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_crypto_gcm_ctx
 * @brief  AES-128-GCM context (192 bytes).
 *
 * Holds the expanded AES-128 round keys (176 bytes) and the
 * GHASH subkey H = AES(K, 0^128) (16 bytes).  Must be
 * initialized with @ref tiku_kits_crypto_gcm_init before use.
 *
 * Example:
 * @code
 *   tiku_kits_crypto_gcm_ctx_t ctx;
 *   uint8_t key[16]   = { ... };
 *   uint8_t nonce[12] = { ... };
 *   uint8_t pt[64], ct[64], tag[16];
 *
 *   tiku_kits_crypto_gcm_init(&ctx, key);
 *   tiku_kits_crypto_gcm_encrypt(&ctx, nonce, NULL, 0,
 *                                 pt, 64, ct, tag);
 * @endcode
 */
typedef struct tiku_kits_crypto_gcm_ctx {
    /** AES-128 context with expanded round keys (176 bytes). */
    tiku_kits_crypto_aes128_ctx_t aes;
    /** GHASH subkey H = AES(K, 0^128). */
    uint8_t h[16];
} tiku_kits_crypto_gcm_ctx_t;

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize AES-128-GCM context from a 16-byte key.
 *
 * Expands the AES-128 key schedule and computes the GHASH subkey
 * H = AES(K, 0^128).  Must be called once before any encrypt or
 * decrypt operation.
 *
 * @param[out] ctx  Context to initialize (must not be NULL).
 * @param[in]  key  16-byte encryption key (must not be NULL).
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if ctx or key is NULL.
 */
int tiku_kits_crypto_gcm_init(tiku_kits_crypto_gcm_ctx_t *ctx,
                               const uint8_t *key);

/**
 * @brief Encrypt and authenticate with AES-128-GCM.
 *
 * Encrypts @p pt_len bytes of plaintext using AES-128 in CTR mode
 * and computes a 16-byte GHASH authentication tag over the
 * associated data and ciphertext per NIST SP 800-38D.
 *
 * @param[in]  ctx     Initialized GCM context (must not be NULL).
 * @param[in]  nonce   12-byte nonce (must not be NULL).
 * @param[in]  aad     Additional authenticated data (may be NULL
 *                     if @p aad_len is 0).
 * @param[in]  aad_len Length of AAD in bytes.
 * @param[in]  pt      Plaintext (may be NULL if @p pt_len is 0).
 * @param[in]  pt_len  Plaintext length in bytes.
 * @param[out] ct      Ciphertext output, same size as plaintext
 *                     (may be NULL if @p pt_len is 0).
 * @param[out] tag     16-byte authentication tag output
 *                     (must not be NULL).
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if a required pointer is NULL.
 */
int tiku_kits_crypto_gcm_encrypt(
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *nonce,
    const uint8_t *aad,
    uint16_t aad_len,
    const uint8_t *pt,
    uint16_t pt_len,
    uint8_t *ct,
    uint8_t *tag);

/**
 * @brief Decrypt and verify with AES-128-GCM.
 *
 * Decrypts @p ct_len bytes of ciphertext using AES-128 in CTR
 * mode and verifies the 16-byte GHASH authentication tag.  Uses
 * constant-time tag comparison to prevent timing side-channels.
 *
 * If authentication fails the plaintext output buffer is zeroed
 * and @ref TIKU_KITS_CRYPTO_ERR_CORRUPT is returned.
 *
 * @param[in]  ctx     Initialized GCM context (must not be NULL).
 * @param[in]  nonce   12-byte nonce (must not be NULL).
 * @param[in]  aad     Additional authenticated data (may be NULL
 *                     if @p aad_len is 0).
 * @param[in]  aad_len Length of AAD in bytes.
 * @param[in]  ct      Ciphertext (may be NULL if @p ct_len is 0).
 * @param[in]  ct_len  Ciphertext length in bytes.
 * @param[in]  tag     16-byte authentication tag to verify
 *                     (must not be NULL).
 * @param[out] pt      Plaintext output, same size as ciphertext
 *                     (may be NULL if @p ct_len is 0).
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if a required pointer is NULL,
 *         TIKU_KITS_CRYPTO_ERR_CORRUPT if tag verification fails.
 */
int tiku_kits_crypto_gcm_decrypt(
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *nonce,
    const uint8_t *aad,
    uint16_t aad_len,
    const uint8_t *ct,
    uint16_t ct_len,
    const uint8_t *tag,
    uint8_t *pt);

#endif /* TIKU_KITS_CRYPTO_GCM_H_ */
