/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_http.c - Minimal HTTP/1.1 client
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Implements the HTTP/1.1 request builder, response parser,
 * and blocking client that drives DNS, TCP, and TLS to
 * completion.  Designed for ultra-low-power MCUs with static
 * allocation only.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.  See the License for the specific
 * language governing permissions and limitations under the
 * License.
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

/*
 * Processes each byte through a three-phase state machine:
 * 1. STATUS -- skips "HTTP/1.x ", extracts the 3-digit status
 * code, then skips the reason phrase until \\n.
 * 2. HEADERS -- counts \\r\\n sequences; when \\r\\n\\r\\n is
 * detected (the empty line after headers), transitions to BODY.
 * 3. BODY -- copies bytes into the caller's body_buf up to
 * body_max capacity.
 * The parser is incremental: it can be called repeatedly with
 * small chunks as data arrives from the TLS layer.
 */

/**
 * @brief Feed raw HTTP response bytes through the parser.
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

/*
 * Assembles the request line ("METHOD path HTTP/1.1\\r\\n") followed
 * by Host, Content-Type (if body_len > 0), x-api-key + anthropic-
 * version (if api_key != NULL), Content-Length (if body_len > 0),
 * Connection: close, and the final blank line.  The body is NOT
 * included -- the caller sends it separately after the headers.
 * Returns the number of bytes written.  If the buffer is too small,
 * the output is truncated and the return value equals buf_max.
 */

/**
 * @brief Format an HTTP/1.1 request line and headers into a buffer.
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
#include <tikukits/net/tls/psk/tiku_kits_crypto_tls.h>

/*---------------------------------------------------------------------------*/
/* CERTIFICATE TLS (public web) -- compiled when the cert clients are linked */
/*---------------------------------------------------------------------------*/
/* TIKU_KITS_NET_HTTP_CERT_ENABLE + the tls13 io come from the header. */

#if TIKU_KITS_NET_HTTP_CERT_ENABLE
#include "../tls/tls12/tiku_kits_crypto_tls12.h"
/* TIKU_CLOCK_ARCH_SECOND must be in scope before tiku_clock.h's macros are
 * used (same explicit routing as ipv4/tiku_kits_net_dns.c -- on Ambiq it
 * happened to arrive transitively, on Nordic nothing else pulls it in). */
#if defined(PLATFORM_MSP430)
#include <arch/msp430/tiku_timer_arch.h>
#elif defined(PLATFORM_RP2350)
#include <arch/arm-rp2350/tiku_timer_arch.h>
#elif defined(PLATFORM_AMBIQ)
#include <arch/ambiq/tiku_timer_arch.h>
#elif defined(PLATFORM_NORDIC)
#include <arch/nordic/tiku_timer_arch.h>
#else
#error "tikukits/net/http: unsupported platform"
#endif
#include <kernel/timers/tiku_clock.h>
#include <kernel/cpu/tiku_watchdog.h>

/** Cooperative-blocking deadline for a cert handshake/transfer (seconds). */
#ifndef TIKU_KITS_NET_HTTP_CERT_TIMEOUT_SEC
#define TIKU_KITS_NET_HTTP_CERT_TIMEOUT_SEC   20u
#endif

/** SRAM staging for decrypted cert-mode response bytes before the sink. */
#ifndef TIKU_KITS_NET_HTTP_CERT_RX_STAGING
#define TIKU_KITS_NET_HTTP_CERT_RX_STAGING    256
#endif
#endif /* TIKU_KITS_NET_HTTP_CERT_ENABLE */

/*---------------------------------------------------------------------------*/
/* FRAM-BACKED REQUEST BUFFER                                                */
/*---------------------------------------------------------------------------*/

/**
 * Holds the formatted HTTP request headers during send, then
 * is reused as a TLS read staging buffer during the response
 * phase.  Placed in FRAM to conserve SRAM.
 */
TIKU_KITS_CRYPTO_TLS_BUF_ATTR
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

/*
 * Equivalent to what the net process protothread does each
 * poll interval.  Called in a tight loop by the blocking HTTP
 * functions to advance TCP, TLS, and DNS state machines while
 * waiting for events.
 */

/**
 * @brief Drain the SLIP link and process incoming IP frames.
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
/* SHARED CONNECT + REQUEST-BUILD HELPERS                                    */
/*---------------------------------------------------------------------------*/

