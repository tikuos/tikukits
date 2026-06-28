/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls12.c - minimal TLS 1.2 client (ECDHE-P256 + AES-128-GCM)
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

#include "tiku_kits_crypto_tls12.h"
#include "../sha256/tiku_kits_crypto_sha256.h"
#include "../sha384/tiku_kits_crypto_sha384.h"
#include "../hmac/tiku_kits_crypto_hmac.h"
#include "../gcm/tiku_kits_crypto_gcm.h"
#include "../p256/tiku_kits_crypto_p256.h"
#include "../p384/tiku_kits_crypto_p384.h"
#include "../rsa/tiku_kits_crypto_rsa.h"
#include "../x509/tiku_kits_crypto_x509.h"
#include <string.h>

#define OK   TIKU_KITS_CRYPTO_TLS12_OK
#define BAD  TIKU_KITS_CRYPTO_TLS12_BAD

#define REC_CCS   20
#define REC_ALERT 21
#define REC_HS    22
#define REC_APP   23

#define HS_CLIENT_HELLO 1
#define HS_SERVER_HELLO 2
#define HS_CERT         11
#define HS_SKE          12
#define HS_SHD          14
#define HS_CKE          16
#define HS_FINISHED     20

#define MAX_CERTS 6
/* TLS-1.2-only servers are the older/enterprise tail and carry small chains,
 * so 8 KB covers the record + reassembled flight.  hsb doubles as the
 * post-handshake app-data decrypt buffer (the flight is done by then). */
#ifndef TLS12_BUF
#define TLS12_BUF  8192
#endif

/* Optional milestone hook (shared spirit with the 1.3 client). */
void (*tiku_kits_crypto_tls12_dbg)(const char *) = 0;
#define DBG(s) do { if (tiku_kits_crypto_tls12_dbg) tiku_kits_crypto_tls12_dbg(s); } while (0)

static uint8_t rec[TLS12_BUF];     /* one incoming record body            */
static uint8_t hsb[TLS12_BUF];     /* reassembled server handshake flight  */

static uint16_t rd16(const uint8_t *p){ return (uint16_t)((p[0]<<8)|p[1]); }
static uint32_t rd24(const uint8_t *p){ return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2]; }
static void wr16(uint8_t *p, uint16_t v){ p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static void wr24(uint8_t *p, uint32_t v){ p[0]=(uint8_t)(v>>16); p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)v; }

/* ---- transport --------------------------------------------------------- */

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

/* Read one record into rec[]; returns type + body length. */
static int read_record(const tiku_kits_crypto_tls13_io_t *io, uint8_t *type, size_t *blen)
{
    uint8_t hdr[5]; size_t n;
    if (io_recv(io, hdr, 5) != OK) return BAD;
    n = rd16(hdr + 3);
    if (n == 0 || n > TLS12_BUF) return BAD;
    if (io_recv(io, rec, n) != OK) return BAD;
    *type = hdr[0]; *blen = n;
    return OK;
}

static int write_record(const tiku_kits_crypto_tls13_io_t *io, uint8_t type,
                        const uint8_t *body, size_t blen)
{
    uint8_t hdr[5];
    hdr[0] = type; hdr[1] = 0x03; hdr[2] = 0x03; wr16(hdr + 3, (uint16_t)blen);
    if (io_send(io, hdr, 5) != OK) return BAD;
    return io_send(io, body, blen);
}

/* ---- PRF (P_SHA256) ----------------------------------------------------- */

static void hmac256(const uint8_t *key, size_t kl, const uint8_t *d, size_t dl, uint8_t out[32])
{
    tiku_kits_crypto_hmac_sha256(key, kl, d, dl, out);
}

