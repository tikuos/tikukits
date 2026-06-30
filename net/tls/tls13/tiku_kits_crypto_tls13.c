/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls13.c - TLS 1.3 client (ECDHE + X.509 certificate)
 *
 * Single-connection TLS 1.3 client: X25519 key exchange, TLS_AES_128_GCM_
 * SHA256, server authentication via the Certificate + CertificateVerify
 * messages validated against a trust store.  Reuses the x25519, hkdf, gcm,
 * sha256, hmac, x509 and rsa/p256 verify kits.  Single-connection: the
 * handshake reassembly buffer is a file-scope static.
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

#include "tiku_kits_crypto_tls13.h"
#include "../../../crypto/x25519/tiku_kits_crypto_x25519.h"
#include "../../../crypto/hkdf/tiku_kits_crypto_hkdf.h"
#include "../../../crypto/hmac/tiku_kits_crypto_hmac.h"
#include "../../../crypto/sha256/tiku_kits_crypto_sha256.h"
#include "../../../crypto/gcm/tiku_kits_crypto_gcm.h"
#include "../../../crypto/rsa/tiku_kits_crypto_rsa.h"
#include "../../../crypto/p256/tiku_kits_crypto_p256.h"
#include "../../../crypto/p384/tiku_kits_crypto_p384.h"
#include "../../../crypto/sha384/tiku_kits_crypto_sha384.h"
#include <string.h>

#define OK   TIKU_KITS_CRYPTO_TLS13_OK
#define BAD  TIKU_KITS_CRYPTO_TLS13_BAD

/* Optional milestone hook for bring-up debugging (NULL = silent). */
void (*tiku_kits_crypto_tls13_dbg)(const char *) = 0;
#define DBG(s) do { if (tiku_kits_crypto_tls13_dbg) tiku_kits_crypto_tls13_dbg(s); } while (0)

#define REC_CCS   0x14
#define REC_ALERT 0x15
#define REC_HS    0x16
#define REC_APP   0x17

#define HS_CLIENT_HELLO 1
#define HS_SERVER_HELLO 2
#define HS_EE           8
#define HS_CERT         11
#define HS_CERT_VERIFY  15
#define HS_FINISHED     20

#define MAX_CERTS 6
/* Embedded sizing: the reassembled server flight (EncryptedExtensions +
 * Certificate chain + CertificateVerify + Finished) and any single incoming
 * record must fit here.  Sized to the RFC 8446 maximum TLSCiphertext.length
 * (2^14 + 256 = 16640): a server may legally send its HTTP response -- or any
 * post-handshake record -- as one full-size record, and read_record() rejects
 * anything longer than REC_BUF.  The previous 12 KB (enough for large RSA
 * Certificate flights, e.g. Microsoft/Azure's ~9.4 KB) wrongly dropped those
 * >12 KB response records: tls13_read returned <=0 mid-stream, so HTTPGET$ read
 * 0 bytes -- which over SLIP surfaced as `HTTP 0, 0 B rdfail=1` after the
 * NewSessionTickets (duckduckgo.com, kernel.org; google fits under 12 KB).
 * #ifndef so SRAM-tight builds can dial it back -- but below 16640 a compliant
 * server that sends a full-size record becomes unreachable. */
#ifndef HS_BUF
#define HS_BUF     16640
#endif
#ifndef REC_BUF
#define REC_BUF    16640
#endif
#define SEAL_BUF   1152   /* outgoing records: client sends are small (cap below) */
#define APP_CHUNK  1024   /* max plaintext per client application_data record    */

typedef tiku_kits_crypto_sha256_ctx_t sha_ctx;

/* ---- small helpers ------------------------------------------------------ */

static void wr16(uint8_t *p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static uint16_t rd16(const uint8_t *p){ return (uint16_t)((p[0]<<8)|p[1]); }
static uint32_t rd24(const uint8_t *p){ return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2]; }

