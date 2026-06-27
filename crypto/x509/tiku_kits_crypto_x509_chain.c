/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_x509_chain.c - X.509 certificate chain validation
 *
 * Builds on the X.509 parser + the rsa/p256/sha256 verify kits to check a
 * server certificate chain to a baked-in trusted root: per-link signature
 * verification, issuer/subject linking, validity windows, and hostname.
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

#include "tiku_kits_crypto_x509.h"
#include "../rsa/tiku_kits_crypto_rsa.h"
#include "../p256/tiku_kits_crypto_p256.h"
#include "../sha256/tiku_kits_crypto_sha256.h"
#include <string.h>

#define OK   TIKU_KITS_CRYPTO_X509_OK
#define BAD  TIKU_KITS_CRYPTO_X509_BAD

static void sha256(const uint8_t *d, size_t n, uint8_t out[32])
{
    tiku_kits_crypto_sha256_ctx_t c;
    tiku_kits_crypto_sha256_init(&c);
    tiku_kits_crypto_sha256_update(&c, d, n);
    tiku_kits_crypto_sha256_final(&c, out);
}

/* Extract a 32-byte big-endian integer from a DER INTEGER body. */
static void der_int32(const uint8_t *b, size_t l, uint8_t out[32])
{
    memset(out, 0, 32);
    while (l > 1 && b[0] == 0x00) { b++; l--; }
    if (l > 32) { b += (l - 32); l = 32; }
    memcpy(out + (32 - l), b, l);
}

int tiku_kits_crypto_x509_verify_signed_by(const tiku_kits_crypto_x509_t *c,
                                           const tiku_kits_crypto_x509_t *iss)
{
    uint8_t h[32];
    if (c->tbs == NULL || c->sig == NULL) return BAD;
    sha256(c->tbs, c->tbs_len, h);

    switch (c->sig_alg) {
    case TIKU_X509_SIG_RSA_PKCS1_SHA256:
        if (iss->pk_alg != TIKU_X509_PK_RSA) return BAD;
        return tiku_kits_crypto_rsa_pkcs1_sha256_verify(
                   iss->rsa_n, iss->rsa_n_len, iss->rsa_e, iss->rsa_e_len,
                   c->sig, c->sig_len, h) == 0 ? OK : BAD;
    case TIKU_X509_SIG_RSA_PSS_SHA256:
        if (iss->pk_alg != TIKU_X509_PK_RSA) return BAD;
        return tiku_kits_crypto_rsa_pss_sha256_verify(
                   iss->rsa_n, iss->rsa_n_len, iss->rsa_e, iss->rsa_e_len,
                   c->sig, c->sig_len, h) == 0 ? OK : BAD;
    case TIKU_X509_SIG_ECDSA_SHA256: {
        const uint8_t *p = c->sig, *e = c->sig + c->sig_len, *rb, *sb;
        uint8_t r[32], s[32]; size_t rl, sl;
        if (iss->pk_alg != TIKU_X509_PK_EC_P256) return BAD;
        if (iss->ec_point_len < 65 || iss->ec_point[0] != 0x04) return BAD;
        /* ECDSA-Sig-Value ::= SEQUENCE { r INTEGER, s INTEGER } */
        if (e - p < 2 || p[0] != 0x30) return BAD;
        p += 2;
        if (p >= e || p[0] != 0x02) return BAD;
        rl = p[1]; rb = p + 2; p = rb + rl;
        if (p >= e || p[0] != 0x02) return BAD;
        sl = p[1]; sb = p + 2;
        der_int32(rb, rl, r); der_int32(sb, sl, s);
        return tiku_kits_crypto_p256_ecdsa_verify(
                   iss->ec_point + 1, iss->ec_point + 33, h, 32, r, s) == 0 ? OK : BAD;
    }
    default:
        return BAD;
    }
}

/* ---- validity-time parsing (ASN.1 UTCTime / GeneralizedTime -> epoch) --- */

static int dig2(const uint8_t *p) { return (p[0]-'0')*10 + (p[1]-'0'); }

/* days from 1970-01-01 to y-m-d (Howard Hinnant's civil algorithm). */
static long days_from_civil(long y, unsigned m, unsigned d)
{
    long era; unsigned yoe, doy, doe;
    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097L + (long)doe - 719468L;
}

/* Parse an ASN.1 time into Unix seconds; returns 0 on malformed. */
static uint64_t asn1_time(uint8_t tag, const uint8_t *b, size_t l)
{
    long year, mon, day, hh, mm, ss; const uint8_t *p = b;
    if (tag == 0x17 && l >= 13) {            /* UTCTime YYMMDDHHMMSSZ */
        int yy = dig2(p);
        year = (yy < 50) ? 2000 + yy : 1900 + yy; p += 2;
    } else if (tag == 0x18 && l >= 15) {     /* GeneralizedTime YYYYMMDD... */
        year = dig2(p) * 100 + dig2(p + 2); p += 4;
    } else {
        return 0;
    }
    mon = dig2(p); p += 2; day = dig2(p); p += 2;
    hh  = dig2(p); p += 2; mm  = dig2(p); p += 2; ss = dig2(p);
    if (mon < 1 || mon > 12 || day < 1 || day > 31) return 0;
    {
        long days = days_from_civil(year, (unsigned)mon, (unsigned)day);
        return (uint64_t)days * 86400ULL + (uint64_t)hh * 3600ULL
             + (uint64_t)mm * 60ULL + (uint64_t)ss;
    }
}

static int time_valid(const tiku_kits_crypto_x509_t *c, uint64_t now)
{
    uint64_t nb = asn1_time(c->nb_tag, c->not_before, c->not_before_len);
    uint64_t na = asn1_time(c->na_tag, c->not_after,  c->not_after_len);
    if (nb == 0 || na == 0) return 0;
    return now >= nb && now <= na;
}

static int dn_eq(const tiku_kits_crypto_x509_t *a, const uint8_t *dn, size_t dl)
{
    return a->subject_len == dl && memcmp(a->subject, dn, dl) == 0;
}

int tiku_kits_crypto_x509_verify_chain(const tiku_kits_crypto_x509_t *chain, int n,
                                       const tiku_kits_crypto_x509_t *roots, int nroots,
                                       const char *host, uint64_t now_unix)
{
    int i, r;
    if (n < 1) return BAD;

    if (host != NULL &&
        tiku_kits_crypto_x509_match_host(&chain[0], host) != OK)
        return BAD;

    if (now_unix != 0) {
        for (i = 0; i < n; i++)
            if (!time_valid(&chain[i], now_unix)) return BAD;
    }

    /* internal links: chain[i] signed by chain[i+1] */
    for (i = 0; i + 1 < n; i++) {
        if (!dn_eq(&chain[i + 1], chain[i].issuer, chain[i].issuer_len))
            return BAD;
        if (tiku_kits_crypto_x509_verify_signed_by(&chain[i], &chain[i + 1]) != OK)
            return BAD;
    }

    /* anchor: topmost cert must be signed by some trusted root */
    for (r = 0; r < nroots; r++) {
        if (now_unix != 0 && !time_valid(&roots[r], now_unix)) continue;
        if (!dn_eq(&roots[r], chain[n - 1].issuer, chain[n - 1].issuer_len))
            continue;
        if (tiku_kits_crypto_x509_verify_signed_by(&chain[n - 1], &roots[r]) == OK)
            return OK;
    }
    return BAD;
}
