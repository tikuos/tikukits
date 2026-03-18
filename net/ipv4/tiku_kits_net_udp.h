/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_udp.h - UDP datagram protocol (RFC 768)
 *
 * Provides port-based demultiplexing with a static binding table,
 * an optional built-in echo service (port 7, RFC 862), and a send
 * API for constructing outgoing datagrams.  Uses byte-offset macros
 * (no packed structs) for MSP430 alignment safety.
 *
 * The receive path validates the UDP header (minimum length, length
 * field consistency, optional pseudo-header checksum), then either
 * handles the built-in echo service or dispatches to a registered
 * callback via the binding table.  Unmatched datagrams are silently
 * dropped (no ICMP port-unreachable).
 *
 * The send path constructs a complete IPv4+UDP packet in the shared
 * net_buf and transmits it.  A re-entrancy guard prevents send from
 * being called inside a receive callback (the buffer is in use).
 *
 * RAM cost: 17 bytes total (16 for binding table + 1 for guard flag).
 * No additional buffers -- all I/O reuses the shared net_buf.
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

/**
 * @defgroup TIKU_KITS_NET_UDP_OFFSETS UDP Header Byte Offsets
 * @brief Offsets into the UDP portion of the packet (starts after IHL).
 *
 * The UDP header is exactly 8 bytes: source port, destination port,
 * length, and checksum.  All multi-byte fields are in network byte
 * order (big-endian) within the packet buffer.  Payload data begins
 * immediately at offset 8.
 * @{
 */
#define TIKU_KITS_NET_UDP_OFF_SPORT    0   /**< Source Port (16b) */
#define TIKU_KITS_NET_UDP_OFF_DPORT    2   /**< Destination Port (16b) */
#define TIKU_KITS_NET_UDP_OFF_LEN      4   /**< Length (16b, hdr+data) */
#define TIKU_KITS_NET_UDP_OFF_CHKSUM   6   /**< Checksum (16b, optional) */
#define TIKU_KITS_NET_UDP_HDR_LEN      8   /**< Fixed UDP header size */
/** @} */

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of simultaneous port bindings.
 *
 * Each slot costs 4 bytes (2-byte port + 2-byte function pointer on
 * MSP430).  The default of 4 slots uses 16 bytes of RAM and is
 * sufficient for most embedded applications (e.g. one command port,
 * one telemetry port, one NTP port, one spare).
 *
 * Override at compile time to adjust:
 * @code
 *   #define TIKU_KITS_NET_UDP_MAX_BINDS 8
 *   #include "tiku_kits_net_udp.h"
 * @endcode
 */
#ifndef TIKU_KITS_NET_UDP_MAX_BINDS
#define TIKU_KITS_NET_UDP_MAX_BINDS    4
#endif

/**
 * @brief Enable the built-in UDP echo service on port 7 (RFC 862).
 *
 * When enabled, datagrams addressed to port 7 are echoed back
 * in-place (same pattern as ICMP echo) without consuming a bind
 * slot.  This provides a simple liveness check alongside ICMP ping.
 * Set to 0 at compile time to disable and save code space.
 */
#ifndef TIKU_KITS_NET_UDP_ECHO_ENABLE
#define TIKU_KITS_NET_UDP_ECHO_ENABLE  1
#endif

/** UDP echo port number (RFC 862). */
#define TIKU_KITS_NET_UDP_ECHO_PORT    7

/**
 * @brief Maximum UDP payload in bytes.
 *
 * Derived from the MTU minus the minimum IPv4 header (20 bytes)
 * and the UDP header (8 bytes).  With the default MTU of 128,
 * this yields 100 bytes of application payload per datagram.
 */
#define TIKU_KITS_NET_UDP_MAX_PAYLOAD \
    (TIKU_KITS_NET_MTU - 20 - TIKU_KITS_NET_UDP_HDR_LEN)

/*---------------------------------------------------------------------------*/
/* CALLBACK TYPE                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Receive callback type for port-bound UDP datagrams.
 *
 * Invoked by udp_input() when an incoming datagram matches a bound
 * port.  The callback runs in the context of the net process
 * protothread, so it must return promptly (no blocking operations).
 *
 * @warning The @p payload pointer points into the shared net_buf.
 * The data is only valid for the duration of the callback.  Copy
 * any bytes you need before returning.  Do NOT call udp_send()
 * from inside this callback -- the buffer is in use and the
 * re-entrancy guard will reject the call.
 *
 * @param src_addr     Pointer to 4-byte sender IP (network order,
 *                     points into the packet buffer)
 * @param src_port     Sender port number in host byte order
 * @param payload      Pointer to the UDP payload (first byte after
 *                     the 8-byte UDP header); NULL if payload_len==0
 * @param payload_len  Payload length in bytes (0 is valid)
 */
