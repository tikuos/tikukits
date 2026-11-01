/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_server.c - TLS 1.3 server, one state per connection
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * RFC 8446 for the server side, one handshake message per record.  The
 * record buffer holds the record being read; once a ClientHello is parsed
 * it holds each message the server builds, sealed in place.  Application
 * data is decrypted in place and stays there until the caller takes it.
 */

#include "tiku_kits_crypto_tls_server.h"
#include "../../../crypto/x25519/tiku_kits_crypto_x25519.h"
#include "../../../crypto/p256/tiku_kits_crypto_p256.h"
#include "../../../crypto/hkdf/tiku_kits_crypto_hkdf.h"
#include "../../../crypto/hmac/tiku_kits_crypto_hmac.h"
#include <string.h>

#define S_HELLO     TIKU_KITS_CRYPTO_TLS_SERVER_HELLO
#define S_HELLO2    TIKU_KITS_CRYPTO_TLS_SERVER_HELLO2
#define S_FINISHED  TIKU_KITS_CRYPTO_TLS_SERVER_FINISHED
#define S_OPEN      TIKU_KITS_CRYPTO_TLS_SERVER_OPEN
#define S_CLOSED    TIKU_KITS_CRYPTO_TLS_SERVER_CLOSED
#define S_FAILED    TIKU_KITS_CRYPTO_TLS_SERVER_FAILED

#define REC_CCS     20u
#define REC_ALERT   21u
#define REC_HS      22u
#define REC_APP     23u

#define HS_CLIENT_HELLO  1u
#define HS_SERVER_HELLO  2u
#define HS_EE            8u
#define HS_CERT          11u
#define HS_CERT_VERIFY   15u
#define HS_FINISHED      20u
#define HS_KEY_UPDATE    24u
#define HS_MESSAGE_HASH  254u

#define EXT_SUPPORTED_GROUPS   10u
#define EXT_SIG_ALGS           13u
#define EXT_RECORD_SIZE_LIMIT  28u
#define EXT_SUPPORTED_VERSIONS 43u
#define EXT_KEY_SHARE          51u

#define GROUP_X25519    0x001Du
#define GROUP_P256      0x0017u
#define G_X25519        1u
#define G_P256          2u

#define AL_CLOSE_NOTIFY        0u
#define AL_UNEXPECTED_MESSAGE  10u
#define AL_BAD_RECORD_MAC      20u
#define AL_RECORD_OVERFLOW     22u
#define AL_HANDSHAKE_FAILURE   40u
#define AL_ILLEGAL_PARAMETER   47u
#define AL_DECODE_ERROR        50u
#define AL_DECRYPT_ERROR       51u
#define AL_PROTOCOL_VERSION    70u
#define AL_INTERNAL_ERROR      80u
#define AL_MISSING_EXTENSION   109u

#define HEAD      TIKU_KITS_CRYPTO_TLS_SERVER_HEAD
#define TAG       16u
#define REC_MAX   (16384u + 256u)   /* the largest TLSCiphertext body */
#define PT_MAX    16384u            /* the largest TLSPlaintext body */

/* The random of a HelloRetryRequest: SHA-256 of "HelloRetryRequest". */
static const uint8_t hrr_random[32] = {
    0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C, 0x02,
    0x1E, 0x65, 0xB8, 0x91, 0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E,
    0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C
};

/* SHA-256 of the empty string, the context of Derive-Secret(., "derived", ""). */
static const uint8_t empty_hash[32] = {
    0xE3, 0xB0, 0xC4, 0x42, 0x98, 0xFC, 0x1C, 0x14, 0x9A, 0xFB, 0xF4, 0xC8,
    0x99, 0x6F, 0xB9, 0x24, 0x27, 0xAE, 0x41, 0xE4, 0x64, 0x9B, 0x93, 0x4C,
    0xA4, 0x95, 0x99, 0x1B, 0x78, 0x52, 0xB8, 0x55
};

/*---------------------------------------------------------------------------*/
/* SMALL HELPERS                                                             */
/*---------------------------------------------------------------------------*/

static void
wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void
wr24(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 16);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)v;
}

