/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_ipv4.h - IPv4 header access, checksum, input/output
 *
 * Uses byte-offset macros instead of packed structs to avoid MSP430
 * alignment issues and ensure portability across TI and GCC compilers.
 * The IPv4 input pipeline validates version, IHL, header checksum,
 * and destination address, then dispatches by protocol number to the
 * appropriate upper-layer handler (ICMP, UDP).
 *
 * A single static buffer (net_buf, TIKU_KITS_NET_MTU bytes) serves
 * as both receive and transmit storage in a half-duplex cycle:
 *   RX -> validate -> process/reply-in-place -> TX -> reset.
 * Two accessor functions (get_buf, get_addr) expose it to upper
 * layers that need to construct outgoing packets (e.g. UDP send).
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

#ifndef TIKU_KITS_NET_IPV4_H_
#define TIKU_KITS_NET_IPV4_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"

/*---------------------------------------------------------------------------*/
/* IPv4 HEADER BYTE OFFSETS                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_IPV4_OFFSETS IPv4 Header Byte Offsets
 * @brief Offsets into the raw packet buffer for each IPv4 header field.
 *
 * These offsets assume IHL=5 (no options, 20-byte header).  Fields
 * wider than one byte are stored in network byte order (big-endian).
 * Use the accessor macros below for convenient extraction.
 * @{
 */
#define TIKU_KITS_NET_IPV4_OFF_VER_IHL   0   /**< Version (4b) + IHL (4b) */
#define TIKU_KITS_NET_IPV4_OFF_TOS       1   /**< Type of Service */
#define TIKU_KITS_NET_IPV4_OFF_TOTLEN    2   /**< Total Length (16b) */
#define TIKU_KITS_NET_IPV4_OFF_ID        4   /**< Identification (16b) */
#define TIKU_KITS_NET_IPV4_OFF_FRAG      6   /**< Flags + Fragment Offset */
#define TIKU_KITS_NET_IPV4_OFF_TTL       8   /**< Time to Live */
#define TIKU_KITS_NET_IPV4_OFF_PROTO     9   /**< Protocol */
#define TIKU_KITS_NET_IPV4_OFF_CHKSUM   10   /**< Header Checksum (16b) */
#define TIKU_KITS_NET_IPV4_OFF_SRC      12   /**< Source Address (32b) */
#define TIKU_KITS_NET_IPV4_OFF_DST      16   /**< Destination Address (32b) */
#define TIKU_KITS_NET_IPV4_HDR_LEN      20   /**< Minimum header (IHL=5) */
/** @} */

/*---------------------------------------------------------------------------*/
/* IPv4 ACCESSOR MACROS                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Extract IP version (upper 4 bits of byte 0).
 * @param pkt  Pointer to the start of the IPv4 header
 * @return IP version number (should be 4)
 */
#define TIKU_KITS_NET_IPV4_VER(pkt) \
    ((uint8_t)((pkt)[TIKU_KITS_NET_IPV4_OFF_VER_IHL] >> 4))

/**
 * @brief Extract IHL in 32-bit words (lower 4 bits of byte 0).
 *
 * Multiply by 4 to get the header length in bytes.  Minimum valid
 * value is 5 (20 bytes); values 6-15 indicate IP options are present.
 *
 * @param pkt  Pointer to the start of the IPv4 header
 * @return IHL in 32-bit words (5-15)
 */
#define TIKU_KITS_NET_IPV4_IHL(pkt) \
    ((uint8_t)((pkt)[TIKU_KITS_NET_IPV4_OFF_VER_IHL] & 0x0F))

/**
 * @brief Extract the protocol field (byte 9).
 * @param pkt  Pointer to the start of the IPv4 header
 * @return Protocol number (1=ICMP, 17=UDP, etc.)
 */
#define TIKU_KITS_NET_IPV4_PROTO(pkt) \
    ((pkt)[TIKU_KITS_NET_IPV4_OFF_PROTO])

/**
 * @brief Extract total length in host byte order.
 *
 * Reads the 16-bit total length field from network byte order and
 * converts to a uint16_t in host byte order.  Includes the IPv4
 * header itself (20 bytes minimum) plus all payload.
 *
 * @param pkt  Pointer to the start of the IPv4 header
 * @return Total packet length in bytes
 */
#define TIKU_KITS_NET_IPV4_TOTLEN(pkt) \
    ((uint16_t)((uint16_t)(pkt)[TIKU_KITS_NET_IPV4_OFF_TOTLEN] << 8 | \
                (pkt)[TIKU_KITS_NET_IPV4_OFF_TOTLEN + 1]))

/**
 * @brief Pointer to the 4-byte source address at offset 12.
 * @param pkt  Pointer to the start of the IPv4 header
 * @return Pointer to the first byte of the source IP address
 */
#define TIKU_KITS_NET_IPV4_SRC(pkt) \
    (&(pkt)[TIKU_KITS_NET_IPV4_OFF_SRC])

/**
 * @brief Pointer to the 4-byte destination address at offset 16.
 * @param pkt  Pointer to the start of the IPv4 header
 * @return Pointer to the first byte of the destination IP address
 */
#define TIKU_KITS_NET_IPV4_DST(pkt) \
    (&(pkt)[TIKU_KITS_NET_IPV4_OFF_DST])

/*---------------------------------------------------------------------------*/
/* PROTOCOL NUMBERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_IPV4_PROTO IPv4 Protocol Numbers
 * @brief IANA protocol numbers used in the IPv4 header protocol field.
 * @{
 */