typedef void (*tiku_kits_net_udp_recv_cb_t)(
    const uint8_t *src_addr,
    uint16_t       src_port,
    const uint8_t *payload,
    uint16_t       payload_len);

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the UDP subsystem.
 *
 * Clears all binding table slots (port=0, cb=NULL) and resets the
 * re-entrancy guard.  Called once during net process startup
 * (tiku_kits_net_ipv4.c), before the poll loop begins.
 *
 * Safe to call again at runtime to unbind all ports and reset state.
 */
void tiku_kits_net_udp_init(void);

/*---------------------------------------------------------------------------*/
/* PORT BINDING                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Bind a receive callback to a UDP port.
 *
 * Registers @p cb to be invoked whenever a datagram arrives with
 * a destination port matching @p port.  The binding table is scanned
 * linearly (O(MAX_BINDS)) to check for duplicates and find a free
 * slot.
 *
 * Port 0 is reserved as the "free" sentinel and cannot be bound.
 * The echo port (7) can be bound, in which case the explicit binding
 * takes priority only if echo is disabled; with echo enabled, port 7
 * datagrams are handled by the echo service before the table lookup.
 *
 * @param port  Port number in host byte order (must be > 0)
 * @param cb    Receive callback (must not be NULL)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if @p cb is NULL, @p port is 0,
 *         or @p port is already bound,
 *         TIKU_KITS_NET_ERR_OVERFLOW if all binding slots are full.
 */
int8_t tiku_kits_net_udp_bind(uint16_t port,
                               tiku_kits_net_udp_recv_cb_t cb);

/**
 * @brief Unbind a previously bound port.
 *
 * Frees the binding table slot so it can be reused.  Linear scan
 * for the matching port (O(MAX_BINDS)).
 *
 * @param port  Port number in host byte order
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if @p port was not bound.
 */
int8_t tiku_kits_net_udp_unbind(uint16_t port);

/*---------------------------------------------------------------------------*/
/* INPUT                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an incoming UDP datagram.
 *
 * Called by the IPv4 input pipeline when protocol == 17 (UDP).
 * Performs three validation steps (silent drop on any failure):
 *   1. UDP data portion must be >= 8 bytes (header fits)
 *   2. Length field must be >= 8 and <= actual data available
 *   3. If checksum field is non-zero, verify the pseudo-header
 *      checksum (RFC 768: checksum=0 means "not computed")
 *
 * After validation, the destination port is checked:
 *   - Port 7 (echo, if enabled): reply in-place and return
 *   - Otherwise: linear scan of the binding table for a match
 *   - No match: silently drop (no ICMP port-unreachable)
 *
 * The buffer may be modified in-place during echo processing.
 *
 * @param buf      Full IP packet buffer (modified for echo replies)
 * @param len      Total packet length (IPv4 header + UDP)
 * @param ihl_len  IPv4 header length in bytes (IHL * 4)
 */
void tiku_kits_net_udp_input(uint8_t *buf, uint16_t len,
                              uint16_t ihl_len);

/*---------------------------------------------------------------------------*/
/* OUTPUT                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send a UDP datagram.
 *
 * Constructs a complete IPv4 + UDP packet in the shared net_buf
 * and transmits it via tiku_kits_net_ipv4_output().  The function
 * builds the 20-byte IPv4 header (ver=4, IHL=5, proto=17) and
 * the 8-byte UDP header, copies the payload, computes the UDP
 * pseudo-header checksum (per RFC 768, result 0 is sent as 0xFFFF),
 * and then calls ipv4_output which recomputes the IPv4 header
 * checksum and passes the packet to the active link layer.
 *
 * @warning Must NOT be called from inside a receive callback.
 * The shared net_buf holds the incoming packet during callback
 * execution, so writing to it would corrupt the RX data.  The
 * re-entrancy guard (in_rx flag) detects this and returns
 * TIKU_KITS_NET_ERR_PARAM.
 *
 * @param dst_addr     Destination IP address (4 bytes, network order)
 * @param dst_port     Destination port in host byte order
 * @param src_port     Source port in host byte order
 * @param payload      Payload bytes to send (may be NULL if len==0)
 * @param payload_len  Payload length (max TIKU_KITS_NET_UDP_MAX_PAYLOAD)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if called from a callback,
 *         TIKU_KITS_NET_ERR_NULL if @p dst_addr is NULL or
 *         @p payload is NULL with payload_len > 0,
 *         TIKU_KITS_NET_ERR_OVERFLOW if payload exceeds max,
 *         TIKU_KITS_NET_ERR_NOLINK if no link backend is set.
 */
int8_t tiku_kits_net_udp_send(const uint8_t *dst_addr,
                               uint16_t dst_port,
                               uint16_t src_port,
                               const uint8_t *payload,
                               uint16_t payload_len);

#endif /* TIKU_KITS_NET_UDP_H_ */
