/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_server.h - TLS 1.3 server, one state per connection
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * TLS 1.3 only: TLS_AES_128_GCM_SHA256, an X25519 or secp256r1 key share
 * (HelloRetryRequest when the client sent neither but supports one), the
 * server authenticated by a P-256 certificate (ecdsa_secp256r1_sha256).
 * No client certificates, tickets, resumption or early data.
 *
 * Nothing blocks and nothing is file-scope: the caller hands in bytes as
 * the network delivers them, reads plaintext in place, and seals records
 * in its own buffers; the handshake's records leave through the out
 * callback.  The curve arithmetic is not reentrant, so the connections of
 * one image are served from one thread.
 */

#ifndef TIKU_KITS_CRYPTO_TLS_SERVER_H_
#define TIKU_KITS_CRYPTO_TLS_SERVER_H_

#include <stdint.h>
#include <stddef.h>
#include "../../../crypto/sha256/tiku_kits_crypto_sha256.h"
#include "../../../crypto/gcm/tiku_kits_crypto_gcm.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Certificates sent, the leaf first. */
#define TIKU_KITS_CRYPTO_TLS_SERVER_CHAIN_MAX 2

/** Bytes a record adds to its plaintext: header, content type, tag. */
#define TIKU_KITS_CRYPTO_TLS_SERVER_OVERHEAD 22u

/** Where the plaintext goes in a buffer handed to _seal(). */
#define TIKU_KITS_CRYPTO_TLS_SERVER_HEAD 5u

/** The smallest record buffer: a ClientHello and the server's messages. */
#define TIKU_KITS_CRYPTO_TLS_SERVER_CAP_MIN 1024u

/** Connection states, in order. */
#define TIKU_KITS_CRYPTO_TLS_SERVER_HELLO     0  /**< awaiting ClientHello */
#define TIKU_KITS_CRYPTO_TLS_SERVER_HELLO2    1  /**< after a retry request */
#define TIKU_KITS_CRYPTO_TLS_SERVER_FINISHED  2  /**< awaiting client Finished */
#define TIKU_KITS_CRYPTO_TLS_SERVER_OPEN      3  /**< application data */
#define TIKU_KITS_CRYPTO_TLS_SERVER_CLOSED    4  /**< the client said close_notify */
#define TIKU_KITS_CRYPTO_TLS_SERVER_FAILED    5  /**< an alert went or came */

/** One certificate, DER. */
typedef struct {
    const uint8_t *der;
    uint16_t       len;
} tiku_kits_crypto_tls_server_cert_t;

/** What the server proves itself with. */
typedef struct {
    tiku_kits_crypto_tls_server_cert_t chain[TIKU_KITS_CRYPTO_TLS_SERVER_CHAIN_MAX];
    uint8_t        nchain;   /**< certificates in @c chain, 1 or more */
    const uint8_t *key;      /**< the leaf's P-256 private scalar, 32 bytes */
} tiku_kits_crypto_tls_server_cred_t;

/** Fills @p buf with @p len bytes from a cryptographic random source. */
typedef void (*tiku_kits_crypto_tls_server_rng_t)(uint8_t *buf, size_t len);

/** Sends @p n bytes to the client; returns 0, or -1 if they cannot go. */
typedef int (*tiku_kits_crypto_tls_server_out_t)(void *ctx,
                                                 const uint8_t *bytes,
                                                 size_t n);

