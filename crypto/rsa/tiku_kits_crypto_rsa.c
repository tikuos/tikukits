/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_rsa.c - RSA signature verification (PKCS#1 v1.5 + PSS)
 *
 * Variable-width (<=4096-bit) bignum modular exponentiation via generic
 * Montgomery multiplication (per-modulus n0 and R^2 derived at runtime),
 * with RSASSA-PKCS1-v1_5 and RSASSA-PSS(MGF1-SHA-256) signature checks.
 * Verify-only and public-data-only -> not constant-time.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_crypto_rsa.h"
#include "../tiku_kits_crypto_bn.h"
#include "../sha256/tiku_kits_crypto_sha256.h"
#include <string.h>

#define ML   (TIKU_KITS_CRYPTO_RSA_MAX_BYTES / 4)   /* max limbs (128) */
#define HLEN 32                                      /* SHA-256 digest */

typedef uint32_t bn[ML];

/* ---- variable-width bignum (k limbs) ----------------------------------- */

static int bn_geqk(const uint32_t *a, const uint32_t *b, int k)
{
    int i;
    for (i = k - 1; i >= 0; i--) if (a[i] != b[i]) return a[i] > b[i];
    return 1;
}

static uint32_t bn_addk(uint32_t *d, const uint32_t *a, const uint32_t *b, int k)
{
    uint64_t c = 0; int i;
    for (i = 0; i < k; i++) { c += (uint64_t)a[i] + b[i]; d[i] = (uint32_t)c; c >>= 32; }
    return (uint32_t)c;
}

static uint32_t bn_subk(uint32_t *d, const uint32_t *a, const uint32_t *b, int k)
{
    uint64_t c = 0; int i;
    for (i = 0; i < k; i++) { uint64_t t = (uint64_t)a[i] - b[i] - c; d[i] = (uint32_t)t; c = (t >> 32) & 1; }
    return (uint32_t)c;
}

/* Parse up to 4*k big-endian bytes into k little-endian limbs (zero-padded). */
static void bn_from_be(uint32_t *a, int k, const uint8_t *be, size_t len)
{
    int i; size_t j;
    for (i = 0; i < k; i++) a[i] = 0;
    /* be[len-1] is the least-significant byte */
    for (j = 0; j < len; j++) {
        size_t pos = len - 1 - j;          /* byte index from LSB */
        a[pos >> 2] |= (uint32_t)be[j] << ((pos & 3) * 8);
    }
}

/* Serialise k limbs to outlen big-endian bytes (left zero-padded). */
static void bn_to_be(uint8_t *be, size_t outlen, const uint32_t *a, int k)
{
    size_t i;
    for (i = 0; i < outlen; i++) {
        size_t pos = outlen - 1 - i;       /* byte index from LSB */
        uint32_t limb = ((int)(pos >> 2) < k) ? a[pos >> 2] : 0;
        be[i] = (uint8_t)(limb >> ((pos & 3) * 8));
    }
}

/* x = 2x mod m  (k limbs). */
static void dbl_mod(uint32_t *x, const uint32_t *m, int k)
{
    uint32_t c = bn_addk(x, x, x, k);
    bn t;
    if (c || bn_geqk(x, m, k)) { bn_subk(t, x, m, k); memcpy(x, t, (size_t)k * 4); }
}

/* CIOS Montgomery multiply mod m (k limbs): out = a*b*R^-1 mod m. */
static void montmul(uint32_t *out, const uint32_t *a, const uint32_t *b,
                    const uint32_t *m, uint32_t n0, int k)
{
    /* static (not stack): these buffers are sized for RSA-4096 and the verify
     * path is deep + non-reentrant -- keeping them off the stack avoids
     * overflowing the (small) caller process stack on 32-bit targets. */
    static uint32_t t[ML + 1];
    int i, j;
    for (i = 0; i <= k; i++) t[i] = 0;

    for (i = 0; i < k; i++) {
        uint32_t c = 0, lo; uint32_t mu, hi; uint64_t v;
        for (j = 0; j < k; j++) {
            lo = t[j];
            tiku_kits_crypto_bn_mac(&lo, &c, a[j], b[i]);
            t[j] = lo;
        }
        v = (uint64_t)t[k] + c; t[k] = (uint32_t)v; hi = (uint32_t)(v >> 32);

        mu = (uint32_t)((uint64_t)t[0] * n0);
        /* The low limb this produces is zero by construction; only its
         * carry into the shifted-down loop below is wanted. */
        lo = t[0]; c = 0;
        tiku_kits_crypto_bn_mac(&lo, &c, mu, m[0]);
        for (j = 1; j < k; j++) {
            lo = t[j];
            tiku_kits_crypto_bn_mac(&lo, &c, mu, m[j]);
            t[j - 1] = lo;
        }
        v = (uint64_t)t[k] + c; t[k - 1] = (uint32_t)v; t[k] = hi + (uint32_t)(v >> 32);
    }
    {
        bn d; uint32_t bw = bn_subk(d, t, m, k);
        if (t[k] != 0 || bw == 0) memcpy(out, d, (size_t)k * 4);
        else                      memcpy(out, t, (size_t)k * 4);
    }
}

/* out = base^e mod m.  e is a big-endian small exponent.  k = limbs of m. */
static void modexp(uint32_t *out, const uint32_t *base, const uint32_t *m,
                   int k, const uint8_t *e, size_t elen)
{
    static uint32_t rr[ML], one[ML], base_m[ML], res[ML];   /* off-stack (see montmul) */
    uint32_t n0, inv;
    int i, started = 0; size_t bytei;

    /* n0 = -m^-1 mod 2^32 */
    inv = 1; for (i = 0; i < 5; i++) inv *= 2u - m[0] * inv; n0 = (uint32_t)(0u - inv);

    /* rr = R^2 mod m = 2^(2*32*k) mod m, via repeated doubling */
    for (i = 0; i < k; i++) rr[i] = 0;
    rr[0] = 1;
    for (i = 0; i < 64 * k; i++) dbl_mod(rr, m, k);

    for (i = 0; i < k; i++) one[i] = 0;
    one[0] = 1;

    montmul(base_m, base, rr, m, n0, k);     /* base -> Montgomery */
    montmul(res, one, rr, m, n0, k);         /* res  = Mont(1)     */

    for (bytei = 0; bytei < elen; bytei++) {
        int bit;
        for (bit = 7; bit >= 0; bit--) {
            int b = (e[bytei] >> bit) & 1;
            if (!started) { if (!b) continue; started = 1; }
            montmul(res, res, res, m, n0, k);
            if (b) montmul(res, res, base_m, m, n0, k);
        }
    }
    if (!started) { memcpy(out, one, (size_t)k * 4); return; }  /* e == 0 */
    montmul(out, res, one, m, n0, k);        /* out of Montgomery  */
}

/* Decrypt sig^e mod n into em[emlen] (emlen == modulus byte length). */
static int rsa_pubop(uint8_t *em, size_t emlen,
                     const uint8_t *n, size_t nlen,
                     const uint8_t *e, size_t elen,
                     const uint8_t *sig, size_t siglen)
{
    static bn nb, sb, mb;          /* off-stack (see montmul) */
    int k;
    if (nlen == 0 || nlen > TIKU_KITS_CRYPTO_RSA_MAX_BYTES) return -1;
    if (siglen > nlen) return -1;
    k = (int)((nlen + 3) / 4);
    bn_from_be(nb, k, n, nlen);
    bn_from_be(sb, k, sig, siglen);
    if (bn_geqk(sb, nb, k)) return -1;       /* sig must be < n */
    modexp(mb, sb, nb, k, e, elen);
    bn_to_be(em, emlen, mb, k);
    return 0;
}

/* ---- PKCS#1 v1.5 (SHA-256) ---------------------------------------------- */

/* DER DigestInfo prefixes (SEQUENCE{ SEQUENCE{ hashOID, NULL }, OCTET STRING }). */
static const uint8_t SHA256_DI[] = {
    0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,
    0x05,0x00,0x04,0x20 };
static const uint8_t SHA384_DI[] = {
    0x30,0x41,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x02,
    0x05,0x00,0x04,0x30 };

/* PKCS#1 v1.5 verify against a given DigestInfo prefix + hash. */
static int pkcs1_check(const uint8_t *n, size_t nlen, const uint8_t *e, size_t elen,
                       const uint8_t *sig, size_t siglen,
                       const uint8_t *di, size_t dilen, const uint8_t *hash, size_t hlen)
{
    static uint8_t em[TIKU_KITS_CRYPTO_RSA_MAX_BYTES];   /* off-stack */
    size_t  tlen = dilen + hlen;
    size_t  i, pad_end;

    if (rsa_pubop(em, nlen, n, nlen, e, elen, sig, siglen) != 0)
        return TIKU_KITS_CRYPTO_RSA_BAD;

    /* EM = 0x00 || 0x01 || PS(0xFF..) || 0x00 || T,  T = DigestInfo||hash */
    if (nlen < tlen + 11) return TIKU_KITS_CRYPTO_RSA_BAD;
    if (em[0] != 0x00 || em[1] != 0x01) return TIKU_KITS_CRYPTO_RSA_BAD;
    for (i = 2; i < nlen - tlen - 1; i++)
        if (em[i] != 0xFF) return TIKU_KITS_CRYPTO_RSA_BAD;
    pad_end = nlen - tlen - 1;
    if (em[pad_end] != 0x00) return TIKU_KITS_CRYPTO_RSA_BAD;
    if (memcmp(em + pad_end + 1, di, dilen) != 0)
        return TIKU_KITS_CRYPTO_RSA_BAD;
    if (memcmp(em + pad_end + 1 + dilen, hash, hlen) != 0)
        return TIKU_KITS_CRYPTO_RSA_BAD;
    return TIKU_KITS_CRYPTO_RSA_OK;
}

int tiku_kits_crypto_rsa_pkcs1_sha256_verify(const uint8_t *n, size_t nlen,
                                             const uint8_t *e, size_t elen,
                                             const uint8_t *sig, size_t siglen,
                                             const uint8_t hash32[32])
{
    return pkcs1_check(n, nlen, e, elen, sig, siglen,
                       SHA256_DI, sizeof SHA256_DI, hash32, 32);
}

int tiku_kits_crypto_rsa_pkcs1_sha384_verify(const uint8_t *n, size_t nlen,
                                             const uint8_t *e, size_t elen,
                                             const uint8_t *sig, size_t siglen,
                                             const uint8_t hash48[48])
{
    return pkcs1_check(n, nlen, e, elen, sig, siglen,
                       SHA384_DI, sizeof SHA384_DI, hash48, 48);
}

/* ---- PSS (MGF1-SHA-256) ------------------------------------------------- */

static void sha256(const uint8_t *d, size_t n, uint8_t out[32])
{
    tiku_kits_crypto_sha256_ctx_t c;
    tiku_kits_crypto_sha256_init(&c);
    tiku_kits_crypto_sha256_update(&c, d, n);
    tiku_kits_crypto_sha256_final(&c, out);
}

/* MGF1-SHA256: mask[masklen] from seed. */
static void mgf1(uint8_t *mask, size_t masklen, const uint8_t *seed, size_t seedlen)
{
    uint8_t cnt[4], h[32];
    uint32_t counter = 0;
    size_t pos = 0;
    while (pos < masklen) {
        tiku_kits_crypto_sha256_ctx_t c;
        size_t take;
        cnt[0] = (uint8_t)(counter >> 24); cnt[1] = (uint8_t)(counter >> 16);
        cnt[2] = (uint8_t)(counter >> 8);  cnt[3] = (uint8_t)counter;
        tiku_kits_crypto_sha256_init(&c);
        tiku_kits_crypto_sha256_update(&c, seed, seedlen);
        tiku_kits_crypto_sha256_update(&c, cnt, 4);
        tiku_kits_crypto_sha256_final(&c, h);
        take = masklen - pos; if (take > 32) take = 32;
        memcpy(mask + pos, h, take);
        pos += take; counter++;
    }
}

int tiku_kits_crypto_rsa_pss_sha256_verify(const uint8_t *n, size_t nlen,
                                           const uint8_t *e, size_t elen,
                                           const uint8_t *sig, size_t siglen,
                                           const uint8_t mhash32[32])
{
    static uint8_t em[TIKU_KITS_CRYPTO_RSA_MAX_BYTES];           /* off-stack */
    static uint8_t dbmask[TIKU_KITS_CRYPTO_RSA_MAX_BYTES];
    static uint8_t mprime[8 + 32 + TIKU_KITS_CRYPTO_RSA_MAX_BYTES];
    uint8_t hprime[32];
    const uint8_t *H, *db;
    size_t emlen, dblen, i, slen, off;
    int modbits, embits, lead;

    /* modBits = exact bit length of n */
    modbits = (int)(nlen * 8);
    {
        uint8_t b = n[0]; int z = 0;
        while (z < 8 && !((b >> (7 - z)) & 1)) z++;
        modbits -= z;
        if (modbits <= 0) return TIKU_KITS_CRYPTO_RSA_BAD;
    }
    embits = modbits - 1;
    emlen  = (size_t)((embits + 7) / 8);

    if (rsa_pubop(em, nlen, n, nlen, e, elen, sig, siglen) != 0)
        return TIKU_KITS_CRYPTO_RSA_BAD;

    /* The raw modexp output is nlen bytes; EM is its rightmost emLen bytes. */
    if (emlen < nlen) {
        for (i = 0; i < nlen - emlen; i++)
            if (em[i] != 0x00) return TIKU_KITS_CRYPTO_RSA_BAD;
        memmove(em, em + (nlen - emlen), emlen);
    } else if (emlen > nlen) {
        return TIKU_KITS_CRYPTO_RSA_BAD;
    }

    if (emlen < HLEN + 2) return TIKU_KITS_CRYPTO_RSA_BAD;
    if (em[emlen - 1] != 0xBC) return TIKU_KITS_CRYPTO_RSA_BAD;

    dblen = emlen - HLEN - 1;
    db = em;                 /* maskedDB */
    H  = em + dblen;         /* H        */

    /* leftmost (8*emLen - emBits) bits of the leading byte must be zero */
    lead = (int)(8 * emlen - (size_t)embits);
    if (lead > 0 && (db[0] >> (8 - lead)) != 0) return TIKU_KITS_CRYPTO_RSA_BAD;

    mgf1(dbmask, dblen, H, HLEN);
    for (i = 0; i < dblen; i++) dbmask[i] ^= db[i];      /* DB = maskedDB ^ mask */
    if (lead > 0) dbmask[0] &= (uint8_t)(0xFF >> lead);  /* clear leading bits   */

    /* DB = PS(0x00..) || 0x01 || salt ; recover salt length */
    off = 0;
    while (off < dblen && dbmask[off] == 0x00) off++;
    if (off >= dblen || dbmask[off] != 0x01) return TIKU_KITS_CRYPTO_RSA_BAD;
    off++;
    slen = dblen - off;

    /* M' = (0x00 * 8) || mHash || salt ; H' = SHA256(M') ; must equal H */
    memset(mprime, 0, 8);
    memcpy(mprime + 8, mhash32, HLEN);
    memcpy(mprime + 8 + HLEN, dbmask + off, slen);
    sha256(mprime, 8 + HLEN + slen, hprime);

    return (memcmp(hprime, H, HLEN) == 0) ? TIKU_KITS_CRYPTO_RSA_OK
                                          : TIKU_KITS_CRYPTO_RSA_BAD;
}
