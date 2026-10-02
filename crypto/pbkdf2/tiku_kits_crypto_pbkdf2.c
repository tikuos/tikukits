/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_pbkdf2.c - PBKDF2 with HMAC-SHA1 (RFC 8018, 5.2)
 *
 * T_i = U_1 ^ ... ^ U_c, with U_1 = PRF(P, S || INT(i)) and U_j =
 * PRF(P, U_{j-1}).  The password is keyed into the HMAC once; each U
 * starts from a copy of that keyed state.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_crypto_pbkdf2.h"
#include "../hmac/tiku_kits_crypto_hmac_sha1.h"
#include <string.h>

int tiku_kits_crypto_pbkdf2_hmac_sha1(const uint8_t *pass, size_t pass_len,
                                      const uint8_t *salt, size_t salt_len,
                                      uint32_t iterations,
                                      uint8_t *out, size_t out_len) {
    tiku_kits_crypto_hmac_sha1_ctx_t keyed, ctx;
    uint8_t u[TIKU_KITS_CRYPTO_HMAC_SHA1_SIZE];
    uint8_t t[TIKU_KITS_CRYPTO_HMAC_SHA1_SIZE];
    uint8_t index[4];
    uint32_t block = 1U, j;
    size_t n, k;

    if ((pass == NULL && pass_len != 0U) || (salt == NULL && salt_len != 0U) ||
        (out == NULL && out_len != 0U)) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (iterations == 0U) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    (void)tiku_kits_crypto_hmac_sha1_init(&keyed, pass, pass_len);
    while (out_len > 0U) {
        index[0] = (uint8_t)(block >> 24);
        index[1] = (uint8_t)(block >> 16);
        index[2] = (uint8_t)(block >> 8);
        index[3] = (uint8_t)block;
        ctx = keyed;
        (void)tiku_kits_crypto_hmac_sha1_update(&ctx, salt, salt_len);
        (void)tiku_kits_crypto_hmac_sha1_update(&ctx, index, sizeof index);
        (void)tiku_kits_crypto_hmac_sha1_final(&ctx, u);
        memcpy(t, u, sizeof t);
        for (j = 1U; j < iterations; j++) {
            ctx = keyed;
            (void)tiku_kits_crypto_hmac_sha1_update(&ctx, u, sizeof u);
            (void)tiku_kits_crypto_hmac_sha1_final(&ctx, u);
            for (k = 0U; k < sizeof t; k++) {
                t[k] ^= u[k];
            }
        }
        n = out_len < sizeof t ? out_len : sizeof t;
        memcpy(out, t, n);
        out += n;
        out_len -= n;
        block++;
    }
    memset(&keyed, 0, sizeof keyed);
    memset(&ctx, 0, sizeof ctx);
    memset(u, 0, sizeof u);
    memset(t, 0, sizeof t);
    return TIKU_KITS_CRYPTO_OK;
}
