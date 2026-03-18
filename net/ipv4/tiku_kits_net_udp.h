/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_udp.h - UDP datagram protocol (RFC 768)
 *
 * Provides port-based demultiplexing with a static binding table,
 * an optional built-in echo service (port 7), and a send API for
 * outgoing datagrams.  Uses byte-offset macros (no packed structs).
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

#ifndef TIKU_KITS_NET_UDP_H_
#define TIKU_KITS_NET_UDP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"

/*---------------------------------------------------------------------------*/
/* UDP HEADER BYTE OFFSETS                                                   */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_NET_UDP_OFF_SPORT    0   /**< Source Port (16b) */
#define TIKU_KITS_NET_UDP_OFF_DPORT    2   /**< Destination Port (16b) */
#define TIKU_KITS_NET_UDP_OFF_LEN      4   /**< Length (16b, hdr+data) */
#define TIKU_KITS_NET_UDP_OFF_CHKSUM   6   /**< Checksum (16b, optional) */
#define TIKU_KITS_NET_UDP_HDR_LEN      8   /**< Minimum UDP header */

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/** Maximum number of simultaneous port bindings. */
#ifndef TIKU_KITS_NET_UDP_MAX_BINDS
#define TIKU_KITS_NET_UDP_MAX_BINDS    4
#endif

/** Enable built-in echo service on port 7 (RFC 862). */
#ifndef TIKU_KITS_NET_UDP_ECHO_ENABLE
#define TIKU_KITS_NET_UDP_ECHO_ENABLE  1
#endif

/** UDP echo port number. */
#define TIKU_KITS_NET_UDP_ECHO_PORT    7

/** Maximum UDP payload: MTU - IPv4 header - UDP header. */
#define TIKU_KITS_NET_UDP_MAX_PAYLOAD \
    (TIKU_KITS_NET_MTU - 20 - TIKU_KITS_NET_UDP_HDR_LEN)

/*---------------------------------------------------------------------------*/
/* CALLBACK TYPE                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Receive callback invoked when a datagram arrives for a bound port.
 *
 * @param src_addr     Pointer to 4-byte sender IP (network order)
 * @param src_port     Sender port in host byte order
 * @param payload      Pointer into the shared net_buf -- DO NOT RETAIN
 * @param payload_len  Payload length in bytes
 */
typedef void (*tiku_kits_net_udp_recv_cb_t)(
    const uint8_t *src_addr,
    uint16_t       src_port,
    const uint8_t *payload,
    uint16_t       payload_len);

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the UDP subsystem.
 *
 * Clears the binding table.  Called once during net process init.
 */
void tiku_kits_net_udp_init(void);

/**
 * @brief Bind a callback to a UDP port.
 *
 * @param port  Port number (host byte order)
 * @param cb    Receive callback (must not be NULL)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if cb is NULL or port already bound,
 *         TIKU_KITS_NET_ERR_OVERFLOW if binding table is full.
 */
int8_t tiku_kits_net_udp_bind(uint16_t port,
                               tiku_kits_net_udp_recv_cb_t cb);

/**
 * @brief Unbind a previously bound port.
 *
 * @param port  Port number (host byte order)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if port was not bound.
 */
int8_t tiku_kits_net_udp_unbind(uint16_t port);

/**
 * @brief Process an incoming UDP datagram.
 *
 * Called by the IPv4 input pipeline when protocol == 17.
 * Validates header, optionally verifies checksum, handles echo,
 * then dispatches to the binding table.
 *
 * @param buf      Full IP packet buffer (modified in-place for echo)
 * @param len      Total packet length (IP header + UDP)
 * @param ihl_len  IPv4 header length in bytes (IHL * 4)
 */
void tiku_kits_net_udp_input(uint8_t *buf, uint16_t len,
                              uint16_t ihl_len);

/**
 * @brief Send a UDP datagram.
 *
 * Builds IPv4 + UDP headers in the shared net_buf and transmits.
 * Must NOT be called from inside a receive callback (buffer in use).
 *
 * @param dst_addr     Destination IP (4 bytes, network order)
 * @param dst_port     Destination port (host byte order)
 * @param src_port     Source port (host byte order)
 * @param payload      Payload bytes to send
 * @param payload_len  Payload length (max TIKU_KITS_NET_UDP_MAX_PAYLOAD)
 * @return TIKU_KITS_NET_OK on success, negative error code otherwise.
 */
int8_t tiku_kits_net_udp_send(const uint8_t *dst_addr,
                               uint16_t dst_port,
                               uint16_t src_port,
                               const uint8_t *payload,
                               uint16_t payload_len);

#endif /* TIKU_KITS_NET_UDP_H_ */
