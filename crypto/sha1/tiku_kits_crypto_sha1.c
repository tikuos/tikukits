/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_sha1.c - SHA-1 hash (FIPS 180-4, section 6.1)
 *
 * Eighty rounds over a sixteen-word rolling schedule, so a block costs
 * 64 bytes of stack rather than 320.  Padding and length as SHA-256's.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_crypto_sha1.h"
#include <string.h>

#define ROTL(x, n)  (((x) << (n)) | ((x) >> (32 - (n))))

static uint32_t be32_load(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void be32_store(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

/** @brief One 64-byte block into the state (FIPS 180-4, 6.1.2). */
static void sha1_compress(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[16], a, b, c, d, e, f, k, t;
    unsigned i;

    for (i = 0U; i < 16U; i++) {
        w[i] = be32_load(block + 4U * i);
    }
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    for (i = 0U; i < 80U; i++) {
        if (i >= 16U) {
            /* W[t] from W[t-3], W[t-8], W[t-14], W[t-16], in place. */
            t = w[(i + 13U) & 15U] ^ w[(i + 8U) & 15U] ^
                w[(i + 2U) & 15U] ^ w[i & 15U];
            w[i & 15U] = ROTL(t, 1);
        }
        if (i < 20U) {
            f = (b & c) | (~b & d);
            k = 0x5A827999UL;
        } else if (i < 40U) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1UL;
        } else if (i < 60U) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCUL;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6UL;
        }
        t = ROTL(a, 5) + f + e + k + w[i & 15U];
        e = d;
        d = c;
        c = ROTL(b, 30);
        b = a;
        a = t;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

int tiku_kits_crypto_sha1_init(tiku_kits_crypto_sha1_ctx_t *ctx) {
    if (ctx == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    ctx->state[0] = 0x67452301UL;
    ctx->state[1] = 0xEFCDAB89UL;
    ctx->state[2] = 0x98BADCFEUL;
    ctx->state[3] = 0x10325476UL;
    ctx->state[4] = 0xC3D2E1F0UL;
    ctx->count_lo = 0UL;
    ctx->count_hi = 0UL;
    ctx->buf_len = 0U;
    return TIKU_KITS_CRYPTO_OK;
}

int tiku_kits_crypto_sha1_update(tiku_kits_crypto_sha1_ctx_t *ctx,
                                 const uint8_t *data, size_t len) {
    uint32_t bits;

    if (ctx == NULL || (data == NULL && len != 0U)) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    bits = (uint32_t)len << 3;
    ctx->count_lo += bits;
    if (ctx->count_lo < bits) {
        ctx->count_hi++;
    }
    ctx->count_hi += (uint32_t)((uint32_t)len >> 29);

    while (len > 0U) {
        size_t space = TIKU_KITS_CRYPTO_SHA1_BLOCK_SIZE - ctx->buf_len;
        size_t copy = (len < space) ? len : space;

        memcpy(ctx->buffer + ctx->buf_len, data, copy);
        ctx->buf_len += (uint8_t)copy;
        data += copy;
        len -= copy;
        if (ctx->buf_len == TIKU_KITS_CRYPTO_SHA1_BLOCK_SIZE) {
            sha1_compress(ctx->state, ctx->buffer);
            ctx->buf_len = 0U;
        }
    }
    return TIKU_KITS_CRYPTO_OK;
}

int tiku_kits_crypto_sha1_final(tiku_kits_crypto_sha1_ctx_t *ctx,
                                uint8_t *digest) {
    unsigned i;

    if (ctx == NULL || digest == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    ctx->buffer[ctx->buf_len++] = 0x80U;
    if (ctx->buf_len > 56U) {
        memset(ctx->buffer + ctx->buf_len, 0,
               TIKU_KITS_CRYPTO_SHA1_BLOCK_SIZE - ctx->buf_len);
        sha1_compress(ctx->state, ctx->buffer);
        ctx->buf_len = 0U;
    }
    memset(ctx->buffer + ctx->buf_len, 0, 56U - ctx->buf_len);
    be32_store(ctx->buffer + 56, ctx->count_hi);
    be32_store(ctx->buffer + 60, ctx->count_lo);
    sha1_compress(ctx->state, ctx->buffer);
    for (i = 0U; i < 5U; i++) {
        be32_store(digest + 4U * i, ctx->state[i]);
    }
    return TIKU_KITS_CRYPTO_OK;
}

int tiku_kits_crypto_sha1(const uint8_t *data, size_t len, uint8_t *digest) {
    tiku_kits_crypto_sha1_ctx_t ctx;
    int rc = tiku_kits_crypto_sha1_init(&ctx);

    if (rc == TIKU_KITS_CRYPTO_OK) {
        rc = tiku_kits_crypto_sha1_update(&ctx, data, len);
    }
    if (rc == TIKU_KITS_CRYPTO_OK) {
        rc = tiku_kits_crypto_sha1_final(&ctx, digest);
    }
    return rc;
}
