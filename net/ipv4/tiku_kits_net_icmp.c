/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_icmp.c - ICMP echo reply handler
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
/* PUBLIC FUNCTIONS                                                          */
/*---------------------------------------------------------------------------*/

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

    /* Verify ICMP checksum */
    chksum = tiku_kits_net_ipv4_chksum(icmp, icmp_len);
    if (chksum != 0) {
        return;
    }

    /* Only handle echo requests */
    if (icmp[TIKU_KITS_NET_ICMP_OFF_TYPE] != TIKU_KITS_NET_ICMP_ECHO_REQUEST) {
        return;
    }

    /* --- Build echo reply in-place --- */

    /* Swap src and dst IP addresses (4-byte temp on stack) */
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

    /* Set ICMP type to echo reply, code to 0 */
    icmp[TIKU_KITS_NET_ICMP_OFF_TYPE] = TIKU_KITS_NET_ICMP_ECHO_REPLY;
    icmp[TIKU_KITS_NET_ICMP_OFF_CODE] = 0;

    /* Recompute ICMP checksum */
    icmp[TIKU_KITS_NET_ICMP_OFF_CHKSUM]     = 0;
    icmp[TIKU_KITS_NET_ICMP_OFF_CHKSUM + 1] = 0;
    chksum = tiku_kits_net_ipv4_chksum(icmp, icmp_len);
    icmp[TIKU_KITS_NET_ICMP_OFF_CHKSUM]     = (uint8_t)(chksum >> 8);
    icmp[TIKU_KITS_NET_ICMP_OFF_CHKSUM + 1] = (uint8_t)(chksum & 0xFF);

    /* Send the reply */
    tiku_kits_net_ipv4_output(buf, len);
}
