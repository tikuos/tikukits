/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_coap.h - CoAP client/server (RFC 7252)
 *
 * Lightweight Constrained Application Protocol implementation for
 * ultra-low-power microcontrollers.  Supports both client and server
 * roles (peer model) with Confirmable and Non-confirmable messages,
 * GET/PUT/POST/DELETE methods, token matching, and simple CON
 * retransmission with exponential backoff.
 *
 * Designed for the TikuOS networking stack constraints:
 *   - Static memory allocation only (no heap)
 *   - Single shared packet buffer (half-duplex RX/TX)
 *   - Cannot call udp_send() from inside a receive callback
 *   - 100-byte maximum UDP payload (128-byte MTU)
 *
 * Uses the same poll-based architecture as TFTP and NTP: the UDP
 * receive callback copies incoming data into static state, and the
 * application calls coap_poll() from its own context to drive
 * message processing and transmission.
 *
 * Typical server usage:
 *   tiku_kits_net_coap_init();
 *   tiku_kits_net_coap_resource_register("/temp", temp_handler);
 *   // In protothread loop:
 *   tiku_kits_net_coap_poll();
 *
 * Typical client usage:
 *   tiku_kits_net_coap_init();
 *   tiku_kits_net_coap_get(server, 5683, "/temp",
 *                          TIKU_KITS_NET_COAP_TYPE_CON,
 *                          my_response_cb);
 *   // In protothread loop:
 *   tiku_kits_net_coap_poll();
 *
 * RAM budget (~250 bytes):
 *   rx_buf[100]  -- received message copy           100 B
 *   tx_buf[100]  -- outgoing CON for retransmit     100 B
 *   CON state    -- msg_id, token, timer, callback   24 B
 *   Resources[4] -- path + handler pointers          16 B
 *   Misc         -- counters, flags                  10 B
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_NET_COAP_H_
#define TIKU_KITS_NET_COAP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <tikukits/net/tiku_kits_net.h>
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* CoAP HEADER CONSTANTS (RFC 7252 Section 3)                                */
/*---------------------------------------------------------------------------*/

/** CoAP version (always 1). */
#define TIKU_KITS_NET_COAP_VERSION          1

/** Fixed CoAP header size (4 bytes). */
#define TIKU_KITS_NET_COAP_HDR_LEN          4

/** Payload marker byte separating options from payload. */
#define TIKU_KITS_NET_COAP_PAYLOAD_MARKER   0xFF

/*---------------------------------------------------------------------------*/
/* MESSAGE TYPES (RFC 7252 Section 3)                                        */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_NET_COAP_TYPE_CON         0  /**< Confirmable */
#define TIKU_KITS_NET_COAP_TYPE_NON         1  /**< Non-confirmable */
#define TIKU_KITS_NET_COAP_TYPE_ACK         2  /**< Acknowledgement */
#define TIKU_KITS_NET_COAP_TYPE_RST         3  /**< Reset */

/*---------------------------------------------------------------------------*/
/* METHOD AND RESPONSE CODES (RFC 7252 Section 5.8-5.9)                      */
/*---------------------------------------------------------------------------*/

/** Encode class.detail into a single byte. */
#define TIKU_KITS_NET_COAP_CODE(cls, det)   \
    ((uint8_t)(((cls) << 5) | (det)))

#define TIKU_KITS_NET_COAP_CODE_EMPTY       \
    TIKU_KITS_NET_COAP_CODE(0, 0)

/* Request methods (class 0) */
#define TIKU_KITS_NET_COAP_METHOD_GET       \
    TIKU_KITS_NET_COAP_CODE(0, 1)
#define TIKU_KITS_NET_COAP_METHOD_POST      \
    TIKU_KITS_NET_COAP_CODE(0, 2)
#define TIKU_KITS_NET_COAP_METHOD_PUT       \
    TIKU_KITS_NET_COAP_CODE(0, 3)
#define TIKU_KITS_NET_COAP_METHOD_DELETE    \
    TIKU_KITS_NET_COAP_CODE(0, 4)

/* Success 2.xx */
#define TIKU_KITS_NET_COAP_RESP_CREATED     \
    TIKU_KITS_NET_COAP_CODE(2, 1)
#define TIKU_KITS_NET_COAP_RESP_DELETED     \
    TIKU_KITS_NET_COAP_CODE(2, 2)
#define TIKU_KITS_NET_COAP_RESP_VALID       \
    TIKU_KITS_NET_COAP_CODE(2, 3)