/*
 * Connects to @p ip : TIKU_KITS_NET_HTTP_PORT from @p src_port, polling the
 * SLIP link until the 3-way handshake completes.  Shared by both trust models;
 * the cert path also uses it to reopen a fresh 4-tuple for the TLS 1.2
 * fallback.  Returns the connection, or NULL on abort/timeout (the socket is
 * aborted before returning NULL so no slot leaks).
 */

/**
 * @brief Open a TCP connection to the resolved server and block until ready.
 */
static tiku_kits_net_tcp_conn_t *
http_tcp_open(const uint8_t ip[4], uint16_t src_port)
{
    tiku_kits_net_tcp_conn_t *tcp;
    uint16_t timeout;

    http_ctx.tcp_event = 0;
    tcp = tiku_kits_net_tcp_connect(
        ip, TIKU_KITS_NET_HTTP_PORT, src_port,
        http_tcp_recv_cb, http_tcp_event_cb);
    if (tcp == NULL) {
        return NULL;
    }

    for (timeout = 0;
         timeout < TIKU_KITS_NET_HTTP_TIMEOUT;
         timeout++) {
        http_net_poll();
        if (http_ctx.tcp_event
            == TIKU_KITS_NET_TCP_EVT_CONNECTED) {
            return tcp;
        }
        if (http_ctx.tcp_event
            == TIKU_KITS_NET_TCP_EVT_ABORTED) {
            break;
        }
    }
    tiku_kits_net_tcp_abort(tcp);
    return NULL;
}

/**
 * @brief Format the request line + headers into the FRAM request buffer.
 *
 * Wraps the TLS-agnostic request builder in an MPU-unlock window (http_req_buf
 * is in NVM on FRAM parts).  Returns the byte count; a value >= the buffer
 * size means the request was truncated and the caller must fail.
 */
static uint16_t
http_build_reqbuf(const char *method, const char *host,
                  const char *path, const char *api_key,
                  uint16_t body_len)
{
    uint16_t req_len;
    uint16_t saved = tiku_mpu_unlock_nvm();
    req_len = tiku_kits_net_http_build_request(
        method, host, path, api_key, body_len,
        http_req_buf, TIKU_KITS_NET_HTTP_REQ_BUF_SIZE);
    tiku_mpu_lock_nvm(saved);
    return req_len;
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
/* PSK TRUST MODEL (event-driven, over the TCP callbacks)                    */
/*---------------------------------------------------------------------------*/

/*
 * On entry http_ctx.tcp is CONNECTED, the request is formatted in
 * http_req_buf[0..req_len), and http_ctx.parser is initialised.  Runs the
 * PSK handshake (which takes over the TCP callbacks), sends the request and
 * optional body, then reads + parses the response.  Closes the TLS layer on
 * success; leaves the TCP socket for the caller to tear down.  Returns
 * TIKU_KITS_NET_OK or a negative HTTP error.
 */

/**
 * @brief Drive the pre-shared-key TLS transport to completion.
 */
static int8_t
http_drive_psk(uint16_t req_len, const uint8_t *body,
               uint16_t body_len)
{
    uint16_t timeout;
    uint16_t n;
    int8_t rc;

    /* Handshake (PSK) -- non-blocking; advanced by polling the link. */
    http_ctx.tls_event = 0;
    http_ctx.tls = tiku_kits_crypto_tls_connect(
        http_ctx.tcp, http_tls_recv_cb, http_tls_event_cb);
    if (http_ctx.tls == NULL) {
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
            return TIKU_KITS_NET_ERR_HTTP_TLS;
        }
    }
    if (http_ctx.tls_event
        != TIKU_KITS_CRYPTO_TLS_EVT_CONNECTED) {
        return TIKU_KITS_NET_ERR_HTTP_TLS;
    }

    /* Send request headers, then the body (POST only). */
    rc = http_tls_send_all(http_req_buf, req_len);
    if (rc != TIKU_KITS_NET_OK) {
        return rc;
    }
    if (body != NULL && body_len > 0) {
        rc = http_tls_send_all(body, body_len);
        if (rc != TIKU_KITS_NET_OK) {
            return rc;
        }
    }

    /* Receive + parse.  Reuse http_req_buf as staging; both it and the
     * caller's response_buf may be in FRAM (.persistent), so unlock the MPU
     * around tls_read (writes http_req_buf) and parser_feed (writes
     * response_buf). */
    for (timeout = 0;
         timeout < TIKU_KITS_NET_HTTP_TIMEOUT;
         timeout++) {
        http_net_poll();
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

        /* Connection closed: drain remaining buffered data. */
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
                        &http_ctx.parser, http_req_buf, n);
                }
                tiku_mpu_lock_nvm(saved);
            } while (n > 0);
            break;
        }
    }

    tiku_kits_crypto_tls_close(http_ctx.tls);
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/
/* CERT TRUST MODEL (public web, over the cert clients' io vtable)           */
/*---------------------------------------------------------------------------*/