static uint16_t
rd16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t
rd24(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

/* Clears @p n bytes in a way the compiler keeps. */
static void
wipe(void *p, size_t n)
{
    volatile uint8_t *v = (volatile uint8_t *)p;

    while (n-- > 0u) {
        *v++ = 0u;
    }
}

/* 0 when the @p n bytes match, without an early exit. */
static uint8_t
differ(const uint8_t *a, const uint8_t *b, size_t n)
{
    uint8_t d = 0u;

    while (n-- > 0u) {
        d |= (uint8_t)(*a++ ^ *b++);
    }
    return d;
}

/* The transcript's hash so far. */
static void
th_hash(const tiku_kits_crypto_tls_server_t *t, uint8_t out[32])
{
    tiku_kits_crypto_sha256_ctx_t c = t->th;

    tiku_kits_crypto_sha256_final(&c, out);
}

static void
th_add(tiku_kits_crypto_tls_server_t *t, const uint8_t *m, size_t n)
{
    tiku_kits_crypto_sha256_update(&t->th, m, n);
}

/* A traffic secret's key and IV into @p gcm and @p iv. */
static void
install(const uint8_t secret[32], tiku_kits_crypto_gcm_ctx_t *gcm,
        uint8_t iv[12])
{
    uint8_t key[16];

    tiku_kits_crypto_hkdf_expand_label(secret, "key", NULL, 0, key, 16);
    tiku_kits_crypto_hkdf_expand_label(secret, "iv", NULL, 0, iv, 12);
    tiku_kits_crypto_gcm_init(gcm, key);
    wipe(key, sizeof key);
}

static void
nonce(const uint8_t iv[12], uint64_t seq, uint8_t out[12])
{
    int i;

    memcpy(out, iv, 12);
    for (i = 0; i < 8; i++) {
        out[11 - i] ^= (uint8_t)(seq >> (8 * i));
    }
}

/* HMAC of the transcript under the Finished key of @p base. */
static void
finished_mac(const tiku_kits_crypto_tls_server_t *t, const uint8_t base[32],
             uint8_t out[32])
{
    uint8_t fk[32], h[32];

    tiku_kits_crypto_hkdf_expand_label(base, "finished", NULL, 0, fk, 32);
    th_hash(t, h);
    tiku_kits_crypto_hmac_sha256(fk, 32, h, 32, out);
    wipe(fk, sizeof fk);
}

/*---------------------------------------------------------------------------*/
/* RECORDS OUT                                                               */
/*---------------------------------------------------------------------------*/

static void fail(tiku_kits_crypto_tls_server_t *t, uint8_t alert);

/* Send @p n bytes, failing the connection if they cannot go. */
static int
emit(tiku_kits_crypto_tls_server_t *t, const uint8_t *b, size_t n)
{
    if (t->out(t->ctx, b, n) != 0) {
        t->state = S_FAILED;
        tiku_kits_crypto_tls_server_wipe(t);
        return -1;
    }
    return 0;
}

/* Seal the @p ptlen bytes at @p rec + HEAD as one record of @p type under
 * the server's traffic key; returns the record's length. */
static size_t
seal_as(tiku_kits_crypto_tls_server_t *t, uint8_t *rec, size_t ptlen,
        uint8_t type)
{
    uint8_t n[12];
    uint16_t len = (uint16_t)(ptlen + 1u + TAG);

    rec[HEAD + ptlen] = type;
    rec[0] = REC_APP;
    rec[1] = 0x03;
    rec[2] = 0x03;
    wr16(rec + 3, len);
    nonce(t->tx_iv, t->tx_seq++, n);
    tiku_kits_crypto_gcm_encrypt(&t->tx, n, rec, HEAD, rec + HEAD,
                                 (uint16_t)(ptlen + 1u), rec + HEAD,
                                 rec + HEAD + ptlen + 1u);
    return HEAD + (size_t)len;
}

/* A plaintext record of @p type around the @p len bytes at buf + HEAD. */
static int
emit_plain(tiku_kits_crypto_tls_server_t *t, uint8_t type, size_t len)
{
    t->buf[0] = type;
    t->buf[1] = 0x03;
    t->buf[2] = 0x03;
    wr16(t->buf + 3, (uint16_t)len);
    return emit(t, t->buf, HEAD + len);
}

/* The handshake message of @p len body bytes at buf + HEAD + 4: its
 * header written, added to the transcript, sent sealed or in clear. */
static int
emit_hs(tiku_kits_crypto_tls_server_t *t, uint8_t type, size_t len,
        int sealed)
{
    uint8_t *m = t->buf + HEAD;

    m[0] = type;
    wr24(m + 1, (uint32_t)len);
    th_add(t, m, 4u + len);
    if (!sealed) {
        return emit_plain(t, REC_HS, 4u + len);
    }
    return emit(t, t->buf, seal_as(t, t->buf, 4u + len, REC_HS));
}

/* The dummy change_cipher_spec of middlebox compatibility mode, once. */
static int
emit_ccs(tiku_kits_crypto_tls_server_t *t)
{
    static const uint8_t ccs[6] = { REC_CCS, 0x03, 0x03, 0x00, 0x01, 0x01 };

    if (t->ccs_sent || t->sid_len == 0u) {
        return 0;
    }
    t->ccs_sent = 1u;
    return emit(t, ccs, sizeof ccs);
}

/* Send an alert, sealed once keys are in place, and end the connection
 * unless it is close_notify. */
static void
send_alert(tiku_kits_crypto_tls_server_t *t, uint8_t level, uint8_t desc)
{
    uint8_t rec[HEAD + 2u + 1u + TAG];

    rec[HEAD] = level;
    rec[HEAD + 1u] = desc;
    if (t->tx_on) {
        (void)t->out(t->ctx, rec, seal_as(t, rec, 2u, REC_ALERT));
    } else {
        rec[0] = REC_ALERT;
        rec[1] = 0x03;
        rec[2] = 0x03;
        wr16(rec + 3, 2u);
        (void)t->out(t->ctx, rec, HEAD + 2u);
    }
    wipe(rec, sizeof rec);
}

static void
fail(tiku_kits_crypto_tls_server_t *t, uint8_t alert)
{
    if (t->state == S_FAILED) {
        return;
    }
    send_alert(t, 2u, alert);
    t->alert = alert;
    t->alert_ours = 1u;
    t->state = S_FAILED;
    t->pt_len = 0u;
    tiku_kits_crypto_tls_server_wipe(t);
}

/*---------------------------------------------------------------------------*/
/* CLIENTHELLO                                                               */
/*---------------------------------------------------------------------------*/

/* What a ClientHello offered; pointers into the record buffer. */
typedef struct {
    const uint8_t *sid;
    uint8_t        sid_len;
    uint8_t        suite;        /* TLS_AES_128_GCM_SHA256 offered */
    uint8_t        tls13;        /* supported_versions lists 0x0304 */
    uint8_t        sigalg;       /* ecdsa_secp256r1_sha256 offered */
    uint8_t        has_sigalgs;
    uint8_t        groups;       /* bit G_X25519 / G_P256: supported */
    const uint8_t *x25519;       /* 32-byte share, or NULL */
    const uint8_t *p256;         /* 65-byte share, or NULL */
    uint16_t       limit;        /* record_size_limit, 0: none */
} hello_t;

/* Parse the ClientHello body of @p len bytes at @p p; 0 or an alert. */
static uint8_t
parse_hello(const uint8_t *p, size_t len, hello_t *h)
{
    const uint8_t *end = p + len;
    size_t n, i;

    memset(h, 0, sizeof *h);
    if (len < 2u + 32u + 1u) {
        return AL_DECODE_ERROR;
    }
    p += 2u + 32u;                                  /* legacy_version, random */
    h->sid_len = *p++;
    if (h->sid_len > 32u || (size_t)(end - p) < h->sid_len + 2u) {
        return AL_DECODE_ERROR;
    }
    h->sid = p;
    p += h->sid_len;
    n = rd16(p);
    p += 2;
    if ((n & 1u) != 0u || (size_t)(end - p) < n + 1u) {
        return AL_DECODE_ERROR;
    }
    for (i = 0; i < n; i += 2u) {
        if (rd16(p + i) == 0x1301u) {
            h->suite = 1u;
        }
    }
    p += n;
    n = *p++;                                       /* compression methods */
    if ((size_t)(end - p) < n || n != 1u || p[0] != 0u) {
        return AL_ILLEGAL_PARAMETER;
    }
    p += n;
    if (end - p < 2) {
        return AL_DECODE_ERROR;
    }
    n = rd16(p);
    p += 2;
    if ((size_t)(end - p) != n) {
        return AL_DECODE_ERROR;
    }
    while (end - p >= 4) {
        uint16_t type = rd16(p), elen = rd16(p + 2);
        const uint8_t *e = p + 4;

        if ((size_t)(end - e) < elen) {
            return AL_DECODE_ERROR;
        }
        p = e + elen;
        if (type == EXT_SUPPORTED_VERSIONS) {
            if (elen < 1u || e[0] + 1u != elen || (e[0] & 1u) != 0u) {
                return AL_DECODE_ERROR;
            }
            for (i = 1; i < elen; i += 2u) {
                if (rd16(e + i) == 0x0304u) {
                    h->tls13 = 1u;
                }
            }
        } else if (type == EXT_SUPPORTED_GROUPS || type == EXT_SIG_ALGS) {
            if (elen < 2u || rd16(e) + 2u != elen || (rd16(e) & 1u) != 0u) {
                return AL_DECODE_ERROR;
            }
            for (i = 2; i < elen; i += 2u) {
                uint16_t v = rd16(e + i);

                if (type == EXT_SIG_ALGS) {
                    h->sigalg |= (uint8_t)(v == 0x0403u);
                } else if (v == GROUP_X25519) {
                    h->groups |= G_X25519;
                } else if (v == GROUP_P256) {
                    h->groups |= G_P256;
                }
            }
            h->has_sigalgs |= (uint8_t)(type == EXT_SIG_ALGS);
        } else if (type == EXT_KEY_SHARE) {
            if (elen < 2u || rd16(e) + 2u != elen) {
                return AL_DECODE_ERROR;
            }
            for (i = 2; i + 4u <= elen;) {
                uint16_t g = rd16(e + i), klen = rd16(e + i + 2u);

                if (i + 4u + klen > elen) {
                    return AL_DECODE_ERROR;
                }
                if (g == GROUP_X25519 && klen == 32u) {
                    h->x25519 = e + i + 4u;
                } else if (g == GROUP_P256 && klen == 65u) {
                    h->p256 = e + i + 4u;
                }
                i += 4u + klen;
            }
        } else if (type == EXT_RECORD_SIZE_LIMIT) {
            if (elen != 2u) {
                return AL_DECODE_ERROR;
            }
            h->limit = rd16(e);
            if (h->limit < 64u) {
                return AL_ILLEGAL_PARAMETER;
            }
        }
    }
    if (p != end) {
        return AL_DECODE_ERROR;
    }
    if (!h->tls13) {
        return AL_PROTOCOL_VERSION;
    }
    if (!h->has_sigalgs) {
        return AL_MISSING_EXTENSION;
    }
    if (!h->suite || !h->sigalg) {
        return AL_HANDSHAKE_FAILURE;
    }
    return 0u;
}

/* The HelloRetryRequest naming the group to send a share for. */
static int
send_retry(tiku_kits_crypto_tls_server_t *t, uint16_t group)
{
    uint8_t h[32], *b = t->buf + HEAD + 4u;
    uint8_t hdr[4] = { HS_MESSAGE_HASH, 0u, 0u, 32u };
    size_t n = 0;

    /* The transcript restarts as message_hash(ClientHello1). */
    th_hash(t, h);
    tiku_kits_crypto_sha256_init(&t->th);
    th_add(t, hdr, 4u);
    th_add(t, h, 32u);

    wr16(b + n, 0x0303u); n += 2;
    memcpy(b + n, hrr_random, 32); n += 32;
    b[n++] = t->sid_len;
    memcpy(b + n, t->sid, t->sid_len); n += t->sid_len;
    wr16(b + n, 0x1301u); n += 2;
    b[n++] = 0u;
    wr16(b + n, 12u); n += 2;
    wr16(b + n, EXT_SUPPORTED_VERSIONS); wr16(b + n + 2, 2u);
    wr16(b + n + 4, 0x0304u); n += 6;
    wr16(b + n, EXT_KEY_SHARE); wr16(b + n + 2, 2u);
    wr16(b + n + 4, group); n += 6;
    if (emit_hs(t, HS_SERVER_HELLO, n, 0) != 0) {
        return -1;
    }
    return emit_ccs(t);
}

/* Derive-Secret(secret, label, transcript so far). */
static void
derive(const tiku_kits_crypto_tls_server_t *t, const uint8_t secret[32],
       const char *label, uint8_t out[32])
{
    uint8_t h[32];

    th_hash(t, h);
    tiku_kits_crypto_hkdf_expand_label(secret, label, h, 32, out, 32);
}

/* DER INTEGER of a 32-byte big-endian value at @p o; its length. */
static size_t
der_int(uint8_t *o, const uint8_t v[32])
{
    size_t i = 0, n;

    while (i < 31u && v[i] == 0u) {
        i++;
    }
    n = 32u - i;
    o[0] = 0x02;
    if ((v[i] & 0x80u) != 0u) {
        o[1] = (uint8_t)(n + 1u);
        o[2] = 0u;
        memcpy(o + 3, v + i, n);
        return n + 3u;
    }
    o[1] = (uint8_t)n;
    memcpy(o + 2, v + i, n);
    return n + 2u;
}

/* The server's flight after ServerHello: EncryptedExtensions,
 * Certificate, CertificateVerify, Finished.  0, or an alert. */
static uint8_t
send_flight(tiku_kits_crypto_tls_server_t *t)
{
    static const char context[] = "TLS 1.3, server CertificateVerify";
    const tiku_kits_crypto_tls_server_cred_t *c = t->cred;
    uint8_t *b = t->buf + HEAD + 4u;
    uint8_t h[32], d[32], r[32], s[32], extra[32];
    size_t n = 0, need = 4u + 1u + 3u;
    unsigned i;

    /* EncryptedExtensions: record_size_limit, when the client sent one */
    if (t->peer_limit != 0u) {
        uint16_t ours = (uint16_t)(t->cap - HEAD - TAG);

        wr16(b, 6u);
        wr16(b + 2, EXT_RECORD_SIZE_LIMIT);
        wr16(b + 4, 2u);
        wr16(b + 6, ours < PT_MAX + 1u ? ours : (uint16_t)(PT_MAX + 1u));
        n = 8u;
    } else {
        wr16(b, 0u);
        n = 2u;
    }
    if (emit_hs(t, HS_EE, n, 1) != 0) {
        return 0u;
    }

    /* Certificate: an empty request context, then each DER with no
     * extensions */
    for (i = 0; i < c->nchain; i++) {
        need += 3u + c->chain[i].len + 2u;
    }
    if (need + 1u + TAG + HEAD > t->cap) {
        return AL_INTERNAL_ERROR;
    }
    n = 0;
    b[n++] = 0u;
    wr24(b + n, (uint32_t)(need - 4u - 1u - 3u)); n += 3;
    for (i = 0; i < c->nchain; i++) {
        wr24(b + n, c->chain[i].len); n += 3;
        memcpy(b + n, c->chain[i].der, c->chain[i].len);
        n += c->chain[i].len;
        wr16(b + n, 0u); n += 2;
    }
    if (emit_hs(t, HS_CERT, n, 1) != 0) {
        return 0u;
    }

    /* CertificateVerify: 64 spaces, the context string, a zero, the
     * transcript hash; SHA-256 of that, signed */
    th_hash(t, h);
    memset(b, 0x20, 64);
    memcpy(b + 64, context, sizeof context);       /* with its zero */
    memcpy(b + 64 + sizeof context, h, 32);
    tiku_kits_crypto_sha256_hash(b, 64u + sizeof context + 32u, d);
    t->rng(extra, sizeof extra);
    if (tiku_kits_crypto_p256_ecdsa_sign(c->key, d, 32, extra, sizeof extra,
                                         r, s) != 0) {
        return AL_INTERNAL_ERROR;
    }
    wr16(b, 0x0403u);
    n = der_int(b + 6, r);
    n += der_int(b + 6 + n, s);
    b[4] = 0x30;
    b[5] = (uint8_t)n;
    wr16(b + 2, (uint16_t)(n + 2u));
    if (emit_hs(t, HS_CERT_VERIFY, 4u + 2u + n, 1) != 0) {
        return 0u;
    }

    /* Finished */
    finished_mac(t, t->s_tr, b);
    if (emit_hs(t, HS_FINISHED, 32u, 1) != 0) {
        return 0u;
    }
    wipe(extra, sizeof extra);
    return 0u;
}

/* Key exchange, ServerHello and the server's flight for a ClientHello
 * carrying a share of the chosen group.  0, or an alert. */
static uint8_t
answer_hello(tiku_kits_crypto_tls_server_t *t, const hello_t *h)
{
    uint8_t priv[32], pub[65], shared[32], early[32], derived[32];
    uint8_t zero[32], *b = t->buf + HEAD + 4u;
    size_t n = 0, klen;
    uint8_t alert = 0u;

    memset(zero, 0, sizeof zero);
    if (t->group == G_X25519) {
        t->rng(priv, 32);
        tiku_kits_crypto_x25519_base(pub, priv);
        tiku_kits_crypto_x25519_scalarmult(shared, priv, h->x25519);
        klen = 32u;
        if (!differ(shared, zero, 32)) {
            alert = AL_ILLEGAL_PARAMETER;
        }
    } else {
        do {
            t->rng(early, 32);
        } while (tiku_kits_crypto_p256_ecdh_keypair(early, priv, pub) != 0);
        klen = 65u;
        if (tiku_kits_crypto_p256_ecdh_shared(priv, h->p256, shared) != 0) {
            alert = AL_ILLEGAL_PARAMETER;
        }
    }
    wipe(priv, sizeof priv);
    if (alert != 0u) {
        wipe(shared, sizeof shared);
        return alert;
    }

    /* ServerHello */
    wr16(b + n, 0x0303u); n += 2;
    t->rng(b + n, 32); n += 32;
    b[n++] = t->sid_len;
    memcpy(b + n, t->sid, t->sid_len); n += t->sid_len;
    wr16(b + n, 0x1301u); n += 2;
    b[n++] = 0u;
    wr16(b + n, (uint16_t)(6u + 8u + klen)); n += 2;
    wr16(b + n, EXT_SUPPORTED_VERSIONS); wr16(b + n + 2, 2u);
    wr16(b + n + 4, 0x0304u); n += 6;
    wr16(b + n, EXT_KEY_SHARE); wr16(b + n + 2, (uint16_t)(4u + klen));
    wr16(b + n + 4, t->group == G_X25519 ? GROUP_X25519 : GROUP_P256);
    wr16(b + n + 6, (uint16_t)klen);
    memcpy(b + n + 8, pub, klen); n += 8u + klen;
    if (emit_hs(t, HS_SERVER_HELLO, n, 0) != 0 || emit_ccs(t) != 0) {
        wipe(shared, sizeof shared);
        return 0u;
    }

    /* Handshake secrets: early secret from zeros, then the shared key */
    tiku_kits_crypto_hkdf_extract(zero, 32, zero, 32, early);
    tiku_kits_crypto_hkdf_expand_label(early, "derived", empty_hash, 32,
                                       derived, 32);
    tiku_kits_crypto_hkdf_extract(derived, 32, shared, 32, t->secret);
    wipe(shared, sizeof shared);
    derive(t, t->secret, "c hs traffic", t->c_tr);
    derive(t, t->secret, "s hs traffic", t->s_tr);
    install(t->c_tr, &t->rx, t->rx_iv);
    install(t->s_tr, &t->tx, t->tx_iv);
    t->rx_seq = 0u;
    t->tx_seq = 0u;
    t->tx_on = 1u;

    alert = send_flight(t);
    if (alert != 0u || t->state == S_FAILED) {
        wipe(early, sizeof early);
        wipe(derived, sizeof derived);
        return alert;
    }

    /* Application secrets over the transcript through the server's
     * Finished; the server's take effect now, the client's after its
     * Finished. */
    tiku_kits_crypto_hkdf_expand_label(t->secret, "derived", empty_hash, 32,
                                       derived, 32);
    tiku_kits_crypto_hkdf_extract(derived, 32, zero, 32, early);   /* master */
    derive(t, early, "c ap traffic", t->secret);
    derive(t, early, "s ap traffic", t->s_tr);
    install(t->s_tr, &t->tx, t->tx_iv);
    t->tx_seq = 0u;
    wipe(early, sizeof early);
    wipe(derived, sizeof derived);
    t->state = S_FINISHED;
    return 0u;
}

/* A ClientHello in the record buffer: @p m the message, @p len its body. */
static uint8_t
on_hello(tiku_kits_crypto_tls_server_t *t, const uint8_t *m, size_t len)
{
    hello_t h;
    uint8_t alert = parse_hello(m + 4, len, &h);

    if (alert != 0u) {
        return alert;
    }
    if (t->state == S_HELLO2) {
        /* the second hello: the same session id, a share of the group */
        if (h.sid_len != t->sid_len || differ(h.sid, t->sid, h.sid_len) ||
            (t->group == G_X25519 ? h.x25519 : h.p256) == NULL) {
            return AL_ILLEGAL_PARAMETER;
        }
    } else {
        t->sid_len = h.sid_len;
        memcpy(t->sid, h.sid, h.sid_len);
        t->peer_limit = h.limit;
        if (h.x25519 != NULL) {
            t->group = G_X25519;
        } else if (h.p256 != NULL) {
            t->group = G_P256;
        } else {
            th_add(t, m, 4u + len);
            if (h.groups & G_X25519) {
                t->group = G_X25519;
            } else if (h.groups & G_P256) {
                t->group = G_P256;
            } else {
                return AL_HANDSHAKE_FAILURE;
            }
            if (send_retry(t, t->group == G_X25519 ? GROUP_X25519
                                                   : GROUP_P256) == 0) {
                t->state = S_HELLO2;
            }
            return 0u;
        }
    }
    if (t->peer_limit != 0u) {
        uint16_t lim = (uint16_t)(t->peer_limit - 1u);

        t->out_max = lim < t->out_max ? lim : t->out_max;
    }
    th_add(t, m, 4u + len);
    return answer_hello(t, &h);
}

/*---------------------------------------------------------------------------*/
/* RECORDS IN                                                                */
/*---------------------------------------------------------------------------*/

/* Decrypt the protected record in the buffer in place; its content type
 * into @p type and the plaintext length into @p len.  0, or an alert. */
static uint8_t
open_record(tiku_kits_crypto_tls_server_t *t, uint8_t *type, size_t *len)
{
    uint8_t n[12], *body = t->buf + HEAD;
    size_t blen = rd16(t->buf + 3);

    if (blen < 1u + TAG) {
        return AL_DECODE_ERROR;
    }
    nonce(t->rx_iv, t->rx_seq, n);
    if (tiku_kits_crypto_gcm_decrypt(&t->rx, n, t->buf, HEAD, body,
                                     (uint16_t)(blen - TAG), body + blen - TAG,
                                     body) != 0) {
        return AL_BAD_RECORD_MAC;
    }
    t->rx_seq++;
    blen -= TAG;
    while (blen > 0u && body[blen - 1u] == 0u) {
        blen--;
    }
    if (blen == 0u) {
        return AL_UNEXPECTED_MESSAGE;
    }
    *type = body[blen - 1u];
    *len = blen - 1u;
    return 0u;
}

/* A handshake message inside a protected record. */
static uint8_t
on_protected_hs(tiku_kits_crypto_tls_server_t *t, const uint8_t *m, size_t len)
{
    uint8_t mac[32];

    if (len < 4u || rd24(m + 1) != len - 4u) {
        return AL_DECODE_ERROR;
    }
    if (t->state == S_FINISHED && m[0] == HS_FINISHED) {
        if (len != 4u + 32u) {
            return AL_DECODE_ERROR;
        }
        finished_mac(t, t->c_tr, mac);
        if (differ(mac, m + 4, 32)) {
            wipe(mac, sizeof mac);
            return AL_DECRYPT_ERROR;
        }
        wipe(mac, sizeof mac);
        th_add(t, m, len);
        memcpy(t->c_tr, t->secret, 32);
        wipe(t->secret, sizeof t->secret);
        install(t->c_tr, &t->rx, t->rx_iv);
        t->rx_seq = 0u;
        t->state = S_OPEN;
        return 0u;
    }
    if (t->state == S_OPEN && m[0] == HS_KEY_UPDATE) {
        uint8_t ask, next[32];

        if (len != 5u || m[4] > 1u) {
            return AL_DECODE_ERROR;
        }
        ask = m[4];
        tiku_kits_crypto_hkdf_expand_label(t->c_tr, "traffic upd", NULL, 0,
                                           next, 32);
        memcpy(t->c_tr, next, 32);
        install(t->c_tr, &t->rx, t->rx_iv);
        t->rx_seq = 0u;
        if (ask) {
            uint8_t *b = t->buf;

            b[HEAD] = HS_KEY_UPDATE;
            wr24(b + HEAD + 1, 1u);
            b[HEAD + 4] = 0u;
            if (emit(t, b, seal_as(t, b, 5u, REC_HS)) != 0) {
                return 0u;
            }
            tiku_kits_crypto_hkdf_expand_label(t->s_tr, "traffic upd", NULL, 0,
                                               next, 32);
            memcpy(t->s_tr, next, 32);
            install(t->s_tr, &t->tx, t->tx_iv);
            t->tx_seq = 0u;
        }
        wipe(next, sizeof next);
        return 0u;
    }
    return AL_UNEXPECTED_MESSAGE;
}

/* The complete record in the buffer. */
static void
on_record(tiku_kits_crypto_tls_server_t *t)
{
    uint8_t type = t->buf[0], alert = 0u;
    size_t len = rd16(t->buf + 3);
    const uint8_t *m = t->buf + HEAD;

    if (type == REC_CCS) {
        /* one byte, between the first ClientHello and the client's
         * Finished only, ignored */
        if (len != 1u || m[0] != 1u ||
            (t->state != S_HELLO2 && t->state != S_FINISHED)) {
            alert = AL_UNEXPECTED_MESSAGE;
        }
    } else if (type == REC_ALERT) {
        /* in clear: a client that could not take the server's hello */
        t->alert = len == 2u ? m[1] : AL_DECODE_ERROR;
        t->state = S_FAILED;
        tiku_kits_crypto_tls_server_wipe(t);
        return;
    } else if (type == REC_HS && (t->state == S_HELLO || t->state == S_HELLO2)) {
        if (len < 4u || m[0] != HS_CLIENT_HELLO || rd24(m + 1) != len - 4u) {
            alert = AL_UNEXPECTED_MESSAGE;
        } else {
            alert = on_hello(t, m, len - 4u);
        }
    } else if (type == REC_APP &&
               (t->state == S_FINISHED || t->state == S_OPEN)) {
        alert = open_record(t, &type, &len);
        if (alert != 0u) {
            /* nothing */
        } else if (len > PT_MAX) {
            alert = AL_RECORD_OVERFLOW;
        } else if (type == REC_APP && t->state == S_OPEN) {
            t->pt_off = HEAD;
            t->pt_len = (uint16_t)len;
        } else if (type == REC_HS) {
            alert = on_protected_hs(t, m, len);
        } else if (type == REC_ALERT && len == 2u) {
            t->alert = m[1];
            t->state = m[1] == AL_CLOSE_NOTIFY ? S_CLOSED : S_FAILED;
            if (t->state == S_FAILED) {
                tiku_kits_crypto_tls_server_wipe(t);
            }
        } else {
            alert = AL_UNEXPECTED_MESSAGE;
        }
    } else {
        alert = AL_UNEXPECTED_MESSAGE;
    }
    if (alert != 0u) {
        fail(t, alert);
    }
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int
tiku_kits_crypto_tls_server_init(tiku_kits_crypto_tls_server_t *t,
                                 const tiku_kits_crypto_tls_server_cred_t *cred,
                                 tiku_kits_crypto_tls_server_rng_t rng,
                                 tiku_kits_crypto_tls_server_out_t out,
                                 void *ctx, uint8_t *buf, uint16_t cap)
{
    if (t == NULL || cred == NULL || cred->nchain == 0u ||
        cred->nchain > TIKU_KITS_CRYPTO_TLS_SERVER_CHAIN_MAX ||
        cred->key == NULL || rng == NULL || out == NULL || buf == NULL ||
        cap < TIKU_KITS_CRYPTO_TLS_SERVER_CAP_MIN) {
        return -1;
    }
    memset(t, 0, sizeof *t);
    t->cred = cred;
    t->rng = rng;
    t->out = out;
    t->ctx = ctx;
    t->buf = buf;
    t->cap = cap;
    t->out_max = PT_MAX;
    t->state = S_HELLO;
    tiku_kits_crypto_sha256_init(&t->th);
    return 0;
}

size_t
tiku_kits_crypto_tls_server_feed(tiku_kits_crypto_tls_server_t *t,
                                 const uint8_t *in, size_t n)
{
    size_t used = 0;

    while (used < n && t->pt_len == 0u && t->state < S_CLOSED) {
        size_t want, take;

        if (t->have < HEAD) {
            want = HEAD - t->have;
        } else {
            want = HEAD + rd16(t->buf + 3) - t->have;
        }
        take = n - used < want ? n - used : want;
        memcpy(t->buf + t->have, in + used, take);
        t->have = (uint16_t)(t->have + take);
        used += take;
        if (t->have == HEAD) {
            uint16_t blen = rd16(t->buf + 3);

            if (t->buf[1] != 0x03u || blen > REC_MAX ||
                (t->buf[0] != REC_APP && blen > PT_MAX)) {
                fail(t, AL_DECODE_ERROR);
                break;
            }
            if ((size_t)HEAD + blen > t->cap) {
                fail(t, AL_RECORD_OVERFLOW);
                break;
            }
        }
        if (t->have >= HEAD && t->have == HEAD + rd16(t->buf + 3)) {
            t->have = 0u;
            on_record(t);
        }
    }
    return used;
}

const uint8_t *
tiku_kits_crypto_tls_server_data(const tiku_kits_crypto_tls_server_t *t,
                                 size_t *n)
{
    *n = t->pt_len;
    return t->buf + t->pt_off;
}

void
tiku_kits_crypto_tls_server_take(tiku_kits_crypto_tls_server_t *t, size_t n)
{
    if (n > t->pt_len) {
        n = t->pt_len;
    }
    t->pt_off = (uint16_t)(t->pt_off + n);
    t->pt_len = (uint16_t)(t->pt_len - n);
}

uint16_t
tiku_kits_crypto_tls_server_room(const tiku_kits_crypto_tls_server_t *t)
{
    return t->out_max;
}

int
tiku_kits_crypto_tls_server_seal(tiku_kits_crypto_tls_server_t *t,
                                 uint8_t *rec, size_t ptlen)
{
    if ((t->state != S_FINISHED && t->state != S_OPEN &&
         t->state != S_CLOSED) || ptlen > t->out_max) {
        return -1;
    }
    return (int)seal_as(t, rec, ptlen, REC_APP);
}

void
tiku_kits_crypto_tls_server_close(tiku_kits_crypto_tls_server_t *t)
{
    if (t->tx_on && t->state != S_FAILED) {
        send_alert(t, 1u, AL_CLOSE_NOTIFY);
    }
}

void
tiku_kits_crypto_tls_server_wipe(tiku_kits_crypto_tls_server_t *t)
{
    wipe(t->secret, sizeof t->secret);
    wipe(t->c_tr, sizeof t->c_tr);
    wipe(t->s_tr, sizeof t->s_tr);
    wipe(&t->rx, sizeof t->rx);
    wipe(&t->tx, sizeof t->tx);
    wipe(t->rx_iv, sizeof t->rx_iv);
    wipe(t->tx_iv, sizeof t->tx_iv);
}