static int io_send(const tiku_kits_crypto_tls13_io_t *io, const uint8_t *b, size_t n)
{
    size_t off = 0;
    while (off < n) {
        int r = io->send(io->ctx, b + off, n - off);
        if (r <= 0) return BAD;
        off += (size_t)r;
    }
    return OK;
}
static int io_recv(const tiku_kits_crypto_tls13_io_t *io, uint8_t *b, size_t n)
{
    size_t off = 0;
    while (off < n) {
        int r = io->recv(io->ctx, b + off, n - off);
        if (r <= 0) return BAD;
        off += (size_t)r;
    }
    return OK;
}

/* transcript snapshot: hash of all messages fed so far */
static void th_hash(const sha_ctx *th, uint8_t out[32])
{
    sha_ctx c = *th;
    tiku_kits_crypto_sha256_final(&c, out);
}
static void th_add(sha_ctx *th, const uint8_t *d, size_t n)
{
    tiku_kits_crypto_sha256_update(th, d, n);
}

static void derive_secret(const uint8_t secret[32], const char *label,
                          const uint8_t thash[32], uint8_t out[32])
{
    tiku_kits_crypto_hkdf_expand_label(secret, label, thash, 32, out, 32);
}
static void traffic_keys(const uint8_t ts[32], uint8_t key[16], uint8_t iv[12])
{
    tiku_kits_crypto_hkdf_expand_label(ts, "key", NULL, 0, key, 16);
    tiku_kits_crypto_hkdf_expand_label(ts, "iv",  NULL, 0, iv,  12);
}
static void mk_nonce(const uint8_t iv[12], uint64_t seq, uint8_t nonce[12])
{
    int i;
    memcpy(nonce, iv, 12);
    for (i = 0; i < 8; i++) nonce[11 - i] ^= (uint8_t)(seq >> (8 * i));
}

/* ---- record layer ------------------------------------------------------- */

static uint8_t rec[REC_BUF];

/* Read one TLS record into rec[]; returns type, sets *blen to body length. */
static int read_record(const tiku_kits_crypto_tls13_io_t *io, uint8_t *type, size_t *blen)
{
    uint8_t hdr[5]; size_t n;
    if (io_recv(io, hdr, 5) != OK) return BAD;
    n = rd16(hdr + 3);
    if (n > REC_BUF) return BAD;
    if (io_recv(io, rec, n) != OK) return BAD;
    *type = hdr[0]; *blen = n;
    return OK;
}

static int write_plain_record(const tiku_kits_crypto_tls13_io_t *io, uint8_t type,
                              const uint8_t *body, size_t blen)
{
    uint8_t hdr[5];
    hdr[0] = type; hdr[1] = 0x03; hdr[2] = 0x03; wr16(hdr + 3, (uint16_t)blen);
    if (io_send(io, hdr, 5) != OK) return BAD;
    return io_send(io, body, blen);
}

/* Decrypt one application_data record body (in rec[]) -> plaintext + inner type. */
static int aead_open(const tiku_kits_crypto_gcm_ctx_t *gcm, const uint8_t iv[12],
                     uint64_t seq, size_t blen, uint8_t *pt, size_t *ptlen, uint8_t *inner_type)
{
    uint8_t nonce[12], aad[5]; size_t ctlen;
    if (blen < 17) return BAD;
    ctlen = blen - 16;
    mk_nonce(iv, seq, nonce);
    aad[0] = REC_APP; aad[1] = 0x03; aad[2] = 0x03; wr16(aad + 3, (uint16_t)blen);
    if (tiku_kits_crypto_gcm_decrypt(gcm, nonce, aad, 5, rec, (uint16_t)ctlen,
                                     rec + ctlen, pt) != 0)
        return BAD;
    /* strip zero padding; last non-zero byte is the real content type */
    while (ctlen > 0 && pt[ctlen - 1] == 0) ctlen--;
    if (ctlen == 0) return BAD;
    *inner_type = pt[ctlen - 1];
    *ptlen = ctlen - 1;
    return OK;
}