#if TIKU_KITS_NET_HTTP_CERT_ENABLE

/* Certificate connection state + a small SRAM staging buffer for decrypted
 * response bytes.  Kept module-static (off the caller's stack): the heavy
 * TLS record buffers live inside the tls13/tls12 clients themselves. */
static tiku_kits_crypto_tls13_conn_t http_tls13;
static tiku_kits_crypto_tls12_conn_t http_tls12;
static uint8_t http_cert_rx[TIKU_KITS_NET_HTTP_CERT_RX_STAGING];

#define HTTP_CERT_DEADLINE()                                        \
    ((tiku_clock_time_t)(tiku_clock_time() +                        \
        (tiku_clock_time_t)(TIKU_KITS_NET_HTTP_CERT_TIMEOUT_SEC     \
                            * TIKU_CLOCK_SECOND)))
#define HTTP_CERT_EXPIRED(dl)  (!TIKU_CLOCK_LT(tiku_clock_time(), (dl)))

/*
 * Delivers inbound IP frames every call and advances the TCP timers at ~8 Hz
 * (a tight loop calling tcp_periodic every iteration would burn through the
 * connect/retransmit timeouts).  Kicks the watchdog so a legitimately slow
 * public-web handshake survives while a genuine hang still trips the WDT.
 */

/**
 * @brief One cooperative-blocking pump step for the cert handshake/transfer.
 */
static void
http_cert_pump(void)
{
    static tiku_clock_time_t last_tcp;
    tiku_clock_time_t now = tiku_clock_time();
    tiku_watchdog_kick();
    http_net_poll();
    if ((tiku_clock_time_t)(now - last_tcp)
        >= (tiku_clock_time_t)(TIKU_CLOCK_SECOND / 8)) {
        last_tcp = now;
        tiku_kits_net_tcp_periodic();
    }
}

/**
 * @brief io.send -- transmit @p len bytes over TCP, pumping between segments.
 *
 * Chunks by the negotiated MSS (tcp_send rejects data_len > snd_mss) and gives
 * up at the deadline.  Returns bytes sent, or < 0 on timeout.
 */
static int
http_cert_send(void *ctx, const uint8_t *buf, size_t len)
{
    tiku_kits_net_tcp_conn_t *c = ctx;
    size_t off = 0;
    tiku_clock_time_t dl = HTTP_CERT_DEADLINE();

    while (off < len) {
        uint16_t mss = c->snd_mss ? c->snd_mss : TIKU_KITS_NET_TCP_MSS;
        size_t chunk = len - off;
        if (chunk > mss) {
            chunk = mss;
        }
        if (tiku_kits_net_tcp_send(c, buf + off, (uint16_t)chunk)
            == TIKU_KITS_NET_OK) {
            off += chunk;
        }
        http_cert_pump();
        if (HTTP_CERT_EXPIRED(dl)) {
            return -1;
        }
    }
    return (int)len;
}

/**
 * @brief io.recv -- block (pumping) until data arrives, EOF, or the deadline.
 *
 * Returns bytes read, or < 0 on peer RST / timeout.
 */
static int
http_cert_recv(void *ctx, uint8_t *buf, size_t len)
{
    tiku_kits_net_tcp_conn_t *c = ctx;
    uint16_t want = (uint16_t)(len > 0xFFFFu ? 0xFFFFu : len);
    tiku_clock_time_t dl = HTTP_CERT_DEADLINE();

    for (;;) {
        uint16_t got = tiku_kits_net_tcp_read(c, buf, want);
        if (got > 0) {
            return (int)got;
        }
        if (http_ctx.tcp_event == TIKU_KITS_NET_TCP_EVT_ABORTED) {
            return -1;
        }
        if (http_ctx.tcp_event == TIKU_KITS_NET_TCP_EVT_CLOSED) {
            got = tiku_kits_net_tcp_read(c, buf, want);
            return got > 0 ? (int)got : -1;
        }
        http_cert_pump();
        if (HTTP_CERT_EXPIRED(dl)) {
            return -1;
        }
    }
}

