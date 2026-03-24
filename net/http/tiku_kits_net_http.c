/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_http.c - Minimal HTTP/1.1 client
 *
 * Implements the HTTP/1.1 request builder, response parser,
 * and blocking client that drives DNS, TCP, and TLS to
 * completion.  Designed for ultra-low-power MCUs with static
 * allocation only.
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

#include "tiku_kits_net_http.h"
#include <kernel/memory/tiku_mem.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* STATUS-LINE SUB-STATES                                                    */
/*---------------------------------------------------------------------------*/

/** Skip "HTTP/1.x" portion until the first space. */
#define HTTP_SLINE_VERSION  0

/** Parse the three status code digits. */
#define HTTP_SLINE_CODE     1

/** Skip the reason phrase until line feed. */
#define HTTP_SLINE_REASON   2

/*---------------------------------------------------------------------------*/
/* STRING HELPERS (no libc dependency)                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append a C string to a byte buffer.
 *
 * Copies characters from @p s into @p buf starting at position
 * @p pos, stopping when the string ends or @p max is reached.
 *
 * @return New write position.
 */
static uint16_t
http_append(uint8_t *buf, uint16_t pos, uint16_t max,
            const char *s)
{
    while (*s != '\0' && pos < max) {
        buf[pos++] = (uint8_t)*s++;
    }
    return pos;
}

/**
 * @brief Convert a uint16_t to a decimal string.
 *
 * Writes at most 5 digits plus a NUL terminator into @p buf.
 *
 * @return Number of digit characters written.
 */
static uint16_t
http_u16_to_str(uint16_t val, char *buf)
{
    char tmp[6];
    uint8_t i = 0;
    uint8_t len;

    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }
    while (val > 0) {
        tmp[i++] = '0' + (uint8_t)(val % 10);
        val /= 10;
    }
    len = i;
    while (i > 0) {
        buf[len - i] = tmp[i - 1];
        i--;
    }
    buf[len] = '\0';
    return len;
}

/*---------------------------------------------------------------------------*/
/* RESPONSE PARSER                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the HTTP response parser to its initial state.
 *
 * Sets the output buffer, zeroes counters, and positions the
 * state machine at the start of the HTTP status line.
 */
void
tiku_kits_net_http_parser_init(
    tiku_kits_net_http_parser_t *p,
    uint8_t *body_buf,
    uint16_t body_max)
{
    p->body_buf    = body_buf;
    p->body_max    = body_max;
    p->body_len    = 0;
    p->status_code = 0;
    p->state       = TIKU_KITS_NET_HTTP_PARSE_STATUS;
    p->sub_state   = HTTP_SLINE_VERSION;
    p->hdr_end_seq = 0;
}

/**
 * @brief Feed raw HTTP response bytes through the parser.
 *
 * Processes each byte through a three-phase state machine:
 *   1. STATUS -- skips "HTTP/1.x ", extracts the 3-digit status
 *      code, then skips the reason phrase until \\n.
 *   2. HEADERS -- counts \\r\\n sequences; when \\r\\n\\r\\n is
 *      detected (the empty line after headers), transitions to BODY.
 *   3. BODY -- copies bytes into the caller's body_buf up to
 *      body_max capacity.
 *
 * The parser is incremental: it can be called repeatedly with
 * small chunks as data arrives from the TLS layer.
 */
void
tiku_kits_net_http_parser_feed(
    tiku_kits_net_http_parser_t *p,
    const uint8_t *data,
    uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++) {
        uint8_t b = data[i];

        switch (p->state) {
        /*-----------------------------------------------------------*/
        /* STATUS LINE: "HTTP/1.x NNN reason\r\n"                    */
        /*-----------------------------------------------------------*/
        case TIKU_KITS_NET_HTTP_PARSE_STATUS:
            if (p->sub_state == HTTP_SLINE_VERSION) {
                if (b == ' ') {
                    p->sub_state = HTTP_SLINE_CODE;
                }
            } else if (p->sub_state == HTTP_SLINE_CODE) {
                if (b >= '0' && b <= '9') {
                    p->status_code =
                        p->status_code * 10 + (b - '0');
                } else {
                    p->sub_state = HTTP_SLINE_REASON;
                }
            }
            if (b == '\n') {
                p->state = TIKU_KITS_NET_HTTP_PARSE_HEADERS;
                p->hdr_end_seq = 2; /* status line \r\n counted */
            }
            break;

        /*-----------------------------------------------------------*/
        /* HEADERS: skip until \r\n\r\n                              */
        /*-----------------------------------------------------------*/
        case TIKU_KITS_NET_HTTP_PARSE_HEADERS:
            if (b == '\r') {
                if (p->hdr_end_seq == 0
                    || p->hdr_end_seq == 2) {
                    p->hdr_end_seq++;
                } else {
                    p->hdr_end_seq = 1;
                }
            } else if (b == '\n') {
                if (p->hdr_end_seq == 1) {
                    p->hdr_end_seq = 2;
                } else if (p->hdr_end_seq == 3) {
                    p->state =
                        TIKU_KITS_NET_HTTP_PARSE_BODY;
                } else {
                    p->hdr_end_seq = 0;
                }
            } else {
                p->hdr_end_seq = 0;
            }
            break;

        /*-----------------------------------------------------------*/
        /* BODY: copy to caller's buffer                             */
        /*-----------------------------------------------------------*/
        case TIKU_KITS_NET_HTTP_PARSE_BODY:
            if (p->body_len < p->body_max) {
                p->body_buf[p->body_len++] = b;
            }
            break;

        case TIKU_KITS_NET_HTTP_PARSE_DONE:
            break;
        }
    }
}

