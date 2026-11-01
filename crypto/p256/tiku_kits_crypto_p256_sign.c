/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_p256_sign.c - ECDSA P-256 signatures with RFC 6979 nonces
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The nonce is HMAC-SHA-256 over the private key and the hash (RFC 6979
 * section 3.2), with the caller's random bytes as the additional data of
 * section 3.6.  Kept apart from the curve code so that a build verifying
 * signatures needs no HMAC.
 */

#include "tiku_kits_crypto_p256.h"
#include "../hmac/tiku_kits_crypto_hmac.h"
#include <string.h>

#define HMAC256 tiku_kits_crypto_hmac_sha256

/* The generator's state for one signature. */
typedef struct {
    const uint8_t *priv;
    const uint8_t *extra;
    size_t         extralen;
    uint8_t        K[32], V[32];
} nonce_gen_t;

/* Clears @p n bytes in a way the compiler keeps. */
static void
wipe(void *p, size_t n)
{
    volatile uint8_t *v = (volatile uint8_t *)p;

    while (n-- > 0u) {
        *v++ = 0u;
    }
}

/* K = HMAC_K(V || sep || x || h1 || extra), then V = HMAC_K(V), with sep 0
 * and then 1 (steps d to g); h1 is already bits2octets(h1). */
static void
nonce_start(nonce_gen_t *g, const uint8_t h1[32])
{
    uint8_t m[32 + 1 + 32 + 32 + 32], t[32];
    uint16_t n = (uint16_t)(97u + g->extralen);
    uint8_t sep;

    memset(g->V, 0x01, 32);
    memset(g->K, 0x00, 32);
    memcpy(m + 33, g->priv, 32);
    memcpy(m + 65, h1, 32);
    if (g->extralen > 0u) {
        memcpy(m + 97, g->extra, g->extralen);
    }
    for (sep = 0u; sep < 2u; sep++) {
        memcpy(m, g->V, 32);
        m[32] = sep;
        HMAC256(g->K, 32, m, n, t);
        memcpy(g->K, t, 32);
        HMAC256(g->K, 32, g->V, 32, t);
        memcpy(g->V, t, 32);
    }
    wipe(m, sizeof m);
    wipe(t, sizeof t);
}

/* The next candidate: after a refused one K = HMAC_K(V || 0) and
 * V = HMAC_K(V) first (step h.3); then V = HMAC_K(V) is the candidate. */
static void
nonce_next(void *ctx, const uint8_t h1[32], int again, uint8_t k[32])
{
    nonce_gen_t *g = ctx;
    uint8_t m[33], t[32];

    if (!again) {
        nonce_start(g, h1);
    } else {
        memcpy(m, g->V, 32);
        m[32] = 0x00;
        HMAC256(g->K, 32, m, 33, t);
        memcpy(g->K, t, 32);
        HMAC256(g->K, 32, g->V, 32, t);
        memcpy(g->V, t, 32);
    }
    HMAC256(g->K, 32, g->V, 32, t);
    memcpy(g->V, t, 32);
    memcpy(k, g->V, 32);
    wipe(m, sizeof m);
    wipe(t, sizeof t);
}

int
tiku_kits_crypto_p256_ecdsa_sign(const uint8_t priv[32],
                                 const uint8_t *hash, size_t hashlen,
                                 const uint8_t *extra, size_t extralen,
                                 uint8_t r[32], uint8_t s[32])
{
    nonce_gen_t g;
    int rc;

    if (extralen > 32u || (extralen > 0u && extra == NULL)) {
        return TIKU_KITS_CRYPTO_P256_BAD;
    }
    g.priv = priv;
    g.extra = extra;
    g.extralen = extralen;
    rc = tiku_kits_crypto_p256_ecdsa_sign_nonce(priv, hash, hashlen,
                                                nonce_next, &g, r, s);
    wipe(&g, sizeof g);
    return rc;
}