/* TLS 1.2 PRF = P_SHA256(secret, label || seed), filling out[outlen]. */
static void prf(const uint8_t *secret, size_t sl, const char *label,
                const uint8_t *seed, size_t seedl, uint8_t *out, size_t outlen)
{
    uint8_t ls[80];                 /* label + seed (labels are short) */
    size_t  ll = strlen(label), msl = ll + seedl;
    uint8_t a[32], tmp[32];
    size_t  pos = 0;
    if (msl > sizeof ls) return;
    memcpy(ls, label, ll); memcpy(ls + ll, seed, seedl);
    hmac256(secret, sl, ls, msl, a);            /* A(1) = HMAC(secret, seed) */
    while (pos < outlen) {
        uint8_t in[32 + 80]; size_t take;
        memcpy(in, a, 32); memcpy(in + 32, ls, msl);
        hmac256(secret, sl, in, 32 + msl, tmp); /* HMAC(secret, A(i)||seed) */
        take = outlen - pos; if (take > 32) take = 32;
        memcpy(out + pos, tmp, take); pos += take;
        hmac256(secret, sl, a, 32, a);          /* A(i+1) = HMAC(secret, A(i)) */
    }
}

/* ---- ClientHello -------------------------------------------------------- */

static size_t build_client_hello(uint8_t *out, const uint8_t random[32],
                                 const uint8_t ecdhe_pub[65], const char *host)
{
    uint8_t *p = out + 4, *ext, *extlen;
    size_t hlen = strlen(host), elen;

    *p++ = 0x03; *p++ = 0x03;                       /* legacy_version 1.2 */
    memcpy(p, random, 32); p += 32;                 /* random            */
    *p++ = 0x00;                                    /* session_id (empty)*/
    wr16(p, 4); p += 2;                             /* cipher_suites len */
    wr16(p, 0xC02B); p += 2;                        /* ECDHE-ECDSA-GCM   */
    wr16(p, 0xC02F); p += 2;                        /* ECDHE-RSA-GCM     */
    *p++ = 0x01; *p++ = 0x00;                        /* compression: null */

    extlen = p; p += 2;
    ext = p;
    /* server_name */
    wr16(p,0x0000); p+=2; wr16(p,(uint16_t)(hlen+5)); p+=2;
    wr16(p,(uint16_t)(hlen+3)); p+=2; *p++=0x00; wr16(p,(uint16_t)hlen); p+=2;
    memcpy(p,host,hlen); p+=hlen;
    /* supported_groups: secp256r1 */
    wr16(p,0x000a); p+=2; wr16(p,4); p+=2; wr16(p,2); p+=2; wr16(p,0x0017); p+=2;
    /* ec_point_formats: uncompressed */
    wr16(p,0x000b); p+=2; wr16(p,2); p+=2; *p++=0x01; *p++=0x00;
    /* signature_algorithms (what we can verify) */
    wr16(p,0x000d); p+=2; wr16(p,12); p+=2; wr16(p,10); p+=2;
    wr16(p,0x0403); p+=2; wr16(p,0x0503); p+=2;     /* ecdsa sha256/384  */
    wr16(p,0x0401); p+=2; wr16(p,0x0501); p+=2;     /* rsa_pkcs1 sha256/384 */
    wr16(p,0x0804); p+=2;                           /* rsa_pss sha256    */
    /* key_share is TLS 1.3 only; 1.2 sends the point in ClientKeyExchange */
    (void)ecdhe_pub;

    elen = (size_t)(p - ext); wr16(extlen, (uint16_t)elen);
    out[0] = HS_CLIENT_HELLO;
    wr24(out + 1, (uint32_t)(p - (out + 4)));
    return (size_t)(p - out);
}

/* ---- ServerKeyExchange signature verification --------------------------- */

static void sha256_of(const uint8_t *d, size_t n, uint8_t o[32])
{ tiku_kits_crypto_sha256_ctx_t c; tiku_kits_crypto_sha256_init(&c);
  tiku_kits_crypto_sha256_update(&c,d,n); tiku_kits_crypto_sha256_final(&c,o); }
static void sha384_of(const uint8_t *d, size_t n, uint8_t o[48])
{ tiku_kits_crypto_sha384_ctx_t c; tiku_kits_crypto_sha384_init(&c);
  tiku_kits_crypto_sha384_update(&c,d,n); tiku_kits_crypto_sha384_final(&c,o); }

static void der_int_n(const uint8_t *b, size_t l, uint8_t *out, size_t n)
{
    memset(out, 0, n);
    while (l > 1 && b[0] == 0x00) { b++; l--; }
    if (l > n) { b += (l - n); l = n; }
    memcpy(out + (n - l), b, l);
}

