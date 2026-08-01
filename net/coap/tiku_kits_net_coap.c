/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_coap.c - CoAP client/server (RFC 7252)
 *
 * Poll-based CoAP implementation that runs on top of the UDP layer.
 * The UDP receive callback copies incoming packets into static state;
 * the application calls coap_poll() from its own context to parse
 * messages, dispatch to resource handlers, match responses to
 * outstanding requests, and handle CON retransmission.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_net_coap.h"
#include <tikukits/net/ipv4/tiku_kits_net_udp.h>
#include <kernel/timers/tiku_timer.h>
#include <kernel/timers/tiku_clock.h>
#include <string.h>

#ifndef TIKU_KITS_NET_COAP_ACK_TIMEOUT
#define TIKU_KITS_NET_COAP_ACK_TIMEOUT     (TIKU_CLOCK_SECOND * 2)
#endif

/*---------------------------------------------------------------------------*/
/* COMPILE-TIME PAYLOAD LIMIT                                                */
/*---------------------------------------------------------------------------*/

/** Maximum CoAP message size (matches UDP max payload). */
#define COAP_BUF_SIZE  (TIKU_KITS_NET_MTU - 20 - 8)

/*---------------------------------------------------------------------------*/
/* STATIC STATE -- RECEIVE BUFFER                                            */
/*---------------------------------------------------------------------------*/

static uint8_t  rx_buf[COAP_BUF_SIZE];
static uint16_t rx_len;
static uint8_t  rx_src_addr[4];
static uint16_t rx_src_port;
static volatile uint8_t rx_pending;

/*---------------------------------------------------------------------------*/
/* STATIC STATE -- OUTGOING CON TRANSACTION                                  */
/*---------------------------------------------------------------------------*/

static uint8_t  tx_buf[COAP_BUF_SIZE];
static uint16_t tx_len;

static struct {
    uint8_t  active;
    uint8_t  dst_addr[4];
    uint16_t dst_port;
    uint16_t msg_id;
    uint8_t  token[TIKU_KITS_NET_COAP_MAX_TOKEN_LEN];
    uint8_t  token_len;
    uint8_t  retries;
    tiku_clock_time_t timeout;
    tiku_clock_time_t next_tick;
    tiku_kits_net_coap_response_cb_t cb;
} con_tx;

/*---------------------------------------------------------------------------*/
/* STATIC STATE -- RESOURCE TABLE                                            */
/*---------------------------------------------------------------------------*/

static struct {
    const char                       *path;
    tiku_kits_net_coap_resource_cb_t  handler;
} resources[TIKU_KITS_NET_COAP_MAX_RESOURCES];

/*---------------------------------------------------------------------------*/
/* STATIC STATE -- COUNTERS                                                  */
/*---------------------------------------------------------------------------*/

static uint16_t msg_id_counter;
static uint16_t token_counter;

/*---------------------------------------------------------------------------*/
/* STATIC STATE -- DEDUP (last CON request from a client)                    */
/*---------------------------------------------------------------------------*/

static struct {
    uint16_t msg_id;
    uint8_t  src_addr[4];
    uint8_t  valid;
} dedup;

/*---------------------------------------------------------------------------*/
/* OPTION CODEC HELPERS                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encode one CoAP option into a buffer.
 *
 * Writes the delta/length header byte(s) and the value.
 * Returns the number of bytes written, or 0 if it would overflow.
 */
