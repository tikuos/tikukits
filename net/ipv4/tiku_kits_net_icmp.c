/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_icmp.c - ICMP echo reply handler
 *
 * Implements the in-place echo reply strategy: the incoming echo
 * request is transformed into an echo reply within the same buffer,
 * avoiding any additional RAM.  Only the ICMP type field, IP
 * addresses, and checksums are modified; the identifier, sequence
 * number, and payload are preserved verbatim.
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_net_icmp.h"
#include "tiku_kits_net_ipv4.h"

/*---------------------------------------------------------------------------*/
/* INPUT HANDLER                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Validate and reply to an incoming ICMP echo request
 *
 * Three validation steps (silent drop on any failure):
 *   1. ICMP data must be at least ICMP_HDR_LEN (8) bytes
 *   2. Ones-complement checksum over entire ICMP data must be 0
 *   3. Message type must be ECHO_REQUEST (8)
 *
 * Reply is built in-place: swap IPs, set type=0, recompute ICMP
 * checksum, then call ipv4_output.  The 4-byte tmp array on the
 * stack holds one IP address during the swap.
 */
void
tiku_kits_net_icmp_input(uint8_t *buf, uint16_t len, uint16_t ihl_len)
{
    uint8_t *icmp;
    uint16_t icmp_len;
    uint16_t chksum;
    uint8_t tmp[4];

    icmp = buf + ihl_len;
    icmp_len = len - ihl_len;

    /* Validate minimum ICMP length */
    if (icmp_len < TIKU_KITS_NET_ICMP_HDR_LEN) {
        return;
    }

    /* Verify ICMP checksum (should fold to 0 if valid) */
    chksum = tiku_kits_net_ipv4_chksum(icmp, icmp_len);
    if (chksum != 0) {
        return;
    }

    /* Only handle echo requests (type 8); drop everything else */
    if (icmp[TIKU_KITS_NET_ICMP_OFF_TYPE] != TIKU_KITS_NET_ICMP_ECHO_REQUEST) {
        return;
    }

    /* --- Build echo reply in-place --- */

    /* Swap source and destination IP addresses using a 4-byte
     * temporary buffer on the stack (cheaper than XOR swap on
     * MSP430 due to byte-addressable memory) */
    tmp[0] = buf[TIKU_KITS_NET_IPV4_OFF_SRC];
    tmp[1] = buf[TIKU_KITS_NET_IPV4_OFF_SRC + 1];
    tmp[2] = buf[TIKU_KITS_NET_IPV4_OFF_SRC + 2];
    tmp[3] = buf[TIKU_KITS_NET_IPV4_OFF_SRC + 3];

    buf[TIKU_KITS_NET_IPV4_OFF_SRC]     = buf[TIKU_KITS_NET_IPV4_OFF_DST];
    buf[TIKU_KITS_NET_IPV4_OFF_SRC + 1] = buf[TIKU_KITS_NET_IPV4_OFF_DST + 1];
    buf[TIKU_KITS_NET_IPV4_OFF_SRC + 2] = buf[TIKU_KITS_NET_IPV4_OFF_DST + 2];
    buf[TIKU_KITS_NET_IPV4_OFF_SRC + 3] = buf[TIKU_KITS_NET_IPV4_OFF_DST + 3];

    buf[TIKU_KITS_NET_IPV4_OFF_DST]     = tmp[0];
    buf[TIKU_KITS_NET_IPV4_OFF_DST + 1] = tmp[1];
    buf[TIKU_KITS_NET_IPV4_OFF_DST + 2] = tmp[2];
    buf[TIKU_KITS_NET_IPV4_OFF_DST + 3] = tmp[3];

    /* Change type from echo request (8) to echo reply (0) */
    icmp[TIKU_KITS_NET_ICMP_OFF_TYPE] = TIKU_KITS_NET_ICMP_ECHO_REPLY;
    icmp[TIKU_KITS_NET_ICMP_OFF_CODE] = 0;

    /* Recompute ICMP checksum over the modified ICMP data
     * (type changed from 8 to 0, everything else unchanged) */
    icmp[TIKU_KITS_NET_ICMP_OFF_CHKSUM]     = 0;
    icmp[TIKU_KITS_NET_ICMP_OFF_CHKSUM + 1] = 0;
    chksum = tiku_kits_net_ipv4_chksum(icmp, icmp_len);
    icmp[TIKU_KITS_NET_ICMP_OFF_CHKSUM]     = (uint8_t)(chksum >> 8);
    icmp[TIKU_KITS_NET_ICMP_OFF_CHKSUM + 1] = (uint8_t)(chksum & 0xFF);

    /* Send the reply (ipv4_output recomputes the IPv4 header checksum
     * to account for the swapped addresses) */
    tiku_kits_net_ipv4_output(buf, len);
}
