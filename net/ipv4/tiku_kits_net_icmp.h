/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_icmp.h - ICMP echo request/reply (ping)
 *
 * Handles incoming ICMP echo requests by building an echo reply
 * in-place (zero extra buffers) and sending it via IPv4 output.
 * Only echo request (type 8) is processed; all other ICMP types
 * are silently dropped.
 *
 * The in-place reply strategy swaps source/destination IP addresses,
 * changes the ICMP type from 8 to 0, recomputes the ICMP checksum,
 * and passes the same buffer to ipv4_output for transmission.  This
 * avoids a second buffer copy and keeps RAM usage at zero beyond
 * the shared net_buf.
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

#ifndef TIKU_KITS_NET_ICMP_H_
#define TIKU_KITS_NET_ICMP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"

/*---------------------------------------------------------------------------*/
/* ICMP HEADER BYTE OFFSETS                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_ICMP_OFFSETS ICMP Header Byte Offsets
 * @brief Offsets into the ICMP portion of the packet (starts after IHL).
 *
 * The echo request/reply header is 8 bytes: type, code, checksum,
 * identifier, and sequence number.  Any echo payload follows
 * immediately at offset 8.
 * @{
 */
#define TIKU_KITS_NET_ICMP_OFF_TYPE     0   /**< Message type (8b) */
#define TIKU_KITS_NET_ICMP_OFF_CODE     1   /**< Message code (8b) */
#define TIKU_KITS_NET_ICMP_OFF_CHKSUM   2   /**< Checksum (16b) */
#define TIKU_KITS_NET_ICMP_OFF_ID       4   /**< Identifier (16b) */
#define TIKU_KITS_NET_ICMP_OFF_SEQ      6   /**< Sequence Number (16b) */
#define TIKU_KITS_NET_ICMP_OFF_DATA     8   /**< Start of echo data */
#define TIKU_KITS_NET_ICMP_HDR_LEN      8   /**< Minimum ICMP header */
/** @} */

/*---------------------------------------------------------------------------*/
/* ICMP MESSAGE TYPES                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_ICMP_TYPES ICMP Message Types
 * @brief Only echo request/reply are handled; all others are dropped.
 * @{
 */
#define TIKU_KITS_NET_ICMP_ECHO_REPLY   0   /**< Echo Reply (type 0) */
#define TIKU_KITS_NET_ICMP_ECHO_REQUEST 8   /**< Echo Request (type 8) */
/** @} */

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Handle an incoming ICMP message.
 *
 * Called by the IPv4 input pipeline when protocol == 1 (ICMP).
 * Validates the ICMP header:
 *   1. Checks minimum length (>= 8 bytes of ICMP data)
 *   2. Verifies the ICMP checksum (ones-complement over ICMP data)
 *   3. Checks message type == 8 (echo request)
 *
 * If all checks pass, builds an echo reply in-place:
 *   - Swaps source and destination IP addresses (4-byte temp)
 *   - Sets ICMP type to 0 (echo reply), code to 0
 *   - Preserves identifier, sequence number, and all payload data
 *   - Recomputes the ICMP checksum
 *   - Sends via tiku_kits_net_ipv4_output()
 *
 * Non-echo-request messages (type != 8) are silently dropped.
 *
 * @param buf      Full IP packet buffer (modified in-place for reply)
 * @param len      Total packet length (IP header + ICMP)
 * @param ihl_len  IPv4 header length in bytes (IHL * 4)
 */
void tiku_kits_net_icmp_input(uint8_t *buf, uint16_t len,
                              uint16_t ihl_len);

/**
 * @brief Build an ICMP echo request (ping) into a TX buffer.
 *
 * Fills a complete IPv4 + ICMP echo-request packet: the IPv4 header
 * (protocol ICMP, source = our address, destination = @p dst_ip; the IPv4
 * header checksum is left zero for tiku_kits_net_ipv4_output() to compute),
 * then the ICMP echo-request header (type 8, the given @p id / @p seq)
 * followed by @p payload_len filler bytes, with the ICMP checksum computed.
 *
 * @param buf          TX buffer, >= 28 + payload_len bytes (use
 *                     tiku_kits_net_ipv4_get_buf()).
 * @param dst_ip       4-byte destination IPv4 address.
 * @param id           ICMP identifier (echoed back in the reply).
 * @param seq          ICMP sequence number.
 * @param payload_len  Echo payload size in bytes (0 is valid).
 * @return Total IPv4 packet length to pass to tiku_kits_net_ipv4_output().
 */
uint16_t tiku_kits_net_icmp_build_echo_request(uint8_t *buf,
                                               const uint8_t *dst_ip,
                                               uint16_t id, uint16_t seq,
                                               uint16_t payload_len);

/**
 * @brief Test whether a received IPv4 packet is an ICMP echo reply for us.
 *
 * @param buf      Received IPv4 packet.
 * @param len      Packet length.
 * @param id       Identifier sent in the request.
 * @param out_seq  If non-NULL and this is a matching reply, receives the
 *                 sequence number.
 * @return 1 if @p buf is an ICMP echo reply (type 0) with id == @p id,
 *         0 otherwise.
 */
uint8_t tiku_kits_net_icmp_match_echo_reply(const uint8_t *buf, uint16_t len,
                                            uint16_t id, uint16_t *out_seq);

/**
 * @brief Callback type for delivering incoming ICMP echo replies.
 * @param src_ip  Pointer to the 4-byte source IPv4 address.
 * @param id      ICMP identifier from the reply.
 * @param seq     ICMP sequence number from the reply.
 */
typedef void (*tiku_kits_net_icmp_reply_cb_t)(const uint8_t *src_ip,
                                              uint16_t id, uint16_t seq);

/**
 * @brief Register a handler for incoming ICMP echo replies (type 0).
 *
 * When set, tiku_kits_net_icmp_input() delivers validated echo replies to
 * @p cb instead of dropping them.  Pass NULL to clear.  Used by the `ping`
 * command so replies arriving through the shared RX path reach it.
 */
void tiku_kits_net_icmp_set_reply_cb(tiku_kits_net_icmp_reply_cb_t cb);

#endif /* TIKU_KITS_NET_ICMP_H_ */