/* Encrypt inner||type and send as an application_data record. */
static int aead_seal(const tiku_kits_crypto_tls13_io_t *io, const tiku_kits_crypto_gcm_ctx_t *gcm,
                     const uint8_t iv[12], uint64_t seq, uint8_t inner_type,
                     const uint8_t *inner, size_t ilen)
{
    static uint8_t out[SEAL_BUF];
    uint8_t nonce[12], aad[5], hdr[5];
    size_t total = ilen + 1 + 16;
    if (total > SEAL_BUF) return BAD;
    memcpy(out, inner, ilen);
    out[ilen] = inner_type;
    mk_nonce(iv, seq, nonce);
    aad[0] = REC_APP; aad[1] = 0x03; aad[2] = 0x03; wr16(aad + 3, (uint16_t)total);
    if (tiku_kits_crypto_gcm_encrypt(gcm, nonce, aad, 5, out, (uint16_t)(ilen + 1),
                                     out, out + ilen + 1) != 0)
        return BAD;
    hdr[0] = REC_APP; hdr[1] = 0x03; hdr[2] = 0x03; wr16(hdr + 3, (uint16_t)total);
    if (io_send(io, hdr, 5) != OK) return BAD;
    return io_send(io, out, total);
}

/* ---- ClientHello -------------------------------------------------------- */

static size_t build_client_hello(uint8_t *out, const uint8_t random[32],
                                 const uint8_t sid[32], const uint8_t pub[32],
                                 const uint8_t p256_pub[65], const char *host)
{
    uint8_t *p = out + 4;          /* leave room for hs header */
    uint8_t *ext, *extlen_pos;
    size_t hlen = strlen(host), elen;

    *p++ = 0x03; *p++ = 0x03;                       /* legacy_version  */
    memcpy(p, random, 32); p += 32;                 /* random          */
    *p++ = 0x20; memcpy(p, sid, 32); p += 32;       /* session_id (32) */
    wr16(p, 2); p += 2; *p++ = 0x13; *p++ = 0x01;   /* cipher: AES128GCM */
    *p++ = 0x01; *p++ = 0x00;                        /* compression none */

    extlen_pos = p; p += 2;                          /* extensions length */
    ext = p;
    /* server_name (SNI) */
    wr16(p,0x0000); p+=2; wr16(p,(uint16_t)(hlen+5)); p+=2;
    wr16(p,(uint16_t)(hlen+3)); p+=2; *p++=0x00; wr16(p,(uint16_t)hlen); p+=2;
    memcpy(p,host,hlen); p+=hlen;
    /* supported_versions */
    wr16(p,0x002b); p+=2; wr16(p,3); p+=2; *p++=0x02; *p++=0x03; *p++=0x04;
    /* supported_groups: x25519, secp256r1 */
    wr16(p,0x000a); p+=2; wr16(p,6); p+=2; wr16(p,4); p+=2;
    wr16(p,0x001d); p+=2; wr16(p,0x0017); p+=2;
    /* signature_algorithms: ecdsa P-256/P-384, rsa_pss, rsa_pkcs1 (SHA-256) */
    wr16(p,0x000d); p+=2; wr16(p,10); p+=2; wr16(p,8); p+=2;
    wr16(p,0x0403); p+=2; wr16(p,0x0503); p+=2;
    wr16(p,0x0804); p+=2; wr16(p,0x0401); p+=2;
    /* key_share: x25519 + secp256r1 (server picks one, no HelloRetry needed) */
    wr16(p,0x0033); p+=2; wr16(p,107); p+=2; wr16(p,105); p+=2;
    wr16(p,0x001d); p+=2; wr16(p,32); p+=2; memcpy(p,pub,32); p+=32;
    wr16(p,0x0017); p+=2; wr16(p,65); p+=2; memcpy(p,p256_pub,65); p+=65;

    elen = (size_t)(p - ext);
    wr16(extlen_pos, (uint16_t)elen);

    {
        size_t body = (size_t)(p - (out + 4));
        out[0] = HS_CLIENT_HELLO;
        out[1] = 0; out[2] = (uint8_t)(body >> 8); out[3] = (uint8_t)body;
    }
    return (size_t)(p - out);
}

/* ---- ServerHello parse: extract the server x25519 key_share -------------- */