static uint16_t
encode_option(uint8_t *buf, uint16_t buf_size, uint16_t pos,
              uint16_t delta, const uint8_t *val, uint16_t val_len)
{
    uint16_t hdr_len = 1;
    uint8_t  d_nibble, l_nibble;
    uint16_t need;

    /* Compute delta nibble + extended bytes */
    if (delta < 13) {
        d_nibble = (uint8_t)delta;
    } else if (delta < 269) {
        d_nibble = 13;
        hdr_len++;
    } else {
        d_nibble = 14;
        hdr_len += 2;
    }

    /* Compute length nibble + extended bytes */
    if (val_len < 13) {
        l_nibble = (uint8_t)val_len;
    } else if (val_len < 269) {
        l_nibble = 13;
        hdr_len++;
    } else {
        l_nibble = 14;
        hdr_len += 2;
    }

    need = hdr_len + val_len;
    if (pos + need > buf_size) {
        return 0;
    }

    /* Header byte */
    buf[pos++] = (uint8_t)((d_nibble << 4) | l_nibble);

    /* Extended delta */
    if (d_nibble == 13) {
        buf[pos++] = (uint8_t)(delta - 13);
    } else if (d_nibble == 14) {
        buf[pos++] = (uint8_t)((delta - 269) >> 8);
        buf[pos++] = (uint8_t)((delta - 269) & 0xFF);
    }

    /* Extended length */
    if (l_nibble == 13) {
        buf[pos++] = (uint8_t)(val_len - 13);
    } else if (l_nibble == 14) {
        buf[pos++] = (uint8_t)((val_len - 269) >> 8);
        buf[pos++] = (uint8_t)((val_len - 269) & 0xFF);
    }

    /* Value */
    if (val_len > 0) {
        memcpy(buf + pos, val, val_len);
    }

    return need;
}

/**
 * @brief Encode a uint16 value as a variable-length CoAP integer.
 *
 * CoAP integers are 0, 1, or 2 bytes.  Returns the byte count.
 */
static uint16_t
encode_uint(uint8_t *out, uint16_t val)
{
    if (val == 0) {
        return 0;
    }
    if (val <= 0xFF) {
        out[0] = (uint8_t)val;
        return 1;
    }
    out[0] = (uint8_t)(val >> 8);
    out[1] = (uint8_t)(val & 0xFF);
    return 2;
}

/*---------------------------------------------------------------------------*/
/* MESSAGE PARSER                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parse a CoAP message from raw bytes into a msg struct.
 *
 * @return 1 on success, 0 on parse error.
 */
static uint8_t
coap_parse(const uint8_t *buf, uint16_t len,
           tiku_kits_net_coap_msg_t *msg)
{
    uint16_t pos;
    uint8_t  tkl;
    uint16_t opt_num = 0;
    uint16_t uri_pos = 0;

    memset(msg, 0, sizeof(*msg));
    msg->content_format = -1;

    if (len < TIKU_KITS_NET_COAP_HDR_LEN) {
        return 0;
    }

    /* Byte 0: VV TT TTTT (version, type, token length) */
    if ((buf[0] >> 6) != TIKU_KITS_NET_COAP_VERSION) {
        return 0;
    }
    msg->type = (buf[0] >> 4) & 0x03;
    tkl = buf[0] & 0x0F;
    if (tkl > 8) {
        return 0;
    }

    msg->code   = buf[1];
    msg->msg_id = (uint16_t)((uint16_t)buf[2] << 8 | buf[3]);

    pos = TIKU_KITS_NET_COAP_HDR_LEN;

    /* Token */
    if (pos + tkl > len) {
        return 0;
    }
    msg->token_len = (tkl <= TIKU_KITS_NET_COAP_MAX_TOKEN_LEN)
                     ? tkl : TIKU_KITS_NET_COAP_MAX_TOKEN_LEN;
    memcpy(msg->token, buf + pos, msg->token_len);
    pos += tkl;

    /* Options */
    while (pos < len && buf[pos] != TIKU_KITS_NET_COAP_PAYLOAD_MARKER) {
        uint16_t delta, opt_len;
        uint8_t  hdr = buf[pos++];

        delta   = (hdr >> 4) & 0x0F;
        opt_len = hdr & 0x0F;

        /* Extended delta */
        if (delta == 13) {
            if (pos >= len) { return 0; }
            delta = (uint16_t)buf[pos++] + 13;
        } else if (delta == 14) {
            if (pos + 1 >= len) { return 0; }
            delta = (uint16_t)(
                (uint16_t)buf[pos] << 8 | buf[pos + 1]) + 269;
            pos += 2;
        } else if (delta == 15) {
            return 0;  /* reserved */
        }

        /* Extended length */
        if (opt_len == 13) {
            if (pos >= len) { return 0; }
            opt_len = (uint16_t)buf[pos++] + 13;
        } else if (opt_len == 14) {
            if (pos + 1 >= len) { return 0; }
            opt_len = (uint16_t)(
                (uint16_t)buf[pos] << 8 | buf[pos + 1]) + 269;
            pos += 2;
        } else if (opt_len == 15) {
            return 0;
        }

        if (pos + opt_len > len) {
            return 0;
        }

        opt_num += delta;

        switch (opt_num) {
        case TIKU_KITS_NET_COAP_OPT_URI_PATH:
            /* Append "/segment" to uri_path */
            if (uri_pos > 0 &&
                uri_pos < TIKU_KITS_NET_COAP_MAX_URI_PATH) {
                msg->uri_path[uri_pos++] = '/';
            }
            if (uri_pos == 0 &&
                uri_pos < TIKU_KITS_NET_COAP_MAX_URI_PATH) {
                msg->uri_path[uri_pos++] = '/';
            }
            {
                uint16_t copy = opt_len;
                if (uri_pos + copy >
                    TIKU_KITS_NET_COAP_MAX_URI_PATH) {
                    copy = TIKU_KITS_NET_COAP_MAX_URI_PATH
                           - uri_pos;
                }
                memcpy(msg->uri_path + uri_pos,
                       buf + pos, copy);
                uri_pos += copy;
            }
            break;

        case TIKU_KITS_NET_COAP_OPT_CONTENT_FMT:
            if (opt_len == 0) {
                msg->content_format = 0;
            } else if (opt_len == 1) {
                msg->content_format = (int16_t)buf[pos];
            } else if (opt_len == 2) {
                msg->content_format = (int16_t)(
                    (uint16_t)buf[pos] << 8 | buf[pos + 1]);
            }
            break;

        default:
            /* Skip unknown options */
            break;
        }

        pos += opt_len;
    }

    msg->uri_path[uri_pos] = '\0';

    /* Payload */
    if (pos < len && buf[pos] == TIKU_KITS_NET_COAP_PAYLOAD_MARKER) {
        pos++;
        if (pos < len) {
            msg->payload     = buf + pos;
            msg->payload_len = len - pos;
        }
    }

    return 1;
}

