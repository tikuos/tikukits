/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_mqtt.c - MQTT 3.1.1 client implementation
 *
 * Implements the MQTT 3.1.1 protocol state machine, packet
 * builders, incremental RX parser, keepalive management, and
 * in-flight QoS 1 tracking.  Builds on the TikuOS TCP stack
 * (and optionally TLS 1.3 PSK) for transport.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.  See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_net_mqtt.h"

#if TIKU_KITS_NET_MQTT_ENABLE

#include <tikukits/net/ipv4/tiku_kits_net_tcp.h>
#include <kernel/timers/tiku_clock.h>
#include <stddef.h>

#if TIKU_KITS_NET_MQTT_TLS_ENABLE
#include <tikukits/crypto/tls/tiku_kits_crypto_tls.h>
#endif

/*---------------------------------------------------------------------------*/
/* RX PARSER STATES                                                          */
/*---------------------------------------------------------------------------*/

#define MQTT_PARSE_TYPE         0   /**< Waiting for first byte */
#define MQTT_PARSE_REMAINING    1   /**< Reading remaining length */
#define MQTT_PARSE_PAYLOAD      2   /**< Accumulating payload */

/*---------------------------------------------------------------------------*/
/* IN-FLIGHT ENTRY TYPES                                                     */
/*---------------------------------------------------------------------------*/

#define MQTT_INFLIGHT_PUBLISH       1
#define MQTT_INFLIGHT_SUBSCRIBE     2
#define MQTT_INFLIGHT_UNSUBSCRIBE   3

/*---------------------------------------------------------------------------*/
/* INTERNAL TYPES                                                            */
/*---------------------------------------------------------------------------*/

/** RX parser state machine. */
typedef struct {
    uint8_t  state;             /**< MQTT_PARSE_* */
    uint8_t  pkt_type;          /**< Packet type (upper nibble) */
    uint8_t  pkt_flags;         /**< Flags (lower nibble) */
    uint16_t remaining_len;     /**< Decoded remaining length */
    uint16_t remaining_mult;    /**< Multiplier for VBI decode */
    uint16_t payload_pos;       /**< Bytes accumulated so far */
} mqtt_parser_t;

/** In-flight table entry. */
typedef struct {
    uint16_t packet_id;
    uint8_t  type;              /**< MQTT_INFLIGHT_* */
    uint8_t  active;
} mqtt_inflight_t;

/** Module-level connection context. */
typedef struct {
    /* Server configuration */
    uint8_t  server_addr[4];
    uint16_t server_port;

    /* Transport pointers */
    tiku_kits_net_tcp_conn_t *tcp;
#if TIKU_KITS_NET_MQTT_TLS_ENABLE
    tiku_kits_crypto_tls_conn_t *tls;
#endif

    /* State */
    uint8_t  state;
    uint8_t  connack_rc;
    uint8_t  suback_rc;
    uint8_t  ping_pending;
    uint8_t  will_qos;
    uint8_t  will_retain;
    uint16_t will_msg_len;

    /* Timekeeping */
    tiku_clock_time_t last_send_time;
    tiku_clock_time_t ping_send_time;
    tiku_clock_time_t connect_time;

    /* Packet ID counter (1-65535, never 0) */
    uint16_t next_packet_id;

    /* Local port (incremented per connection) */
    uint16_t local_port;

    /* Application callbacks */
    tiku_kits_net_mqtt_msg_cb_t   msg_cb;
    tiku_kits_net_mqtt_event_cb_t event_cb;

    /* In-flight table */
    mqtt_inflight_t
        inflight[TIKU_KITS_NET_MQTT_MAX_INFLIGHT];
} mqtt_ctx_t;

/*---------------------------------------------------------------------------*/
/* STATIC DATA -- FRAM-backed buffers                                        */
/*---------------------------------------------------------------------------*/

/** TX buffer for building outgoing MQTT packets. */
static uint8_t mqtt_tx_buf[TIKU_KITS_NET_MQTT_TX_BUF_SIZE]
    __attribute__((section(".persistent")));

/** RX buffer for incoming MQTT packet payloads. */
static uint8_t mqtt_rx_buf[TIKU_KITS_NET_MQTT_RX_BUF_SIZE]
    __attribute__((section(".persistent")));

/** Credential storage. */
static char mqtt_client_id[TIKU_KITS_NET_MQTT_MAX_CLIENT_ID + 1]
    __attribute__((section(".persistent")));
static char mqtt_username[TIKU_KITS_NET_MQTT_MAX_USERNAME + 1]
    __attribute__((section(".persistent")));
static char mqtt_password[TIKU_KITS_NET_MQTT_MAX_PASSWORD + 1]
    __attribute__((section(".persistent")));

/** Will storage. */
static char mqtt_will_topic[TIKU_KITS_NET_MQTT_MAX_WILL_TOPIC + 1]
    __attribute__((section(".persistent")));
static uint8_t mqtt_will_msg[TIKU_KITS_NET_MQTT_MAX_WILL_MSG]
    __attribute__((section(".persistent")));

/*---------------------------------------------------------------------------*/
/* STATIC DATA -- SRAM (fast-access state)                                   */
/*---------------------------------------------------------------------------*/

static mqtt_ctx_t    mqtt_ctx;
static mqtt_parser_t mqtt_parser;

/*---------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS                                                      */
/*---------------------------------------------------------------------------*/

static void mqtt_process_packet(void);

static void mqtt_tcp_recv_cb(
    tiku_kits_net_tcp_conn_t *conn,
    uint16_t available);
static void mqtt_tcp_event_cb(
    tiku_kits_net_tcp_conn_t *conn,
    uint8_t event);

#if TIKU_KITS_NET_MQTT_TLS_ENABLE
static void mqtt_tls_recv_cb(
    tiku_kits_crypto_tls_conn_t *conn,
    uint16_t available);
static void mqtt_tls_event_cb(
    tiku_kits_crypto_tls_conn_t *conn,
    uint8_t event);
#endif

/*---------------------------------------------------------------------------*/
/* STRING HELPERS (no libc dependency)                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Measure NUL-terminated string length.
 */
