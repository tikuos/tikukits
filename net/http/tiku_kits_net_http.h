/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_http.h - Minimal HTTP/1.1 client
 *
 * Provides a minimal HTTP/1.1 client that runs on top of the
 * existing TLS 1.3 PSK-only transport.  Only GET and POST are
 * supported.  No chunked encoding, redirects, cookies, gzip,
 * or keep-alive.
 *
 * Architecture:
 *   - Request builder formats headers into a caller-supplied
 *     buffer.  No heap allocation.
 *   - Response parser is a byte-by-byte state machine that
 *     extracts the status code, skips headers, and copies the
 *     body into the caller's buffer.
 *   - The blocking post/get functions drive DNS, TCP, TLS,
 *     and the HTTP exchange to completion before returning.
 *   - FRAM-backed request buffer avoids SRAM pressure during
 *     header assembly and doubles as TLS read staging after
 *     the request is sent.
 *
 * Usage:
 *   @code
 *     uint8_t resp[512];
 *     uint16_t resp_len;
 *     int8_t rc = tiku_kits_net_http_post(
 *         "api.anthropic.com", "/v1/messages",
 *         "sk-ant-...", json, json_len,
 *         resp, sizeof(resp), &resp_len);
 *   @endcode
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

#ifndef TIKU_KITS_NET_HTTP_H_
#define TIKU_KITS_NET_HTTP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* COMPILE GUARD                                                             */
/*---------------------------------------------------------------------------*/

#ifndef TIKU_KITS_NET_HTTP_ENABLE
#define TIKU_KITS_NET_HTTP_ENABLE   0
#endif

/*---------------------------------------------------------------------------*/
/* CONFIGURATION (compile-time overrideable)                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief FRAM-backed buffer size for HTTP request headers.
 *
 * Must hold the complete request line plus all headers.  384
 * bytes accommodates typical Anthropic API requests with API
 * keys up to ~100 characters.  Also reused as TLS read staging
 * after the request is sent.
 */
#ifndef TIKU_KITS_NET_HTTP_REQ_BUF_SIZE
#define TIKU_KITS_NET_HTTP_REQ_BUF_SIZE     384
#endif

/** HTTPS port (TLS over TCP). */
#ifndef TIKU_KITS_NET_HTTP_PORT
#define TIKU_KITS_NET_HTTP_PORT             443
#endif

/** Local TCP source port for HTTP connections. */
#ifndef TIKU_KITS_NET_HTTP_LOCAL_PORT
#define TIKU_KITS_NET_HTTP_LOCAL_PORT        49500
#endif

/**
 * @brief Maximum poll iterations before timeout.
 *
 * Each iteration drains the SLIP link and processes any
 * incoming frames.  At ~50 ms per poll tick this gives
 * roughly 5 minutes before timeout.
 */
#ifndef TIKU_KITS_NET_HTTP_TIMEOUT
#define TIKU_KITS_NET_HTTP_TIMEOUT          6000
#endif

/** Maximum TLS send retries per fragment. */
#ifndef TIKU_KITS_NET_HTTP_SEND_RETRIES
#define TIKU_KITS_NET_HTTP_SEND_RETRIES     200
#endif

/*---------------------------------------------------------------------------*/
/* HTTP-SPECIFIC ERROR CODES                                                 */
/*---------------------------------------------------------------------------*/

/** Non-200 HTTP status code received. */
#define TIKU_KITS_NET_ERR_HTTP_STATUS   (-10)

/** Response parse error (malformed HTTP). */
#define TIKU_KITS_NET_ERR_HTTP_PARSE    (-11)

/** DNS resolution failed. */
#define TIKU_KITS_NET_ERR_HTTP_DNS      (-12)

/** TCP connection failed. */
#define TIKU_KITS_NET_ERR_HTTP_TCP      (-13)

/** TLS handshake failed. */
#define TIKU_KITS_NET_ERR_HTTP_TLS      (-14)

/*---------------------------------------------------------------------------*/
/* RESPONSE PARSER                                                           */
/*---------------------------------------------------------------------------*/

/** @defgroup HTTP_PARSE_STATES Response parser states
 * @{ */
#define TIKU_KITS_NET_HTTP_PARSE_STATUS     0
#define TIKU_KITS_NET_HTTP_PARSE_HEADERS    1
#define TIKU_KITS_NET_HTTP_PARSE_BODY       2
#define TIKU_KITS_NET_HTTP_PARSE_DONE       3
/** @} */

/**
 * @struct tiku_kits_net_http_parser
 * @brief HTTP response parser state.
 *
 * A byte-by-byte state machine that extracts the status code,
 * skips response headers (until \\r\\n\\r\\n), and copies the
 * body into the caller's buffer.  Uses ~11 bytes of SRAM.
 */
