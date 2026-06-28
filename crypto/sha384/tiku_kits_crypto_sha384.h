/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_sha384.h - SHA-384 cryptographic hash (FIPS 180-4)
 *
 * SHA-384 is SHA-512 with a distinct initial hash value, truncated to 384
 * bits.  Needed to verify P-384 certificate signatures (ecdsa-with-SHA384)
 * and TLS 1.3 ecdsa_secp384r1_sha384 CertificateVerify.  Same streaming API
 * shape as the SHA-256 kit.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_SHA384_H_
#define TIKU_KITS_CRYPTO_SHA384_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIKU_KITS_CRYPTO_SHA384_BLOCK_SIZE   128
#define TIKU_KITS_CRYPTO_SHA384_DIGEST_SIZE  48

/** Streaming SHA-384 context (SHA-512 state truncated on output). */
typedef struct tiku_kits_crypto_sha384_ctx {
    uint64_t state[8];   /**< Intermediate hash value (H0..H7)        */
    uint8_t  buffer[TIKU_KITS_CRYPTO_SHA384_BLOCK_SIZE];
    uint64_t count;      /**< Total message length in bits            */
    uint8_t  buf_len;    /**< Bytes currently buffered (0..127)       */
} tiku_kits_crypto_sha384_ctx_t;

/** Initialise a context with the SHA-384 IV. */
int tiku_kits_crypto_sha384_init(tiku_kits_crypto_sha384_ctx_t *ctx);

/** Feed @p len bytes of @p data into the running hash. */
int tiku_kits_crypto_sha384_update(tiku_kits_crypto_sha384_ctx_t *ctx,
                                   const uint8_t *data, size_t len);

/** Finalise and write the 48-byte digest. */
int tiku_kits_crypto_sha384_final(tiku_kits_crypto_sha384_ctx_t *ctx,
                                  uint8_t *digest);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_SHA384_H_ */