static int parse_server_hello(const uint8_t *b, size_t n,
                              uint8_t server_pub[65], int *group)
{
    const uint8_t *p = b, *e = b + n; uint16_t cipher, extn; size_t sidl;
    if (n < 38) return BAD;
    p += 2;                                /* legacy_version */
    p += 32;                               /* random         */
    sidl = *p++; if ((size_t)(e - p) < sidl + 4) return BAD; p += sidl;
    cipher = rd16(p); p += 2;
    if (cipher != 0x1301) return BAD;
    p += 1;                                /* compression    */
    extn = rd16(p); p += 2;
    if ((size_t)(e - p) < extn) return BAD;
    {
        const uint8_t *ee = p + extn;
        while (p + 4 <= ee) {
            uint16_t et = rd16(p), el = rd16(p + 2); p += 4;
            if (p + el > ee) return BAD;
            if (et == 0x0033) {            /* key_share */
                uint16_t grp = rd16(p), kl = rd16(p + 2);
                if (grp == 0x001d && kl == 32) {            /* x25519 */
                    memcpy(server_pub, p + 4, 32); *group = 0x001d; return OK;
                }
                if (grp == 0x0017 && kl == 65 && p[4] == 0x04) {  /* secp256r1 */
                    memcpy(server_pub, p + 4, 65); *group = 0x0017; return OK;
                }
                return BAD;
            }
            p += el;
        }
    }
    return BAD;
}

/* extract an n-byte big-endian integer from a DER INTEGER body */
static void der_int_n(const uint8_t *b, size_t l, uint8_t *out, size_t n)
{
    memset(out, 0, n);
    while (l > 1 && b[0] == 0x00) { b++; l--; }
    if (l > n) { b += (l - n); l = n; }
    memcpy(out + (n - l), b, l);
}

static void cv_sha256(const uint8_t *d, size_t n, uint8_t out[32])
{ sha_ctx c; tiku_kits_crypto_sha256_init(&c); tiku_kits_crypto_sha256_update(&c, d, n); tiku_kits_crypto_sha256_final(&c, out); }
static void cv_sha384(const uint8_t *d, size_t n, uint8_t out[48])
{ tiku_kits_crypto_sha384_ctx_t c; tiku_kits_crypto_sha384_init(&c); tiku_kits_crypto_sha384_update(&c, d, n); tiku_kits_crypto_sha384_final(&c, out); }

/* CertificateVerify check: hash the signed content with the scheme's hash,
 * then verify with the leaf key.  Dispatched by TLS SignatureScheme. */
static int cv_verify(const tiku_kits_crypto_x509_t *leaf, uint16_t scheme,
                     const uint8_t *sig, uint16_t slen,
                     const uint8_t *content, size_t clen)
{
    uint8_t h[48];
    switch (scheme) {
    case 0x0403: {                             /* ecdsa_secp256r1_sha256 */
        const uint8_t *p = sig, *e = sig + slen; uint8_t r[32], s[32]; size_t rl, sl;
        if (leaf->pk_alg != TIKU_X509_PK_EC_P256) return BAD;
        if (leaf->ec_point_len < 65 || leaf->ec_point[0] != 0x04) return BAD;
        cv_sha256(content, clen, h);
        if (e - p < 2 || p[0] != 0x30) return BAD;
        p += 2;
        if (p >= e || p[0] != 0x02) return BAD;
        rl = p[1]; der_int_n(p + 2, rl, r, 32); p += 2 + rl;
        if (p >= e || p[0] != 0x02) return BAD;
        sl = p[1]; der_int_n(p + 2, sl, s, 32);
        return tiku_kits_crypto_p256_ecdsa_verify(leaf->ec_point + 1, leaf->ec_point + 33,
                                                  h, 32, r, s) == 0 ? OK : BAD;
    }
    case 0x0503: {                             /* ecdsa_secp384r1_sha384 */
        const uint8_t *p = sig, *e = sig + slen; uint8_t r[48], s[48]; size_t rl, sl;
        if (leaf->pk_alg != TIKU_X509_PK_EC_P384) return BAD;
        if (leaf->ec_point_len < 97 || leaf->ec_point[0] != 0x04) return BAD;
        cv_sha384(content, clen, h);
        if (e - p < 2 || p[0] != 0x30) return BAD;
        p += 2;
        if (p >= e || p[0] != 0x02) return BAD;
        rl = p[1]; der_int_n(p + 2, rl, r, 48); p += 2 + rl;
        if (p >= e || p[0] != 0x02) return BAD;
        sl = p[1]; der_int_n(p + 2, sl, s, 48);
        return tiku_kits_crypto_p384_ecdsa_verify(leaf->ec_point + 1, leaf->ec_point + 49,
                                                  h, 48, r, s) == 0 ? OK : BAD;
    }
    case 0x0804:                               /* rsa_pss_rsae_sha256 */
        if (leaf->pk_alg != TIKU_X509_PK_RSA) return BAD;
        cv_sha256(content, clen, h);
        return tiku_kits_crypto_rsa_pss_sha256_verify(leaf->rsa_n, leaf->rsa_n_len,
                   leaf->rsa_e, leaf->rsa_e_len, sig, slen, h) == 0 ? OK : BAD;
    case 0x0401:                               /* rsa_pkcs1_sha256 */
        if (leaf->pk_alg != TIKU_X509_PK_RSA) return BAD;
        cv_sha256(content, clen, h);
        return tiku_kits_crypto_rsa_pkcs1_sha256_verify(leaf->rsa_n, leaf->rsa_n_len,
                   leaf->rsa_e, leaf->rsa_e_len, sig, slen, h) == 0 ? OK : BAD;
    default:
        return BAD;
    }
}