/*---------------------------------------------------------------------------*/
/* MESSAGE BUILDER                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build a CoAP message into a buffer.
 *
 * @param buf       Output buffer
 * @param buf_size  Buffer capacity
 * @param type      Message type (CON/NON/ACK/RST)
 * @param code      Method or response code
 * @param msg_id    Message ID
 * @param token     Token bytes (may be NULL if tkl == 0)
 * @param tkl       Token length
 * @param uri_path  URI path (NULL to omit, e.g. for ACK)
 * @param cfmt      Content-Format (-1 to omit)
 * @param payload   Payload bytes (NULL if none)
 * @param plen      Payload length
 * @return Total message length, or 0 on overflow.
 */
static uint16_t
coap_build(uint8_t *buf, uint16_t buf_size,
           uint8_t type, uint8_t code, uint16_t msg_id,
           const uint8_t *token, uint8_t tkl,
           const char *uri_path, int16_t cfmt,
           const uint8_t *payload, uint16_t plen)
{
    uint16_t pos;
    uint16_t prev_opt = 0;
    uint16_t n;

    if (buf_size < TIKU_KITS_NET_COAP_HDR_LEN + tkl) {
        return 0;
    }

    /* Header */
    buf[0] = (uint8_t)(
        (TIKU_KITS_NET_COAP_VERSION << 6) |
        ((type & 0x03) << 4) |
        (tkl & 0x0F));
    buf[1] = code;
    buf[2] = (uint8_t)(msg_id >> 8);
    buf[3] = (uint8_t)(msg_id & 0xFF);

    pos = TIKU_KITS_NET_COAP_HDR_LEN;

    /* Token */
    if (tkl > 0 && token != (void *)0) {
        memcpy(buf + pos, token, tkl);
        pos += tkl;
    }

    /* Uri-Path options (split on '/') */
    if (uri_path != (void *)0 && uri_path[0] != '\0') {
        const char *p = uri_path;
        if (*p == '/') {
            p++;
        }
        while (*p != '\0') {
            const char *seg = p;
            uint16_t seg_len;

            while (*p != '\0' && *p != '/') {
                p++;
            }
            seg_len = (uint16_t)(p - seg);
            if (seg_len > 0) {
                n = encode_option(buf, buf_size, pos,
                                  TIKU_KITS_NET_COAP_OPT_URI_PATH
                                  - prev_opt,
                                  (const uint8_t *)seg, seg_len);
                if (n == 0) {
                    return 0;
                }
                pos += n;
                prev_opt = TIKU_KITS_NET_COAP_OPT_URI_PATH;
            }
            if (*p == '/') {
                p++;
            }
        }
    }

    /* Content-Format option */
    if (cfmt >= 0) {
        uint8_t  cfmt_buf[2];
        uint16_t cfmt_len;

        cfmt_len = encode_uint(cfmt_buf, (uint16_t)cfmt);
        n = encode_option(buf, buf_size, pos,
                          TIKU_KITS_NET_COAP_OPT_CONTENT_FMT
                          - prev_opt,
                          cfmt_buf, cfmt_len);
        if (n == 0) {
            return 0;
        }
        pos += n;
        prev_opt = TIKU_KITS_NET_COAP_OPT_CONTENT_FMT;
    }

    /* Payload marker + payload */
    if (plen > 0 && payload != (void *)0) {
        if (pos + 1 + plen > buf_size) {
            return 0;
        }
        buf[pos++] = TIKU_KITS_NET_COAP_PAYLOAD_MARKER;
        memcpy(buf + pos, payload, plen);
        pos += plen;
    }

    return pos;
}