/*
 * Kit-side plumbing that lets the kit's own http_get()/http_post() reuse the
 * shared engine below: a sink that streams decrypted bytes into the response
 * parser, and a reconnect that reopens the kit's transport for the fallback.
 */

/* Sink: feed decrypted bytes to the parser (which caps at body_max itself, so
 * we always keep reading).  Unlock the MPU around the feed -- response_buf may
 * be in FRAM (.persistent). */
static uint8_t
http_parser_sink(void *ctx, const uint8_t *data, uint16_t len)
{
    tiku_kits_net_http_parser_t *p = ctx;
    uint16_t saved = tiku_mpu_unlock_nvm();
    tiku_kits_net_http_parser_feed(p, data, len);
    tiku_mpu_lock_nvm(saved);
    return 1;
}

/* Reconnect for the TLS 1.2 fallback: close the old socket, reopen a fresh
 * 4-tuple.  @p rc_ctx is the resolved server IP (uint8_t[4]). */
static void *
http_kit_reconnect(void *rc_ctx, void *old_ctx)
{
    tiku_kits_net_tcp_close((tiku_kits_net_tcp_conn_t *)old_ctx);
    return http_tcp_open((const uint8_t *)rc_ctx,
                         (uint16_t)(TIKU_KITS_NET_HTTP_LOCAL_PORT + 1));
}

/*---------------------------------------------------------------------------*/
/* SHARED CERTIFICATE HTTPS ENGINE (transport-injected; declared in header)  */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_http_cert_exchange(
    tiku_kits_crypto_tls13_io_t *io,
    tiku_kits_net_http_reconnect_t reconnect,
    void *reconnect_ctx,
    const tiku_kits_net_http_tls_t *tls,
    const char *host,
    const uint8_t *req,
    uint16_t req_len,
    const uint8_t *body,
    uint16_t body_len,
    tiku_kits_net_http_sink_t sink,
    void *sink_ctx)
{
    int n;
    uint8_t use12 = 0;

    if (io == NULL || tls == NULL || tls->rng == NULL) {
        return TIKU_KITS_NET_ERR_HTTP_TLS;   /* need transport + a CSPRNG */
    }

    /* TLS 1.3 handshake over the caller's already-connected transport. */
    if (tiku_kits_crypto_tls13_connect(
            io, tls->rng, host, tls->roots, tls->nroots,
            tls->now_unix, &http_tls13) != 0) {
        /* 1.3 failed (ServerHello consumed): reopen and try TLS 1.2.  The
         * reconnect callback owns closing the spent socket; it returns the
         * fresh one (or NULL).  Socket ownership after this call:
         *   - no reconnect  -> io->ctx unchanged (still open) -> caller frees
         *   - reconnect NULL -> old already freed, io->ctx = NULL -> caller
         *                       must not double-free (it guards on NULL)
         *   - reconnect ok   -> io->ctx = fresh socket -> caller frees it */
        if (reconnect == NULL) {
            return TIKU_KITS_NET_ERR_HTTP_TLS;
        }
        io->ctx = reconnect(reconnect_ctx, io->ctx);
        if (io->ctx == NULL) {
            return TIKU_KITS_NET_ERR_HTTP_TCP;
        }
        if (tiku_kits_crypto_tls12_connect(
                io, tls->rng, host, tls->roots, tls->nroots,
                tls->now_unix, &http_tls12) != 0) {
            return TIKU_KITS_NET_ERR_HTTP_TLS;
        }
        use12 = 1;
    }

    /* Send request headers, then the body (if any) as a second record. */
    n = use12
        ? tiku_kits_crypto_tls12_write(&http_tls12, req, req_len)
        : tiku_kits_crypto_tls13_write(&http_tls13, req, req_len);
    if (n >= 0 && body != NULL && body_len > 0) {
        n = use12
            ? tiku_kits_crypto_tls12_write(&http_tls12, body, body_len)
            : tiku_kits_crypto_tls13_write(&http_tls13, body, body_len);
    }
    if (n < 0) {
        return TIKU_KITS_NET_ERR_HTTP_TLS;
    }

    /* Read + decrypt into SRAM staging (so the net pump inside tls*_read never
     * runs under an MPU window), handing each chunk to the caller's sink.
     * tls*_read returns 0 at close, < 0 on error. */
    for (;;) {
        n = use12
            ? tiku_kits_crypto_tls12_read(&http_tls12, http_cert_rx,
                                          sizeof http_cert_rx)
            : tiku_kits_crypto_tls13_read(&http_tls13, http_cert_rx,
                                          sizeof http_cert_rx);
        if (n <= 0) {
            break;
        }
        if (sink != NULL && !sink(sink_ctx, http_cert_rx, (uint16_t)n)) {
            break;   /* sink is full */
        }
    }

    return TIKU_KITS_NET_OK;
}