/* ---- handshake ---------------------------------------------------------- */

static uint8_t hs_buf[HS_BUF];

/* Handshake diagnostics: the furthest stage reached (positive) or the failure
 * point (negative), plus total handshake record bytes read.  A blocking caller
 * (BASIC HTTPGET$/BROWSE) reads these after a failed connect to separate a
 * transport failure (short / garbled server flight) from a real cert/logic one.
 *   1 ClientHello sent   2 ServerHello ok   3 reading flight   4 flight complete
 *   5 chain trusted     10 connected
 *  -2 ServerHello read  -3 ServerHello bad  -5 flight read (transport)
 *  -6 unexpected record -7 decrypt (corrupt flight)  -9 cert parse
 * -10 cert-verify sig  -11 chain untrusted  -12 Finished  -13 flight overflow
 * -14 client Finished send */
int      tiku_kits_crypto_tls13_last_stage;
uint32_t tiku_kits_crypto_tls13_last_rx;
/* Post-handshake read diagnostics: which condition broke tls13_read, so an
 * empty HTTPGET$/BROWSE over SLIP -- where the red stage print is lost in the
 * shared console/SLIP mux -- is explained from the BROWSE summary line.
 *   0 = delivered application data;   1 = no full record from the wire;
 *   2 = unexpected wire record type;  3 = record body failed to decrypt;
 *   4 = peer sent a TLS alert.
 * last_read_type/seq give the wire record type and the server application
 * read-sequence at the break (so "broke on the Nth post-handshake record"). */
int      tiku_kits_crypto_tls13_last_read_fail;
uint8_t  tiku_kits_crypto_tls13_last_read_type;
uint32_t tiku_kits_crypto_tls13_last_read_seq;
#define HS_DIAG(n)  (tiku_kits_crypto_tls13_last_stage = (n))
#define HS_FAIL(n)  do { tiku_kits_crypto_tls13_last_stage = (n); return BAD; } while (0)