#define TIKU_KITS_NET_COAP_RESP_CHANGED     \
    TIKU_KITS_NET_COAP_CODE(2, 4)
#define TIKU_KITS_NET_COAP_RESP_CONTENT     \
    TIKU_KITS_NET_COAP_CODE(2, 5)

/* Client Error 4.xx */
#define TIKU_KITS_NET_COAP_RESP_BAD_REQ     \
    TIKU_KITS_NET_COAP_CODE(4, 0)
#define TIKU_KITS_NET_COAP_RESP_NOT_FOUND   \
    TIKU_KITS_NET_COAP_CODE(4, 4)
#define TIKU_KITS_NET_COAP_RESP_METHOD_NA   \
    TIKU_KITS_NET_COAP_CODE(4, 5)

/* Server Error 5.xx */
#define TIKU_KITS_NET_COAP_RESP_INTERNAL    \
    TIKU_KITS_NET_COAP_CODE(5, 0)
#define TIKU_KITS_NET_COAP_RESP_NOT_IMPL    \
    TIKU_KITS_NET_COAP_CODE(5, 1)

/*---------------------------------------------------------------------------*/
/* OPTION NUMBERS (RFC 7252 Section 5.10)                                    */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_NET_COAP_OPT_URI_HOST      3
#define TIKU_KITS_NET_COAP_OPT_URI_PORT      7
#define TIKU_KITS_NET_COAP_OPT_URI_PATH     11
#define TIKU_KITS_NET_COAP_OPT_CONTENT_FMT  12
#define TIKU_KITS_NET_COAP_OPT_URI_QUERY    15
#define TIKU_KITS_NET_COAP_OPT_ACCEPT       17

/*---------------------------------------------------------------------------*/
/* CONTENT-FORMAT VALUES (RFC 7252 Section 12.3)                             */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_NET_COAP_FMT_TEXT          0  /**< text/plain */
#define TIKU_KITS_NET_COAP_FMT_LINK        40  /**< application/link-format */
#define TIKU_KITS_NET_COAP_FMT_OCTET       42  /**< application/octet-stream */
#define TIKU_KITS_NET_COAP_FMT_JSON        50  /**< application/json */

/*---------------------------------------------------------------------------*/
/* CONFIGURATION (compile-time overrideable)                                 */
/*---------------------------------------------------------------------------*/

/** Maximum token length (RFC 7252 allows 0-8). */
#ifndef TIKU_KITS_NET_COAP_MAX_TOKEN_LEN
#define TIKU_KITS_NET_COAP_MAX_TOKEN_LEN    4
#endif

/** Server-side resource handler slots. */
#ifndef TIKU_KITS_NET_COAP_MAX_RESOURCES
#define TIKU_KITS_NET_COAP_MAX_RESOURCES    4
#endif

/** Maximum CON retransmissions (RFC 7252 default: 4). */
#ifndef TIKU_KITS_NET_COAP_MAX_RETRANSMIT
#define TIKU_KITS_NET_COAP_MAX_RETRANSMIT   4
#endif

/** Initial ACK timeout in clock ticks (RFC 7252 default: 2 s).
 *  Defined in the .c file where the clock header is available.
 *  Override at compile time with -D if needed. */

/** UDP port for CoAP (both client source and server listen). */
#ifndef TIKU_KITS_NET_COAP_PORT
#define TIKU_KITS_NET_COAP_PORT             5683
#endif

/** Maximum URI path length (excluding NUL). */
#ifndef TIKU_KITS_NET_COAP_MAX_URI_PATH
#define TIKU_KITS_NET_COAP_MAX_URI_PATH     31
#endif

/*---------------------------------------------------------------------------*/
/* PARSED MESSAGE STRUCTURE                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parsed CoAP message.
 *
 * Populated from the raw bytes in the receive buffer.  The payload
 * pointer is only valid until the next coap_poll() call.
 */
typedef struct {
    uint8_t  type;          /**< CON / NON / ACK / RST */
    uint8_t  code;          /**< Method or response code */
    uint16_t msg_id;        /**< Message ID */
    uint8_t  token_len;     /**< Token length (0-MAX_TOKEN_LEN) */
    uint8_t  token[TIKU_KITS_NET_COAP_MAX_TOKEN_LEN];

    char     uri_path[TIKU_KITS_NET_COAP_MAX_URI_PATH + 1];
    int16_t  content_format; /**< -1 if absent */

    const uint8_t *payload; /**< Points into rx_buf (NULL if none) */
    uint16_t payload_len;

    uint8_t  src_addr[4];   /**< Sender IP (network order) */
    uint16_t src_port;      /**< Sender port (host order) */
} tiku_kits_net_coap_msg_t;