/*---------------------------------------------------------------------------*/
/* UDP RECEIVE CALLBACK                                                      */
/*---------------------------------------------------------------------------*/

static void
coap_recv_cb(const uint8_t *src_addr, uint16_t src_port,
             const uint8_t *payload, uint16_t payload_len)
{
    if (rx_pending) {
        return;  /* previous message not yet consumed */
    }
    if (payload_len == 0 || payload_len > COAP_BUF_SIZE) {
        return;
    }

    memcpy(rx_buf, payload, payload_len);
    rx_len = payload_len;
    memcpy(rx_src_addr, src_addr, 4);
    rx_src_port = src_port;
    rx_pending = 1;
}

/*---------------------------------------------------------------------------*/
/* INTERNAL SEND HELPERS                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send a pre-built CoAP message via UDP.
 */
static int8_t
coap_send(const uint8_t *dst_addr, uint16_t dst_port,
          const uint8_t *msg_buf, uint16_t msg_len)
{
    return tiku_kits_net_udp_send(
        dst_addr, dst_port,
        TIKU_KITS_NET_COAP_PORT,
        msg_buf, msg_len);
}

/**
 * @brief Send an empty ACK for a given message ID.
 */
static void
send_empty_ack(const uint8_t *dst_addr, uint16_t dst_port,
               uint16_t mid)
{
    uint8_t ack[TIKU_KITS_NET_COAP_HDR_LEN];

    ack[0] = (uint8_t)(
        (TIKU_KITS_NET_COAP_VERSION << 6) |
        (TIKU_KITS_NET_COAP_TYPE_ACK << 4));
    ack[1] = TIKU_KITS_NET_COAP_CODE_EMPTY;
    ack[2] = (uint8_t)(mid >> 8);
    ack[3] = (uint8_t)(mid & 0xFF);

    coap_send(dst_addr, dst_port, ack, sizeof(ack));
}

/**
 * @brief Send a RST for a given message ID.
 */
static void
send_rst(const uint8_t *dst_addr, uint16_t dst_port,
         uint16_t mid)
{
    uint8_t rst[TIKU_KITS_NET_COAP_HDR_LEN];

    rst[0] = (uint8_t)(
        (TIKU_KITS_NET_COAP_VERSION << 6) |
        (TIKU_KITS_NET_COAP_TYPE_RST << 4));
    rst[1] = TIKU_KITS_NET_COAP_CODE_EMPTY;
    rst[2] = (uint8_t)(mid >> 8);
    rst[3] = (uint8_t)(mid & 0xFF);

    coap_send(dst_addr, dst_port, rst, sizeof(rst));
}