int tiku_kits_crypto_tls13_connect(const tiku_kits_crypto_tls13_io_t *io,
                                   tiku_kits_crypto_tls13_rng_t rng,
                                   const char *host,
                                   const tiku_kits_crypto_x509_root_t *store, int nstore,
                                   uint64_t now_unix,
                                   tiku_kits_crypto_tls13_conn_t *conn)
{
    uint8_t priv[32], pub[32], crand[32], sid[32], server_pub[65], ecdhe[32];
    uint8_t p256_priv[32], p256_pub[65]; int sgroup = 0;
    uint8_t early[32], derived[32], hs_secret[32], master[32];
    uint8_t c_hs[32], s_hs[32], c_hs_key[16], c_hs_iv[12], s_hs_key[16], s_hs_iv[12];
    uint8_t c_fin_key[32], s_fin_key[32];
    uint8_t zero[32] = {0}, ehash[32], thash[32], mac[32];
    sha_ctx th;
    tiku_kits_crypto_gcm_ctx_t s_gcm, c_gcm;
    tiku_kits_crypto_x509_t chain[MAX_CERTS]; int nchain = 0;
    uint8_t thash_cv[32]; int have_cv_hash = 0;
    uint8_t type; size_t blen, chlen, ptlen, parsed; uint8_t inner;
    int got_fin = 0, leaf_ok = 0;

    HS_DIAG(1); tiku_kits_crypto_tls13_last_rx = 0;

    /* 1. ephemeral keys (x25519 + P-256) + ClientHello */
    rng(priv, 32); tiku_kits_crypto_x25519_base(pub, priv);
    { uint8_t seed[32];                      /* P-256 ephemeral (reseed if d==0) */
      do { rng(seed, 32); }
      while (tiku_kits_crypto_p256_ecdh_keypair(seed, p256_priv, p256_pub) != 0); }
    rng(crand, 32); rng(sid, 32);
    tiku_kits_crypto_sha256_init(&th);
    chlen = build_client_hello(rec, crand, sid, pub, p256_pub, host);
    th_add(&th, rec, chlen);
    if (write_plain_record(io, REC_HS, rec, chlen) != OK) return BAD;

    /* 2. ServerHello */
    if (read_record(io, &type, &blen) != OK || type != REC_HS) HS_FAIL(-2);
    tiku_kits_crypto_tls13_last_rx += (uint32_t)blen;
    if (blen < 4 || rec[0] != HS_SERVER_HELLO) HS_FAIL(-3);
    {
        static uint8_t sh[2048];
        if (4 + rd24(rec + 1) > blen || blen > sizeof sh) HS_FAIL(-3);
        memcpy(sh, rec, blen);
        th_add(&th, sh, blen);
        if (parse_server_hello(sh + 4, rd24(sh + 1), server_pub, &sgroup) != OK) HS_FAIL(-3);
    }
    HS_DIAG(2);

    /* 3. ECDHE + key schedule (handshake secrets) -- the server's chosen group
     * selects which ephemeral we agree with (x25519 or secp256r1). */
    if (sgroup == 0x001d) {
        tiku_kits_crypto_x25519_scalarmult(ecdhe, priv, server_pub);
    } else if (sgroup == 0x0017) {
        if (tiku_kits_crypto_p256_ecdh_shared(p256_priv, server_pub, ecdhe) != OK) return BAD;
    } else {
        return BAD;
    }
    tiku_kits_crypto_hkdf_extract(zero, 32, zero, 32, early);
    { sha_ctx e0; tiku_kits_crypto_sha256_init(&e0); tiku_kits_crypto_sha256_final(&e0, ehash); }
    derive_secret(early, "derived", ehash, derived);
    tiku_kits_crypto_hkdf_extract(derived, 32, ecdhe, 32, hs_secret);
    th_hash(&th, thash);                                 /* Hash(CH..SH) */
    derive_secret(hs_secret, "c hs traffic", thash, c_hs);
    derive_secret(hs_secret, "s hs traffic", thash, s_hs);
    traffic_keys(c_hs, c_hs_key, c_hs_iv);
    traffic_keys(s_hs, s_hs_key, s_hs_iv);
    tiku_kits_crypto_hkdf_expand_label(c_hs, "finished", NULL, 0, c_fin_key, 32);
    tiku_kits_crypto_hkdf_expand_label(s_hs, "finished", NULL, 0, s_fin_key, 32);
    tiku_kits_crypto_gcm_init(&s_gcm, s_hs_key);
    conn->s_seq = 0;
    HS_DIAG(3);
    DBG("keysched ok, reading flight");

    /* 4. read + decrypt the server flight (EE, Cert, CertVerify, Finished) */
    {
        size_t hs_len = 0; parsed = 0;
        while (!got_fin) {
            if (read_record(io, &type, &blen) != OK) HS_FAIL(-5);
            if (type == REC_CCS) continue;               /* ignore change_cipher_spec */
            if (type != REC_APP) HS_FAIL(-6);
            tiku_kits_crypto_tls13_last_rx += (uint32_t)blen;
            if ((size_t)hs_len + blen > sizeof hs_buf)   /* never write past hs_buf */
                HS_FAIL(-13);
            if (aead_open(&s_gcm, s_hs_iv, conn->s_seq, blen, hs_buf + hs_len, &ptlen, &inner) != OK)
                HS_FAIL(-7);
            conn->s_seq++;
            if (inner == REC_ALERT) return BAD;
            if (inner != REC_HS) return BAD;
            hs_len += ptlen;

            while (hs_len - parsed >= 4) {
                const uint8_t *m = hs_buf + parsed;
                uint8_t mt = m[0]; size_t ml = rd24(m + 1);
                if (hs_len - parsed < 4 + ml) break;     /* message incomplete */

                if (mt == HS_CERT_VERIFY) { th_hash(&th, thash_cv); have_cv_hash = 1; }
                if (mt == HS_FINISHED)    { th_hash(&th, thash); }   /* Hash(CH..CV) */

                if (mt == HS_EE)   DBG("got EncryptedExtensions");
                if (mt == HS_CERT) DBG("got Certificate, parsing");
                if (mt == HS_CERT_VERIFY) DBG("got CertVerify, verifying sig");
                if (mt == HS_FINISHED) DBG("got Finished, checking");

                if (mt == HS_CERT) {
                    const uint8_t *cp = m + 4, *ce = m + 4 + ml; size_t ctxl;
                    if (cp >= ce) return BAD;
                    ctxl = *cp++; cp += ctxl;            /* cert request context */
                    if (cp + 3 > ce) return BAD;
                    cp += 3;                             /* certificate_list len */
                    while (cp + 3 <= ce && nchain < MAX_CERTS) {
                        size_t clen = rd24(cp); cp += 3;
                        if (cp + clen > ce) return BAD;
                        if (tiku_kits_crypto_x509_parse(cp, clen, &chain[nchain]) == 0)
                            nchain++;
                        else HS_FAIL(-9);
                        cp += clen;
                        if (cp + 2 > ce) break;
                        cp += 2 + rd16(cp);              /* cert extensions */
                    }
                } else if (mt == HS_CERT_VERIFY) {
                    const uint8_t *cp = m + 4; uint16_t scheme = rd16(cp), slen = rd16(cp + 2);
                    static uint8_t signed_buf[130]; int v = -1;
                    if (nchain < 1 || !have_cv_hash) return BAD;
                    /* RFC 8446 4.4.3: 64 spaces || context || 0x00 || transcript */
                    memset(signed_buf, 0x20, 64);
                    memcpy(signed_buf + 64, "TLS 1.3, server CertificateVerify", 33);
                    signed_buf[97] = 0x00;
                    memcpy(signed_buf + 98, thash_cv, 32);
                    v = cv_verify(&chain[0], scheme, cp + 4, slen, signed_buf, 130);
                    if (v != 0) HS_FAIL(-10);
                    leaf_ok = 1;
                }

                th_add(&th, m, 4 + ml);                  /* now fold msg into transcript */

                if (mt == HS_FINISHED) {
                    tiku_kits_crypto_hmac_sha256(s_fin_key, 32, thash, 32, mac);
                    if (ml != 32 || memcmp(mac, m + 4, 32) != 0) HS_FAIL(-12);
                    got_fin = 1;
                }
                parsed += 4 + ml;
            }
        }
    }
    if (!leaf_ok || nchain < 1) HS_FAIL(-10);
    HS_DIAG(4);

    /* 5. validate the certificate chain to a trusted root */
    DBG("flight done, validating chain");
    if (tiku_kits_crypto_x509_verify_chain_store(chain, nchain, store, nstore, host, now_unix) != 0)
        HS_FAIL(-11);
    HS_DIAG(5);
    DBG("chain trusted, finishing");

    /* 6. application keys (master secret) + client Finished */
    th_hash(&th, thash);                                 /* Hash(CH..server Finished) */
    derive_secret(hs_secret, "derived", ehash, derived);
    tiku_kits_crypto_hkdf_extract(derived, 32, zero, 32, master);
    {
        uint8_t c_ap[32], s_ap[32];
        derive_secret(master, "c ap traffic", thash, c_ap);
        derive_secret(master, "s ap traffic", thash, s_ap);
        traffic_keys(c_ap, conn->c_key, conn->c_iv);
        traffic_keys(s_ap, conn->s_key, conn->s_iv);
    }

    /* client Finished = HMAC(c_fin_key, Hash(CH..server Finished)) */
    {
        uint8_t fin_msg[36];
        tiku_kits_crypto_hmac_sha256(c_fin_key, 32, thash, 32, mac);
        fin_msg[0] = HS_FINISHED; fin_msg[1] = 0; fin_msg[2] = 0; fin_msg[3] = 32;
        memcpy(fin_msg + 4, mac, 32);
        /* legacy change_cipher_spec for middlebox compatibility */
        { uint8_t ccs = 0x01; write_plain_record(io, REC_CCS, &ccs, 1); }
        tiku_kits_crypto_gcm_init(&c_gcm, c_hs_key);
        conn->c_seq = 0;
        if (aead_seal(io, &c_gcm, c_hs_iv, conn->c_seq, REC_HS, fin_msg, 36) != OK) HS_FAIL(-14);
    }

    /* 7. switch to application-data keys; fresh sequence numbers */
    conn->io = *io;
    conn->c_seq = 0; conn->s_seq = 0;
    conn->rx_len = conn->rx_off = 0; conn->closed = 0;
    HS_DIAG(10);
    return OK;
}

