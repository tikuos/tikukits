/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls13.h - TLS 1.3 client (ECDHE + X.509 certificate)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A TLS 1.3 client that authenticates the server with a certificate chain
 * (X25519 key exchange, AES-128-GCM/SHA-256, certificate + CertificateVerify
 * verified against a baked-in trust store).  This is the certificate-based
 * counterpart to the PSK-only tiku_kits_crypto_tls client; it ties together
 * the x25519, hkdf, gcm, x509 and rsa/p256 verify kits.
 *
 * Transport- and entropy-agnostic: the caller supplies blocking send/recv
 * callbacks and a random-fill callback, so the same code runs on a host
 * socket (for testing) or the TikuOS TCP stack.
 */

#ifndef TIKU_KITS_CRYPTO_TLS13_H_
#define TIKU_KITS_CRYPTO_TLS13_H_

#include <stdint.h>
#include <stddef.h>
#include "../x509/tiku_kits_crypto_x509.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIKU_KITS_CRYPTO_TLS13_OK     0
#define TIKU_KITS_CRYPTO_TLS13_BAD  (-1)

/** Blocking transport: return bytes moved, or < 0 on error/EOF. */
typedef struct {
    int (*send)(void *ctx, const uint8_t *buf, size_t len);
    int (*recv)(void *ctx, uint8_t *buf, size_t len);
    void *ctx;
        /*
     * its CPU-bound public-key ops (ECDHE scalar-mult, CertVerify, the
     * certificate-chain verify) through this hook instead of calling them
     * inline: @p fn is a pure closure over caller buffers, @p closure its
     * argument, and the return value is @p fn's result.  A caller can thus
     * run the crypto on a worker thread while keeping its own timers, net
     * pump and services alive.  NULL (the default) runs every op inline,
     * byte-identical to a build without this field.  The callee MUST
     * serialise crypto — the underlying primitives carry non-reentrant
     * static scratch — so a single dedicated worker, never concurrent with
     * any other crypto.
     */

    /**
     * Optional heavy-crypto offload.  When non-NULL, the handshake runs
     */
    int (*offload)(int (*fn)(void *closure), void *closure);
} tiku_kits_crypto_tls13_io_t;

/** Fill @p buf with @p len cryptographically-random bytes. */
typedef void (*tiku_kits_crypto_tls13_rng_t)(uint8_t *buf, size_t len);

/** Optional handshake-milestone debug hook (NULL = silent). */
extern void (*tiku_kits_crypto_tls13_dbg)(const char *msg);

/** An established TLS 1.3 connection (application-data keys + sequence). */
typedef struct {
    tiku_kits_crypto_tls13_io_t io;
    uint8_t  c_key[16], c_iv[12];
    uint8_t  s_key[16], s_iv[12];
    uint64_t c_seq, s_seq;
    /* offsets into the shared decrypt buffer for app data not yet returned */
    size_t   rx_len, rx_off;
    int      closed;
} tiku_kits_crypto_tls13_conn_t;

/**
 * @brief Perform a full TLS 1.3 handshake and authenticate the server.
 *
 * @param io       Transport callbacks (connected TCP socket).
 * @param rng      Random-fill callback for the ephemeral key + ClientHello.
 * @param host     Server hostname: sent as SNI and matched against the leaf SAN.
 * @param store    Baked-in trusted-root store (DER + subject DN per entry).
 * @param nstore   Number of roots in the store.
 * @param now_unix Current time (Unix seconds) for validity checks (0 = skip).
 * @param conn     Output connection on success.
 * @return TIKU_KITS_CRYPTO_TLS13_OK, or _BAD on any handshake/auth failure.
 */
int tiku_kits_crypto_tls13_connect(const tiku_kits_crypto_tls13_io_t *io,
                                   tiku_kits_crypto_tls13_rng_t rng,
                                   const char *host,
                                   const tiku_kits_crypto_x509_root_t *store, int nstore,
                                   uint64_t now_unix,
                                   tiku_kits_crypto_tls13_conn_t *conn);

/* Diagnostics for the last connect attempt: the furthest handshake stage
 * reached (positive) or the failure point (negative), and the total handshake
 * record bytes read.  A caller reads these after a failed connect to tell a
 * transport failure from a real cert/logic failure.  Stage codes:
 *   1 ClientHello sent  2 ServerHello ok  3 reading flight  4 flight complete
 *   5 chain trusted    10 connected;  -2 ServerHello read  -3 ServerHello bad
 *  -5 flight read (transport)  -6 unexpected record  -7 decrypt (corrupt)
 *  -9 cert parse  -10 cert-verify  -11 chain untrusted  -12 Finished
 * -13 flight overflow  -14 client Finished send */
extern int      tiku_kits_crypto_tls13_last_stage;
extern uint32_t tiku_kits_crypto_tls13_last_rx;
/* Post-handshake read break reason (see the .c): 0 ok / 1 no-record /
 * 2 wire-type / 3 decrypt-fail / 4 alert, plus the wire type and read-seq. */
extern int      tiku_kits_crypto_tls13_last_read_fail;
extern uint8_t  tiku_kits_crypto_tls13_last_read_type;
extern uint32_t tiku_kits_crypto_tls13_last_read_seq;

/** Encrypt and send application data.  Returns bytes sent or < 0. */
int tiku_kits_crypto_tls13_write(tiku_kits_crypto_tls13_conn_t *c,
                                 const uint8_t *data, size_t len);

/** Receive and decrypt application data.  Returns bytes read, 0 at close, <0 error. */
int tiku_kits_crypto_tls13_read(tiku_kits_crypto_tls13_conn_t *c,
                                uint8_t *buf, size_t max);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_TLS13_H_ */
