/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_sha1.h - SHA-1 hash (FIPS 180-4)
 *
 * For the protocols that still name it -- WPA2's key derivation and
 * handshake MIC above all -- not for new designs: SHA-1 is broken for
 * collisions.  Same shape as the SHA-256 kit; zero heap.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_SHA1_H_
#define TIKU_KITS_CRYPTO_SHA1_H_

#include <stddef.h>
#include "../tiku_kits_crypto.h"

/** @brief SHA-1 block size in bytes. */
#define TIKU_KITS_CRYPTO_SHA1_BLOCK_SIZE    64

/** @brief SHA-1 digest size in bytes. */
#define TIKU_KITS_CRYPTO_SHA1_DIGEST_SIZE   20

/** @brief Incremental SHA-1 state; init, update any number of times, final. */
typedef struct tiku_kits_crypto_sha1_ctx {
    uint32_t state[5];  /**< Intermediate hash value (H0..H4) */
    uint8_t  buffer[TIKU_KITS_CRYPTO_SHA1_BLOCK_SIZE];
    uint32_t count_lo;  /**< Total message length in bits (low 32) */
    uint32_t count_hi;  /**< Total message length in bits (high 32) */
    uint8_t  buf_len;   /**< Bytes currently buffered (0..63) */
} tiku_kits_crypto_sha1_ctx_t;

/** @brief Start a hash. @return TIKU_KITS_CRYPTO_OK, or _ERR_NULL */
int tiku_kits_crypto_sha1_init(tiku_kits_crypto_sha1_ctx_t *ctx);

/** @brief Hash @p len more bytes. @return TIKU_KITS_CRYPTO_OK, or _ERR_NULL */
int tiku_kits_crypto_sha1_update(tiku_kits_crypto_sha1_ctx_t *ctx,
                                 const uint8_t *data, size_t len);

/** @brief Pad, finish, and write the 20-byte digest; the context is spent.
 *         @return TIKU_KITS_CRYPTO_OK, or _ERR_NULL */
int tiku_kits_crypto_sha1_final(tiku_kits_crypto_sha1_ctx_t *ctx,
                                uint8_t *digest);

/** @brief One-shot SHA-1 of @p len bytes. @return As _final() */
int tiku_kits_crypto_sha1(const uint8_t *data, size_t len, uint8_t *digest);

#endif /* TIKU_KITS_CRYPTO_SHA1_H_ */