/**
 * @brief Common implementation for all client request methods.
 */
static int8_t
send_request(const uint8_t *dst_addr, uint16_t dst_port,
             uint8_t method, const char *uri_path,
             uint8_t type,
             const uint8_t *payload, uint16_t plen,
             int16_t cfmt,
             tiku_kits_net_coap_response_cb_t cb)
{
    uint8_t  msg_buf[COAP_BUF_SIZE];
    uint16_t msg_len;
    uint16_t mid;
    uint8_t  token[2];
    uint8_t  tkl = 2;
    int8_t   rc;

    if (dst_addr == (void *)0 || uri_path == (void *)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }
    if (type != TIKU_KITS_NET_COAP_TYPE_CON &&
        type != TIKU_KITS_NET_COAP_TYPE_NON) {
        return TIKU_KITS_NET_ERR_PARAM;
    }
    if (type == TIKU_KITS_NET_COAP_TYPE_CON && con_tx.active) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Generate message ID and token */
    mid = msg_id_counter++;
    token_counter++;
    token[0] = (uint8_t)(token_counter >> 8);
    token[1] = (uint8_t)(token_counter & 0xFF);

    /* Build the message */
    msg_len = coap_build(msg_buf, COAP_BUF_SIZE,
                         type, method, mid,
                         token, tkl,
                         uri_path, cfmt,
                         payload, plen);
    if (msg_len == 0) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    /* Send */
    rc = coap_send(dst_addr, dst_port, msg_buf, msg_len);
    if (rc != TIKU_KITS_NET_OK) {
        return rc;
    }

    /* Track CON for retransmission */
    if (type == TIKU_KITS_NET_COAP_TYPE_CON) {
        memcpy(tx_buf, msg_buf, msg_len);
        tx_len = msg_len;
        con_tx.active    = 1;
        memcpy(con_tx.dst_addr, dst_addr, 4);
        con_tx.dst_port  = dst_port;
        con_tx.msg_id    = mid;
        memcpy(con_tx.token, token, tkl);
        con_tx.token_len = tkl;
        con_tx.retries   = 0;
        con_tx.timeout   = TIKU_KITS_NET_COAP_ACK_TIMEOUT;
        /* First retransmit at 1.5x timeout (RFC 7252 random factor
         * approximation without RNG) */
        con_tx.next_tick = tiku_clock_time()
                           + con_tx.timeout
                           + (con_tx.timeout >> 1);
        con_tx.cb        = cb;
    }

    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_coap_init(void)
{
    uint8_t i;

    tiku_kits_net_udp_unbind(TIKU_KITS_NET_COAP_PORT);

    rx_pending     = 0;
    rx_len         = 0;
    tx_len         = 0;
    msg_id_counter = 1;
    token_counter  = 0;

    memset(&con_tx, 0, sizeof(con_tx));
    memset(&dedup, 0, sizeof(dedup));

    for (i = 0; i < TIKU_KITS_NET_COAP_MAX_RESOURCES; i++) {
        resources[i].path    = (void *)0;
        resources[i].handler = (tiku_kits_net_coap_resource_cb_t)0;
    }

    return tiku_kits_net_udp_bind(TIKU_KITS_NET_COAP_PORT,
                                   coap_recv_cb);
}

/*---------------------------------------------------------------------------*/
/* SERVER API                                                                */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_coap_resource_register(
    const char                       *path,
    tiku_kits_net_coap_resource_cb_t  handler)
{
    uint8_t i;
    int8_t  free_slot = -1;

    if (path == (void *)0 || handler == (void *)0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    for (i = 0; i < TIKU_KITS_NET_COAP_MAX_RESOURCES; i++) {
        if (resources[i].path != (void *)0 &&
            strcmp(resources[i].path, path) == 0) {
            return TIKU_KITS_NET_ERR_PARAM;  /* duplicate */
        }
        if (resources[i].path == (void *)0 && free_slot < 0) {
            free_slot = (int8_t)i;
        }
    }

    if (free_slot < 0) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    resources[free_slot].path    = path;
    resources[free_slot].handler = handler;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_coap_resource_unregister(const char *path)
{
    uint8_t i;

    if (path == (void *)0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    for (i = 0; i < TIKU_KITS_NET_COAP_MAX_RESOURCES; i++) {
        if (resources[i].path != (void *)0 &&
            strcmp(resources[i].path, path) == 0) {
            resources[i].path    = (void *)0;
            resources[i].handler = (tiku_kits_net_coap_resource_cb_t)0;
            return TIKU_KITS_NET_OK;
        }
    }
    return TIKU_KITS_NET_ERR_PARAM;
}

/*---------------------------------------------------------------------------*/
/* CLIENT API                                                                */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_coap_get(
    const uint8_t *dst_addr, uint16_t dst_port,
    const char *uri_path, uint8_t type,
    tiku_kits_net_coap_response_cb_t cb)
{
    return send_request(dst_addr, dst_port,
                        TIKU_KITS_NET_COAP_METHOD_GET,
                        uri_path, type,
                        (void *)0, 0, -1, cb);
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_coap_put(
    const uint8_t *dst_addr, uint16_t dst_port,
    const char *uri_path, uint8_t type,
    const uint8_t *payload, uint16_t payload_len,
    int16_t content_fmt,
    tiku_kits_net_coap_response_cb_t cb)
{
    return send_request(dst_addr, dst_port,
                        TIKU_KITS_NET_COAP_METHOD_PUT,
                        uri_path, type,
                        payload, payload_len, content_fmt, cb);
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_coap_post(
    const uint8_t *dst_addr, uint16_t dst_port,
    const char *uri_path, uint8_t type,
    const uint8_t *payload, uint16_t payload_len,
    int16_t content_fmt,
    tiku_kits_net_coap_response_cb_t cb)
{
    return send_request(dst_addr, dst_port,
                        TIKU_KITS_NET_COAP_METHOD_POST,
                        uri_path, type,
                        payload, payload_len, content_fmt, cb);
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_coap_delete(
    const uint8_t *dst_addr, uint16_t dst_port,
    const char *uri_path, uint8_t type,
    tiku_kits_net_coap_response_cb_t cb)
{
    return send_request(dst_addr, dst_port,
                        TIKU_KITS_NET_COAP_METHOD_DELETE,
                        uri_path, type,
                        (void *)0, 0, -1, cb);
}

/*---------------------------------------------------------------------------*/
/* INTERNAL: TOKEN MATCHING                                                  */
/*---------------------------------------------------------------------------*/

static uint8_t
token_matches(const tiku_kits_net_coap_msg_t *msg)
{
    if (msg->token_len != con_tx.token_len) {
        return 0;
    }
    return (memcmp(msg->token, con_tx.token,
                   con_tx.token_len) == 0) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
/* INTERNAL: RESOURCE DISPATCH                                               */
/*---------------------------------------------------------------------------*/

static void
handle_request(const tiku_kits_net_coap_msg_t *msg)
{
    uint8_t i;
    tiku_kits_net_coap_resp_t resp;
    uint8_t  resp_buf[COAP_BUF_SIZE];
    uint16_t resp_len;
    uint8_t  resp_type;

    /* Dedup: reject duplicate CON with same msg_id + src */
    if (msg->type == TIKU_KITS_NET_COAP_TYPE_CON) {
        if (dedup.valid &&
            dedup.msg_id == msg->msg_id &&
            memcmp(dedup.src_addr, msg->src_addr, 4) == 0) {
            return;  /* duplicate, ignore (response already sent) */
        }
        dedup.msg_id = msg->msg_id;
        memcpy(dedup.src_addr, msg->src_addr, 4);
        dedup.valid = 1;
    }

    /* Find matching resource */
    for (i = 0; i < TIKU_KITS_NET_COAP_MAX_RESOURCES; i++) {
        if (resources[i].path != (void *)0 &&
            strcmp(resources[i].path, msg->uri_path) == 0) {
            break;
        }
    }

    if (i >= TIKU_KITS_NET_COAP_MAX_RESOURCES) {
        /* No matching resource */
        if (msg->type == TIKU_KITS_NET_COAP_TYPE_CON) {
            send_rst(msg->src_addr, msg->src_port, msg->msg_id);
        }
        return;
    }

    /* Call handler */
    memset(&resp, 0, sizeof(resp));
    resp.content_format = -1;
    resources[i].handler(msg, &resp);

    /* Build response */
    resp_type = (msg->type == TIKU_KITS_NET_COAP_TYPE_CON)
                ? TIKU_KITS_NET_COAP_TYPE_ACK
                : TIKU_KITS_NET_COAP_TYPE_NON;

    resp_len = coap_build(
        resp_buf, COAP_BUF_SIZE,
        resp_type, resp.code,
        /* ACK reuses request msg_id; NON gets a new one */
        (resp_type == TIKU_KITS_NET_COAP_TYPE_ACK)
            ? msg->msg_id : msg_id_counter++,
        msg->token, msg->token_len,
        (void *)0,   /* no URI in responses */
        resp.content_format,
        resp.payload, resp.payload_len);

    if (resp_len > 0) {
        coap_send(msg->src_addr, msg->src_port,
                  resp_buf, resp_len);
    }
}

/*---------------------------------------------------------------------------*/
/* POLLING                                                                   */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_coap_poll(void)
{
    tiku_kits_net_coap_msg_t msg;
    tiku_clock_time_t now;

    /* ---- Process received message ---- */
    if (rx_pending) {
        rx_pending = 0;

        if (!coap_parse(rx_buf, rx_len, &msg)) {
            goto retransmit_check;
        }
        memcpy(msg.src_addr, rx_src_addr, 4);
        msg.src_port = rx_src_port;

        if (msg.type == TIKU_KITS_NET_COAP_TYPE_ACK ||
            msg.type == TIKU_KITS_NET_COAP_TYPE_RST) {
            /* Match to outstanding CON by message ID */
            if (con_tx.active &&
                msg.msg_id == con_tx.msg_id) {
                if (msg.type == TIKU_KITS_NET_COAP_TYPE_ACK &&
                    msg.code != TIKU_KITS_NET_COAP_CODE_EMPTY) {
                    /* Piggybacked response */
                    if (con_tx.cb != (void *)0) {
                        con_tx.cb(&msg);
                    }
                }
                /* RST or empty ACK: transaction done */
                con_tx.active = 0;
            }
        } else if ((msg.code >> 5) >= 2) {
            /* Separate response (class 2-5), matched by token */
            if (con_tx.active && token_matches(&msg)) {
                if (msg.type == TIKU_KITS_NET_COAP_TYPE_CON) {
                    send_empty_ack(msg.src_addr, msg.src_port,
                                   msg.msg_id);
                }
                if (con_tx.cb != (void *)0) {
                    con_tx.cb(&msg);
                }
                con_tx.active = 0;
            }
        } else if ((msg.code >> 5) == 0 && msg.code != 0) {
            /* Request (class 0, code 0.01-0.04) */
            handle_request(&msg);
        }
    }

retransmit_check:

    /* ---- CON retransmit timer ---- */
    if (!con_tx.active) {
        return;
    }

    now = tiku_clock_time();
    if (!TIKU_CLOCK_LT(now, con_tx.next_tick)) {
        if (con_tx.retries < TIKU_KITS_NET_COAP_MAX_RETRANSMIT) {
            /* Retransmit */
            coap_send(con_tx.dst_addr, con_tx.dst_port,
                      tx_buf, tx_len);
            con_tx.retries++;
            con_tx.timeout *= 2;  /* exponential backoff */
            con_tx.next_tick = now + con_tx.timeout;
        } else {
            /* Timeout -- notify caller with NULL */
            if (con_tx.cb != (void *)0) {
                con_tx.cb((void *)0);
            }
            con_tx.active = 0;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* SHUTDOWN                                                                  */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_coap_shutdown(void)
{
    tiku_kits_net_udp_unbind(TIKU_KITS_NET_COAP_PORT);
    con_tx.active = 0;
    rx_pending    = 0;
}
