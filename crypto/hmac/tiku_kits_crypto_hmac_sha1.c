/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_hmac_sha1.c - HMAC-SHA1 (RFC 2104)
 *
 * MAC = H((K ^ opad) || H((K ^ ipad) || message)), the key zero-padded to
 * the 64-byte block, or hashed to 20 bytes first when longer.  Both pads
 * are hashed at init, so the key is not kept, only the two half-hashes.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_crypto_hmac_sha1.h"
#include <string.h>

#define HMAC_IPAD   0x36U
#define HMAC_OPAD   0x5CU

int tiku_kits_crypto_hmac_sha1_init(tiku_kits_crypto_hmac_sha1_ctx_t *ctx,
                                    const uint8_t *key, size_t key_len) {
    uint8_t pad[TIKU_KITS_CRYPTO_SHA1_BLOCK_SIZE];
    unsigned i;

    if (ctx == NULL || (key == NULL && key_len != 0U)) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    memset(pad, 0, sizeof pad);
    if (key_len > sizeof pad) {
        (void)tiku_kits_crypto_sha1(key, key_len, pad);
    } else if (key_len != 0U) {
        memcpy(pad, key, key_len);
    }
    for (i = 0U; i < sizeof pad; i++) {
        pad[i] ^= HMAC_IPAD;
    }
    (void)tiku_kits_crypto_sha1_init(&ctx->inner);
    (void)tiku_kits_crypto_sha1_update(&ctx->inner, pad, sizeof pad);
    for (i = 0U; i < sizeof pad; i++) {
        pad[i] ^= HMAC_IPAD ^ HMAC_OPAD;
    }
    (void)tiku_kits_crypto_sha1_init(&ctx->outer);
    (void)tiku_kits_crypto_sha1_update(&ctx->outer, pad, sizeof pad);
    memset(pad, 0, sizeof pad);
    return TIKU_KITS_CRYPTO_OK;
}

int tiku_kits_crypto_hmac_sha1_update(tiku_kits_crypto_hmac_sha1_ctx_t *ctx,
                                      const uint8_t *data, size_t len) {
    if (ctx == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    return tiku_kits_crypto_sha1_update(&ctx->inner, data, len);
}

int tiku_kits_crypto_hmac_sha1_final(tiku_kits_crypto_hmac_sha1_ctx_t *ctx,
                                     uint8_t *mac) {
    uint8_t inner[TIKU_KITS_CRYPTO_SHA1_DIGEST_SIZE];

    if (ctx == NULL || mac == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    (void)tiku_kits_crypto_sha1_final(&ctx->inner, inner);
    (void)tiku_kits_crypto_sha1_update(&ctx->outer, inner, sizeof inner);
    (void)tiku_kits_crypto_sha1_final(&ctx->outer, mac);
    memset(inner, 0, sizeof inner);
    return TIKU_KITS_CRYPTO_OK;
}

int tiku_kits_crypto_hmac_sha1(const uint8_t *key, size_t key_len,
                               const uint8_t *data, size_t data_len,
                               uint8_t *mac) {
    tiku_kits_crypto_hmac_sha1_ctx_t ctx;
    int rc = tiku_kits_crypto_hmac_sha1_init(&ctx, key, key_len);

    if (rc == TIKU_KITS_CRYPTO_OK) {
        rc = tiku_kits_crypto_hmac_sha1_update(&ctx, data, data_len);
    }
    if (rc == TIKU_KITS_CRYPTO_OK) {
        rc = tiku_kits_crypto_hmac_sha1_final(&ctx, mac);
    }
    memset(&ctx, 0, sizeof ctx);
    return rc;
}