/**
 * @brief Check if the parser has finished processing the response.
 *
 * Returns 1 when the parser state has reached PARSE_DONE (the TLS
 * connection signalled end-of-data), 0 otherwise.
 */
uint8_t
tiku_kits_net_http_parser_done(
    const tiku_kits_net_http_parser_t *p)
{
    return (p->state == TIKU_KITS_NET_HTTP_PARSE_DONE) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
/* REQUEST BUILDER                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Format an HTTP/1.1 request line and headers into a buffer.
 *
 * Assembles the request line ("METHOD path HTTP/1.1\\r\\n") followed
 * by Host, Content-Type (if body_len > 0), x-api-key + anthropic-
 * version (if api_key != NULL), Content-Length (if body_len > 0),
 * Connection: close, and the final blank line.  The body is NOT
 * included -- the caller sends it separately after the headers.
 *
 * Returns the number of bytes written.  If the buffer is too small,
 * the output is truncated and the return value equals buf_max.
 */
uint16_t
tiku_kits_net_http_build_request(
    const char *method,
    const char *host,
    const char *path,
    const char *api_key,
    uint16_t body_len,
    uint8_t *buf,
    uint16_t buf_max)
{
    uint16_t p = 0;
    char len_str[6];

    /* Request line: METHOD path HTTP/1.1\r\n */
    p = http_append(buf, p, buf_max, method);
    p = http_append(buf, p, buf_max, " ");
    p = http_append(buf, p, buf_max, path);
    p = http_append(buf, p, buf_max, " HTTP/1.1\r\n");

    /* Host header */
    p = http_append(buf, p, buf_max, "Host: ");
    p = http_append(buf, p, buf_max, host);
    p = http_append(buf, p, buf_max, "\r\n");

    /* Content-Type (POST only, indicated by body_len > 0) */
    if (body_len > 0) {
        p = http_append(buf, p, buf_max,
            "Content-Type: application/json\r\n");
    }

    /* API key and Anthropic version */
    if (api_key != NULL) {
        p = http_append(buf, p, buf_max, "x-api-key: ");
        p = http_append(buf, p, buf_max, api_key);
        p = http_append(buf, p, buf_max, "\r\n");
        p = http_append(buf, p, buf_max,
            "anthropic-version: 2023-06-01\r\n");
    }

    /* Content-Length (POST only) */
    if (body_len > 0) {
        http_u16_to_str(body_len, len_str);
        p = http_append(buf, p, buf_max,
            "Content-Length: ");
        p = http_append(buf, p, buf_max, len_str);
        p = http_append(buf, p, buf_max, "\r\n");
    }

    /* Connection: close + blank line */
    p = http_append(buf, p, buf_max,
        "Connection: close\r\n\r\n");

    return p;
}

/*---------------------------------------------------------------------------*/
/* HTTP CLIENT (requires TLS + TCP)                                          */
/*---------------------------------------------------------------------------*/

#if TIKU_KITS_NET_HTTP_ENABLE

#include "../ipv4/tiku_kits_net_tcp.h"
#include "../ipv4/tiku_kits_net_dns.h"
#include "../ipv4/tiku_kits_net_ipv4.h"
#include "../slip/tiku_kits_net_slip.h"
#include <tikukits/crypto/tls/tiku_kits_crypto_tls.h>

/*---------------------------------------------------------------------------*/
/* FRAM-BACKED REQUEST BUFFER                                                */
/*---------------------------------------------------------------------------*/

/**
 * Holds the formatted HTTP request headers during send, then
 * is reused as a TLS read staging buffer during the response
 * phase.  Placed in FRAM to conserve SRAM.
 */
__attribute__((section(".persistent"), aligned(2)))
static uint8_t http_req_buf[TIKU_KITS_NET_HTTP_REQ_BUF_SIZE];

/*---------------------------------------------------------------------------*/
/* INTERNAL STATE                                                            */
/*---------------------------------------------------------------------------*/

static struct {
    tiku_kits_net_tcp_conn_t        *tcp;
    tiku_kits_crypto_tls_conn_t     *tls;
    tiku_kits_net_http_parser_t      parser;
    volatile uint8_t                 tcp_event;
    volatile uint8_t                 tls_event;
} http_ctx;

/*---------------------------------------------------------------------------*/
/* CALLBACKS                                                                 */
/*---------------------------------------------------------------------------*/

static void
http_tcp_recv_cb(tiku_kits_net_tcp_conn_t *conn,
                 uint16_t available)
{
    (void)conn;
    (void)available;
    /* TLS layer intercepts TCP data after tls_connect(). */
}

static void
http_tcp_event_cb(tiku_kits_net_tcp_conn_t *conn,
                  uint8_t event)
{
    (void)conn;
    http_ctx.tcp_event = event;
}

static void
http_tls_recv_cb(struct tiku_kits_crypto_tls_conn *conn,
                 uint16_t available)
{
    (void)conn;
    (void)available;
    /* Data is buffered by TLS; we drain via tls_read(). */
}

static void
http_tls_event_cb(struct tiku_kits_crypto_tls_conn *conn,
                  uint8_t event)
{
    (void)conn;
    http_ctx.tls_event = event;
}

/*---------------------------------------------------------------------------*/
/* NET STACK POLL                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Drain the SLIP link and process incoming IP frames.
 *
 * Equivalent to what the net process protothread does each
 * poll interval.  Called in a tight loop by the blocking HTTP
 * functions to advance TCP, TLS, and DNS state machines while
 * waiting for events.
 */
static void
http_net_poll(void)
{
    uint16_t buf_size;
    uint8_t *buf;
    uint16_t pos = 0;

    buf = tiku_kits_net_ipv4_get_buf(&buf_size);
    while (tiku_kits_net_slip_link.poll_rx(
               buf, buf_size, &pos)) {
        tiku_kits_net_ipv4_input(buf, pos);
        pos = 0;
    }
}

/*---------------------------------------------------------------------------*/
/* TLS SEND HELPER                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send all bytes through TLS in max-fragment chunks.
 *
 * Retries each fragment with net polling if TLS cannot accept
 * it immediately (e.g. TCP send window exhausted).
 */
static int8_t
http_tls_send_all(const uint8_t *data, uint16_t len)
{
    uint16_t sent = 0;
    uint16_t retries;

    while (sent < len) {
        uint16_t chunk = len - sent;
        if (chunk > TIKU_KITS_CRYPTO_TLS_MAX_FRAG_LEN) {
            chunk = TIKU_KITS_CRYPTO_TLS_MAX_FRAG_LEN;
        }
        retries = 0;
        while (tiku_kits_crypto_tls_send(
                   http_ctx.tls, data + sent, chunk)
               != TIKU_KITS_CRYPTO_OK) {
            http_net_poll();
            if (++retries
                > TIKU_KITS_NET_HTTP_SEND_RETRIES) {
                return TIKU_KITS_NET_ERR_OVERFLOW;
            }
        }
        sent += chunk;
        http_net_poll();
    }
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/
/* CORE HTTP EXECUTE (shared by POST and GET)                                */
/*---------------------------------------------------------------------------*/

static int8_t
http_execute(
    const char *method,
    const char *host,
    const char *path,
    const char *api_key,
    const uint8_t *body,
    uint16_t body_len,
    uint8_t *response_buf,
    uint16_t response_max,
    uint16_t *response_len)
{
    uint16_t timeout;
    uint8_t ip[4];
    uint16_t req_len;
    uint16_t n;
    int8_t rc;

    /*---------------------------------------------------------------*/
    /* 1. DNS resolution                                             */
    /*---------------------------------------------------------------*/
    rc = tiku_kits_net_dns_resolve(host);
    if (rc != TIKU_KITS_NET_OK) {
        return TIKU_KITS_NET_ERR_HTTP_DNS;
    }

    for (timeout = 0;
         timeout < TIKU_KITS_NET_HTTP_TIMEOUT;
         timeout++) {
        http_net_poll();
        tiku_kits_net_dns_poll();

        if (tiku_kits_net_dns_get_state()
            == TIKU_KITS_NET_DNS_STATE_DONE) {
            break;
        }
        if (tiku_kits_net_dns_get_state()
            == TIKU_KITS_NET_DNS_STATE_ERROR) {
            return TIKU_KITS_NET_ERR_HTTP_DNS;
        }
    }
    if (tiku_kits_net_dns_get_state()
        != TIKU_KITS_NET_DNS_STATE_DONE) {
        tiku_kits_net_dns_abort();
        return TIKU_KITS_NET_ERR_HTTP_DNS;
    }
    tiku_kits_net_dns_get_addr(ip);

    /*---------------------------------------------------------------*/
    /* 2. TCP connect                                                */
    /*---------------------------------------------------------------*/
    http_ctx.tcp_event = 0;
    http_ctx.tcp = tiku_kits_net_tcp_connect(
        ip,
        TIKU_KITS_NET_HTTP_PORT,
        TIKU_KITS_NET_HTTP_LOCAL_PORT,
        http_tcp_recv_cb,
        http_tcp_event_cb);

    if (http_ctx.tcp == NULL) {
        return TIKU_KITS_NET_ERR_HTTP_TCP;
    }

    for (timeout = 0;
         timeout < TIKU_KITS_NET_HTTP_TIMEOUT;
         timeout++) {
        http_net_poll();
        if (http_ctx.tcp_event
            == TIKU_KITS_NET_TCP_EVT_CONNECTED) {
            break;
        }
        if (http_ctx.tcp_event
            == TIKU_KITS_NET_TCP_EVT_ABORTED) {
            return TIKU_KITS_NET_ERR_HTTP_TCP;
        }
    }
    if (http_ctx.tcp_event
        != TIKU_KITS_NET_TCP_EVT_CONNECTED) {
        tiku_kits_net_tcp_abort(http_ctx.tcp);
        return TIKU_KITS_NET_ERR_HTTP_TCP;
    }

    /*---------------------------------------------------------------*/
    /* 3. TLS handshake                                              */
    /*---------------------------------------------------------------*/
    http_ctx.tls_event = 0;
    http_ctx.tls = tiku_kits_crypto_tls_connect(
        http_ctx.tcp,
        http_tls_recv_cb,
        http_tls_event_cb);

    if (http_ctx.tls == NULL) {
        tiku_kits_net_tcp_close(http_ctx.tcp);
        return TIKU_KITS_NET_ERR_HTTP_TLS;
    }

    for (timeout = 0;
         timeout < TIKU_KITS_NET_HTTP_TIMEOUT;
         timeout++) {
        http_net_poll();
        if (http_ctx.tls_event
            == TIKU_KITS_CRYPTO_TLS_EVT_CONNECTED) {
            break;
        }
        if (http_ctx.tls_event
            == TIKU_KITS_CRYPTO_TLS_EVT_ERROR) {
            tiku_kits_net_tcp_abort(http_ctx.tcp);
            return TIKU_KITS_NET_ERR_HTTP_TLS;
        }
    }
    if (http_ctx.tls_event
        != TIKU_KITS_CRYPTO_TLS_EVT_CONNECTED) {
        tiku_kits_net_tcp_abort(http_ctx.tcp);
        return TIKU_KITS_NET_ERR_HTTP_TLS;
    }

    /*---------------------------------------------------------------*/
    /* 4. Build and send HTTP request                                */
    /*---------------------------------------------------------------*/
    {
        uint16_t saved = tiku_mpu_unlock_nvm();
        req_len = tiku_kits_net_http_build_request(
            method, host, path, api_key, body_len,
            http_req_buf,
            TIKU_KITS_NET_HTTP_REQ_BUF_SIZE);
        tiku_mpu_lock_nvm(saved);
    }

    if (req_len >= TIKU_KITS_NET_HTTP_REQ_BUF_SIZE) {
        tiku_kits_crypto_tls_close(http_ctx.tls);
        tiku_kits_net_tcp_close(http_ctx.tcp);
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    rc = http_tls_send_all(http_req_buf, req_len);
    if (rc != TIKU_KITS_NET_OK) {
        tiku_kits_crypto_tls_close(http_ctx.tls);
        tiku_kits_net_tcp_close(http_ctx.tcp);
        return rc;
    }

    /* Send body (POST only) */
    if (body != NULL && body_len > 0) {
        rc = http_tls_send_all(body, body_len);
        if (rc != TIKU_KITS_NET_OK) {
            tiku_kits_crypto_tls_close(http_ctx.tls);
            tiku_kits_net_tcp_close(http_ctx.tcp);
            return rc;
        }
    }

    /*---------------------------------------------------------------*/
    /* 5. Receive and parse response                                 */
    /*---------------------------------------------------------------*/
    tiku_kits_net_http_parser_init(
        &http_ctx.parser, response_buf, response_max);

    for (timeout = 0;
         timeout < TIKU_KITS_NET_HTTP_TIMEOUT;
         timeout++) {
        http_net_poll();

        /* Read decrypted data (reuse req_buf as staging).
         * Both http_req_buf and the caller's response_buf may
         * be in FRAM (.persistent), so MPU must be unlocked
         * for tls_read (writes to http_req_buf) and
         * parser_feed (writes to response_buf). */
        {
            uint16_t saved = tiku_mpu_unlock_nvm();
            n = tiku_kits_crypto_tls_read(
                http_ctx.tls, http_req_buf,
                TIKU_KITS_NET_HTTP_REQ_BUF_SIZE);

            if (n > 0) {
                tiku_kits_net_http_parser_feed(
                    &http_ctx.parser, http_req_buf, n);
            }
            tiku_mpu_lock_nvm(saved);
        }

        /* Connection closed: read remaining buffered data */
        if (http_ctx.tls_event
                == TIKU_KITS_CRYPTO_TLS_EVT_CLOSED
            || http_ctx.tls_event
                == TIKU_KITS_CRYPTO_TLS_EVT_ERROR) {
            do {
                uint16_t saved = tiku_mpu_unlock_nvm();
                n = tiku_kits_crypto_tls_read(
                    http_ctx.tls, http_req_buf,
                    TIKU_KITS_NET_HTTP_REQ_BUF_SIZE);
                if (n > 0) {
                    tiku_kits_net_http_parser_feed(
                        &http_ctx.parser,
                        http_req_buf, n);
                }
                tiku_mpu_lock_nvm(saved);
            } while (n > 0);
            break;
        }
    }

    /*---------------------------------------------------------------*/
    /* 6. Cleanup                                                    */
    /*---------------------------------------------------------------*/
    tiku_kits_crypto_tls_close(http_ctx.tls);
    tiku_kits_net_tcp_close(http_ctx.tcp);

    if (response_len != NULL) {
        *response_len = http_ctx.parser.body_len;
    }

    if (http_ctx.parser.status_code != 200) {
        return TIKU_KITS_NET_ERR_HTTP_STATUS;
    }

    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Perform an HTTP/1.1 POST over TLS.
 *
 * Thin wrapper around http_execute() with method="POST".  Validates
 * required parameters and delegates the full DNS -> TCP -> TLS ->
 * HTTP sequence to the internal blocking helper.
 */
int8_t
tiku_kits_net_http_post(
    const char *host,
    const char *path,
    const char *api_key,
    const uint8_t *json_body,
    uint16_t body_len,
    uint8_t *response_buf,
    uint16_t response_max,
    uint16_t *response_len)
{
    if (host == NULL || path == NULL
        || response_buf == NULL) {
        return TIKU_KITS_NET_ERR_NULL;
    }
    return http_execute(
        "POST", host, path, api_key,
        json_body, body_len,
        response_buf, response_max, response_len);
}

/**
 * @brief Perform an HTTP/1.1 GET over TLS.
 *
 * Same as http_post but sends a GET request with no body.
 * Delegates to http_execute() with method="GET" and body_len=0.
 */
int8_t
tiku_kits_net_http_get(
    const char *host,
    const char *path,
    const char *api_key,
    uint8_t *response_buf,
    uint16_t response_max,
    uint16_t *response_len)
{
    if (host == NULL || path == NULL
        || response_buf == NULL) {
        return TIKU_KITS_NET_ERR_NULL;
    }
    return http_execute(
        "GET", host, path, api_key,
        NULL, 0,
        response_buf, response_max, response_len);
}

/**
 * @brief Return the HTTP status code from the last transaction.
 *
 * Reads the status_code field from the parser context.  Returns
 * the 3-digit code (e.g. 200, 404) or 0 if no request has been
 * made yet.
 */
uint16_t
tiku_kits_net_http_get_status_code(void)
{
    return http_ctx.parser.status_code;
}

#endif /* TIKU_KITS_NET_HTTP_ENABLE */
