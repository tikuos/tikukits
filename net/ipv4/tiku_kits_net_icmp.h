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

#define TIKU_KITS_NET_ICMP_OFF_TYPE     0   /**< Message type */
#define TIKU_KITS_NET_ICMP_OFF_CODE     1   /**< Message code */
#define TIKU_KITS_NET_ICMP_OFF_CHKSUM   2   /**< Checksum (16b) */
#define TIKU_KITS_NET_ICMP_OFF_ID       4   /**< Identifier (16b) */
#define TIKU_KITS_NET_ICMP_OFF_SEQ      6   /**< Sequence Number (16b) */
#define TIKU_KITS_NET_ICMP_OFF_DATA     8   /**< Start of echo data */
#define TIKU_KITS_NET_ICMP_HDR_LEN      8   /**< Minimum ICMP header */

/*---------------------------------------------------------------------------*/
/* ICMP MESSAGE TYPES                                                        */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_NET_ICMP_ECHO_REPLY   0   /**< Echo Reply */
#define TIKU_KITS_NET_ICMP_ECHO_REQUEST 8   /**< Echo Request */

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Handle an incoming ICMP message.
 *
 * Called by the IPv4 input pipeline when protocol == 1 (ICMP).
 * Validates length and checksum.  If the message is an echo request,
 * builds an echo reply in-place (swaps src/dst IP, sets type=0,
 * recomputes ICMP checksum) and sends via tiku_kits_net_ipv4_output().
 *
 * @param buf      Full IP packet buffer (modified in-place for reply)
 * @param len      Total packet length (IP header + ICMP)
 * @param ihl_len  IPv4 header length in bytes (IHL * 4)
 */
void tiku_kits_net_icmp_input(uint8_t *buf, uint16_t len,
                              uint16_t ihl_len);

#endif /* TIKU_KITS_NET_ICMP_H_ */