/* Verify the ServerKeyExchange signature over crand||srand||params using the
 * leaf key, dispatched by the TLS 1.2 SignatureAndHashAlgorithm. */
static int ske_verify(const tiku_kits_crypto_x509_t *leaf,
                      uint8_t hashalg, uint8_t sigalg,
                      const uint8_t *signed_data, size_t sdlen,
                      const uint8_t *sig, size_t slen)
{
    uint8_t h[48]; size_t hl;
    if (hashalg == 4) { sha256_of(signed_data, sdlen, h); hl = 32; }
    else if (hashalg == 5) { sha384_of(signed_data, sdlen, h); hl = 48; }
    else return BAD;

    if (sigalg == 1) {                       /* RSA PKCS#1 v1.5 */
        if (leaf->pk_alg != TIKU_X509_PK_RSA) return BAD;
        if (hl == 32)
            return tiku_kits_crypto_rsa_pkcs1_sha256_verify(leaf->rsa_n, leaf->rsa_n_len,
                       leaf->rsa_e, leaf->rsa_e_len, sig, slen, h) == 0 ? OK : BAD;
        return tiku_kits_crypto_rsa_pkcs1_sha384_verify(leaf->rsa_n, leaf->rsa_n_len,
                   leaf->rsa_e, leaf->rsa_e_len, sig, slen, h) == 0 ? OK : BAD;
    }
    if (sigalg == 3) {                       /* ECDSA (DER SEQ{r,s}) */
        const uint8_t *p = sig, *e = sig + slen; uint8_t r[48], s[48]; size_t rl, sl;
        if (e - p < 2 || p[0] != 0x30) return BAD;
        p += 2;
        if (p >= e || p[0] != 0x02) return BAD;
        rl = p[1]; { const uint8_t *rb = p + 2; p = rb + rl;
        if (p >= e || p[0] != 0x02) return BAD;
        sl = p[1]; { const uint8_t *sb = p + 2;
        if (leaf->pk_alg == TIKU_X509_PK_EC_P256 && hl == 32) {
            der_int_n(rb, rl, r, 32); der_int_n(sb, sl, s, 32);
            return tiku_kits_crypto_p256_ecdsa_verify(leaf->ec_point+1, leaf->ec_point+33,
                                                      h, 32, r, s) == 0 ? OK : BAD;
        }
        if (leaf->pk_alg == TIKU_X509_PK_EC_P384) {
            der_int_n(rb, rl, r, 48); der_int_n(sb, sl, s, 48);
            return tiku_kits_crypto_p384_ecdsa_verify(leaf->ec_point+1, leaf->ec_point+49,
                                                      h, hl, r, s) == 0 ? OK : BAD;
        } } }
        return BAD;
    }
    return BAD;
}

/* ---- record protection (AES-128-GCM, TLS 1.2 framing) ------------------- */

static void gcm_aad(uint8_t aad[13], uint64_t seq, uint8_t type, uint16_t ptlen)
{
    int i; for (i = 0; i < 8; i++) aad[i] = (uint8_t)(seq >> (8 * (7 - i)));
    aad[8] = type; aad[9] = 0x03; aad[10] = 0x03; wr16(aad + 11, ptlen);
}

/* ---- handshake ---------------------------------------------------------- */