#define TIKU_KITS_NET_IPV4_PROTO_ICMP   1   /**< ICMP (RFC 792) */
#define TIKU_KITS_NET_IPV4_PROTO_TCP    6   /**< TCP (RFC 793) */
#define TIKU_KITS_NET_IPV4_PROTO_UDP   17   /**< UDP (RFC 768) */
/** @} */

/*---------------------------------------------------------------------------*/
/* CHECKSUM                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief RFC 1071 ones-complement checksum.
 *
 * Computes the Internet checksum over @p len bytes of @p data using
 * a uint32_t accumulator with byte-pair processing and a final fold.
 * Reused for IPv4 header, ICMP, and UDP pseudo-header checksums.
 *
 * To verify a received header, compute the checksum over the entire
 * header (including the checksum field); the result should be 0.
 *
 * To compute a checksum for transmission, zero the checksum field,
 * compute, and write the result back.
 *
 * @param data  Pointer to the data to checksum
 * @param len   Length in bytes (odd trailing byte handled correctly)
 * @return 16-bit checksum in network byte order
 */
uint16_t tiku_kits_net_ipv4_chksum(const uint8_t *data, uint16_t len);

/*---------------------------------------------------------------------------*/
/* INPUT / OUTPUT                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an incoming IPv4 packet.
 *
 * Validates the packet through eight checks in order:
 *   1. Minimum length (>= 20 bytes)
 *   2. Version == 4
 *   3. IHL >= 5 (header >= 20 bytes)
 *   4. IHL <= frame length (prevents buffer over-read)
 *   5. Header checksum == 0
 *   6. total_len is plausible (>= IHL bytes, <= frame length)
 *   7. Packet is not fragmented (MF=0, offset=0)
 *   8. Destination IP matches our address
 *
 * Packets that fail any check are silently dropped (no ICMP error).
 * Valid packets are dispatched by protocol field using total_len
 * (trimming any link-layer padding):
 *   - Protocol 1 (ICMP): tiku_kits_net_icmp_input()
 *   - Protocol 17 (UDP): tiku_kits_net_udp_input()
 *   - Other: silently dropped
 *
 * @param buf  Packet buffer (will be modified in-place for replies)
 * @param len  Packet length in bytes
 */
void tiku_kits_net_ipv4_input(uint8_t *buf, uint16_t len);

/**
 * @brief Transmit an IPv4 packet through the active link layer.
 *
 * Recomputes the IPv4 header checksum (zeroes the field, runs
 * the RFC 1071 algorithm, writes the result) and then calls the
 * active link layer's send() function.
 *
 * @param buf  Packet buffer (header checksum field will be updated)
 * @param len  Total packet length in bytes
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NOLINK if no link is registered
 */
int8_t tiku_kits_net_ipv4_output(uint8_t *buf, uint16_t len);

/*---------------------------------------------------------------------------*/
/* LINK MANAGEMENT                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the active link-layer backend.
 *
 * The caller retains ownership of the link struct; it must remain
 * valid for the lifetime of the net process.  Call once during init
 * (the net process uses SLIP by default).
 *
 * @param link  Link-layer backend descriptor
 */
void tiku_kits_net_ipv4_set_link(const tiku_kits_net_link_t *link);

/*---------------------------------------------------------------------------*/
/* BUFFER ACCESSORS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get a pointer to the shared packet buffer.
 *
 * Upper-layer protocols (e.g. UDP send) use this to construct
 * outgoing packets in-place.  The buffer is MTU bytes long and
 * is shared with the receive path, so it must not be called from
 * inside a receive callback (the buffer already holds the incoming
 * packet at that point).
 *
 * @param size  If non-NULL, receives the buffer capacity (MTU)
 * @return Pointer to the shared net_buf
 */
uint8_t *tiku_kits_net_ipv4_get_buf(uint16_t *size);

/**
 * @brief Get a pointer to our IPv4 address.
 *
 * Returns a pointer to 4 bytes in network order, matching the
 * TIKU_KITS_NET_IP_ADDR configuration macro.  Used by upper-layer
 * protocols to fill the source address in outgoing packets.
 *
 * @return Pointer to 4-byte address (network order, static storage)
 */
const uint8_t *tiku_kits_net_ipv4_get_addr(void);

/**
 * @brief Update our IPv4 address at runtime.
 *
 * Used by DHCP (or other runtime configuration) to set the IP
 * address after obtaining a lease.  Passing NULL is a no-op.
 *
 * @param addr  New IPv4 address (4 bytes, network order)
 */
void tiku_kits_net_ipv4_set_addr(const uint8_t *addr);

/**
 * @brief Check if the SLIP receive buffer is idle (no partial frame).
 *
 * Returns 1 if net_buf_len == 0, meaning no partially-decoded SLIP
 * frame is in the buffer.  Used by DHCP to avoid retransmitting
 * (which overwrites net_buf) while an incoming response is being
 * assembled by the SLIP decoder.
 *
 * @return 1 if idle, 0 if a partial frame is in progress
 */
uint8_t tiku_kits_net_ipv4_rx_idle(void);

/*---------------------------------------------------------------------------*/
/* NET PROCESS                                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief The net process protothread.
 *
 * Initialises SLIP, registers the link backend, and enters a
 * periodic poll loop that drains UART bytes through the SLIP
 * decoder and feeds complete frames to tiku_kits_net_ipv4_input().
 * Defined in tiku_kits_net_ipv4.c; autostarted by the APP=net
 * wrapper (apps/net/tiku_app_net.c).
 */
extern struct tiku_process tiku_kits_net_process;

#endif /* TIKU_KITS_NET_IPV4_H_ */
