/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_http.h - Minimal HTTP/1.1 client
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Provides a minimal HTTP/1.1 client.  Only GET and POST are
 * supported.  No chunked encoding, redirects, cookies, gzip,
 * or keep-alive.
 *
 * TLS trust model: http_get()/http_post() take a
 * tiku_kits_net_http_tls_t that selects one of two trust models for
 * the connection, so a single entry point reaches either a private
 * peer or the public web:
 * - PSK  -- the pre-shared-key TLS client (net/tls/psk); reaches
 * only peers that share the key (your own gateway/broker).
 * - CERT -- the certificate-based TLS 1.3/1.2 clients
 * (net/tls/{tls13,tls12}); validate the server chain
 * against a caller-supplied X.509 root store, reaching
 * arbitrary HTTPS servers.  Requires the kit built with
 * the cert clients (HAS_TLS => TIKU_KITS_NET_HTTP_CERT_ENABLE).
 * Both models share the request builder and response parser below
 * (they are TLS-agnostic) and drive TCP through the same io vtable
 * the cert clients define.  A NULL descriptor selects PSK.
 *
 * Architecture:
 * - Request builder formats headers into a caller-supplied
 * buffer.  No heap allocation.
 * - Response parser is a byte-by-byte state machine that
 * extracts the status code, skips headers, and copies the
 * body into the caller's buffer.
 * - The blocking post/get functions drive DNS, TCP, TLS,
 * and the HTTP exchange to completion before returning.
 * - FRAM-backed request buffer avoids SRAM pressure during
 * header assembly and doubles as TLS read staging after
 * the request is sent.
 *
 * Usage (public-web POST to the Anthropic API over certificate TLS):
 * @code
 * static const tiku_kits_net_http_tls_t tls = {
 * .trust = TIKU_KITS_NET_HTTP_CERT,
 * .roots = my_roots, .nroots = MY_NROOTS,
 * .rng   = my_csprng,          // platform CSPRNG
 * };
 * uint8_t resp[512];
 * uint16_t resp_len;
 * int8_t rc = tiku_kits_net_http_post(
 * &tls, "api.anthropic.com", "/v1/messages",
 * "sk-ant-...", json, json_len,
 * resp, sizeof(resp), &resp_len);
 * @endcode
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

#ifndef TIKU_KITS_NET_HTTP_H_
#define TIKU_KITS_NET_HTTP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"
#include "../tls/x509/tiku_kits_crypto_x509.h"  /* CERT root store */
#include <stdint.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* COMPILE GUARD                                                             */
/*---------------------------------------------------------------------------*/

#ifndef TIKU_KITS_NET_HTTP_ENABLE
#define TIKU_KITS_NET_HTTP_ENABLE   0
#endif

/*
 * CERT trust model needs the certificate clients (net/tls/tls13, tls12) linked.
 * The Makefile sets this to 1 when HAS_TLS does that.  Default off -> the kit
 * stays PSK-only and makes no reference to the cert clients.
 */
#ifndef TIKU_KITS_NET_HTTP_CERT_ENABLE
#define TIKU_KITS_NET_HTTP_CERT_ENABLE 0
#endif

/*---------------------------------------------------------------------------*/
/* CONFIGURATION (compile-time overrideable)                                 */
/*---------------------------------------------------------------------------*/

/*
 * Must hold the complete request line plus all headers.  384
 * bytes accommodates typical Anthropic API requests with API
 * keys up to ~100 characters.  Also reused as TLS read staging
 * after the request is sent.
 */

/**
 * @brief FRAM-backed buffer size for HTTP request headers.
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

/** Requested trust model not compiled in (e.g. CERT without HAS_TLS). */
#define TIKU_KITS_NET_ERR_HTTP_NOSUP    (-15)

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

/*
 * Formats the request line and all headers.  The body is NOT
 * included --- the caller sends it separately.
 * Headers included:
 * - Host (always)
 * - Content-Type: application/json (when body_len > 0)
 * - x-api-key (when api_key is not NULL)
 * - anthropic-version: 2023-06-01 (when api_key is not NULL)
 * - Content-Length (when body_len > 0)
 * - Connection: close (always)
 */

/**
 * @brief Build an HTTP/1.1 request into a buffer.
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
/* TLS TRUST MODEL                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Which TLS trust model http_get()/http_post() drive over.
 */
typedef enum {
    TIKU_KITS_NET_HTTP_PSK  = 0,  /**< pre-shared key (own peer)  */
    TIKU_KITS_NET_HTTP_CERT = 1,  /**< X.509 vs. root store (web) */
} tiku_kits_net_http_trust_t;