static uint16_t
mqtt_strlen(const char *s)
{
    uint16_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/**
 * @brief Copy @p n bytes from @p src to @p dst.
 */
static void
mqtt_memcpy(void *dst, const void *src, uint16_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    uint16_t i;
    for (i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/**
 * @brief Zero @p n bytes starting at @p dst.
 */
static void
mqtt_memzero(void *dst, uint16_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint16_t i;
    for (i = 0; i < n; i++) {
        d[i] = 0;
    }
}

/**
 * @brief Copy a NUL-terminated string into a fixed buffer.
 *
 * @return String length, or 0 if src is NULL.
 */
static uint16_t
mqtt_strcpy(char *dst, const char *src, uint16_t max_len)
{
    uint16_t i = 0;
    if (src == NULL) {
        dst[0] = '\0';
        return 0;
    }
    while (src[i] != '\0' && i < max_len) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}

/*---------------------------------------------------------------------------*/
/* REMAINING LENGTH ENCODING (MQTT Variable Byte Integer)                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encode remaining length into buffer.
 *
 * @param buf  Destination buffer
 * @param pos  Write position
 * @param len  Value to encode (0 to 268435455)
 * @return New write position
 */
static uint16_t
mqtt_encode_remaining(uint8_t *buf, uint16_t pos,
                      uint16_t len)
{
    do {
        uint8_t byte = (uint8_t)(len & 0x7F);
        len >>= 7;
        if (len > 0) {
            byte |= 0x80;
        }
        buf[pos++] = byte;
    } while (len > 0);
    return pos;
}

/*---------------------------------------------------------------------------*/
/* MQTT UTF-8 STRING ENCODING                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encode a string as MQTT UTF-8 (2-byte length + data).
 *
 * @param buf      Destination buffer
 * @param pos      Write position
 * @param str      String data
 * @param str_len  String length
 * @return New write position
 */
static uint16_t
mqtt_encode_utf8(uint8_t *buf, uint16_t pos,
                 const char *str, uint16_t str_len)
{
    uint16_t i;
    buf[pos++] = (uint8_t)(str_len >> 8);
    buf[pos++] = (uint8_t)(str_len & 0xFF);
    for (i = 0; i < str_len; i++) {
        buf[pos++] = (uint8_t)str[i];
    }
    return pos;
}

/*---------------------------------------------------------------------------*/
/* TRANSPORT ABSTRACTION                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send raw bytes over the active transport (TCP or TLS).
 *
 * Handles chunking at MSS (TCP) or MAX_FRAG_LEN (TLS).
 * If a chunk fails mid-send, returns the error immediately;
 * the caller should abort the connection.
 */
static int8_t
mqtt_send_raw(const uint8_t *data, uint16_t len)
{
    uint16_t sent = 0;
    uint16_t max_chunk;

#if TIKU_KITS_NET_MQTT_TLS_ENABLE
    if (mqtt_ctx.tls) {
        max_chunk = TIKU_KITS_CRYPTO_TLS_MAX_FRAG_LEN;
        while (sent < len) {
            uint16_t chunk = len - sent;
            if (chunk > max_chunk) {
                chunk = max_chunk;
            }
            int rc = tiku_kits_crypto_tls_send(
                mqtt_ctx.tls, data + sent, chunk);
            if (rc != TIKU_KITS_CRYPTO_OK) {
                return TIKU_KITS_NET_ERR_OVERFLOW;
            }
            sent += chunk;
        }
        mqtt_ctx.last_send_time = tiku_clock_time();
        return TIKU_KITS_NET_OK;
    }
#endif

    if (!mqtt_ctx.tcp) {
        return TIKU_KITS_NET_ERR_NULL;
    }
    max_chunk = mqtt_ctx.tcp->snd_mss;
    if (max_chunk == 0) {
        max_chunk = TIKU_KITS_NET_TCP_MSS;
    }

    while (sent < len) {
        uint16_t chunk = len - sent;
        if (chunk > max_chunk) {
            chunk = max_chunk;
        }
        int8_t rc = tiku_kits_net_tcp_send(
            mqtt_ctx.tcp, data + sent, chunk);
        if (rc != TIKU_KITS_NET_OK) {
            return rc;
        }
        sent += chunk;
    }

    mqtt_ctx.last_send_time = tiku_clock_time();
    return TIKU_KITS_NET_OK;
}

/**
 * @brief Read bytes from the active transport.
 */
static uint16_t
mqtt_read_raw(uint8_t *buf, uint16_t len)
{
#if TIKU_KITS_NET_MQTT_TLS_ENABLE
    if (mqtt_ctx.tls) {
        return tiku_kits_crypto_tls_read(
            mqtt_ctx.tls, buf, len);
    }
#endif
    if (!mqtt_ctx.tcp) {
        return 0;
    }
    return tiku_kits_net_tcp_read(mqtt_ctx.tcp, buf, len);
}

/**
 * @brief Abort the transport immediately (RST, no FIN).
 */
static void
mqtt_abort_transport(void)
{
    if (mqtt_ctx.tcp) {
        tiku_kits_net_tcp_abort(mqtt_ctx.tcp);
        mqtt_ctx.tcp = NULL;
    }
#if TIKU_KITS_NET_MQTT_TLS_ENABLE
    mqtt_ctx.tls = NULL;
#endif
}

/**
 * @brief Gracefully close the transport (FIN exchange).
 */
static void
mqtt_close_transport(void)
{
#if TIKU_KITS_NET_MQTT_TLS_ENABLE
    if (mqtt_ctx.tls) {
        tiku_kits_crypto_tls_close(mqtt_ctx.tls);
        mqtt_ctx.tls = NULL;
    }
#endif
    if (mqtt_ctx.tcp) {
        tiku_kits_net_tcp_close(mqtt_ctx.tcp);
        mqtt_ctx.tcp = NULL;
    }
}

/*---------------------------------------------------------------------------*/
/* PACKET ID MANAGEMENT                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Allocate the next packet identifier (1-65535).
 */
static uint16_t
mqtt_alloc_packet_id(void)
{
    mqtt_ctx.next_packet_id++;
    if (mqtt_ctx.next_packet_id == 0) {
        mqtt_ctx.next_packet_id = 1;
    }
    return mqtt_ctx.next_packet_id;
}

/*---------------------------------------------------------------------------*/
/* IN-FLIGHT TABLE MANAGEMENT                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add an entry to the in-flight table.
 *
 * @return Allocated packet ID, or 0 if table full.
 */
static uint16_t
mqtt_inflight_add(uint8_t type)
{
    uint8_t i;
    for (i = 0; i < TIKU_KITS_NET_MQTT_MAX_INFLIGHT; i++) {
        if (!mqtt_ctx.inflight[i].active) {
            uint16_t id = mqtt_alloc_packet_id();
            mqtt_ctx.inflight[i].packet_id = id;
            mqtt_ctx.inflight[i].type = type;
            mqtt_ctx.inflight[i].active = 1;
            return id;
        }
    }
    return 0;
}

/**
 * @brief Remove an entry from the in-flight table by ID.
 */
static void
mqtt_inflight_remove(uint16_t packet_id)
{
    uint8_t i;
    for (i = 0; i < TIKU_KITS_NET_MQTT_MAX_INFLIGHT; i++) {
        if (mqtt_ctx.inflight[i].active &&
            mqtt_ctx.inflight[i].packet_id == packet_id) {
            mqtt_ctx.inflight[i].active = 0;
            return;
        }
    }
}

/**
 * @brief Clear all in-flight entries.
 */
static void
mqtt_inflight_clear(void)
{
    uint8_t i;
    for (i = 0; i < TIKU_KITS_NET_MQTT_MAX_INFLIGHT; i++) {
        mqtt_ctx.inflight[i].active = 0;
    }
}

/*---------------------------------------------------------------------------*/
/* TX PACKET BUILDERS                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build and send MQTT CONNECT packet.
 */
static int8_t
mqtt_send_connect(void)
{
    uint8_t *buf = mqtt_tx_buf;
    uint16_t pos;
    uint16_t rem_len;
    uint8_t  flags = 0x02;  /* Clean Session */

    uint16_t cid_len = mqtt_strlen(mqtt_client_id);
    rem_len = 10 + 2 + cid_len;

    /* Will */
    if (mqtt_will_topic[0] != '\0') {
        uint16_t wt_len = mqtt_strlen(mqtt_will_topic);
        rem_len += 2 + wt_len + 2 + mqtt_ctx.will_msg_len;
        flags |= 0x04;
        flags |= (uint8_t)((mqtt_ctx.will_qos & 0x03) << 3);
        if (mqtt_ctx.will_retain) {
            flags |= 0x20;
        }
    }

    /* Username */
    if (mqtt_username[0] != '\0') {
        rem_len += 2 + mqtt_strlen(mqtt_username);
        flags |= 0x80;
    }

    /* Password */
    if (mqtt_password[0] != '\0') {
        rem_len += 2 + mqtt_strlen(mqtt_password);
        flags |= 0x40;
    }

    /* Check buffer capacity (1 type + max 2 VBI + payload) */
    if ((uint16_t)(3 + rem_len) >
        TIKU_KITS_NET_MQTT_TX_BUF_SIZE) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    /* Fixed header */
    pos = 0;
    buf[pos++] = 0x10;     /* CONNECT (type 1, flags 0) */
    pos = mqtt_encode_remaining(buf, pos, rem_len);

    /* Variable header -- Protocol Name */
    buf[pos++] = 0x00;
    buf[pos++] = 0x04;
    buf[pos++] = 'M';
    buf[pos++] = 'Q';
    buf[pos++] = 'T';
    buf[pos++] = 'T';

    /* Protocol Level (4 = MQTT 3.1.1) */
    buf[pos++] = 0x04;

    /* Connect Flags */
    buf[pos++] = flags;

    /* Keep Alive (seconds, big-endian) */
    buf[pos++] = (uint8_t)(TIKU_KITS_NET_MQTT_KEEPALIVE_SEC
                            >> 8);
    buf[pos++] = (uint8_t)(TIKU_KITS_NET_MQTT_KEEPALIVE_SEC
                            & 0xFF);

    /* Payload -- Client ID */
    pos = mqtt_encode_utf8(buf, pos,
                           mqtt_client_id, cid_len);

    /* Payload -- Will Topic + Will Message */
    if (mqtt_will_topic[0] != '\0') {
        uint16_t wt_len = mqtt_strlen(mqtt_will_topic);
        pos = mqtt_encode_utf8(buf, pos,
                               mqtt_will_topic, wt_len);
        buf[pos++] = (uint8_t)(mqtt_ctx.will_msg_len >> 8);
        buf[pos++] = (uint8_t)(mqtt_ctx.will_msg_len & 0xFF);
        mqtt_memcpy(&buf[pos], mqtt_will_msg,
                    mqtt_ctx.will_msg_len);
        pos += mqtt_ctx.will_msg_len;
    }

    /* Payload -- Username */
    if (mqtt_username[0] != '\0') {
        uint16_t un_len = mqtt_strlen(mqtt_username);
        pos = mqtt_encode_utf8(buf, pos,
                               mqtt_username, un_len);
    }

    /* Payload -- Password */
    if (mqtt_password[0] != '\0') {
        uint16_t pw_len = mqtt_strlen(mqtt_password);
        pos = mqtt_encode_utf8(buf, pos,
                               mqtt_password, pw_len);
    }

    mqtt_ctx.state =
        TIKU_KITS_NET_MQTT_STATE_CONNECTING;
    mqtt_ctx.connect_time = tiku_clock_time();
    return mqtt_send_raw(buf, pos);
}

/**
 * @brief Build and send MQTT PUBLISH packet.
 */
static int8_t
mqtt_send_publish(const char *topic,
                  const uint8_t *data, uint16_t data_len,
                  uint8_t qos, uint8_t retain,
                  uint16_t packet_id)
{
    uint8_t *buf = mqtt_tx_buf;
    uint16_t pos;
    uint16_t topic_len = mqtt_strlen(topic);
    uint16_t rem_len;

    rem_len = 2 + topic_len + data_len;
    if (qos > 0) {
        rem_len += 2;
    }

    if ((uint16_t)(3 + rem_len) >
        TIKU_KITS_NET_MQTT_TX_BUF_SIZE) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    /* Fixed header */
    pos = 0;
    uint8_t byte0 = 0x30;  /* PUBLISH type */
    if (qos > 0) {
        byte0 |= (uint8_t)((qos & 0x03) << 1);
    }
    if (retain) {
        byte0 |= 0x01;
    }
    buf[pos++] = byte0;
    pos = mqtt_encode_remaining(buf, pos, rem_len);

    /* Variable header -- Topic Name */
    pos = mqtt_encode_utf8(buf, pos, topic, topic_len);

    /* Variable header -- Packet Identifier (QoS 1 only) */
    if (qos > 0) {
        buf[pos++] = (uint8_t)(packet_id >> 8);
        buf[pos++] = (uint8_t)(packet_id & 0xFF);
    }

    /* Payload */
    if (data_len > 0 && data != NULL) {
        mqtt_memcpy(&buf[pos], data, data_len);
        pos += data_len;
    }

    return mqtt_send_raw(buf, pos);
}

/**
 * @brief Build and send MQTT SUBSCRIBE packet.
 */
static int8_t
mqtt_send_subscribe(const char *topic, uint8_t qos,
                    uint16_t packet_id)
{
    uint8_t *buf = mqtt_tx_buf;
    uint16_t pos;
    uint16_t topic_len = mqtt_strlen(topic);
    uint16_t rem_len = 2 + 2 + topic_len + 1;

    if ((uint16_t)(3 + rem_len) >
        TIKU_KITS_NET_MQTT_TX_BUF_SIZE) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    pos = 0;
    buf[pos++] = 0x82;     /* SUBSCRIBE (type 8, flags 0x02) */
    pos = mqtt_encode_remaining(buf, pos, rem_len);

    /* Packet Identifier */
    buf[pos++] = (uint8_t)(packet_id >> 8);
    buf[pos++] = (uint8_t)(packet_id & 0xFF);

    /* Topic Filter + Requested QoS */
    pos = mqtt_encode_utf8(buf, pos, topic, topic_len);
    buf[pos++] = qos;

    return mqtt_send_raw(buf, pos);
}

/**
 * @brief Build and send MQTT UNSUBSCRIBE packet.
 */
static int8_t
mqtt_send_unsubscribe(const char *topic,
                      uint16_t packet_id)
{
    uint8_t *buf = mqtt_tx_buf;
    uint16_t pos;
    uint16_t topic_len = mqtt_strlen(topic);
    uint16_t rem_len = 2 + 2 + topic_len;

    if ((uint16_t)(3 + rem_len) >
        TIKU_KITS_NET_MQTT_TX_BUF_SIZE) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    pos = 0;
    buf[pos++] = 0xA2;     /* UNSUBSCRIBE (type 10, flags 0x02) */
    pos = mqtt_encode_remaining(buf, pos, rem_len);

    buf[pos++] = (uint8_t)(packet_id >> 8);
    buf[pos++] = (uint8_t)(packet_id & 0xFF);

    pos = mqtt_encode_utf8(buf, pos, topic, topic_len);

    return mqtt_send_raw(buf, pos);
}

/**
 * @brief Send MQTT PINGREQ (2 bytes).
 */
static int8_t
mqtt_send_pingreq(void)
{
    uint8_t pkt[2] = {0xC0, 0x00};
    return mqtt_send_raw(pkt, 2);
}

/**
 * @brief Send MQTT DISCONNECT (2 bytes).
 */
static int8_t
mqtt_send_disconnect_pkt(void)
{
    uint8_t pkt[2] = {0xE0, 0x00};
    return mqtt_send_raw(pkt, 2);
}

/**
 * @brief Send MQTT PUBACK for a received QoS 1 message.
 */
static int8_t
mqtt_send_puback(uint16_t packet_id)
{
    uint8_t pkt[4];
    pkt[0] = 0x40;
    pkt[1] = 0x02;
    pkt[2] = (uint8_t)(packet_id >> 8);
    pkt[3] = (uint8_t)(packet_id & 0xFF);
    return mqtt_send_raw(pkt, 4);
}

/*---------------------------------------------------------------------------*/
/* RX PARSER                                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the RX parser to its initial state.
 */
static void
mqtt_parser_reset(void)
{
    mqtt_parser.state = MQTT_PARSE_TYPE;
    mqtt_parser.remaining_len = 0;
    mqtt_parser.remaining_mult = 1;
    mqtt_parser.payload_pos = 0;
}

/**
 * @brief Feed one byte to the incremental MQTT parser.
 *
 * The parser handles packets split across TCP segments.
 * When a complete packet is assembled, mqtt_process_packet()
 * is called and the parser resets for the next packet.
 */
static void
mqtt_parser_feed_byte(uint8_t byte)
{
    switch (mqtt_parser.state) {
    case MQTT_PARSE_TYPE:
        mqtt_parser.pkt_type = (byte >> 4) & 0x0F;
        mqtt_parser.pkt_flags = byte & 0x0F;
        mqtt_parser.remaining_len = 0;
        mqtt_parser.remaining_mult = 1;
        mqtt_parser.state = MQTT_PARSE_REMAINING;
        break;

    case MQTT_PARSE_REMAINING:
        mqtt_parser.remaining_len +=
            (uint16_t)(byte & 0x7F) *
            mqtt_parser.remaining_mult;
        mqtt_parser.remaining_mult *= 128;

        if ((byte & 0x80) == 0) {
            /* Remaining length decode complete */
            if (mqtt_parser.remaining_len == 0) {
                mqtt_parser.payload_pos = 0;
                mqtt_process_packet();
                mqtt_parser_reset();
            } else if (mqtt_parser.remaining_len >
                       TIKU_KITS_NET_MQTT_RX_BUF_SIZE) {
                /* Packet too large, discard */
                mqtt_parser_reset();
            } else {
                mqtt_parser.payload_pos = 0;
                mqtt_parser.state = MQTT_PARSE_PAYLOAD;
            }
        }
        break;

    case MQTT_PARSE_PAYLOAD:
        if (mqtt_parser.payload_pos <
            TIKU_KITS_NET_MQTT_RX_BUF_SIZE) {
            mqtt_rx_buf[mqtt_parser.payload_pos] = byte;
        }
        mqtt_parser.payload_pos++;
        if (mqtt_parser.payload_pos >=
            mqtt_parser.remaining_len) {
            mqtt_process_packet();
            mqtt_parser_reset();
        }
        break;

    default:
        mqtt_parser_reset();
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* PACKET HANDLERS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Handle incoming CONNACK.
 */
static void
mqtt_handle_connack(void)
{
    if (mqtt_ctx.state !=
        TIKU_KITS_NET_MQTT_STATE_CONNECTING) {
        return;
    }
    if (mqtt_parser.remaining_len < 2) {
        return;
    }

    /* Byte 0: session present (bit 0) -- ignored (clean) */
    /* Byte 1: return code */
    mqtt_ctx.connack_rc = mqtt_rx_buf[1];

    if (mqtt_ctx.connack_rc ==
        TIKU_KITS_NET_MQTT_CONNACK_ACCEPTED) {
        mqtt_ctx.state =
            TIKU_KITS_NET_MQTT_STATE_CONNECTED;
        mqtt_ctx.last_send_time = tiku_clock_time();
        mqtt_ctx.ping_pending = 0;
        if (mqtt_ctx.event_cb) {
            mqtt_ctx.event_cb(
                TIKU_KITS_NET_MQTT_EVT_CONNECTED);
        }
    } else {
        /* Broker rejected -- abort */
        mqtt_abort_transport();
        mqtt_ctx.state =
            TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
        if (mqtt_ctx.event_cb) {
            mqtt_ctx.event_cb(
                TIKU_KITS_NET_MQTT_EVT_ERROR);
        }
    }
}

/**
 * @brief Handle incoming PUBLISH.
 */
static void
mqtt_handle_publish(void)
{
    uint8_t  qos;
    uint8_t  retain;
    uint16_t topic_len;
    uint16_t offset;
    uint16_t packet_id = 0;
    const char    *topic;
    const uint8_t *payload;
    uint16_t payload_len;

    if (mqtt_ctx.state !=
        TIKU_KITS_NET_MQTT_STATE_CONNECTED) {
        return;
    }
    if (mqtt_parser.remaining_len < 2) {
        return;
    }

    qos = (mqtt_parser.pkt_flags >> 1) & 0x03;
    retain = mqtt_parser.pkt_flags & 0x01;

    /* Ignore QoS 2 (not supported) */
    if (qos > 1) {
        return;
    }

    /* Parse topic */
    topic_len = ((uint16_t)mqtt_rx_buf[0] << 8) |
                 mqtt_rx_buf[1];
    offset = 2;

    if (offset + topic_len > mqtt_parser.remaining_len) {
        return;
    }
    topic = (const char *)&mqtt_rx_buf[offset];
    offset += topic_len;

    /* Parse packet ID (QoS 1) */
    if (qos > 0) {
        if (offset + 2 > mqtt_parser.remaining_len) {
            return;
        }
        packet_id = ((uint16_t)mqtt_rx_buf[offset] << 8) |
                     mqtt_rx_buf[offset + 1];
        offset += 2;
    }

    /* Payload */
    payload = &mqtt_rx_buf[offset];
    payload_len = mqtt_parser.remaining_len - offset;

    /* Deliver to application */
    if (mqtt_ctx.msg_cb) {
        mqtt_ctx.msg_cb(topic, topic_len,
                        payload, payload_len,
                        qos, retain);
    }

    /* Auto-send PUBACK for QoS 1 */
    if (qos == 1 && packet_id != 0) {
        mqtt_send_puback(packet_id);
    }
}

/**
 * @brief Handle incoming PUBACK.
 */
static void
mqtt_handle_puback(void)
{
    uint16_t packet_id;

    if (mqtt_parser.remaining_len < 2) {
        return;
    }

    packet_id = ((uint16_t)mqtt_rx_buf[0] << 8) |
                 mqtt_rx_buf[1];
    mqtt_inflight_remove(packet_id);

    if (mqtt_ctx.event_cb) {
        mqtt_ctx.event_cb(TIKU_KITS_NET_MQTT_EVT_PUBACK);
    }
}

/**
 * @brief Handle incoming SUBACK.
 */
static void
mqtt_handle_suback(void)
{
    uint16_t packet_id;

    if (mqtt_parser.remaining_len < 3) {
        return;
    }

    packet_id = ((uint16_t)mqtt_rx_buf[0] << 8) |
                 mqtt_rx_buf[1];
    mqtt_ctx.suback_rc = mqtt_rx_buf[2];
    mqtt_inflight_remove(packet_id);

    if (mqtt_ctx.event_cb) {
        mqtt_ctx.event_cb(TIKU_KITS_NET_MQTT_EVT_SUBACK);
    }
}

/**
 * @brief Handle incoming UNSUBACK.
 */
static void
mqtt_handle_unsuback(void)
{
    uint16_t packet_id;

    if (mqtt_parser.remaining_len < 2) {
        return;
    }

    packet_id = ((uint16_t)mqtt_rx_buf[0] << 8) |
                 mqtt_rx_buf[1];
    mqtt_inflight_remove(packet_id);

    if (mqtt_ctx.event_cb) {
        mqtt_ctx.event_cb(
            TIKU_KITS_NET_MQTT_EVT_UNSUBACK);
    }
}

/**
 * @brief Handle incoming PINGRESP.
 */
static void
mqtt_handle_pingresp(void)
{
    mqtt_ctx.ping_pending = 0;
}

/**
 * @brief Dispatch a fully assembled MQTT packet.
 */
static void
mqtt_process_packet(void)
{
    switch (mqtt_parser.pkt_type) {
    case TIKU_KITS_NET_MQTT_TYPE_CONNACK:
        mqtt_handle_connack();
        break;
    case TIKU_KITS_NET_MQTT_TYPE_PUBLISH:
        mqtt_handle_publish();
        break;
    case TIKU_KITS_NET_MQTT_TYPE_PUBACK:
        mqtt_handle_puback();
        break;
    case TIKU_KITS_NET_MQTT_TYPE_SUBACK:
        mqtt_handle_suback();
        break;
    case TIKU_KITS_NET_MQTT_TYPE_UNSUBACK:
        mqtt_handle_unsuback();
        break;
    case TIKU_KITS_NET_MQTT_TYPE_PINGRESP:
        mqtt_handle_pingresp();
        break;
    default:
        /* Unknown or unsupported packet type -- ignore */
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* FEED DATA TO PARSER                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Feed a block of bytes from the transport to the parser.
 *
 * Handles multiple MQTT packets in a single read and stops
 * early if a state transition to DISCONNECTED occurs.
 */
static void
mqtt_feed_data(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        if (mqtt_ctx.state ==
            TIKU_KITS_NET_MQTT_STATE_DISCONNECTED) {
            return;
        }
        mqtt_parser_feed_byte(data[i]);
    }
}

/*---------------------------------------------------------------------------*/
/* TCP CALLBACKS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief TCP data arrival callback.
 *
 * Reads all available bytes from the TCP RX ring buffer and
 * feeds them to the MQTT parser.
 */
static void
mqtt_tcp_recv_cb(tiku_kits_net_tcp_conn_t *conn,
                 uint16_t available)
{
    uint8_t tmp[64];
    (void)conn;

    while (available > 0) {
        uint16_t to_read = available;
        if (to_read > sizeof(tmp)) {
            to_read = sizeof(tmp);
        }
        uint16_t got = mqtt_read_raw(tmp, to_read);
        if (got == 0) {
            break;
        }
        mqtt_feed_data(tmp, got);
        if (available >= got) {
            available -= got;
        } else {
            available = 0;
        }
    }
}

/**
 * @brief TCP connection event callback.
 *
 * Handles: TCP CONNECTED (start TLS or MQTT handshake),
 * TCP CLOSED/ABORTED (unexpected disconnection).
 */
static void
mqtt_tcp_event_cb(tiku_kits_net_tcp_conn_t *conn,
                  uint8_t event)
{
    (void)conn;

    if (event == TIKU_KITS_NET_TCP_EVT_CONNECTED) {
#if TIKU_KITS_NET_MQTT_TLS_ENABLE
        /* TCP up -- start TLS handshake */
        mqtt_ctx.state =
            TIKU_KITS_NET_MQTT_STATE_TLS_CONNECTING;
        mqtt_ctx.tls = tiku_kits_crypto_tls_connect(
            mqtt_ctx.tcp,
            mqtt_tls_recv_cb,
            mqtt_tls_event_cb);
        if (!mqtt_ctx.tls) {
            mqtt_abort_transport();
            mqtt_ctx.state =
                TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
            if (mqtt_ctx.event_cb) {
                mqtt_ctx.event_cb(
                    TIKU_KITS_NET_MQTT_EVT_ERROR);
            }
        }
#else
        /* TCP up -- send MQTT CONNECT directly */
        int8_t rc = mqtt_send_connect();
        if (rc != TIKU_KITS_NET_OK) {
            mqtt_abort_transport();
            mqtt_ctx.state =
                TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
            if (mqtt_ctx.event_cb) {
                mqtt_ctx.event_cb(
                    TIKU_KITS_NET_MQTT_EVT_ERROR);
            }
        }
#endif
    } else {
        /* CLOSED or ABORTED -- unexpected */
        if (mqtt_ctx.state !=
            TIKU_KITS_NET_MQTT_STATE_DISCONNECTED) {
            mqtt_ctx.tcp = NULL;
#if TIKU_KITS_NET_MQTT_TLS_ENABLE
            mqtt_ctx.tls = NULL;
#endif
            mqtt_ctx.state =
                TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
            if (mqtt_ctx.event_cb) {
                mqtt_ctx.event_cb(
                    TIKU_KITS_NET_MQTT_EVT_DISCONNECTED);
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
/* TLS CALLBACKS (only when TLS is enabled)                                  */
/*---------------------------------------------------------------------------*/

#if TIKU_KITS_NET_MQTT_TLS_ENABLE

/**
 * @brief TLS decrypted data arrival callback.
 */
static void
mqtt_tls_recv_cb(tiku_kits_crypto_tls_conn_t *conn,
                 uint16_t available)
{
    uint8_t tmp[64];
    (void)conn;

    while (available > 0) {
        uint16_t to_read = available;
        if (to_read > sizeof(tmp)) {
            to_read = sizeof(tmp);
        }
        uint16_t got = mqtt_read_raw(tmp, to_read);
        if (got == 0) {
            break;
        }
        mqtt_feed_data(tmp, got);
        if (available >= got) {
            available -= got;
        } else {
            available = 0;
        }
    }
}

/**
 * @brief TLS connection event callback.
 */
static void
mqtt_tls_event_cb(tiku_kits_crypto_tls_conn_t *conn,
                  uint8_t event)
{
    (void)conn;

    if (event == TIKU_KITS_CRYPTO_TLS_EVT_CONNECTED) {
        /* TLS handshake complete -- send MQTT CONNECT */
        int8_t rc = mqtt_send_connect();
        if (rc != TIKU_KITS_NET_OK) {
            mqtt_abort_transport();
            mqtt_ctx.state =
                TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
            if (mqtt_ctx.event_cb) {
                mqtt_ctx.event_cb(
                    TIKU_KITS_NET_MQTT_EVT_ERROR);
            }
        }
    } else {
        /* TLS closed or error */
        if (mqtt_ctx.state !=
            TIKU_KITS_NET_MQTT_STATE_DISCONNECTED) {
            mqtt_ctx.tcp = NULL;
            mqtt_ctx.tls = NULL;
            mqtt_ctx.state =
                TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
            if (mqtt_ctx.event_cb) {
                mqtt_ctx.event_cb(
                    TIKU_KITS_NET_MQTT_EVT_DISCONNECTED);
            }
        }
    }
}

#endif /* TIKU_KITS_NET_MQTT_TLS_ENABLE */

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_mqtt_init(void)
{
    mqtt_memzero(&mqtt_ctx, sizeof(mqtt_ctx));
    mqtt_parser_reset();

    mqtt_ctx.state =
        TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
    mqtt_ctx.connack_rc = 0xFF;
    mqtt_ctx.suback_rc = 0xFF;
    mqtt_ctx.next_packet_id = 0;
    mqtt_ctx.local_port = TIKU_KITS_NET_MQTT_LOCAL_PORT;

    mqtt_client_id[0] = '\0';
    mqtt_username[0] = '\0';
    mqtt_password[0] = '\0';
    mqtt_will_topic[0] = '\0';
    mqtt_ctx.will_msg_len = 0;

    mqtt_inflight_clear();
}

int8_t
tiku_kits_net_mqtt_set_server(const uint8_t *addr,
                              uint16_t port)
{
    if (!addr) {
        return TIKU_KITS_NET_ERR_NULL;
    }
    mqtt_memcpy(mqtt_ctx.server_addr, addr, 4);
    mqtt_ctx.server_port = port;
    return TIKU_KITS_NET_OK;
}

int8_t
tiku_kits_net_mqtt_set_credentials(const char *client_id,
                                   const char *username,
                                   const char *password)
{
    uint16_t len;

    if (!client_id) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    len = mqtt_strlen(client_id);
    if (len > TIKU_KITS_NET_MQTT_MAX_CLIENT_ID) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }
    mqtt_strcpy(mqtt_client_id, client_id,
                TIKU_KITS_NET_MQTT_MAX_CLIENT_ID);

    if (username) {
        len = mqtt_strlen(username);
        if (len > TIKU_KITS_NET_MQTT_MAX_USERNAME) {
            return TIKU_KITS_NET_ERR_OVERFLOW;
        }
        mqtt_strcpy(mqtt_username, username,
                    TIKU_KITS_NET_MQTT_MAX_USERNAME);
    } else {
        mqtt_username[0] = '\0';
    }

    if (password) {
        len = mqtt_strlen(password);
        if (len > TIKU_KITS_NET_MQTT_MAX_PASSWORD) {
            return TIKU_KITS_NET_ERR_OVERFLOW;
        }
        mqtt_strcpy(mqtt_password, password,
                    TIKU_KITS_NET_MQTT_MAX_PASSWORD);
    } else {
        mqtt_password[0] = '\0';
    }

    return TIKU_KITS_NET_OK;
}

int8_t
tiku_kits_net_mqtt_set_will(const char *topic,
                            const uint8_t *msg,
                            uint16_t msg_len,
                            uint8_t qos,
                            uint8_t retain)
{
    /* NULL topic clears the will */
    if (!topic) {
        mqtt_will_topic[0] = '\0';
        mqtt_ctx.will_msg_len = 0;
        return TIKU_KITS_NET_OK;
    }

    if (mqtt_strlen(topic) >
        TIKU_KITS_NET_MQTT_MAX_WILL_TOPIC) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }
    if (msg_len > TIKU_KITS_NET_MQTT_MAX_WILL_MSG) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    mqtt_strcpy(mqtt_will_topic, topic,
                TIKU_KITS_NET_MQTT_MAX_WILL_TOPIC);

    if (msg && msg_len > 0) {
        mqtt_memcpy(mqtt_will_msg, msg, msg_len);
    }
    mqtt_ctx.will_msg_len = msg_len;
    mqtt_ctx.will_qos = qos;
    mqtt_ctx.will_retain = retain;

    return TIKU_KITS_NET_OK;
}

int8_t
tiku_kits_net_mqtt_connect(
    tiku_kits_net_mqtt_msg_cb_t msg_cb,
    tiku_kits_net_mqtt_event_cb_t event_cb)
{
    if (mqtt_ctx.state !=
        TIKU_KITS_NET_MQTT_STATE_DISCONNECTED) {
        return TIKU_KITS_NET_ERR_MQTT_STATE;
    }

    /* Store callbacks */
    mqtt_ctx.msg_cb = msg_cb;
    mqtt_ctx.event_cb = event_cb;

    /* Reset state */
    mqtt_parser_reset();
    mqtt_inflight_clear();
    mqtt_ctx.connack_rc = 0xFF;
    mqtt_ctx.suback_rc = 0xFF;
    mqtt_ctx.ping_pending = 0;

    /* Increment local port to avoid TIME_WAIT conflicts */
    mqtt_ctx.local_port++;
    if (mqtt_ctx.local_port < TIKU_KITS_NET_MQTT_LOCAL_PORT) {
        mqtt_ctx.local_port = TIKU_KITS_NET_MQTT_LOCAL_PORT;
    }

    /* Initiate TCP connection */
    mqtt_ctx.state =
        TIKU_KITS_NET_MQTT_STATE_TCP_CONNECTING;
    mqtt_ctx.connect_time = tiku_clock_time();

    mqtt_ctx.tcp = tiku_kits_net_tcp_connect(
        mqtt_ctx.server_addr,
        mqtt_ctx.server_port,
        mqtt_ctx.local_port,
        mqtt_tcp_recv_cb,
        mqtt_tcp_event_cb);

    if (!mqtt_ctx.tcp) {
        mqtt_ctx.state =
            TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
        return TIKU_KITS_NET_ERR_MQTT_TCP;
    }

    return TIKU_KITS_NET_OK;
}

int8_t
tiku_kits_net_mqtt_disconnect(void)
{
    if (mqtt_ctx.state ==
        TIKU_KITS_NET_MQTT_STATE_DISCONNECTED) {
        return TIKU_KITS_NET_ERR_MQTT_STATE;
    }

    /* Send MQTT DISCONNECT if connected */
    if (mqtt_ctx.state ==
        TIKU_KITS_NET_MQTT_STATE_CONNECTED) {
        mqtt_send_disconnect_pkt();
    }

    /* Close transport gracefully */
    mqtt_close_transport();
    mqtt_ctx.state =
        TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;

    /* No event callback -- caller initiated */
    return TIKU_KITS_NET_OK;
}

int8_t
tiku_kits_net_mqtt_publish(const char *topic,
                           const uint8_t *data,
                           uint16_t data_len,
                           uint8_t qos,
                           uint8_t retain)
{
    uint16_t packet_id = 0;

    if (mqtt_ctx.state !=
        TIKU_KITS_NET_MQTT_STATE_CONNECTED) {
        return TIKU_KITS_NET_ERR_MQTT_STATE;
    }
    if (!topic) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    /* Allocate packet ID for QoS 1 */
    if (qos > 0) {
        packet_id = mqtt_inflight_add(
            MQTT_INFLIGHT_PUBLISH);
        if (packet_id == 0) {
            return TIKU_KITS_NET_ERR_MQTT_INFLIGHT;
        }
    }

    return mqtt_send_publish(topic, data, data_len,
                             qos, retain, packet_id);
}

int8_t
tiku_kits_net_mqtt_subscribe(const char *topic,
                             uint8_t qos)
{
    uint16_t packet_id;

    if (mqtt_ctx.state !=
        TIKU_KITS_NET_MQTT_STATE_CONNECTED) {
        return TIKU_KITS_NET_ERR_MQTT_STATE;
    }
    if (!topic) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    packet_id = mqtt_inflight_add(
        MQTT_INFLIGHT_SUBSCRIBE);
    if (packet_id == 0) {
        return TIKU_KITS_NET_ERR_MQTT_INFLIGHT;
    }

    return mqtt_send_subscribe(topic, qos, packet_id);
}

int8_t
tiku_kits_net_mqtt_unsubscribe(const char *topic)
{
    uint16_t packet_id;

    if (mqtt_ctx.state !=
        TIKU_KITS_NET_MQTT_STATE_CONNECTED) {
        return TIKU_KITS_NET_ERR_MQTT_STATE;
    }
    if (!topic) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    packet_id = mqtt_inflight_add(
        MQTT_INFLIGHT_UNSUBSCRIBE);
    if (packet_id == 0) {
        return TIKU_KITS_NET_ERR_MQTT_INFLIGHT;
    }

    return mqtt_send_unsubscribe(topic, packet_id);
}

void
tiku_kits_net_mqtt_periodic(void)
{
    tiku_clock_time_t now = tiku_clock_time();

    /* CONNACK timeout (applies to TCP/TLS connecting too) */
    if (mqtt_ctx.state ==
        TIKU_KITS_NET_MQTT_STATE_CONNECTING ||
        mqtt_ctx.state ==
        TIKU_KITS_NET_MQTT_STATE_TCP_CONNECTING ||
        mqtt_ctx.state ==
        TIKU_KITS_NET_MQTT_STATE_TLS_CONNECTING) {
        tiku_clock_time_t elapsed =
            now - mqtt_ctx.connect_time;
        tiku_clock_time_t timeout =
            (tiku_clock_time_t)
            TIKU_KITS_NET_MQTT_CONNECT_TIMEOUT *
            TIKU_CLOCK_SECOND;
        if (elapsed >= timeout) {
            mqtt_abort_transport();
            mqtt_ctx.state =
                TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
            if (mqtt_ctx.event_cb) {
                mqtt_ctx.event_cb(
                    TIKU_KITS_NET_MQTT_EVT_ERROR);
            }
            return;
        }
    }

    /* Keepalive (only when connected) */
    if (mqtt_ctx.state !=
        TIKU_KITS_NET_MQTT_STATE_CONNECTED) {
        return;
    }

#if TIKU_KITS_NET_MQTT_KEEPALIVE_SEC > 0
    if (mqtt_ctx.ping_pending) {
        /* Waiting for PINGRESP */
        tiku_clock_time_t since_ping =
            now - mqtt_ctx.ping_send_time;
        tiku_clock_time_t ping_timeout =
            (tiku_clock_time_t)
            TIKU_KITS_NET_MQTT_PING_TIMEOUT_SEC *
            TIKU_CLOCK_SECOND;
        if (since_ping >= ping_timeout) {
            /* Server unresponsive -- abort */
            mqtt_abort_transport();
            mqtt_ctx.state =
                TIKU_KITS_NET_MQTT_STATE_DISCONNECTED;
            if (mqtt_ctx.event_cb) {
                mqtt_ctx.event_cb(
                    TIKU_KITS_NET_MQTT_EVT_DISCONNECTED);
            }
        }
    } else {
        /* Check if keepalive interval elapsed */
        tiku_clock_time_t since_send =
            now - mqtt_ctx.last_send_time;
        tiku_clock_time_t ka_ticks =
            (tiku_clock_time_t)
            TIKU_KITS_NET_MQTT_KEEPALIVE_SEC *
            TIKU_CLOCK_SECOND;
        if (since_send >= ka_ticks) {
            mqtt_send_pingreq();
            mqtt_ctx.ping_pending = 1;
            mqtt_ctx.ping_send_time = tiku_clock_time();
        }
    }
#endif
}

uint8_t
tiku_kits_net_mqtt_is_connected(void)
{
    return (mqtt_ctx.state ==
            TIKU_KITS_NET_MQTT_STATE_CONNECTED) ? 1 : 0;
}

uint8_t
tiku_kits_net_mqtt_get_state(void)
{
    return mqtt_ctx.state;
}

uint8_t
tiku_kits_net_mqtt_get_connack_rc(void)
{
    return mqtt_ctx.connack_rc;
}

uint8_t
tiku_kits_net_mqtt_get_suback_rc(void)
{
    return mqtt_ctx.suback_rc;
}

#endif /* TIKU_KITS_NET_MQTT_ENABLE */
