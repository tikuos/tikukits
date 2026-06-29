/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_x509.c - minimal X.509 (DER) certificate parser
 *
 * A small zero-copy DER walker that extracts exactly the fields a TLS 1.3
 * client needs to authenticate a server certificate chain.  Not a general
 * ASN.1 library: it understands the specific Certificate / TBSCertificate /
 * SubjectPublicKeyInfo / extension shapes RFC 5280 mandates.
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
#include <string.h>

#define OK   TIKU_KITS_CRYPTO_X509_OK
#define BAD  TIKU_KITS_CRYPTO_X509_BAD

/* ASN.1 tags we care about. */
#define T_BOOL   0x01
#define T_INT    0x02
#define T_BIT    0x03
#define T_OCT    0x04
#define T_OID    0x06
#define T_SEQ    0x30
#define T_SET    0x31
#define T_CTX0   0xA0   /* [0] EXPLICIT version  */
#define T_CTX3   0xA3   /* [3] EXPLICIT extensions */

static const uint8_t OID_RSA[]        = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01};
static const uint8_t OID_EC_PK[]      = {0x2a,0x86,0x48,0xce,0x3d,0x02,0x01};
static const uint8_t OID_P256[]       = {0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,0x07};
static const uint8_t OID_P384[]       = {0x2b,0x81,0x04,0x00,0x22};
static const uint8_t OID_RSA_SHA256[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x0b};
static const uint8_t OID_RSA_SHA384[] = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x0c};
static const uint8_t OID_RSA_PSS[]    = {0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x0a};
/* bare hash OIDs -- used to read the real hash out of RSA-PSS parameters */
static const uint8_t OID_HASH_SHA384[]= {0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x02};
static const uint8_t OID_HASH_SHA512[]= {0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x03};
static const uint8_t OID_ECDSA_256[]  = {0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x02};
static const uint8_t OID_ECDSA_384[]  = {0x2a,0x86,0x48,0xce,0x3d,0x04,0x03,0x03};
static const uint8_t OID_SAN[]        = {0x55,0x1d,0x11};
static const uint8_t OID_BC[]         = {0x55,0x1d,0x13};            /* basicConstraints */
static const uint8_t OID_KU[]         = {0x55,0x1d,0x0f};            /* keyUsage         */
static const uint8_t OID_EKU[]        = {0x55,0x1d,0x25};            /* extKeyUsage      */
static const uint8_t OID_EKU_SERVER[] = {0x2b,0x06,0x01,0x05,0x05,0x07,0x03,0x01}; /* id-kp-serverAuth */
static const uint8_t OID_EKU_ANY[]    = {0x55,0x1d,0x25,0x00};       /* anyExtendedKeyUsage */

#define OID_EQ(b,l,O)  ((l)==sizeof(O) && memcmp((b),(O),sizeof(O))==0)

/* Read one TLV from the cursor.  On success the tag, body pointer and body
 * length are returned and the cursor is advanced past the value.  0/-1. */
static int der(const uint8_t **p, const uint8_t *end, uint8_t *tag,
               const uint8_t **body, size_t *blen)
{
    size_t len; uint8_t l;
    if (end - *p < 2) return -1;
    *tag = *(*p)++;
    l = *(*p)++;
    if (l < 0x80) {
        len = l;
    } else {
        int nb = l & 0x7f;
        if (nb < 1 || nb > 4 || (end - *p) < nb) return -1;
        len = 0;
        while (nb--) len = (len << 8) | *(*p)++;
    }
    if ((size_t)(end - *p) < len) return -1;
    *body = *p; *blen = len; *p += len;
    return 0;
}

/* Expect an element of a specific tag. */
static int der_exp(const uint8_t **p, const uint8_t *end, uint8_t want,
                   const uint8_t **body, size_t *blen)
{
    uint8_t t;
    if (der(p, end, &t, body, blen) != 0) return -1;
    return (t == want) ? 0 : -1;
}