/*
 * PSK  -- uses the key installed with tiku_kits_crypto_tls_set_psk(); the
 * cert fields are ignored.  Reaches only peers that share the key.
 * CERT -- validates the server certificate chain against @roots (X.509),
 * drawing handshake randomness from @rng.  Reaches the public web.
 * The kit must be built with TIKU_KITS_NET_HTTP_CERT_ENABLE (which
 * the Makefile sets when HAS_TLS links the tls13/tls12 clients);
 * otherwise a CERT request returns TIKU_KITS_NET_ERR_HTTP_NOSUP.
 * A NULL descriptor is treated as PSK (the historical default).
 */

/**
 * @brief  Selects and parameterises the TLS trust model for a request.
 *
 * @struct tiku_kits_net_http_tls_t
 */
typedef struct {
    tiku_kits_net_http_trust_t trust;

    /* --- CERT mode only (ignored for PSK) --- */
    const tiku_kits_crypto_x509_root_t *roots;   /**< trusted-root store      */
    int      nroots;                             /**< entries in @roots       */
    void   (*rng)(uint8_t *buf, size_t len);     /**< CSPRNG (required, CERT) */
    uint64_t now_unix;                           /**< Unix secs; 0 = skip     */
    int    (*offload)(int (*fn)(void *closure),  /**< heavy-crypto offload;   */
                      void *closure);            /**< NULL = run inline */
} tiku_kits_net_http_tls_t;

/*---------------------------------------------------------------------------*/
/* CERTIFICATE HTTPS ENGINE (shared, transport-injected)                     */
/*---------------------------------------------------------------------------*/

#if TIKU_KITS_NET_HTTP_CERT_ENABLE

#include "../tls/tls13/tiku_kits_crypto_tls13.h"  /* io vtable (CERT) */

/**
 * @brief Consumer for decrypted response bytes.
 *
 * Called with each plaintext chunk as it is decrypted.  Return 1 to keep
 * reading, 0 to stop (e.g. the output buffer is full).
 */
typedef uint8_t (*tiku_kits_net_http_sink_t)(
    void *ctx, const uint8_t *data, uint16_t len);

/**
 * @brief Reopen the transport for the TLS 1.2 fallback.
 *
 * The failed TLS 1.3 attempt consumes the ServerHello, so 1.2 must run on a
 * fresh connection.  Close @p old_ctx, open a new socket (a fresh 4-tuple), and
 * return the new transport handle for io->ctx, or NULL on failure.
 */
typedef void *(*tiku_kits_net_http_reconnect_t)(
    void *reconnect_ctx, void *old_ctx);

/*
 * The shared engine behind the kit's own http_get()/http_post() and any
 * app-level HTTPS path (e.g. BASIC HTTPGET$).  Over the already-connected
 * Transport, connect/reopen, and response consumption are all injected, so the
 * caller keeps its own DNS/connect/pump, request format, and parse policy.
 */

/**
 * @brief Drive certificate-based HTTPS over a caller-supplied transport.
 *
 * @p io it runs the TLS 1.3 handshake; on failure it calls @p reconnect and
 * retries with TLS 1.2, validating the chain against @p tls (roots/rng/
 * now_unix).  It then sends @p req (headers) and @p body (may be NULL), and
 * reads + decrypts the response, handing every plaintext chunk to @p sink.
 * @p io->ctx is updated in place on reconnect; the CALLER owns the final
 * socket's teardown (io->ctx) and installs the transport's own offload on
 * io->offload before calling.
 * @return TIKU_KITS_NET_OK, or a negative TIKU_KITS_NET_ERR_HTTP_* error.
 */
int8_t tiku_kits_net_http_cert_exchange(
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
    void *sink_ctx);

#endif /* TIKU_KITS_NET_HTTP_CERT_ENABLE */

/*---------------------------------------------------------------------------*/
/* HTTP CLIENT                                                               */
/*---------------------------------------------------------------------------*/

#if TIKU_KITS_NET_HTTP_ENABLE

/*
 * Drives the complete sequence: DNS resolve, TCP connect,
 * TLS handshake, send request + JSON body, receive and parse
 * response.  Blocks until the transaction completes or times
 * out.  Internally polls the SLIP link to advance the
 * network stack.
 * On success (HTTP 200), the response body is copied into
 * response_buf and response_len is set.  On non-200 status,
 * returns TIKU_KITS_NET_ERR_HTTP_STATUS and the caller can
 * retrieve the status code via get_status_code().
 */

/**
 * @brief Perform an HTTP/1.1 POST over TLS.
 *
 * @param tls           Trust model (NULL = PSK); CERT reaches the public web
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
    const tiku_kits_net_http_tls_t *tls,
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
 * @param tls           Trust model (NULL = PSK); CERT reaches the public web
 * @param host          Hostname to connect to
 * @param path          Request path
 * @param api_key       API key (NULL to omit)
 * @param response_buf  Buffer for response body
 * @param response_max  Capacity of response_buf
 * @param response_len  Output: bytes written to response_buf
 * @return TIKU_KITS_NET_OK on 200, negative error otherwise
 */
int8_t tiku_kits_net_http_get(
    const tiku_kits_net_http_tls_t *tls,
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
