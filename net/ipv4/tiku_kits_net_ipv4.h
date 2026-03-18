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
#define TIKU_KITS_NET_IPV4_HDR_LEN      20   /**< Minimum header length */

/*---------------------------------------------------------------------------*/
/* IPv4 ACCESSOR MACROS                                                      */
/*---------------------------------------------------------------------------*/

/** Extract IP version (upper 4 bits of byte 0) */
#define TIKU_KITS_NET_IPV4_VER(pkt) \
    ((uint8_t)((pkt)[TIKU_KITS_NET_IPV4_OFF_VER_IHL] >> 4))

/** Extract IHL in 32-bit words (lower 4 bits of byte 0) */
#define TIKU_KITS_NET_IPV4_IHL(pkt) \
    ((uint8_t)((pkt)[TIKU_KITS_NET_IPV4_OFF_VER_IHL] & 0x0F))

/** Extract protocol field */
#define TIKU_KITS_NET_IPV4_PROTO(pkt) \
    ((pkt)[TIKU_KITS_NET_IPV4_OFF_PROTO])

/** Extract total length (network byte order -> host) */
#define TIKU_KITS_NET_IPV4_TOTLEN(pkt) \
    ((uint16_t)((uint16_t)(pkt)[TIKU_KITS_NET_IPV4_OFF_TOTLEN] << 8 | \
                (pkt)[TIKU_KITS_NET_IPV4_OFF_TOTLEN + 1]))

/** Pointer to source address (4 bytes at offset 12) */
#define TIKU_KITS_NET_IPV4_SRC(pkt) \
    (&(pkt)[TIKU_KITS_NET_IPV4_OFF_SRC])

/** Pointer to destination address (4 bytes at offset 16) */
#define TIKU_KITS_NET_IPV4_DST(pkt) \
    (&(pkt)[TIKU_KITS_NET_IPV4_OFF_DST])

/*---------------------------------------------------------------------------*/
/* PROTOCOL NUMBERS                                                          */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_NET_IPV4_PROTO_ICMP   1

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief RFC 1071 ones-complement checksum.
 *
 * Computes the Internet checksum over @p len bytes of @p data using
 * a uint32_t accumulator with byte-pair processing.  Reused for both
 * IPv4 header and ICMP checksums.
 *
 * @param data  Pointer to the data
 * @param len   Length in bytes
 * @return 16-bit checksum in network byte order
 */
uint16_t tiku_kits_net_ipv4_chksum(const uint8_t *data, uint16_t len);

/**
 * @brief Process an incoming IPv4 packet.
 *
 * Validates version, IHL, length, header checksum, and destination
 * address.  Dispatches to the appropriate upper-layer handler (ICMP).
 * Packets that fail validation are silently dropped.
 *
 * @param buf  Packet buffer (will be modified in-place for replies)
 * @param len  Packet length in bytes
 */
void tiku_kits_net_ipv4_input(uint8_t *buf, uint16_t len);

/**
 * @brief Transmit an IPv4 packet through the active link layer.
 *
 * Recomputes the IPv4 header checksum and calls link->send().
 *
 * @param buf  Packet buffer (header checksum field will be updated)
 * @param len  Total packet length in bytes
 * @return TIKU_KITS_NET_OK on success, negative error code otherwise
 */
int8_t tiku_kits_net_ipv4_output(uint8_t *buf, uint16_t len);

/**
 * @brief Set the active link-layer backend.
 *
 * @param link  Link-layer backend (caller keeps ownership)
 */
void tiku_kits_net_ipv4_set_link(const tiku_kits_net_link_t *link);

/** The net process (defined in tiku_kits_net_ipv4.c) */
extern struct tiku_process tiku_kits_net_process;

#endif /* TIKU_KITS_NET_IPV4_H_ */