#endif /* TIKU_KITS_NET_HTTP_CERT_ENABLE */

/*---------------------------------------------------------------------------*/
/* CORE HTTP EXECUTE (shared by POST and GET, either trust model)            */
/*---------------------------------------------------------------------------*/

static int8_t
http_execute(
    const tiku_kits_net_http_tls_t *tls,
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
    tiku_kits_net_http_trust_t trust =
        (tls != NULL) ? tls->trust : TIKU_KITS_NET_HTTP_PSK;
    uint16_t timeout;
    uint8_t ip[4];
    uint16_t req_len;
    int8_t rc;

#if !TIKU_KITS_NET_HTTP_CERT_ENABLE
    /* Fail fast (before any network work) if the caller asks for certificate
     * trust on a build without the cert clients linked. */
    if (trust == TIKU_KITS_NET_HTTP_CERT) {
        return TIKU_KITS_NET_ERR_HTTP_NOSUP;
    }
#endif

    /*---------------------------------------------------------------*/
    /* 1. DNS resolution (shared)                                    */
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
    /* 2. TCP connect (shared)                                       */
    /*---------------------------------------------------------------*/
    http_ctx.tcp = http_tcp_open(ip, TIKU_KITS_NET_HTTP_LOCAL_PORT);
    if (http_ctx.tcp == NULL) {
        return TIKU_KITS_NET_ERR_HTTP_TCP;
    }

    /*---------------------------------------------------------------*/
    /* 3. Build request into the FRAM buffer (shared, TLS-agnostic)  */
    /*---------------------------------------------------------------*/
    req_len = http_build_reqbuf(method, host, path, api_key, body_len);
    if (req_len >= TIKU_KITS_NET_HTTP_REQ_BUF_SIZE) {
        tiku_kits_net_tcp_close(http_ctx.tcp);
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    /*---------------------------------------------------------------*/
    /* 4. Response parser (shared)                                   */
    /*---------------------------------------------------------------*/
    tiku_kits_net_http_parser_init(
        &http_ctx.parser, response_buf, response_max);

    /*---------------------------------------------------------------*/
    /* 5. Drive the selected trust model's TLS transport             */
    /*---------------------------------------------------------------*/
    if (trust == TIKU_KITS_NET_HTTP_CERT) {
#if TIKU_KITS_NET_HTTP_CERT_ENABLE
        /* Drive the shared engine over the kit's own SLIP transport: io.send/
         * recv pump the link, http_kit_reconnect reopens for the 1.2 fallback,
         * and the parser sink streams the body into response_buf. */
        tiku_kits_crypto_tls13_io_t io;
        io.send    = http_cert_send;
        io.recv    = http_cert_recv;
        io.ctx     = http_ctx.tcp;
        io.offload = tls->offload;   /* NULL -> heavy crypto runs inline */
        rc = tiku_kits_net_http_cert_exchange(
            &io, http_kit_reconnect, ip, tls, host,
            http_req_buf, req_len, body, body_len,
            http_parser_sink, &http_ctx.parser);
        http_ctx.tcp = io.ctx;       /* engine may have reconnected */
#else
        rc = TIKU_KITS_NET_ERR_HTTP_NOSUP;
#endif
    } else {
        rc = http_drive_psk(req_len, body, body_len);
    }

    /*---------------------------------------------------------------*/
    /* 6. Tear down TCP: graceful close on success, abort on failure */
    /*---------------------------------------------------------------*/
    if (http_ctx.tcp != NULL) {
        if (rc == TIKU_KITS_NET_OK) {
            tiku_kits_net_tcp_close(http_ctx.tcp);
        } else {
            tiku_kits_net_tcp_abort(http_ctx.tcp);
        }
    }
    if (rc != TIKU_KITS_NET_OK) {
        return rc;
    }

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
    const tiku_kits_net_http_tls_t *tls,
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
        tls, "POST", host, path, api_key,
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
    const tiku_kits_net_http_tls_t *tls,
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
        tls, "GET", host, path, api_key,
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