typedef struct tiku_kits_net_http_parser {
    uint8_t  *body_buf;     /**< Caller's body output buffer */
    uint16_t  body_max;     /**< Capacity of body_buf */
    uint16_t  body_len;     /**< Bytes of body written so far */
    uint16_t  status_code;  /**< Parsed HTTP status code */
    uint8_t   state;        /**< TIKU_KITS_NET_HTTP_PARSE_* */
    uint8_t   sub_state;    /**< Status-line sub-state */
    uint8_t   hdr_end_seq;  /**< \\r\\n\\r\\n detector (0-3) */
} tiku_kits_net_http_parser_t;

/**
 * @brief Initialise the response parser.
 *
 * @param p         Parser context
 * @param body_buf  Buffer to receive HTTP body
 * @param body_max  Capacity of body_buf
 */
void tiku_kits_net_http_parser_init(
    tiku_kits_net_http_parser_t *p,
    uint8_t *body_buf,
    uint16_t body_max);

/**
 * @brief Feed raw HTTP response bytes to the parser.
 *
 * Processes each byte through the state machine: extracts
 * the status code from the status line, skips headers until
 * \\r\\n\\r\\n, then copies body bytes into body_buf.
 *
 * @param p     Parser context
 * @param data  Incoming bytes
 * @param len   Number of bytes
 */
void tiku_kits_net_http_parser_feed(
    tiku_kits_net_http_parser_t *p,
    const uint8_t *data,
    uint16_t len);

/**
 * @brief Check if the parser has finished.
 *
 * @param p  Parser context
 * @return 1 if state is PARSE_DONE, 0 otherwise
 */
uint8_t tiku_kits_net_http_parser_done(
    const tiku_kits_net_http_parser_t *p);

/*---------------------------------------------------------------------------*/
/* REQUEST BUILDER                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build an HTTP/1.1 request into a buffer.
 *
 * Formats the request line and all headers.  The body is NOT
 * included --- the caller sends it separately.
 *
 * Headers included:
 *   - Host (always)
 *   - Content-Type: application/json (when body_len > 0)
 *   - x-api-key (when api_key is not NULL)
 *   - anthropic-version: 2023-06-01 (when api_key is not NULL)
 *   - Content-Length (when body_len > 0)
 *   - Connection: close (always)
 *
 * @param method    "GET" or "POST"
 * @param host      Hostname (e.g. "api.anthropic.com")
 * @param path      Request path (e.g. "/v1/messages")
 * @param api_key   API key (NULL to omit x-api-key header)
 * @param body_len  Body length (0 for GET)
 * @param buf       Output buffer for formatted request
 * @param buf_max   Capacity of buf
 * @return Number of bytes written (== buf_max if truncated)
 */
uint16_t tiku_kits_net_http_build_request(
    const char *method,
    const char *host,
    const char *path,
    const char *api_key,
    uint16_t body_len,
    uint8_t *buf,
    uint16_t buf_max);

/*---------------------------------------------------------------------------*/
/* HTTP CLIENT                                                               */
/*---------------------------------------------------------------------------*/

#if TIKU_KITS_NET_HTTP_ENABLE

/**
 * @brief Perform an HTTP/1.1 POST over TLS.
 *
 * Drives the complete sequence: DNS resolve, TCP connect,
 * TLS handshake, send request + JSON body, receive and parse
 * response.  Blocks until the transaction completes or times
 * out.  Internally polls the SLIP link to advance the
 * network stack.
 *
 * On success (HTTP 200), the response body is copied into
 * response_buf and response_len is set.  On non-200 status,
 * returns TIKU_KITS_NET_ERR_HTTP_STATUS and the caller can
 * retrieve the status code via get_status_code().
 *
 * @param host          Hostname to connect to
 * @param path          Request path
 * @param api_key       API key (NULL to omit)
 * @param json_body     JSON request body
 * @param body_len      Length of json_body
 * @param response_buf  Buffer for response body
 * @param response_max  Capacity of response_buf
 * @param response_len  Output: bytes written to response_buf
 * @return TIKU_KITS_NET_OK on 200, negative error otherwise
 */
int8_t tiku_kits_net_http_post(
    const char *host,
    const char *path,
    const char *api_key,
    const uint8_t *json_body,
    uint16_t body_len,
    uint8_t *response_buf,
    uint16_t response_max,
    uint16_t *response_len);

/**
 * @brief Perform an HTTP/1.1 GET over TLS.
 *
 * Same as http_post but sends a GET request with no body.
 *
 * @param host          Hostname to connect to
 * @param path          Request path
 * @param api_key       API key (NULL to omit)
 * @param response_buf  Buffer for response body
 * @param response_max  Capacity of response_buf
 * @param response_len  Output: bytes written to response_buf
 * @return TIKU_KITS_NET_OK on 200, negative error otherwise
 */
int8_t tiku_kits_net_http_get(
    const char *host,
    const char *path,
    const char *api_key,
    uint8_t *response_buf,
    uint16_t response_max,
    uint16_t *response_len);

/**
 * @brief Get the HTTP status code from the last transaction.
 *
 * @return Status code (e.g. 200, 404), or 0 if no request
 *         has been made.
 */
uint16_t tiku_kits_net_http_get_status_code(void);

#endif /* TIKU_KITS_NET_HTTP_ENABLE */

#endif /* TIKU_KITS_NET_HTTP_H_ */
