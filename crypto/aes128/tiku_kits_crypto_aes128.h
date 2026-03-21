/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_aes128.h - AES-128 ECB encryption/decryption
 *
 * Minimal AES-128 in Electronic Codebook (ECB) mode for embedded
 * systems.  All storage is statically allocated; no heap usage.
 * Implements the FIPS 197 standard with 10 rounds.
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

#ifndef TIKU_KITS_CRYPTO_AES128_H_
#define TIKU_KITS_CRYPTO_AES128_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_crypto.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** @brief AES block size in bytes (128 bits). */
#define TIKU_KITS_CRYPTO_AES128_BLOCK_SIZE  16

/** @brief AES-128 key size in bytes (128 bits). */
#define TIKU_KITS_CRYPTO_AES128_KEY_SIZE    16

/** @brief Number of AES-128 rounds per FIPS 197. */
#define TIKU_KITS_CRYPTO_AES128_NUM_ROUNDS  10

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_crypto_aes128_ctx
 * @brief  AES-128 context holding the expanded round keys.
 *
 * The key schedule expands the 16-byte user key into 11 round keys
 * (one for the initial AddRoundKey plus one per round).  Each round
 * key is 16 bytes, giving 176 bytes total.  The context must be
 * initialized with @ref tiku_kits_crypto_aes128_init before use.
 *
 * Example:
 * @code
 *   tiku_kits_crypto_aes128_ctx_t ctx;
 *   uint8_t key[16] = { ... };
 *   uint8_t pt[16]  = { ... };
 *   uint8_t ct[16];
 *
 *   tiku_kits_crypto_aes128_init(&ctx, key);
 *   tiku_kits_crypto_aes128_encrypt(&ctx, pt, ct);
 * @endcode
 */
typedef struct tiku_kits_crypto_aes128_ctx {
    /** Expanded round keys: 11 x 16 = 176 bytes. */
    uint8_t round_keys[(TIKU_KITS_CRYPTO_AES128_NUM_ROUNDS + 1)
                       * TIKU_KITS_CRYPTO_AES128_BLOCK_SIZE];
} tiku_kits_crypto_aes128_ctx_t;

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Expand a 16-byte key into the AES-128 round-key schedule.
 *
 * Must be called once before any encrypt/decrypt operation.  The
 * expanded schedule is stored inside @p ctx and reused for every
 * subsequent block operation until the key changes.
 *
 * @param ctx  Context to initialize (must not be NULL).
 * @param key  16-byte encryption key (must not be NULL).
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if ctx or key is NULL.
 */
int tiku_kits_crypto_aes128_init(tiku_kits_crypto_aes128_ctx_t *ctx,
                                 const uint8_t *key);

/**
 * @brief Encrypt one 16-byte block in ECB mode.
 *
 * Applies the standard AES-128 forward cipher (10 rounds) to a
 * single 16-byte plaintext block and writes the result to
 * @p ciphertext.  The @p plaintext and @p ciphertext buffers may
 * alias (in-place encryption).
 *
 * @param ctx        Initialized AES-128 context (must not be NULL).
 * @param plaintext  16-byte input block (must not be NULL).
 * @param ciphertext 16-byte output block (must not be NULL).
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if any pointer is NULL.
 */
int tiku_kits_crypto_aes128_encrypt(
    const tiku_kits_crypto_aes128_ctx_t *ctx,
    const uint8_t *plaintext,
    uint8_t *ciphertext);

/**
 * @brief Decrypt one 16-byte block in ECB mode.
 *
 * Applies the standard AES-128 inverse cipher (10 rounds) to a
 * single 16-byte ciphertext block and writes the result to
 * @p plaintext.  The @p ciphertext and @p plaintext buffers may
 * alias (in-place decryption).
 *
 * @param ctx        Initialized AES-128 context (must not be NULL).
 * @param ciphertext 16-byte input block (must not be NULL).
 * @param plaintext  16-byte output block (must not be NULL).
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if any pointer is NULL.
 */
int tiku_kits_crypto_aes128_decrypt(
    const tiku_kits_crypto_aes128_ctx_t *ctx,
    const uint8_t *ciphertext,
    uint8_t *plaintext);

#endif /* TIKU_KITS_CRYPTO_AES128_H_ */