int tiku_kits_crypto_tls12_connect(const tiku_kits_crypto_tls13_io_t *io,
                                   tiku_kits_crypto_tls13_rng_t rng,
                                   const char *host,
                                   const tiku_kits_crypto_x509_root_t *store, int nstore,
                                   uint64_t now_unix,
                                   tiku_kits_crypto_tls12_conn_t *conn)
{
    static uint8_t chbuf[512];
    uint8_t crand[32], srand[32], priv[32], pub[65], peer[65];
    uint8_t pre[32], master[48], kb[40], seedb[64];
    uint8_t verify[12], fin_seed[32], type;
    tiku_kits_crypto_x509_t chain[MAX_CERTS]; int nchain = 0;
    tiku_kits_crypto_sha256_ctx_t th;           /* handshake transcript */
    tiku_kits_crypto_gcm_ctx_t cg, sg;
    size_t blen, chlen, hs_len = 0, parsed = 0;
    int got_shd = 0, cipher_ecdsa = 0, have_cipher = 0;

    memset(conn, 0, sizeof *conn);
    conn->io = *io;

    /* 1. ephemeral P-256 key + ClientHello */
    { uint8_t seed[32]; do { rng(seed,32); }
      while (tiku_kits_crypto_p256_ecdh_keypair(seed, priv, pub) != 0); }
    rng(crand, 32);
    tiku_kits_crypto_sha256_init(&th);
    chlen = build_client_hello(chbuf, crand, pub, host);
    tiku_kits_crypto_sha256_update(&th, chbuf, chlen);
    if (write_record(io, REC_HS, chbuf, chlen) != OK) return BAD;
    DBG("client hello sent");

    /* 2. read the server flight (SH, Cert, SKE, SHD) */
    {
        const uint8_t *ske_params = 0; size_t ske_params_len = 0;
        uint8_t ske_ha = 0, ske_sa = 0; const uint8_t *ske_sig = 0; size_t ske_slen = 0;
        while (!got_shd) {
            if (read_record(io, &type, &blen) != OK) return BAD;
            if (type == REC_CCS) continue;
            if (type == REC_ALERT) return BAD;
            if (type != REC_HS) return BAD;
            if (hs_len + blen > TLS12_BUF) return BAD;
            memcpy(hsb + hs_len, rec, blen); hs_len += blen;

            while (hs_len - parsed >= 4) {
                const uint8_t *m = hsb + parsed; uint8_t mt = m[0];
                size_t ml = rd24(m + 1);
                if (hs_len - parsed < 4 + ml) break;
                tiku_kits_crypto_sha256_update(&th, m, 4 + ml);   /* transcript */

                if (mt == HS_SERVER_HELLO) {
                    const uint8_t *p = m + 4; uint16_t cs; size_t sidl;
                    if (ml < 38) return BAD;
                    p += 2; memcpy(srand, p, 32); p += 32;
                    sidl = *p++; p += sidl;
                    cs = rd16(p);
                    if (cs == 0xC02B) cipher_ecdsa = 1;
                    else if (cs == 0xC02F) cipher_ecdsa = 0;
                    else return BAD;
                    have_cipher = 1;
                    DBG("got ServerHello");
                } else if (mt == HS_CERT) {
                    const uint8_t *cp = m + 4, *ce = m + 4 + ml;
                    if (cp + 3 > ce) return BAD;
                    cp += 3;                              /* certificate_list len */
                    while (cp + 3 <= ce && nchain < MAX_CERTS) {
                        size_t clen = rd24(cp); cp += 3;
                        if (cp + clen > ce) return BAD;
                        if (tiku_kits_crypto_x509_parse(cp, clen, &chain[nchain]) != 0) return BAD;
                        nchain++; cp += clen;
                    }
                    DBG("got Certificate");
                } else if (mt == HS_SKE) {
                    const uint8_t *p = m + 4, *e = m + 4 + ml;
                    if (e - p < 4) return BAD;
                    if (p[0] != 0x03 || rd16(p + 1) != 0x0017) return BAD;  /* named secp256r1 */
                    { size_t pl = p[3];
                      if (4 + pl + 4 > ml || p[4] != 0x04 || pl != 65) return BAD;
                      ske_params = p; ske_params_len = 4 + pl;     /* curve_type..point */
                      memcpy(peer, p + 4, 65);
                      p += 4 + pl;
                      ske_ha = p[0]; ske_sa = p[1];
                      ske_slen = rd16(p + 2); ske_sig = p + 4;
                      if (p + 4 + ske_slen > e) return BAD; }
                    DBG("got ServerKeyExchange");
                } else if (mt == HS_SHD) {
                    got_shd = 1;
                    DBG("got ServerHelloDone");
                }
                parsed += 4 + ml;
                if (got_shd) break;
            }
        }
        if (!have_cipher || nchain < 1 || ske_params == 0) return BAD;

        /* 3. authenticate: chain to a trusted root + SKE signature over
         *    client_random || server_random || ECDHE params */
        if (tiku_kits_crypto_x509_verify_chain_store(chain, nchain, store, nstore,
                                                     host, now_unix) != OK) return BAD;
        DBG("chain trusted");
        {
            static uint8_t sd[160];
            memcpy(sd, crand, 32); memcpy(sd + 32, srand, 32);
            memcpy(sd + 64, ske_params, ske_params_len);
            if (ske_verify(&chain[0], ske_ha, ske_sa, sd, 64 + ske_params_len,
                           ske_sig, ske_slen) != OK) return BAD;
        }
        (void)cipher_ecdsa;
        DBG("ServerKeyExchange verified");
    }

    /* 4. ECDHE shared secret -> master secret -> key block */
    if (tiku_kits_crypto_p256_ecdh_shared(priv, peer, pre) != OK) return BAD;
    memcpy(seedb, crand, 32); memcpy(seedb + 32, srand, 32);
    prf(pre, 32, "master secret", seedb, 64, master, 48);
    memcpy(seedb, srand, 32); memcpy(seedb + 32, crand, 32);     /* note: server||client */
    prf(master, 48, "key expansion", seedb, 64, kb, 40);
    memcpy(conn->c_key, kb + 0,  16);
    memcpy(conn->s_key, kb + 16, 16);
    memcpy(conn->c_iv,  kb + 32, 4);
    memcpy(conn->s_iv,  kb + 36, 4);
    tiku_kits_crypto_gcm_init(&cg, conn->c_key);
    tiku_kits_crypto_gcm_init(&sg, conn->s_key);

    /* 5. ClientKeyExchange (our point) */
    {
        uint8_t cke[4 + 1 + 65];
        cke[0] = HS_CKE; wr24(cke + 1, 66);
        cke[4] = 65; memcpy(cke + 5, pub, 65);
        tiku_kits_crypto_sha256_update(&th, cke, sizeof cke);
        if (write_record(io, REC_HS, cke, sizeof cke) != OK) return BAD;
    }

    /* 6. ChangeCipherSpec */
    { uint8_t ccs = 0x01; if (write_record(io, REC_CCS, &ccs, 1) != OK) return BAD; }

    /* 7. client Finished (encrypted): verify_data =
     *    PRF(master, "client finished", Hash(CH..ClientKeyExchange)) */
    { tiku_kits_crypto_sha256_ctx_t snap = th;        /* snapshot, keep th alive */
      tiku_kits_crypto_sha256_final(&snap, fin_seed); }
    prf(master, 48, "client finished", fin_seed, 32, verify, 12);
    {
        uint8_t finmsg[16], aad[13], nonce[12], ct[16], tag[16], outrec[40];
        finmsg[0] = HS_FINISHED; wr24(finmsg + 1, 12); memcpy(finmsg + 4, verify, 12);
        tiku_kits_crypto_sha256_update(&th, finmsg, 16);   /* fold into transcript */
        memcpy(nonce, conn->c_iv, 4);
        { int i; for (i=0;i<8;i++) nonce[4+i] = (uint8_t)(conn->c_seq >> (8*(7-i))); }
        gcm_aad(aad, conn->c_seq, REC_HS, 16);
        if (tiku_kits_crypto_gcm_encrypt(&cg, nonce, aad, 13, finmsg, 16, ct, tag) != 0) return BAD;
        memcpy(outrec, nonce + 4, 8); memcpy(outrec + 8, ct, 16); memcpy(outrec + 24, tag, 16);
        if (write_record(io, REC_HS, outrec, 40) != OK) return BAD;
        conn->c_seq++;
    }
    DBG("client Finished sent");

    /* 8. server ChangeCipherSpec + Finished */
    {
        if (read_record(io, &type, &blen) != OK || type != REC_CCS) return BAD;
        if (read_record(io, &type, &blen) != OK || type != REC_HS) return BAD;
        if (blen < 8 + 16 + 16) return BAD;
        {
            uint8_t aad[13], nonce[12], pt[16], exp[12]; size_t ctlen = blen - 8 - 16;
            memcpy(nonce, conn->s_iv, 4); memcpy(nonce + 4, rec, 8);
            gcm_aad(aad, conn->s_seq, REC_HS, (uint16_t)ctlen);
            if (tiku_kits_crypto_gcm_decrypt(&sg, nonce, aad, 13, rec + 8,
                                             (uint16_t)ctlen, rec + 8 + ctlen, pt) != 0) return BAD;
            conn->s_seq++;
            if (ctlen != 16 || pt[0] != HS_FINISHED || rd24(pt + 1) != 12) return BAD;
            tiku_kits_crypto_sha256_final(&th, fin_seed);   /* Hash(CH..clientFinished) */
            prf(master, 48, "server finished", fin_seed, 32, exp, 12);
            if (memcmp(pt + 4, exp, 12) != 0) return BAD;
        }
    }
    DBG("server Finished ok -- established");
    return OK;
}

