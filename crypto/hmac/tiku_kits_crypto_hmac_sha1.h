/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_hmac_sha1.h - HMAC-SHA1 (RFC 2104)
 *
 * WPA2 derives and checks its keys with it.  Unlike the one-shot
 * HMAC-SHA256 beside it, the state lives in a context, so it is
 * reentrant and takes its message in pieces.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_HMAC_SHA1_H_
#define TIKU_KITS_CRYPTO_HMAC_SHA1_H_

#include <stddef.h>
#include "../tiku_kits_crypto.h"
#include "../sha1/tiku_kits_crypto_sha1.h"

/** @brief HMAC-SHA1 output size in bytes. */
#define TIKU_KITS_CRYPTO_HMAC_SHA1_SIZE  20

/** @brief An HMAC in progress: the inner hash open, the outer keyed. */
typedef struct {
    tiku_kits_crypto_sha1_ctx_t inner;
    tiku_kits_crypto_sha1_ctx_t outer;
} tiku_kits_crypto_hmac_sha1_ctx_t;

/** @brief Key a new HMAC; a key over 64 bytes is hashed first.
 *         @return TIKU_KITS_CRYPTO_OK, or _ERR_NULL */
int tiku_kits_crypto_hmac_sha1_init(tiku_kits_crypto_hmac_sha1_ctx_t *ctx,
                                    const uint8_t *key, size_t key_len);

/** @brief Authenticate @p len more bytes. @return As _init() */
int tiku_kits_crypto_hmac_sha1_update(tiku_kits_crypto_hmac_sha1_ctx_t *ctx,
                                      const uint8_t *data, size_t len);

/** @brief Write the 20-byte MAC; the context is spent. @return As _init() */
int tiku_kits_crypto_hmac_sha1_final(tiku_kits_crypto_hmac_sha1_ctx_t *ctx,
                                     uint8_t *mac);

/** @brief One-shot HMAC-SHA1. @return As _init() */
int tiku_kits_crypto_hmac_sha1(const uint8_t *key, size_t key_len,
                               const uint8_t *data, size_t data_len,
                               uint8_t *mac);

#endif /* TIKU_KITS_CRYPTO_HMAC_SHA1_H_ */