/* ---- application data --------------------------------------------------- */

int tiku_kits_crypto_tls13_write(tiku_kits_crypto_tls13_conn_t *c,
                                 const uint8_t *data, size_t len)
{
    tiku_kits_crypto_gcm_ctx_t gcm;
    size_t off = 0;
    if (c->closed) return BAD;
    tiku_kits_crypto_gcm_init(&gcm, c->c_key);
    while (off < len) {
        size_t chunk = len - off; if (chunk > APP_CHUNK) chunk = APP_CHUNK;
        if (aead_seal(&c->io, &gcm, c->c_iv, c->c_seq, REC_APP, data + off, chunk) != OK)
            return BAD;
        c->c_seq++; off += chunk;
    }
    return (int)len;
}

int tiku_kits_crypto_tls13_read(tiku_kits_crypto_tls13_conn_t *c, uint8_t *buf, size_t max)
{
    tiku_kits_crypto_gcm_ctx_t gcm;
    if (c->closed) return 0;
    if (c->rx_off >= c->rx_len) {
        uint8_t type, inner; size_t blen, ptlen;
        tiku_kits_crypto_gcm_init(&gcm, c->s_key);
        for (;;) {
            tiku_kits_crypto_tls13_last_read_seq = (uint32_t)c->s_seq;
            if (read_record(&c->io, &type, &blen) != OK) {
                tiku_kits_crypto_tls13_last_read_fail = 1;   /* no full record from the wire */
                c->closed = 1; return 0;
            }
            if (type == REC_CCS) continue;
            if (type != REC_APP) {
                tiku_kits_crypto_tls13_last_read_fail = 2;   /* unexpected wire record type */
                tiku_kits_crypto_tls13_last_read_type = type;
                c->closed = 1; return BAD;
            }
            if (aead_open(&gcm, c->s_iv, c->s_seq, blen, hs_buf, &ptlen, &inner) != OK) {
                tiku_kits_crypto_tls13_last_read_fail = 3;   /* record body failed to decrypt */
                c->closed = 1; return BAD;
            }
            c->s_seq++;
            if (inner == REC_ALERT) {
                tiku_kits_crypto_tls13_last_read_fail = 4;   /* peer sent a TLS alert */
                c->closed = 1; return 0;
            }
            if (inner == REC_HS)    continue;        /* post-handshake (e.g. NewSessionTicket) */
            if (inner != REC_APP)   continue;
            c->rx_len = ptlen; c->rx_off = 0;
            break;
        }
    }
    {
        size_t avail = c->rx_len - c->rx_off;
        size_t take = avail < max ? avail : max;
        tiku_kits_crypto_tls13_last_read_fail = 0;       /* delivered application data */
        memcpy(buf, hs_buf + c->rx_off, take);
        c->rx_off += take;
        return (int)take;
    }
}