/* ---- application data (AES-128-GCM, explicit nonce = seq) --------------- */

int tiku_kits_crypto_tls12_write(tiku_kits_crypto_tls12_conn_t *c,
                                 const uint8_t *buf, size_t len)
{
    static uint8_t out[8 + 1024 + 16];
    tiku_kits_crypto_gcm_ctx_t g;
    size_t off = 0;
    tiku_kits_crypto_gcm_init(&g, c->c_key);
    while (off < len) {
        size_t chunk = len - off; if (chunk > 1024) chunk = 1024;
        uint8_t aad[13], nonce[12], hdr[5];
        memcpy(nonce, c->c_iv, 4);
        { int i; for (i=0;i<8;i++) nonce[4+i] = (uint8_t)(c->c_seq >> (8*(7-i))); }
        memcpy(out, nonce + 4, 8);
        gcm_aad(aad, c->c_seq, REC_APP, (uint16_t)chunk);
        if (tiku_kits_crypto_gcm_encrypt(&g, nonce, aad, 13, buf + off, (uint16_t)chunk,
                                         out + 8, out + 8 + chunk) != 0) return -1;
        hdr[0] = REC_APP; hdr[1] = 0x03; hdr[2] = 0x03; wr16(hdr + 3, (uint16_t)(chunk + 8 + 16));
        if (io_send(&c->io, hdr, 5) != OK) return -1;
        if (io_send(&c->io, out, chunk + 8 + 16) != OK) return -1;
        c->c_seq++;
        off += chunk;
    }
    return (int)len;
}