static int parse_spki(const uint8_t *b, size_t l, tiku_kits_crypto_x509_t *o)
{
    const uint8_t *p = b, *e = b + l, *alg, *oid, *bit, *kp, *ke2;
    const uint8_t *seqb, *nb, *eb;
    size_t al, ol, bl, kl, sl, nl, el;
    uint8_t t;

    if (der_exp(&p, e, T_SEQ, &alg, &al) != 0) return BAD;   /* algorithm   */
    if (der_exp(&p, e, T_BIT, &bit, &bl) != 0) return BAD;   /* publicKey   */
    if (bl < 1 || bit[0] != 0) return BAD;
    kp = bit + 1; kl = bl - 1;                               /* key bytes   */

    {   /* algorithm ::= SEQ { OID, params } */
        const uint8_t *ap = alg, *ae = alg + al;
        if (der_exp(&ap, ae, T_OID, &oid, &ol) != 0) return BAD;
        if (OID_EQ(oid, ol, OID_RSA)) {
            o->pk_alg = TIKU_X509_PK_RSA;
        } else if (OID_EQ(oid, ol, OID_EC_PK)) {
            const uint8_t *cb; size_t cl;
            if (der_exp(&ap, ae, T_OID, &cb, &cl) != 0) return BAD;
            if      (OID_EQ(cb, cl, OID_P256)) o->pk_alg = TIKU_X509_PK_EC_P256;
            else if (OID_EQ(cb, cl, OID_P384)) o->pk_alg = TIKU_X509_PK_EC_P384;
            else return BAD;                                 /* unsupported curve */
        } else {
            return BAD;
        }
    }

    if (o->pk_alg == TIKU_X509_PK_RSA) {
        const uint8_t *sp = kp, *se = kp + kl;
        if (der_exp(&sp, se, T_SEQ, &seqb, &sl) != 0) return BAD;
        {
            const uint8_t *q = seqb, *qe = seqb + sl;
            if (der(&q, qe, &t, &nb, &nl) != 0 || t != T_INT) return BAD;
            if (der(&q, qe, &t, &eb, &el) != 0 || t != T_INT) return BAD;
        }
        if (nl > 1 && nb[0] == 0x00) { nb++; nl--; }         /* strip sign  */
        o->rsa_n = nb; o->rsa_n_len = nl;
        o->rsa_e = eb; o->rsa_e_len = el;
        (void)ke2;
    } else {
        if (kl < 1 || kp[0] != 0x04) return BAD;             /* uncompressed*/
        o->ec_point = kp; o->ec_point_len = kl;
    }
    return OK;
}

/* substring search: 1 if needle n[nl] occurs anywhere within haystack h[hl] */
static int mem_find(const uint8_t *h, size_t hl, const uint8_t *n, size_t nl)
{
    size_t i;
    if (nl == 0 || hl < nl) return 0;
    for (i = 0; i + nl <= hl; i++)
        if (memcmp(h + i, n, nl) == 0) return 1;
    return 0;
}

static int sig_alg_of(const uint8_t *b, size_t l)
{
    const uint8_t *p = b, *e = b + l, *oid; size_t ol;
    if (der_exp(&p, e, T_OID, &oid, &ol) != 0) return TIKU_X509_SIG_UNKNOWN;
    if (OID_EQ(oid, ol, OID_RSA_SHA256)) return TIKU_X509_SIG_RSA_PKCS1_SHA256;
    if (OID_EQ(oid, ol, OID_RSA_SHA384)) return TIKU_X509_SIG_RSA_PKCS1_SHA384;
    if (OID_EQ(oid, ol, OID_RSA_PSS)) {
        /* The PSS hash lives in the AlgorithmIdentifier parameters; only
         * PSS-SHA256 is verifiable here, so detect SHA-384/512 and reject them
         * cleanly (UNKNOWN -> verify_signed_by default) rather than silently
         * mislabelling them as SHA-256. */
        if (mem_find(b, l, OID_HASH_SHA384, sizeof OID_HASH_SHA384) ||
            mem_find(b, l, OID_HASH_SHA512, sizeof OID_HASH_SHA512))
            return TIKU_X509_SIG_UNKNOWN;
        return TIKU_X509_SIG_RSA_PSS_SHA256;
    }
    if (OID_EQ(oid, ol, OID_ECDSA_256))  return TIKU_X509_SIG_ECDSA_SHA256;
    if (OID_EQ(oid, ol, OID_ECDSA_384))  return TIKU_X509_SIG_ECDSA_SHA384;
    return TIKU_X509_SIG_UNKNOWN;
}

static void parse_san(const uint8_t *b, size_t l, tiku_kits_crypto_x509_t *o)
{
    /* extnValue OCTET STRING -> GeneralNames SEQUENCE; store its contents. */
    const uint8_t *p = b, *e = b + l, *oct, *seq; size_t ol, sl;
    if (der_exp(&p, e, T_OCT, &oct, &ol) != 0) return;
    {
        const uint8_t *q = oct, *qe = oct + ol;
        if (der_exp(&q, qe, T_SEQ, &seq, &sl) != 0) return;
        o->san = seq; o->san_len = sl;
    }
}

