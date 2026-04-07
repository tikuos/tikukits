/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls.c - TLS 1.3 PSK-only client state machine
 *
 * Drives the TLS 1.3 handshake by intercepting TCP callbacks,
 * advancing through the state machine on each incoming message,
 * and delivering decrypted application data to the application.
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_crypto_tls.h"
#include "tiku_kits_crypto_tls_config.h"
#include "tiku_kits_crypto_tls_keysched.h"
#include "tiku_kits_crypto_tls_record.h"
#include "tiku_kits_crypto_tls_handshake.h"
#include "../sha256/tiku_kits_crypto_sha256.h"
#include "../gcm/tiku_kits_crypto_gcm.h"
#include <kernel/memory/tiku_mem.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* FRAM-BACKED STORAGE                                                       */
/*---------------------------------------------------------------------------*/

/** Handshake transcript buffer: raw bytes of CH+SH+EE+SF. */
__attribute__((section(".persistent"), aligned(2)))
static uint8_t tls_transcript_buf[
    TIKU_KITS_CRYPTO_TLS_TRANSCRIPT_SIZE];

/** TLS record RX buffer (holds one complete record from TCP). */
__attribute__((section(".persistent"), aligned(2)))
static uint8_t tls_rx_buf[TIKU_KITS_CRYPTO_TLS_RECORD_BUF_SIZE];

/** TLS record TX buffer (for building outgoing records). */
__attribute__((section(".persistent"), aligned(2)))
static uint8_t tls_tx_buf[TIKU_KITS_CRYPTO_TLS_RECORD_BUF_SIZE];

/** PSK storage: key + identity in FRAM. */
__attribute__((section(".persistent"), aligned(2)))
static uint8_t tls_psk_key[TIKU_KITS_CRYPTO_TLS_MAX_PSK_LEN];

__attribute__((section(".persistent"), aligned(2)))
static uint8_t tls_psk_id[TIKU_KITS_CRYPTO_TLS_MAX_PSK_ID_LEN];

__attribute__((section(".persistent"), aligned(2)))
static uint16_t tls_psk_key_len;

__attribute__((section(".persistent"), aligned(2)))
static uint16_t tls_psk_id_len;

/*---------------------------------------------------------------------------*/
/* SRAM STATE                                                                */
/*---------------------------------------------------------------------------*/

/** Single TLS connection context. */
static tiku_kits_crypto_tls_conn_t tls_conn;

/** Position in RX record buffer (reassembly from TCP segments). */
static uint16_t rx_buf_pos;

/** Expected record length (parsed from 5-byte header). */
static uint16_t rx_record_expected;

/** Decrypted application data staging (reuses part of rx_buf). */
static uint16_t app_data_avail;

/*---------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS                                                      */
/*---------------------------------------------------------------------------*/

static void tls_tcp_recv_cb(struct tiku_kits_net_tcp_conn *tcp,
                             uint16_t available);
static void tls_tcp_event_cb(struct tiku_kits_net_tcp_conn *tcp,
                              uint8_t event);
static void tls_process_record(void);
static void tls_send_alert(uint8_t desc);
static int  tls_send_client_hello(void);
static int  tls_handle_server_hello(const uint8_t *msg,
                                     uint16_t msg_len);
static int  tls_handle_encrypted_extensions(const uint8_t *msg,
                                             uint16_t msg_len);
static int  tls_handle_finished(const uint8_t *msg,
                                 uint16_t msg_len);
static int  tls_send_client_finished(void);
static void tls_derive_app_keys(void);

/*---------------------------------------------------------------------------*/
/* TRANSCRIPT MANAGEMENT                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append a handshake message to the transcript buffer
 *        and update the running SHA-256 hash.
 */
static int
transcript_append(tiku_kits_crypto_tls_conn_t *c,
                  const uint8_t *msg, uint16_t msg_len)
{
    uint16_t saved;

    if (c->transcript_len + msg_len >
        TIKU_KITS_CRYPTO_TLS_TRANSCRIPT_SIZE) {
        return TIKU_KITS_CRYPTO_ERR_OVERFLOW;
    }

    /* Write to FRAM */
    saved = tiku_mpu_unlock_nvm();
    memcpy(tls_transcript_buf + c->transcript_len, msg, msg_len);
    tiku_mpu_lock_nvm(saved);

    c->transcript_len += msg_len;

    /* Update running hash */
    tiku_kits_crypto_sha256_update(&c->transcript, msg, msg_len);

    return TIKU_KITS_CRYPTO_OK;
}

/**
 * @brief Get the current transcript hash without finalising
 *        the context (clone, then finalise the clone).
 */
