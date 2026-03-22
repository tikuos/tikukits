/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls.h - TLS 1.3 PSK-only client
 *
 * Provides a minimal TLS 1.3 client using pre-shared keys (PSK)
 * with the TLS_AES_128_GCM_SHA256 cipher suite (RFC 8446).
 *
 * No certificates, no ECDH, no RSA --- only symmetric crypto.
 * Designed for ultra-low-power MCUs with 2 KB SRAM and FRAM.
 *
 * Architecture:
 *   - SRAM: connection context (~345 B during handshake, ~537 B
 *     during application data with both GCM contexts active)
 *   - FRAM (.persistent): transcript buffer (512 B), record
 *     RX/TX buffers (300 B each), PSK storage (64 B)
 *   - Builds on existing AES-128, SHA-256, HMAC, and TCP
 *
 * Usage:
 *   1. Call tiku_kits_crypto_tls_init() once at boot
 *   2. Set PSK with tiku_kits_crypto_tls_set_psk()
 *   3. Establish TCP connection, then call tls_connect()
 *   4. Send/receive application data via tls_send()/tls_read()
 *   5. Close with tls_close()
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

#ifndef TIKU_KITS_CRYPTO_TLS_H_
#define TIKU_KITS_CRYPTO_TLS_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_crypto.h"
#include "../sha256/tiku_kits_crypto_sha256.h"
#include "../gcm/tiku_kits_crypto_gcm.h"
#include "tiku_kits_crypto_tls_config.h"
#include <tikukits/net/ipv4/tiku_kits_net_tcp.h>

/*---------------------------------------------------------------------------*/
/* FORWARD DECLARATION                                                       */
/*---------------------------------------------------------------------------*/

struct tiku_kits_crypto_tls_conn;

/*---------------------------------------------------------------------------*/
/* CALLBACK TYPES                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Decrypted data arrival notification.
 *
 * Invoked when application data has been decrypted and is available
 * for reading via tiku_kits_crypto_tls_read().  Runs in the net
 * process context.
 *
 * @param conn       TLS connection
 * @param available  Bytes available to read
 */
typedef void (*tiku_kits_crypto_tls_recv_cb_t)(
    struct tiku_kits_crypto_tls_conn *conn,
    uint16_t available);

/**
 * @brief TLS connection event notification.
 *
 * Delivered on: handshake complete (CONNECTED), graceful close
 * (CLOSED), or fatal error (ERROR).
 *
 * @param conn   TLS connection
 * @param event  One of TIKU_KITS_CRYPTO_TLS_EVT_*
 */
typedef void (*tiku_kits_crypto_tls_event_cb_t)(
    struct tiku_kits_crypto_tls_conn *conn,
    uint8_t event);

/*---------------------------------------------------------------------------*/
/* CONNECTION CONTEXT                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_crypto_tls_conn
 * @brief TLS 1.3 connection state.
 *
 * Lives in SRAM for fast access to hot crypto state.  Large
 * buffers (transcript, record data) are in FRAM and accessed
 * via static module-level arrays.
 *
 * Memory: ~345 bytes during handshake (only rx_gcm active),
 * ~537 bytes during application data (both GCM contexts).
 */
typedef struct tiku_kits_crypto_tls_conn {
    /* --- Underlying TCP connection --- */
    tiku_kits_net_tcp_conn_t *tcp;

    /* --- Running transcript hash (SHA-256 context) --- */
    tiku_kits_crypto_sha256_ctx_t transcript;

    /* --- AEAD contexts for record protection --- */
    tiku_kits_crypto_gcm_ctx_t rx_gcm;      /**< Decrypt (server) */
    tiku_kits_crypto_gcm_ctx_t tx_gcm;      /**< Encrypt (client) */
    uint8_t rx_iv[TIKU_KITS_CRYPTO_TLS_IV_SIZE];   /**< RX nonce base */
    uint8_t tx_iv[TIKU_KITS_CRYPTO_TLS_IV_SIZE];   /**< TX nonce base */
    uint64_t rx_seq;                        /**< RX sequence number */
    uint64_t tx_seq;                        /**< TX sequence number */

    /* --- Handshake secrets (reused as scratch during handshake) --- */
    uint8_t hs_secret[TIKU_KITS_CRYPTO_TLS_HASH_SIZE];

    /* --- State machine --- */
    uint8_t state;                          /**< TLS_STATE_* */
    uint8_t flags;                          /**< Internal flags */
    uint8_t alert_desc;                     /**< Last alert received */

    /* --- Transcript buffer position --- */
    uint16_t transcript_len;                /**< Bytes in FRAM buf */

    /* --- Application callbacks --- */
    tiku_kits_crypto_tls_recv_cb_t  app_recv_cb;
    tiku_kits_crypto_tls_event_cb_t app_event_cb;
} tiku_kits_crypto_tls_conn_t;

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the TLS subsystem.
 *
 * Clears the connection context and FRAM buffers.  Call once
 * during boot, after crypto subsystems (AES, SHA, HMAC) are
 * available.
 */