/* From a cursor positioned just after an extension's OID, skip the optional
 * critical BOOLEAN and return the extnValue OCTET STRING's contents. */
static int ext_value(const uint8_t *p, const uint8_t *e,
                     const uint8_t **val, size_t *vlen)
{
    const uint8_t *body; size_t bl; uint8_t t;
    if (der(&p, e, &t, &body, &bl) != 0) return -1;
    if (t == T_BOOL) {                         /* critical -> read the next TLV */
        if (der(&p, e, &t, &body, &bl) != 0) return -1;
    }
    if (t != T_OCT) return -1;
    *val = body; *vlen = bl;
    return 0;
}

/* basicConstraints ::= SEQUENCE { cA BOOLEAN DEFAULT FALSE,
 *                                 pathLenConstraint INTEGER (0..MAX) OPTIONAL } */
static void parse_bc(const uint8_t *xp, const uint8_t *xe, tiku_kits_crypto_x509_t *o)
{
    const uint8_t *val, *seq, *s, *se, *body; size_t vlen, sl, bl; uint8_t t;
    if (ext_value(xp, xe, &val, &vlen) != 0) return;
    if (der_exp(&val, val + vlen, T_SEQ, &seq, &sl) != 0) return;
    o->has_bc = 1;
    s = seq; se = seq + sl;
    if (s < se && der(&s, se, &t, &body, &bl) == 0 && t == T_BOOL) {
        o->is_ca = (bl >= 1 && body[0] != 0) ? 1 : 0;
        if (s < se && der(&s, se, &t, &body, &bl) == 0 && t == T_INT && bl >= 1)
            o->path_len = (int)body[bl - 1];   /* pathLen is always small */
    }
}

/* keyUsage ::= BIT STRING -- store the first content octet (KU_* bits). */
static void parse_ku(const uint8_t *xp, const uint8_t *xe, tiku_kits_crypto_x509_t *o)
{
    const uint8_t *val, *bits; size_t vlen, bl;
    if (ext_value(xp, xe, &val, &vlen) != 0) return;
    if (der_exp(&val, val + vlen, T_BIT, &bits, &bl) != 0) return;
    o->has_ku = 1;
    if (bl >= 2) o->key_usage = bits[1];        /* bits[0] = unused-bit count */
}

/* extKeyUsage ::= SEQUENCE OF KeyPurposeId(OID) -- flag serverAuth / anyEKU. */
static void parse_eku(const uint8_t *xp, const uint8_t *xe, tiku_kits_crypto_x509_t *o)
{
    const uint8_t *val, *seq, *s, *se, *oid; size_t vlen, sl, ol;
    if (ext_value(xp, xe, &val, &vlen) != 0) return;
    if (der_exp(&val, val + vlen, T_SEQ, &seq, &sl) != 0) return;
    o->has_eku = 1;
    s = seq; se = seq + sl;
    while (s < se) {
        if (der_exp(&s, se, T_OID, &oid, &ol) != 0) break;
        if (OID_EQ(oid, ol, OID_EKU_SERVER) || OID_EQ(oid, ol, OID_EKU_ANY))
            o->eku_server = 1;
    }
}

static void parse_exts(const uint8_t *b, size_t l, tiku_kits_crypto_x509_t *o)
{
    const uint8_t *p = b, *e = b + l, *seq; size_t sl;
    if (der_exp(&p, e, T_SEQ, &seq, &sl) != 0) return;       /* SEQUENCE OF */
    {
        const uint8_t *q = seq, *qe = seq + sl;
        while (q < qe) {
            const uint8_t *ext, *oid; size_t el, ol;
            if (der_exp(&q, qe, T_SEQ, &ext, &el) != 0) return;
            {
                const uint8_t *xp = ext, *xe = ext + el;
                if (der_exp(&xp, xe, T_OID, &oid, &ol) != 0) continue;
                if      (OID_EQ(oid, ol, OID_SAN)) parse_san(xp, (size_t)(xe - xp), o);
                else if (OID_EQ(oid, ol, OID_BC))  parse_bc(xp, xe, o);
                else if (OID_EQ(oid, ol, OID_KU))  parse_ku(xp, xe, o);
                else if (OID_EQ(oid, ol, OID_EKU)) parse_eku(xp, xe, o);
            }
        }
    }
}