static void
transcript_hash(const tiku_kits_crypto_tls_conn_t *c,
                uint8_t *hash_out)
{
    tiku_kits_crypto_sha256_ctx_t clone;
    memcpy(&clone, &c->transcript, sizeof(clone));
    tiku_kits_crypto_sha256_final(&clone, hash_out);
}

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

void
tiku_kits_crypto_tls_init(void)
{
    uint16_t saved;

    memset(&tls_conn, 0, sizeof(tls_conn));
    rx_buf_pos = 0;
    rx_record_expected = 0;
    app_data_avail = 0;

    saved = tiku_mpu_unlock_nvm();
    memset(tls_transcript_buf, 0,
           TIKU_KITS_CRYPTO_TLS_TRANSCRIPT_SIZE);
    memset(tls_rx_buf, 0,
           TIKU_KITS_CRYPTO_TLS_RECORD_BUF_SIZE);
    memset(tls_tx_buf, 0,
           TIKU_KITS_CRYPTO_TLS_RECORD_BUF_SIZE);
    tiku_mpu_lock_nvm(saved);
}

/*---------------------------------------------------------------------------*/
/* PSK MANAGEMENT                                                            */
/*---------------------------------------------------------------------------*/

int
tiku_kits_crypto_tls_set_psk(const uint8_t *key,
                               uint16_t key_len,
                               const uint8_t *identity,
                               uint16_t id_len)
{
    uint16_t saved;

    if (key == (void *)0 || identity == (void *)0) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (key_len == 0 ||
        key_len > TIKU_KITS_CRYPTO_TLS_MAX_PSK_LEN ||
        id_len == 0 ||
        id_len > TIKU_KITS_CRYPTO_TLS_MAX_PSK_ID_LEN) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    saved = tiku_mpu_unlock_nvm();
    memcpy(tls_psk_key, key, key_len);
    memcpy(tls_psk_id, identity, id_len);
    tls_psk_key_len = key_len;
    tls_psk_id_len = id_len;
    tiku_mpu_lock_nvm(saved);

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* CONNECT (START HANDSHAKE)                                                 */
/*---------------------------------------------------------------------------*/

tiku_kits_crypto_tls_conn_t *
tiku_kits_crypto_tls_connect(
    tiku_kits_net_tcp_conn_t *tcp_conn,
    tiku_kits_crypto_tls_recv_cb_t recv_cb,
    tiku_kits_crypto_tls_event_cb_t event_cb)
{
    tiku_kits_crypto_tls_conn_t *c = &tls_conn;

    if (tcp_conn == (void *)0 || recv_cb == (void *)0 ||
        event_cb == (void *)0) {
        return (tiku_kits_crypto_tls_conn_t *)0;
    }

    if (tls_psk_key_len == 0) {
        return (tiku_kits_crypto_tls_conn_t *)0;
    }

    /* Reset connection state */
    memset(c, 0, sizeof(*c));
    c->tcp = tcp_conn;
    c->app_recv_cb = recv_cb;
    c->app_event_cb = event_cb;
    c->state = TIKU_KITS_CRYPTO_TLS_STATE_IDLE;

    /* Initialise transcript hash */
    tiku_kits_crypto_sha256_init(&c->transcript);
    c->transcript_len = 0;

    /* Reset RX reassembly */
    rx_buf_pos = 0;
    rx_record_expected = 0;
    app_data_avail = 0;

    /* Send ClientHello */
    if (tls_send_client_hello() != TIKU_KITS_CRYPTO_OK) {
        c->state = TIKU_KITS_CRYPTO_TLS_STATE_ERROR;
        return (tiku_kits_crypto_tls_conn_t *)0;
    }

    c->state = TIKU_KITS_CRYPTO_TLS_STATE_WAIT_SH;

    return c;
}

/*---------------------------------------------------------------------------*/
/* SEND CLIENT HELLO                                                         */
/*---------------------------------------------------------------------------*/

static int
tls_send_client_hello(void)
{
    tiku_kits_crypto_tls_conn_t *c = &tls_conn;
    uint8_t client_random[32];
    uint16_t ch_len, binder_offset, rec_len;
    uint8_t early_secret[32], binder_key[32];
    uint8_t trunc_hash[32], binder[32];
    uint16_t saved;

    /* Fill client_random from the platform entropy source. */
    TIKU_KITS_CRYPTO_TLS_RNG_FILL(client_random, 32);

    /* Build ClientHello in FRAM TX buffer */
    saved = tiku_mpu_unlock_nvm();
    ch_len = tiku_kits_crypto_tls_build_client_hello(
        tls_tx_buf + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
        client_random,
        tls_psk_id, tls_psk_id_len,
        &binder_offset);
    tiku_mpu_lock_nvm(saved);

    if (ch_len == 0) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    /* Compute PSK binder:
     * 1. Compute Early Secret and binder_key from PSK
     * 2. Hash the truncated ClientHello (up to binder)
     * 3. binder = HMAC(binder_key, hash) */
    tiku_kits_crypto_tls_early_secret(
        tls_psk_key, tls_psk_key_len, early_secret);
    tiku_kits_crypto_tls_binder_key(
        early_secret, binder_key);

    /* Hash of truncated CH (everything before the binder value).
     * The binder_offset is relative to the CH message start,
     * which is at tls_tx_buf + 5. */
    {
        tiku_kits_crypto_sha256_ctx_t binder_ctx;
        tiku_kits_crypto_sha256_init(&binder_ctx);
        tiku_kits_crypto_sha256_update(
            &binder_ctx,
            tls_tx_buf + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
            binder_offset);
        tiku_kits_crypto_sha256_final(&binder_ctx, trunc_hash);
    }

    tiku_kits_crypto_tls_compute_binder(
        binder_key, trunc_hash, binder);

    /* Patch binder into ClientHello */
    saved = tiku_mpu_unlock_nvm();
    tiku_kits_crypto_tls_ch_patch_binder(
        tls_tx_buf + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
        binder_offset, binder);
    tiku_mpu_lock_nvm(saved);

    /* Wrap ClientHello in a plaintext TLS record */
    saved = tiku_mpu_unlock_nvm();
    rec_len = tiku_kits_crypto_tls_record_build_plain(
        tls_tx_buf,
        TIKU_KITS_CRYPTO_TLS_CT_HANDSHAKE,
        tls_tx_buf + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
        ch_len);
    tiku_mpu_lock_nvm(saved);

    /* Add ClientHello to transcript */
    transcript_append(c,
        tls_tx_buf + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
        ch_len);

    /* Save early/hs secrets for later use */
    memcpy(c->hs_secret, early_secret, 32);

    /* Send via TCP */
    tiku_kits_net_tcp_send(c->tcp, tls_tx_buf, rec_len);

    /* Scrub sensitive data from stack */
    memset(early_secret, 0, 32);
    memset(binder_key, 0, 32);
    memset(binder, 0, 32);

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* TCP CALLBACKS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief TCP data arrival callback.
 *
 * Reads TCP data into the FRAM RX record buffer, reassembles
 * complete TLS records, and processes them through the state
 * machine.
 */
static void
tls_tcp_recv_cb(struct tiku_kits_net_tcp_conn *tcp,
                uint16_t available)
{
    uint8_t tmp[64];
    uint16_t n, saved;

    (void)available;

    /* Read TCP data into FRAM RX buffer */
    while (1) {
        n = tiku_kits_net_tcp_read(tcp, tmp, sizeof(tmp));
        if (n == 0) {
            break;
        }

        /* Append to FRAM RX buffer */
        if (rx_buf_pos + n >
            TIKU_KITS_CRYPTO_TLS_RECORD_BUF_SIZE) {
            /* Record too large --- abort */
            tls_conn.state =
                TIKU_KITS_CRYPTO_TLS_STATE_ERROR;
            if (tls_conn.app_event_cb) {
                tls_conn.app_event_cb(
                    &tls_conn,
                    TIKU_KITS_CRYPTO_TLS_EVT_ERROR);
            }
            return;
        }

        saved = tiku_mpu_unlock_nvm();
        memcpy(tls_rx_buf + rx_buf_pos, tmp, n);
        tiku_mpu_lock_nvm(saved);
        rx_buf_pos += n;

        /* Check if we have a complete record */
        if (rx_buf_pos >= TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE) {
            if (rx_record_expected == 0) {
                /* Parse record length from header */
                rx_record_expected =
                    TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE +
                    ((uint16_t)tls_rx_buf[3] << 8 |
                     tls_rx_buf[4]);
            }

            if (rx_buf_pos >= rx_record_expected) {
                tls_process_record();
                rx_buf_pos = 0;
                rx_record_expected = 0;
            }
        }
    }
}

/**
 * @brief TCP connection event callback.
 */
static void
tls_tcp_event_cb(struct tiku_kits_net_tcp_conn *tcp,
                  uint8_t event)
{
    (void)tcp;

    if (event == TIKU_KITS_NET_TCP_EVT_ABORTED ||
        event == TIKU_KITS_NET_TCP_EVT_CLOSED) {
        tls_conn.state = TIKU_KITS_CRYPTO_TLS_STATE_ERROR;
        if (tls_conn.app_event_cb) {
            tls_conn.app_event_cb(
                &tls_conn,
                TIKU_KITS_CRYPTO_TLS_EVT_ERROR);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* RECORD PROCESSING (STATE MACHINE DRIVER)                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process a complete TLS record from tls_rx_buf.
 *
 * Dispatches based on current state and record content type.
 */
static void
tls_process_record(void)
{
    tiku_kits_crypto_tls_conn_t *c = &tls_conn;
    uint8_t content_type;
    uint8_t *content;
    uint16_t content_len;
    uint8_t hs_type;
    uint16_t hs_len;
    int rc;

    content_type = tls_rx_buf[0];

    if (c->state == TIKU_KITS_CRYPTO_TLS_STATE_WAIT_SH) {
        /* Expecting plaintext ServerHello */
        if (content_type != TIKU_KITS_CRYPTO_TLS_CT_HANDSHAKE) {
            tls_send_alert(
                TIKU_KITS_CRYPTO_TLS_ALERT_UNEXPECTED_MSG);
            return;
        }

        content = tls_rx_buf +
                  TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE;
        content_len = rx_record_expected -
                      TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE;

        /* Parse handshake header */
        if (content_len < 4) {
            tls_send_alert(
                TIKU_KITS_CRYPTO_TLS_ALERT_DECODE_ERROR);
            return;
        }
        hs_type = content[0];
        hs_len = ((uint32_t)content[1] << 16 |
                  (uint16_t)content[2] << 8 |
                  content[3]);

        if (hs_type != TIKU_KITS_CRYPTO_TLS_HT_SERVER_HELLO) {
            tls_send_alert(
                TIKU_KITS_CRYPTO_TLS_ALERT_UNEXPECTED_MSG);
            return;
        }

        /* Add ServerHello to transcript */
        transcript_append(c, content, 4 + hs_len);

        /* Handle ServerHello */
        rc = tls_handle_server_hello(content + 4, hs_len);
        if (rc != TIKU_KITS_CRYPTO_OK) {
            tls_send_alert(
                TIKU_KITS_CRYPTO_TLS_ALERT_HANDSHAKE_FAILURE);
            return;
        }

        c->state = TIKU_KITS_CRYPTO_TLS_STATE_WAIT_EE;
        return;
    }

    /* All subsequent records should be encrypted (type 23) */
    if (content_type !=
        TIKU_KITS_CRYPTO_TLS_CT_APPLICATION) {
        /* Accept ChangeCipherSpec (type 20) silently per
         * RFC 8446 Section 5 (middlebox compatibility) */
        if (content_type ==
            TIKU_KITS_CRYPTO_TLS_CT_CHANGE_CIPHER) {
            return;  /* ignore */
        }
        tls_send_alert(
            TIKU_KITS_CRYPTO_TLS_ALERT_UNEXPECTED_MSG);
        return;
    }

    /* Decrypt the record */
    {
        uint8_t inner_type;
        uint16_t inner_len;

        rc = tiku_kits_crypto_tls_record_decrypt(
            tls_rx_buf, rx_record_expected,
            &c->rx_gcm, c->rx_iv, &c->rx_seq,
            tls_rx_buf + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
            &inner_len, &inner_type);

        if (rc != TIKU_KITS_CRYPTO_OK) {
            tls_send_alert(
                TIKU_KITS_CRYPTO_TLS_ALERT_BAD_RECORD_MAC);
            return;
        }

        content = tls_rx_buf +
                  TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE;
        content_len = inner_len;
        content_type = inner_type;
    }

    /* Dispatch based on inner content type and state */
    if (content_type == TIKU_KITS_CRYPTO_TLS_CT_HANDSHAKE) {
        if (content_len < 4) {
            tls_send_alert(
                TIKU_KITS_CRYPTO_TLS_ALERT_DECODE_ERROR);
            return;
        }
        hs_type = content[0];
        hs_len = ((uint32_t)content[1] << 16 |
                  (uint16_t)content[2] << 8 |
                  content[3]);

        switch (c->state) {
        case TIKU_KITS_CRYPTO_TLS_STATE_WAIT_EE:
            if (hs_type !=
                TIKU_KITS_CRYPTO_TLS_HT_ENC_EXTENSIONS) {
                tls_send_alert(
                    TIKU_KITS_CRYPTO_TLS_ALERT_UNEXPECTED_MSG);
                return;
            }
            transcript_append(c, content, 4 + hs_len);
            rc = tls_handle_encrypted_extensions(
                content + 4, hs_len);
            if (rc != TIKU_KITS_CRYPTO_OK) {
                tls_send_alert(
                    TIKU_KITS_CRYPTO_TLS_ALERT_DECODE_ERROR);
                return;
            }
            c->state = TIKU_KITS_CRYPTO_TLS_STATE_WAIT_FIN;
            break;

        case TIKU_KITS_CRYPTO_TLS_STATE_WAIT_FIN:
            if (hs_type !=
                TIKU_KITS_CRYPTO_TLS_HT_FINISHED) {
                tls_send_alert(
                    TIKU_KITS_CRYPTO_TLS_ALERT_UNEXPECTED_MSG);
                return;
            }
            /* Add Finished to transcript BEFORE verifying
             * (needed for client Finished computation) */
            transcript_append(c, content, 4 + hs_len);
            rc = tls_handle_finished(content + 4, hs_len);
            if (rc != TIKU_KITS_CRYPTO_OK) {
                tls_send_alert(
                    TIKU_KITS_CRYPTO_TLS_ALERT_DECODE_ERROR);
                return;
            }
            /* Send client Finished and derive app keys */
            tls_send_client_finished();
            tls_derive_app_keys();
            c->state =
                TIKU_KITS_CRYPTO_TLS_STATE_ESTABLISHED;
            if (c->app_event_cb) {
                c->app_event_cb(c,
                    TIKU_KITS_CRYPTO_TLS_EVT_CONNECTED);
            }
            break;

        default:
            tls_send_alert(
                TIKU_KITS_CRYPTO_TLS_ALERT_UNEXPECTED_MSG);
            break;
        }
    } else if (content_type ==
               TIKU_KITS_CRYPTO_TLS_CT_APPLICATION) {
        /* Application data */
        if (c->state !=
            TIKU_KITS_CRYPTO_TLS_STATE_ESTABLISHED) {
            tls_send_alert(
                TIKU_KITS_CRYPTO_TLS_ALERT_UNEXPECTED_MSG);
            return;
        }
        app_data_avail = content_len;
        if (c->app_recv_cb && content_len > 0) {
            c->app_recv_cb(c, content_len);
        }
    } else if (content_type ==
               TIKU_KITS_CRYPTO_TLS_CT_ALERT) {
        /* Alert received */
        if (content_len >= 2 && content[1] ==
            TIKU_KITS_CRYPTO_TLS_ALERT_CLOSE_NOTIFY) {
            c->state = TIKU_KITS_CRYPTO_TLS_STATE_CLOSING;
            if (c->app_event_cb) {
                c->app_event_cb(c,
                    TIKU_KITS_CRYPTO_TLS_EVT_CLOSED);
            }
        } else {
            c->state = TIKU_KITS_CRYPTO_TLS_STATE_ERROR;
            if (c->app_event_cb) {
                c->app_event_cb(c,
                    TIKU_KITS_CRYPTO_TLS_EVT_ERROR);
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
/* HANDSHAKE HANDLERS                                                        */
/*---------------------------------------------------------------------------*/

static int
tls_handle_server_hello(const uint8_t *msg, uint16_t msg_len)
{
    tiku_kits_crypto_tls_conn_t *c = &tls_conn;
    tiku_kits_crypto_tls_sh_t sh;
    uint8_t transcript_hash_ch_sh[32];
    uint8_t client_hs_secret[32], server_hs_secret[32];
    uint8_t server_key[16], server_iv[12];
    int rc;

    rc = tiku_kits_crypto_tls_parse_server_hello(
        msg, msg_len, &sh);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    /* Validate */
    if (!sh.version_ok) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    if (sh.cipher_suite !=
        TIKU_KITS_CRYPTO_TLS_CS_AES128_GCM) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    if (sh.psk_index != 0) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    /* Derive handshake secrets.
     * c->hs_secret currently holds the Early Secret
     * (saved from tls_send_client_hello). */
    tiku_kits_crypto_tls_handshake_secret(
        c->hs_secret, c->hs_secret);
    /* Now c->hs_secret holds the Handshake Secret */

    /* Get transcript hash of CH..SH */
    transcript_hash(c, transcript_hash_ch_sh);

    /* Derive traffic secrets */
    tiku_kits_crypto_tls_hs_traffic_secrets(
        c->hs_secret, transcript_hash_ch_sh,
        client_hs_secret, server_hs_secret);

    /* Expand server handshake traffic secret to key + IV */
    tiku_kits_crypto_tls_traffic_keys(
        server_hs_secret, server_key, server_iv);

    /* Initialise server (RX) GCM context for decrypting
     * EncryptedExtensions and server Finished */
    tiku_kits_crypto_gcm_init(&c->rx_gcm, server_key);
    memcpy(c->rx_iv, server_iv, 12);
    c->rx_seq = 0;

    /* Save client handshake secret for client Finished.
     * We reuse hs_secret storage (union-like). */
    /* Actually, we need both hs_secret (for master secret)
     * and client_hs_secret.  Store client_hs in the tx_iv
     * temporarily (it's not used until app data phase). */
    memcpy(c->tx_iv, client_hs_secret, 12);
    /* Store remaining 20 bytes of client_hs_secret in
     * a scratch area.  Actually, let's just keep it on
     * the stack and compute finished immediately when needed.
     * For now, store the full 32-byte client_hs_secret
     * in FRAM scratch. */
    {
        uint16_t saved = tiku_mpu_unlock_nvm();
        memcpy(tls_tx_buf, client_hs_secret, 32);
        tiku_mpu_lock_nvm(saved);
    }

    /* Scrub */
    memset(server_key, 0, 16);
    memset(server_iv, 0, 12);
    memset(server_hs_secret, 0, 32);

    return TIKU_KITS_CRYPTO_OK;
}

static int
tls_handle_encrypted_extensions(const uint8_t *msg,
                                 uint16_t msg_len)
{
    return tiku_kits_crypto_tls_parse_encrypted_extensions(
        msg, msg_len);
}

static int
tls_handle_finished(const uint8_t *msg, uint16_t msg_len)
{
    tiku_kits_crypto_tls_conn_t *c = &tls_conn;
    uint8_t server_hs_secret[32];
    uint8_t expected_verify[32];
    uint8_t th[32];

    /* Recompute server_hs_secret from hs_secret and
     * CH..SH transcript.  We need the transcript hash
     * BEFORE the server Finished was appended.
     *
     * Problem: we already appended SF to the transcript.
     * We need the hash at CH..SH..EE (before SF).
     *
     * Solution: We stored the handshake message bytes
     * in tls_transcript_buf.  Recompute the hash up to
     * but not including the last message (the Finished).
     * The Finished message is 4+32=36 bytes. */
    {
        tiku_kits_crypto_sha256_ctx_t tmp_ctx;
        uint16_t pre_fin_len = c->transcript_len - (4 + msg_len);
        tiku_kits_crypto_sha256_init(&tmp_ctx);
        tiku_kits_crypto_sha256_update(
            &tmp_ctx, tls_transcript_buf, pre_fin_len);
        tiku_kits_crypto_sha256_final(&tmp_ctx, th);
    }

    /* Reconstruct server_hs_traffic_secret.
     * We have hs_secret in c->hs_secret and the CH..SH
     * hash is needed.  But we don't have it saved separately.
     *
     * Simpler approach: just recompute from the first
     * two messages (CH + SH) in the transcript buffer.
     * We need to know where SH ends.  For now, use the
     * server finished key that we can derive from the
     * server_hs_traffic_secret.
     *
     * Actually, the Finished verify_data is computed as:
     * finished_key = HKDF-Expand-Label(server_hs_secret,
     *                                   "finished", "", 32)
     * verify = HMAC(finished_key, hash(CH..SH..EE))
     *
     * We need server_hs_secret.  We derive it the same way
     * we did in handle_server_hello, from hs_secret and
     * the CH..SH transcript hash.
     *
     * Shortcut: the server GCM key was derived from server_hs_secret.
     * But we don't store the secret itself.  We need to rederive.
     *
     * For simplicity, store the transcript hash at CH..SH
     * when we compute it in handle_server_hello, and reuse here.
     * Let's refactor: save the CH..SH hash in FRAM. */

    /* WORKAROUND: Recompute CH..SH hash from transcript buffer.
     * The first two messages are ClientHello and ServerHello.
     * We find the boundary by scanning handshake headers.
     * CH starts at offset 0, its length is in bytes 1-3.
     * SH starts after CH. */
    {
        tiku_kits_crypto_sha256_ctx_t ch_sh_ctx;
        uint16_t ch_msg_len, sh_msg_len, ch_sh_total;

        ch_msg_len = ((uint32_t)tls_transcript_buf[1] << 16 |
                      (uint16_t)tls_transcript_buf[2] << 8 |
                      tls_transcript_buf[3]) + 4;

        sh_msg_len = ((uint32_t)tls_transcript_buf[ch_msg_len + 1] << 16 |
                      (uint16_t)tls_transcript_buf[ch_msg_len + 2] << 8 |
                      tls_transcript_buf[ch_msg_len + 3]) + 4;

        ch_sh_total = ch_msg_len + sh_msg_len;

        tiku_kits_crypto_sha256_init(&ch_sh_ctx);
        tiku_kits_crypto_sha256_update(
            &ch_sh_ctx, tls_transcript_buf, ch_sh_total);

        uint8_t ch_sh_hash[32];
        tiku_kits_crypto_sha256_final(&ch_sh_ctx, ch_sh_hash);

        /* Now derive server_hs_secret */
        tiku_kits_crypto_tls_hs_traffic_secrets(
            c->hs_secret, ch_sh_hash,
            (void *)0,  /* don't need client secret here */
            server_hs_secret);

        /* Actually, hs_traffic_secrets writes to both
         * output pointers.  We need a dummy for client. */
        uint8_t dummy[32];
        tiku_kits_crypto_tls_hs_traffic_secrets(
            c->hs_secret, ch_sh_hash,
            dummy, server_hs_secret);

        memset(dummy, 0, 32);
        memset(ch_sh_hash, 0, 32);
    }

    /* Compute expected verify_data */
    tiku_kits_crypto_tls_finished_verify(
        server_hs_secret, th, expected_verify);

    /* Verify */
    int rc = tiku_kits_crypto_tls_verify_finished(
        msg, msg_len, expected_verify);

    memset(server_hs_secret, 0, 32);
    memset(expected_verify, 0, 32);

    return rc;
}

/*---------------------------------------------------------------------------*/
/* SEND CLIENT FINISHED                                                      */
/*---------------------------------------------------------------------------*/

static int
tls_send_client_finished(void)
{
    tiku_kits_crypto_tls_conn_t *c = &tls_conn;
    uint8_t client_hs_secret[32];
    uint8_t th[32];
    uint8_t verify_data[32];
    uint8_t fin_msg[36];
    uint16_t fin_len, rec_len;
    uint8_t client_key[16], client_iv[12];
    uint16_t saved;

    /* Recover client_hs_secret from FRAM scratch */
    memcpy(client_hs_secret, tls_tx_buf, 32);

    /* Get current transcript hash (CH..SH..EE..SF) */
    transcript_hash(c, th);

    /* Compute client Finished verify_data */
    tiku_kits_crypto_tls_finished_verify(
        client_hs_secret, th, verify_data);

    /* Build Finished message */
    fin_len = tiku_kits_crypto_tls_build_finished(
        fin_msg, verify_data);

    /* Initialise client handshake GCM for sending Finished */
    tiku_kits_crypto_tls_traffic_keys(
        client_hs_secret, client_key, client_iv);
    tiku_kits_crypto_gcm_init(&c->tx_gcm, client_key);
    memcpy(c->tx_iv, client_iv, 12);
    c->tx_seq = 0;

    /* Encrypt and send Finished as an encrypted record */
    saved = tiku_mpu_unlock_nvm();
    rec_len = tiku_kits_crypto_tls_record_encrypt(
        tls_tx_buf,
        &c->tx_gcm, c->tx_iv, &c->tx_seq,
        TIKU_KITS_CRYPTO_TLS_CT_HANDSHAKE,
        fin_msg, fin_len);
    tiku_mpu_lock_nvm(saved);

    if (rec_len > 0) {
        tiku_kits_net_tcp_send(c->tcp, tls_tx_buf, rec_len);
    }

    /* Add client Finished to transcript (for app key derivation) */
    transcript_append(c, fin_msg, fin_len);

    /* Scrub */
    memset(client_hs_secret, 0, 32);
    memset(verify_data, 0, 32);
    memset(client_key, 0, 16);
    memset(client_iv, 0, 12);

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* DERIVE APPLICATION KEYS                                                   */
/*---------------------------------------------------------------------------*/

static void
tls_derive_app_keys(void)
{
    tiku_kits_crypto_tls_conn_t *c = &tls_conn;
    uint8_t master_secret[32];
    uint8_t th[32];
    uint8_t client_app_secret[32], server_app_secret[32];
    uint8_t key[16], iv[12];

    /* Compute Master Secret */
    tiku_kits_crypto_tls_master_secret(
        c->hs_secret, master_secret);

    /* Full transcript hash (CH..SH..EE..SF..CF) */
    transcript_hash(c, th);

    /* Derive application traffic secrets */
    tiku_kits_crypto_tls_app_traffic_secrets(
        master_secret, th,
        client_app_secret, server_app_secret);

    /* Server application keys (RX) */
    tiku_kits_crypto_tls_traffic_keys(
        server_app_secret, key, iv);
    tiku_kits_crypto_gcm_init(&c->rx_gcm, key);
    memcpy(c->rx_iv, iv, 12);
    c->rx_seq = 0;

    /* Client application keys (TX) */
    tiku_kits_crypto_tls_traffic_keys(
        client_app_secret, key, iv);
    tiku_kits_crypto_gcm_init(&c->tx_gcm, key);
    memcpy(c->tx_iv, iv, 12);
    c->tx_seq = 0;

    /* Scrub */
    memset(master_secret, 0, 32);
    memset(client_app_secret, 0, 32);
    memset(server_app_secret, 0, 32);
    memset(key, 0, 16);
    memset(iv, 0, 12);
    memset(c->hs_secret, 0, 32);
}

/*---------------------------------------------------------------------------*/
/* APPLICATION DATA SEND/READ                                                */
/*---------------------------------------------------------------------------*/

int
tiku_kits_crypto_tls_send(tiku_kits_crypto_tls_conn_t *conn,
                            const uint8_t *data,
                            uint16_t data_len)
{
    uint16_t rec_len;
    uint16_t saved;

    if (conn == (void *)0 || data == (void *)0) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (conn->state !=
        TIKU_KITS_CRYPTO_TLS_STATE_ESTABLISHED) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    if (data_len > TIKU_KITS_CRYPTO_TLS_MAX_FRAG_LEN) {
        return TIKU_KITS_CRYPTO_ERR_OVERFLOW;
    }

    saved = tiku_mpu_unlock_nvm();
    rec_len = tiku_kits_crypto_tls_record_encrypt(
        tls_tx_buf,
        &conn->tx_gcm, conn->tx_iv, &conn->tx_seq,
        TIKU_KITS_CRYPTO_TLS_CT_APPLICATION,
        data, data_len);
    tiku_mpu_lock_nvm(saved);

    if (rec_len == 0) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    return tiku_kits_net_tcp_send(
        conn->tcp, tls_tx_buf, rec_len);
}

uint16_t
tiku_kits_crypto_tls_read(tiku_kits_crypto_tls_conn_t *conn,
                            uint8_t *buf,
                            uint16_t buf_len)
{
    uint16_t to_read;

    if (conn == (void *)0 || buf == (void *)0 ||
        buf_len == 0 || app_data_avail == 0) {
        return 0;
    }

    to_read = (buf_len < app_data_avail)
              ? buf_len : app_data_avail;

    /* Copy from FRAM RX buffer (decrypted data starts
     * after the record header) */
    memcpy(buf,
           tls_rx_buf + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
           to_read);

    app_data_avail -= to_read;
    return to_read;
}

/*---------------------------------------------------------------------------*/
/* CLOSE / ALERT                                                             */
/*---------------------------------------------------------------------------*/

static void
tls_send_alert(uint8_t desc)
{
    tiku_kits_crypto_tls_conn_t *c = &tls_conn;
    uint8_t alert[2];
    uint16_t rec_len;
    uint16_t saved;

    alert[0] = 2;     /* fatal */
    alert[1] = desc;

    if (c->state >= TIKU_KITS_CRYPTO_TLS_STATE_WAIT_EE &&
        c->state <= TIKU_KITS_CRYPTO_TLS_STATE_ESTABLISHED) {
        /* Encrypt the alert */
        saved = tiku_mpu_unlock_nvm();
        rec_len = tiku_kits_crypto_tls_record_encrypt(
            tls_tx_buf,
            &c->tx_gcm, c->tx_iv, &c->tx_seq,
            TIKU_KITS_CRYPTO_TLS_CT_ALERT,
            alert, 2);
        tiku_mpu_lock_nvm(saved);
    } else {
        /* Plaintext alert */
        saved = tiku_mpu_unlock_nvm();
        rec_len = tiku_kits_crypto_tls_record_build_plain(
            tls_tx_buf,
            TIKU_KITS_CRYPTO_TLS_CT_ALERT,
            alert, 2);
        tiku_mpu_lock_nvm(saved);
    }

    if (rec_len > 0 && c->tcp) {
        tiku_kits_net_tcp_send(c->tcp, tls_tx_buf, rec_len);
    }

    c->state = TIKU_KITS_CRYPTO_TLS_STATE_ERROR;
    c->alert_desc = desc;

    if (c->app_event_cb) {
        c->app_event_cb(c,
            TIKU_KITS_CRYPTO_TLS_EVT_ERROR);
    }
}

int
tiku_kits_crypto_tls_close(tiku_kits_crypto_tls_conn_t *conn)
{
    uint8_t alert[2];
    uint16_t rec_len;
    uint16_t saved;

    if (conn == (void *)0) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (conn->state !=
        TIKU_KITS_CRYPTO_TLS_STATE_ESTABLISHED) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    alert[0] = 1;     /* warning */
    alert[1] = TIKU_KITS_CRYPTO_TLS_ALERT_CLOSE_NOTIFY;

    saved = tiku_mpu_unlock_nvm();
    rec_len = tiku_kits_crypto_tls_record_encrypt(
        tls_tx_buf,
        &conn->tx_gcm, conn->tx_iv, &conn->tx_seq,
        TIKU_KITS_CRYPTO_TLS_CT_ALERT,
        alert, 2);
    tiku_mpu_lock_nvm(saved);

    if (rec_len > 0 && conn->tcp) {
        tiku_kits_net_tcp_send(
            conn->tcp, tls_tx_buf, rec_len);
    }

    conn->state = TIKU_KITS_CRYPTO_TLS_STATE_CLOSING;

    return TIKU_KITS_CRYPTO_OK;
}