void tiku_kits_crypto_tls_init(void);

/*---------------------------------------------------------------------------*/
/* PSK MANAGEMENT                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the pre-shared key and identity.
 *
 * The key and identity are copied into FRAM-backed storage.
 * Must be called before tls_connect().
 *
 * @param key       PSK key bytes
 * @param key_len   Key length (1 to MAX_PSK_LEN)
 * @param identity  PSK identity bytes
 * @param id_len    Identity length (1 to MAX_PSK_ID_LEN)
 * @return TIKU_KITS_CRYPTO_OK on success
 */
int tiku_kits_crypto_tls_set_psk(const uint8_t *key,
                                   uint16_t key_len,
                                   const uint8_t *identity,
                                   uint16_t id_len);

/*---------------------------------------------------------------------------*/
/* HANDSHAKE                                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Start TLS 1.3 handshake over an established TCP connection.
 *
 * Takes ownership of the TCP connection's recv/event callbacks.
 * The TLS layer intercepts TCP data, drives the handshake state
 * machine, and delivers decrypted application data to the
 * provided callbacks once the handshake completes.
 *
 * The TCP connection must already be in the ESTABLISHED state
 * (3-way handshake complete).
 *
 * @param tcp_conn   Established TCP connection
 * @param recv_cb    Application data callback
 * @param event_cb   TLS event callback (CONNECTED/CLOSED/ERROR)
 * @return Pointer to TLS connection, or NULL on failure
 */
tiku_kits_crypto_tls_conn_t *tiku_kits_crypto_tls_connect(
    tiku_kits_net_tcp_conn_t *tcp_conn,
    tiku_kits_crypto_tls_recv_cb_t recv_cb,
    tiku_kits_crypto_tls_event_cb_t event_cb);

/*---------------------------------------------------------------------------*/
/* DATA TRANSFER                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send encrypted application data.
 *
 * Encrypts the payload with AES-128-GCM and sends it as a TLS
 * application data record over the TCP connection.
 *
 * @param conn      TLS connection (must be ESTABLISHED)
 * @param data      Plaintext to send
 * @param data_len  Length (max TIKU_KITS_CRYPTO_TLS_MAX_FRAG_LEN)
 * @return TIKU_KITS_CRYPTO_OK on success
 */
int tiku_kits_crypto_tls_send(tiku_kits_crypto_tls_conn_t *conn,
                                const uint8_t *data,
                                uint16_t data_len);

/**
 * @brief Read decrypted application data.
 *
 * Copies up to buf_len bytes of decrypted data into the caller's
 * buffer.  Returns the number of bytes actually read (0 if no
 * data available).
 *
 * @param conn     TLS connection
 * @param buf      Destination buffer
 * @param buf_len  Maximum bytes to read
 * @return Number of bytes read
 */
uint16_t tiku_kits_crypto_tls_read(
    tiku_kits_crypto_tls_conn_t *conn,
    uint8_t *buf,
    uint16_t buf_len);

/*---------------------------------------------------------------------------*/
/* CLOSE                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initiate TLS close (send close_notify alert).
 *
 * Sends an encrypted close_notify alert and transitions to
 * CLOSING state.  The underlying TCP connection is not closed;
 * the application should close TCP separately after TLS close
 * completes.
 *
 * @param conn  TLS connection
 * @return TIKU_KITS_CRYPTO_OK on success
 */
int tiku_kits_crypto_tls_close(
    tiku_kits_crypto_tls_conn_t *conn);

#endif /* TIKU_KITS_CRYPTO_TLS_H_ */