int tiku_kits_crypto_tls12_read(tiku_kits_crypto_tls12_conn_t *c,
                                uint8_t *buf, size_t len)
{
    uint8_t *app_pt = hsb;                   /* reuse the flight buffer */
    if (c->rx_off >= c->rx_len) {            /* refill */
        uint8_t type; size_t blen;
        if (c->closed) return 0;
        for (;;) {
            if (read_record(&c->io, &type, &blen) != OK) { c->closed = 1; return 0; }
            if (type == REC_ALERT) { c->closed = 1; return 0; }
            if (type == REC_HS || type == REC_CCS) continue;   /* tickets etc. */
            if (type != REC_APP || blen < 8 + 16) return -1;
            {
                tiku_kits_crypto_gcm_ctx_t g; uint8_t aad[13], nonce[12];
                size_t ctlen = blen - 8 - 16;
                tiku_kits_crypto_gcm_init(&g, c->s_key);
                memcpy(nonce, c->s_iv, 4); memcpy(nonce + 4, rec, 8);
                gcm_aad(aad, c->s_seq, REC_APP, (uint16_t)ctlen);
                if (tiku_kits_crypto_gcm_decrypt(&g, nonce, aad, 13, rec + 8,
                                                 (uint16_t)ctlen, rec + 8 + ctlen, app_pt) != 0)
                    return -1;
                c->s_seq++; c->rx_len = ctlen; c->rx_off = 0;
            }
            break;
        }
    }
    {
        size_t avail = c->rx_len - c->rx_off, take = len < avail ? len : avail;
        memcpy(buf, app_pt + c->rx_off, take); c->rx_off += take;
        return (int)take;
    }
}