static int parse_tbs(const uint8_t *b, size_t l, tiku_kits_crypto_x509_t *o)
{
    const uint8_t *p = b, *e = b + l, *el, *body, *start;
    size_t bl; uint8_t t;

    /* [0] version (optional), then serialNumber */
    start = p;
    if (der(&p, e, &t, &body, &bl) != 0) return BAD;
    if (t == T_CTX0) { if (der(&p, e, &t, &body, &bl) != 0) return BAD; } /* serial */
    /* signature AlgorithmIdentifier */
    if (der_exp(&p, e, T_SEQ, &body, &bl) != 0) return BAD;
    /* issuer Name */
    start = p;
    if (der_exp(&p, e, T_SEQ, &body, &bl) != 0) return BAD;
    o->issuer = start; o->issuer_len = (size_t)(p - start);
    /* validity SEQUENCE { notBefore Time, notAfter Time } */
    if (der_exp(&p, e, T_SEQ, &body, &bl) != 0) return BAD;
    {
        const uint8_t *vp = body, *ve = body + bl, *tb; size_t tl;
        if (der(&vp, ve, &o->nb_tag, &tb, &tl) != 0) return BAD;
        o->not_before = tb; o->not_before_len = tl;
        if (der(&vp, ve, &o->na_tag, &tb, &tl) != 0) return BAD;
        o->not_after = tb; o->not_after_len = tl;
    }
    /* subject Name */
    start = p;
    if (der_exp(&p, e, T_SEQ, &body, &bl) != 0) return BAD;
    o->subject = start; o->subject_len = (size_t)(p - start);
    /* SubjectPublicKeyInfo */
    if (der_exp(&p, e, T_SEQ, &body, &bl) != 0) return BAD;
    if (parse_spki(body, bl, o) != OK) return BAD;
    /* optional [3] extensions */
    while (p < e) {
        el = p;
        if (der(&p, e, &t, &body, &bl) != 0) break;
        if (t == T_CTX3) parse_exts(body, bl, o);
        (void)el;
    }
    return OK;
}

int tiku_kits_crypto_x509_parse(const uint8_t *der_, size_t len,
                                tiku_kits_crypto_x509_t *out)
{
    const uint8_t *p = der_, *end = der_ + len, *cbody, *tbs_start, *body, *sig;
    size_t cl, bl, siglen;
    uint8_t t;

    memset(out, 0, sizeof *out);
    out->path_len = -1;                          /* absent = unbounded */

    if (der_exp(&p, end, T_SEQ, &cbody, &cl) != 0) return BAD;   /* Certificate */
    {
        const uint8_t *cp = cbody, *ce = cbody + cl;
        /* tbsCertificate (capture full element incl. header) */
        tbs_start = cp;
        if (der_exp(&cp, ce, T_SEQ, &body, &bl) != 0) return BAD;
        out->tbs = tbs_start; out->tbs_len = (size_t)(cp - tbs_start);
        if (parse_tbs(body, bl, out) != OK) return BAD;
        /* signatureAlgorithm */
        if (der_exp(&cp, ce, T_SEQ, &body, &bl) != 0) return BAD;
        out->sig_alg = sig_alg_of(body, bl);
        /* signatureValue BIT STRING */
        if (der(&cp, ce, &t, &sig, &siglen) != 0 || t != T_BIT) return BAD;
        if (siglen < 1 || sig[0] != 0) return BAD;
        out->sig = sig + 1; out->sig_len = siglen - 1;
    }
    return OK;
}

/* ---- hostname matching against subjectAltName dNSName ------------------- */

static int ci_eq(const char *a, const uint8_t *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        char ca = a[i], cb = (char)b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb || ca == 0) return 0;
    }
    return a[n] == 0;   /* host fully consumed */
}

int tiku_kits_crypto_x509_match_host(const tiku_kits_crypto_x509_t *c, const char *host)
{
    const uint8_t *p, *e;
    if (c->san == NULL || host == NULL || host[0] == 0) return BAD;
    p = c->san; e = c->san + c->san_len;
    while (p < e) {
        uint8_t t = *p++;
        size_t len, i;
        if (p >= e) break;
        len = *p++;                                  /* dNSName names are short */
        if (len > (size_t)(e - p)) break;
        if (t == 0x82) {                             /* [2] dNSName             */
            const uint8_t *name = p; size_t nlen = len;
            if (nlen > 2 && name[0] == '*' && name[1] == '.') {
                /* wildcard: match exactly one leading label of host */
                const char *dot = host;
                while (*dot && *dot != '.') dot++;
                if (*dot == '.' && ci_eq(dot + 1, name + 2, nlen - 2))
                    return OK;
            } else if (ci_eq(host, name, nlen)) {
                return OK;
            }
            for (i = 0; i < nlen; i++) { /* nothing */ }
        }
        p += len;
    }
    return BAD;
}