/** One connection.  Fields are the server's; read @c state and @c alert. */
typedef struct {
    const tiku_kits_crypto_tls_server_cred_t *cred;
    tiku_kits_crypto_tls_server_rng_t rng;
    tiku_kits_crypto_tls_server_out_t out;
    void          *ctx;
    uint8_t       *buf;          /* the record being read, then its plaintext */
    uint16_t       cap;
    uint16_t       have;         /* bytes of the record read, header included */
    uint16_t       pt_off;       /* plaintext not yet taken, at buf + pt_off */
    uint16_t       pt_len;
    uint16_t       out_max;      /* plaintext bytes per record sent */
    uint16_t       peer_limit;   /* the client's record_size_limit, 0: none */
    uint8_t        state;
    uint8_t        alert;        /* the description of the alert sent or seen */
    uint8_t        alert_ours;   /* 1: the server sent it */
    uint8_t        group;        /* 1 X25519, 2 secp256r1, 0 none yet */
    uint8_t        tx_on;        /* records sent are protected */
    uint8_t        ccs_sent;
    uint8_t        sid_len;
    uint8_t        sid[32];      /* legacy_session_id, echoed */
    tiku_kits_crypto_sha256_ctx_t th;   /* the handshake transcript */
    uint8_t        secret[32];   /* the client's application secret, pending */
    uint8_t        c_tr[32];     /* the client's traffic secret in use */
    uint8_t        s_tr[32];     /* the server's traffic secret in use */
    tiku_kits_crypto_gcm_ctx_t rx, tx;
    uint8_t        rx_iv[12], tx_iv[12];
    uint64_t       rx_seq, tx_seq;
} tiku_kits_crypto_tls_server_t;

/**
 * @brief Make @p t ready for a connection's first byte.
 * @param buf  The connection's record buffer, at least
 *             TIKU_KITS_CRYPTO_TLS_SERVER_CAP_MIN bytes; the largest
 *             record a client may send is @p cap - 5 bytes, and the
 *             server's certificates and messages are built in it too.
 * @return 0, or -1 if @p cap is below the minimum or an argument is NULL
 */
int tiku_kits_crypto_tls_server_init(tiku_kits_crypto_tls_server_t *t,
                                     const tiku_kits_crypto_tls_server_cred_t *cred,
                                     tiku_kits_crypto_tls_server_rng_t rng,
                                     tiku_kits_crypto_tls_server_out_t out,
                                     void *ctx, uint8_t *buf, uint16_t cap);

/**
 * @brief Bytes from the client, taken a record at a time.
 *
 * Handshake records are answered through the out callback; application
 * data stops the intake until _take() has consumed it.  Afterwards
 * @c state says whether the connection is still open.
 *
 * @return how many of the @p n bytes were taken
 */
size_t tiku_kits_crypto_tls_server_feed(tiku_kits_crypto_tls_server_t *t,
                                        const uint8_t *in, size_t n);

/**
 * @brief The application data decrypted and not yet taken.
 * @param n  Output: its length, 0 for none
 * @return a pointer into the record buffer, valid until the next _feed()
 */
const uint8_t *tiku_kits_crypto_tls_server_data(const tiku_kits_crypto_tls_server_t *t,
                                                size_t *n);

/** @brief Mark @p n bytes of the application data as taken. */
void tiku_kits_crypto_tls_server_take(tiku_kits_crypto_tls_server_t *t,
                                      size_t n);

/**
 * @brief Encrypt @p ptlen bytes of application data into one record, in
 *        place: the plaintext at @p rec + TIKU_KITS_CRYPTO_TLS_SERVER_HEAD,
 *        @p rec with TIKU_KITS_CRYPTO_TLS_SERVER_OVERHEAD bytes to spare.
 * @return the record's length, for the caller to send, or -1 before the
 *         server's Finished, after a failure, or for more than
 *         tiku_kits_crypto_tls_server_room() bytes
 */
int tiku_kits_crypto_tls_server_seal(tiku_kits_crypto_tls_server_t *t,
                                     uint8_t *rec, size_t ptlen);

/** @brief The most plaintext one sealed record may carry. */
uint16_t tiku_kits_crypto_tls_server_room(const tiku_kits_crypto_tls_server_t *t);

/**
 * @brief Send close_notify through the out callback, once the keys are
 *        in place; the caller then closes the transport.
 */
void tiku_kits_crypto_tls_server_close(tiku_kits_crypto_tls_server_t *t);

/** @brief Clear the connection's keys and secrets. */
void tiku_kits_crypto_tls_server_wipe(tiku_kits_crypto_tls_server_t *t);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_TLS_SERVER_H_ */
