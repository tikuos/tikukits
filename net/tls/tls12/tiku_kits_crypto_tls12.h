/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls12.h - minimal TLS 1.2 client (ECDHE + AES-128-GCM)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A second handshake for the TLS-1.2-only tail of the web that the TLS 1.3
 * client (tiku_kits_crypto_tls13) cannot reach.  Supports the two ECDHE +
 * AES-128-GCM-SHA256 suites -- ECDHE-RSA (0xC02F) and ECDHE-ECDSA (0xC02B) --
 * over the P-256 group, with full X.509 chain validation (shared with the 1.3
 * client) and ServerKeyExchange signature authentication.  Single connection,
 * no session resumption, static allocation.  Reuses the tls13 I/O + RNG
 * callback types so a caller can drive either client over the same transport.
 */

#ifndef TIKU_KITS_CRYPTO_TLS12_H_
#define TIKU_KITS_CRYPTO_TLS12_H_

#include <stdint.h>
#include <stddef.h>
#include "../tls13/tiku_kits_crypto_tls13.h"   /* io_t, rng_t (shared) */
#include "../x509/tiku_kits_crypto_x509.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIKU_KITS_CRYPTO_TLS12_OK    0
#define TIKU_KITS_CRYPTO_TLS12_BAD (-1)

/** An established TLS 1.2 connection (AES-128-GCM record state). */
typedef struct {
    tiku_kits_crypto_tls13_io_t io;
    uint8_t  c_key[32], c_iv[4];   /**< client write key (16/32) + fixed IV  */
    uint8_t  s_key[32], s_iv[4];   /**< server write key (16/32) + fixed IV  */
    uint8_t  is256;                /**< 1 = AES-256-GCM suite, 0 = AES-128    */
    uint64_t c_seq, s_seq;         /**< record sequence numbers               */
    size_t   rx_len, rx_off;       /**< decrypted app bytes not yet returned   */
    int      closed;
} tiku_kits_crypto_tls12_conn_t;

/**
 * @brief Perform a TLS 1.2 handshake and authenticate the server.
 *
 * @param io     Transport send/recv callbacks.
 * @param rng    Cryptographic RNG (ephemeral key + client random).
 * @param host   Server hostname (SNI + certificate name check).
 * @param store  Baked-in trust store (same type as the 1.3 client).
 * @param nstore Number of roots in @p store.
 * @param now_unix Current Unix time for validity checks (0 to skip).
 * @param conn   Filled with the established connection on success.
 * @return _OK on a completed, authenticated handshake, else _BAD.
 */
int tiku_kits_crypto_tls12_connect(const tiku_kits_crypto_tls13_io_t *io,
                                   tiku_kits_crypto_tls13_rng_t rng,
                                   const char *host,
                                   const tiku_kits_crypto_x509_root_t *store, int nstore,
                                   uint64_t now_unix,
                                   tiku_kits_crypto_tls12_conn_t *conn);

/** Send application data.  Returns bytes sent, or <0 on error. */
int tiku_kits_crypto_tls12_write(tiku_kits_crypto_tls12_conn_t *c,
                                 const uint8_t *buf, size_t len);

/** Read application data.  Returns bytes read, 0 on clean close, <0 on error. */
int tiku_kits_crypto_tls12_read(tiku_kits_crypto_tls12_conn_t *c,
                                uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_TLS12_H_ */
