/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_aeskw.c - AES key wrap (RFC 3394, section 2.2)
 *
 * Six passes over the n data blocks, each block enciphered with the running
 * integrity register A and the step counter t = n*j + i folded into A; the
 * unwrap runs them backwards and checks A against the initial value.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_crypto_aeskw.h"
#include "../aes128/tiku_kits_crypto_aes128.h"
#include <string.h>

#define KW_BLOCK    8U
#define KW_PASSES   6U

static const uint8_t kw_iv[KW_BLOCK] = {
    0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6
};

/** @brief The cipher for a 16- or 32-byte KEK. @return 0, or -1 */
static int kw_key(tiku_kits_crypto_aes128_ctx_t *ctx, const uint8_t *kek,
                  size_t kek_len) {
    if (kek_len == 16U) {
        return tiku_kits_crypto_aes128_init(ctx, kek) ==
               TIKU_KITS_CRYPTO_OK ? 0 : -1;
    }
    if (kek_len == 32U) {
        return tiku_kits_crypto_aes256_init(ctx, kek) ==
               TIKU_KITS_CRYPTO_OK ? 0 : -1;
    }
    return -1;
}

/** @brief A ^= t, t big-endian in the register's low bytes. */
static void kw_xor_t(uint8_t a[KW_BLOCK], uint32_t t) {
    a[4] ^= (uint8_t)(t >> 24);
    a[5] ^= (uint8_t)(t >> 16);
    a[6] ^= (uint8_t)(t >> 8);
    a[7] ^= (uint8_t)t;
}

int tiku_kits_crypto_aeskw_wrap(const uint8_t *kek, size_t kek_len,
                                const uint8_t *in, size_t len, uint8_t *out) {
    tiku_kits_crypto_aes128_ctx_t ctx;
    uint8_t b[16];
    size_t n, i, j;
    uint8_t *r;

    if (kek == NULL || in == NULL || out == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (len < 2U * KW_BLOCK || (len % KW_BLOCK) != 0U ||
        kw_key(&ctx, kek, kek_len) != 0) {
        return TIKU_KITS_CRYPTO_ERR_SIZE;
    }
    n = len / KW_BLOCK;
    r = out + KW_BLOCK;
    memmove(r, in, len);
    memcpy(b, kw_iv, KW_BLOCK);                 /* A, in b[0..7] */
    for (j = 0U; j < KW_PASSES; j++) {
        for (i = 1U; i <= n; i++) {
            memcpy(b + KW_BLOCK, r + (i - 1U) * KW_BLOCK, KW_BLOCK);
            (void)tiku_kits_crypto_aes128_encrypt(&ctx, b, b);
            kw_xor_t(b, (uint32_t)(n * j + i));
            memcpy(r + (i - 1U) * KW_BLOCK, b + KW_BLOCK, KW_BLOCK);
        }
    }
    memcpy(out, b, KW_BLOCK);
    memset(&ctx, 0, sizeof ctx);
    memset(b, 0, sizeof b);
    return TIKU_KITS_CRYPTO_OK;
}

int tiku_kits_crypto_aeskw_unwrap(const uint8_t *kek, size_t kek_len,
                                  const uint8_t *in, size_t len, uint8_t *out) {
    tiku_kits_crypto_aes128_ctx_t ctx;
    uint8_t b[16];
    size_t n, i, j;
    int ok;

    if (kek == NULL || in == NULL || out == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (len < 3U * KW_BLOCK || (len % KW_BLOCK) != 0U ||
        kw_key(&ctx, kek, kek_len) != 0) {
        return TIKU_KITS_CRYPTO_ERR_SIZE;
    }
    n = len / KW_BLOCK - 1U;
    memcpy(b, in, KW_BLOCK);                    /* A */
    memmove(out, in + KW_BLOCK, n * KW_BLOCK);
    for (j = KW_PASSES; j-- > 0U;) {
        for (i = n; i >= 1U; i--) {
            kw_xor_t(b, (uint32_t)(n * j + i));
            memcpy(b + KW_BLOCK, out + (i - 1U) * KW_BLOCK, KW_BLOCK);
            (void)tiku_kits_crypto_aes128_decrypt(&ctx, b, b);
            memcpy(out + (i - 1U) * KW_BLOCK, b + KW_BLOCK, KW_BLOCK);
        }
    }
    ok = memcmp(b, kw_iv, KW_BLOCK) == 0;
    memset(&ctx, 0, sizeof ctx);
    memset(b, 0, sizeof b);
    if (!ok) {
        memset(out, 0, n * KW_BLOCK);           /* nothing unchecked leaks */
        return TIKU_KITS_CRYPTO_ERR_CORRUPT;
    }
    return TIKU_KITS_CRYPTO_OK;
}