/*---------------------------------------------------------------------------*/
/* RESPONSE BUILDER                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Response descriptor filled by a resource handler.
 *
 * The handler sets the code and optional payload.  coap_poll()
 * serialises the response and sends it as a piggybacked ACK
 * (for CON requests) or NON (for NON requests).
 */
typedef struct {
    uint8_t        code;           /**< Response code (RESP_* macros) */
    const uint8_t *payload;        /**< NULL if none */
    uint16_t       payload_len;
    int16_t        content_format; /**< -1 to omit */
} tiku_kits_net_coap_resp_t;

/*---------------------------------------------------------------------------*/
/* CALLBACK TYPES                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Client-side response callback.
 *
 * Called from coap_poll() when a matching response arrives.
 * Called with NULL on timeout.
 */
typedef void (*tiku_kits_net_coap_response_cb_t)(
    const tiku_kits_net_coap_msg_t *msg);

/**
 * @brief Server-side resource handler.
 *
 * Called from coap_poll() when a request matches a registered path.
 * The handler inspects the request and fills the response descriptor.
 */
typedef void (*tiku_kits_net_coap_resource_cb_t)(
    const tiku_kits_net_coap_msg_t *req,
    tiku_kits_net_coap_resp_t      *resp);

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the CoAP module and bind the UDP port.
 *
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_OVERFLOW if the UDP bind table is full.
 */
int8_t tiku_kits_net_coap_init(void);

/*---------------------------------------------------------------------------*/
/* SERVER API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Register a resource handler for a URI path.
 *
 * @param path     URI path (e.g. "/temp").  Must be static storage
 *                 (pointer stored, not copied).
 * @param handler  Resource handler callback.
 * @return TIKU_KITS_NET_OK,
 *         TIKU_KITS_NET_ERR_PARAM if NULL or duplicate,
 *         TIKU_KITS_NET_ERR_OVERFLOW if table full.
 */
int8_t tiku_kits_net_coap_resource_register(
    const char                       *path,
    tiku_kits_net_coap_resource_cb_t  handler);

/**
 * @brief Unregister a resource handler.
 */
int8_t tiku_kits_net_coap_resource_unregister(const char *path);

/*---------------------------------------------------------------------------*/
/* CLIENT API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send a CoAP GET request.
 *
 * Only one outstanding CON at a time.  NON requests do not
 * consume the transaction slot.
 */
int8_t tiku_kits_net_coap_get(
    const uint8_t                     *dst_addr,
    uint16_t                           dst_port,
    const char                        *uri_path,
    uint8_t                            type,
    tiku_kits_net_coap_response_cb_t   cb);

/**
 * @brief Send a CoAP PUT request with payload.
 */
int8_t tiku_kits_net_coap_put(
    const uint8_t                     *dst_addr,
    uint16_t                           dst_port,
    const char                        *uri_path,
    uint8_t                            type,
    const uint8_t                     *payload,
    uint16_t                           payload_len,
    int16_t                            content_fmt,
    tiku_kits_net_coap_response_cb_t   cb);

/**
 * @brief Send a CoAP POST request with payload.
 */
int8_t tiku_kits_net_coap_post(
    const uint8_t                     *dst_addr,
    uint16_t                           dst_port,
    const char                        *uri_path,
    uint8_t                            type,
    const uint8_t                     *payload,
    uint16_t                           payload_len,
    int16_t                            content_fmt,
    tiku_kits_net_coap_response_cb_t   cb);

/**
 * @brief Send a CoAP DELETE request.
 */
int8_t tiku_kits_net_coap_delete(
    const uint8_t                     *dst_addr,
    uint16_t                           dst_port,
    const char                        *uri_path,
    uint8_t                            type,
    tiku_kits_net_coap_response_cb_t   cb);

/*---------------------------------------------------------------------------*/
/* POLLING                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Poll for incoming messages and CON retransmit timeouts.
 *
 * Must be called periodically from application context (~500 ms).
 *
 * Processing order:
 *   1. If rx_pending: parse, match ACK/response to outstanding CON,
 *      or dispatch to resource handler and send piggybacked reply.
 *   2. If CON retransmit timer expired: retransmit or signal timeout.
 */
void tiku_kits_net_coap_poll(void);

/*---------------------------------------------------------------------------*/
/* SHUTDOWN                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Unbind the UDP port and clear all state.
 */
void tiku_kits_net_coap_shutdown(void);

#endif /* TIKU_KITS_NET_COAP_H_ */
