/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_udp.c - UDP datagram protocol (RFC 768)
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

#include "tiku_kits_net_udp.h"
#include "tiku_kits_net_ipv4.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* BINDING TABLE                                                             */
/*---------------------------------------------------------------------------*/

/** One entry in the static binding table. */
typedef struct {
    uint16_t                     port;  /**< Host byte order, 0 = free */
    tiku_kits_net_udp_recv_cb_t  cb;
} udp_bind_t;

static udp_bind_t bind_table[TIKU_KITS_NET_UDP_MAX_BINDS];

/** Guard flag: set while inside udp_input to prevent re-entrant send. */
static uint8_t in_rx;

/*---------------------------------------------------------------------------*/
/* UDP PSEUDO-HEADER CHECKSUM                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute UDP checksum including the IPv4 pseudo-header.
 *
 * Pseudo-header: src_ip(4) + dst_ip(4) + zero(1) + proto(1) + udp_len(2)
 * followed by the UDP header + data.
 */
static uint16_t
udp_chksum(const uint8_t *ip_hdr, const uint8_t *udp,
           uint16_t udp_len)
{
    uint8_t pseudo[12];
    uint32_t sum = 0;
    uint16_t i;

    /* Build pseudo-header */
    memcpy(pseudo,     ip_hdr + TIKU_KITS_NET_IPV4_OFF_SRC, 4);
    memcpy(pseudo + 4, ip_hdr + TIKU_KITS_NET_IPV4_OFF_DST, 4);
    pseudo[8]  = 0;
    pseudo[9]  = TIKU_KITS_NET_IPV4_PROTO_UDP;
    pseudo[10] = (uint8_t)(udp_len >> 8);
    pseudo[11] = (uint8_t)(udp_len & 0xFF);

    /* Sum pseudo-header (always 12 bytes, even count) */
    for (i = 0; i < 12; i += 2) {
        sum += (uint16_t)((uint16_t)pseudo[i] << 8 | pseudo[i + 1]);
    }

    /* Sum UDP header + data */
    for (i = 0; i + 1 < udp_len; i += 2) {
        sum += (uint16_t)((uint16_t)udp[i] << 8 | udp[i + 1]);
    }
    if (udp_len & 1) {
        sum += (uint16_t)((uint16_t)udp[udp_len - 1] << 8);
    }

    /* Fold */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum & 0xFFFF);
}

/*---------------------------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                                          */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_udp_init(void)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_UDP_MAX_BINDS; i++) {
        bind_table[i].port = 0;
        bind_table[i].cb   = (tiku_kits_net_udp_recv_cb_t)0;
    }
    in_rx = 0;
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_udp_bind(uint16_t port, tiku_kits_net_udp_recv_cb_t cb)
{
    uint8_t i;
    int8_t  free_slot = -1;

    if (cb == (tiku_kits_net_udp_recv_cb_t)0 || port == 0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    for (i = 0; i < TIKU_KITS_NET_UDP_MAX_BINDS; i++) {
        if (bind_table[i].port == port) {
            return TIKU_KITS_NET_ERR_PARAM;  /* already bound */
        }
        if (bind_table[i].port == 0 && free_slot < 0) {
            free_slot = (int8_t)i;
        }
    }

    if (free_slot < 0) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    bind_table[free_slot].port = port;
    bind_table[free_slot].cb   = cb;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_udp_unbind(uint16_t port)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_UDP_MAX_BINDS; i++) {
        if (bind_table[i].port == port) {
            bind_table[i].port = 0;
            bind_table[i].cb   = (tiku_kits_net_udp_recv_cb_t)0;
            return TIKU_KITS_NET_OK;
        }
    }
    return TIKU_KITS_NET_ERR_PARAM;
}

/*---------------------------------------------------------------------------*/

void
tiku_kits_net_udp_input(uint8_t *buf, uint16_t len, uint16_t ihl_len)
{
    uint8_t *udp;
    uint16_t udp_len;
    uint16_t udp_total;
    uint16_t dport, sport;
    uint16_t payload_len;
    uint16_t chk;
    uint8_t tmp[4];
    uint16_t tmp16;
    uint8_t i;

    udp = buf + ihl_len;
    udp_total = len - ihl_len;

    /* Validate minimum UDP length */
    if (udp_total < TIKU_KITS_NET_UDP_HDR_LEN) {
        return;
    }

    /* Extract and validate length field */
    udp_len = (uint16_t)((uint16_t)udp[TIKU_KITS_NET_UDP_OFF_LEN] << 8 |
                          udp[TIKU_KITS_NET_UDP_OFF_LEN + 1]);
    if (udp_len < TIKU_KITS_NET_UDP_HDR_LEN || udp_len > udp_total) {
        return;
    }

    /* Verify checksum if non-zero */
    chk = (uint16_t)((uint16_t)udp[TIKU_KITS_NET_UDP_OFF_CHKSUM] << 8 |
                      udp[TIKU_KITS_NET_UDP_OFF_CHKSUM + 1]);
    if (chk != 0) {
        if (udp_chksum(buf, udp, udp_len) != 0) {
            return;
        }
    }

    /* Extract ports */
    sport = (uint16_t)((uint16_t)udp[TIKU_KITS_NET_UDP_OFF_SPORT] << 8 |
                        udp[TIKU_KITS_NET_UDP_OFF_SPORT + 1]);
    dport = (uint16_t)((uint16_t)udp[TIKU_KITS_NET_UDP_OFF_DPORT] << 8 |
                        udp[TIKU_KITS_NET_UDP_OFF_DPORT + 1]);

    payload_len = udp_len - TIKU_KITS_NET_UDP_HDR_LEN;

#if TIKU_KITS_NET_UDP_ECHO_ENABLE
    /* Built-in echo service (port 7): reply in-place */
    if (dport == TIKU_KITS_NET_UDP_ECHO_PORT) {
        /* Swap src/dst IP */
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

        /* Swap src/dst ports */
        tmp16 = sport;
        udp[TIKU_KITS_NET_UDP_OFF_SPORT]     = udp[TIKU_KITS_NET_UDP_OFF_DPORT];
        udp[TIKU_KITS_NET_UDP_OFF_SPORT + 1] = udp[TIKU_KITS_NET_UDP_OFF_DPORT + 1];
        udp[TIKU_KITS_NET_UDP_OFF_DPORT]     = (uint8_t)(tmp16 >> 8);
        udp[TIKU_KITS_NET_UDP_OFF_DPORT + 1] = (uint8_t)(tmp16 & 0xFF);

        /* Zero checksum (optional per RFC 768) */
        udp[TIKU_KITS_NET_UDP_OFF_CHKSUM]     = 0;
        udp[TIKU_KITS_NET_UDP_OFF_CHKSUM + 1] = 0;

        tiku_kits_net_ipv4_output(buf, len);
        return;
    }
#endif /* TIKU_KITS_NET_UDP_ECHO_ENABLE */

    /* Binding table lookup */
    in_rx = 1;
    for (i = 0; i < TIKU_KITS_NET_UDP_MAX_BINDS; i++) {
        if (bind_table[i].port == dport && bind_table[i].cb != 0) {
            bind_table[i].cb(
                TIKU_KITS_NET_IPV4_SRC(buf),
                sport,
                udp + TIKU_KITS_NET_UDP_HDR_LEN,
                payload_len);
            in_rx = 0;
            return;
        }
    }
    in_rx = 0;

    /* No match — silently drop */
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_udp_send(const uint8_t *dst_addr,
                        uint16_t dst_port,
                        uint16_t src_port,
                        const uint8_t *payload,
                        uint16_t payload_len)
{
    uint8_t *buf;
    uint16_t buf_size;
    const uint8_t *our_ip;
    uint16_t total_len;
    uint16_t udp_len;
    uint8_t *udp;
    uint16_t chk;

    /* Cannot send from inside a receive callback (buffer in use) */
    if (in_rx) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    if (dst_addr == NULL) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    if (payload_len > TIKU_KITS_NET_UDP_MAX_PAYLOAD) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    if (payload_len > 0 && payload == NULL) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    buf = tiku_kits_net_ipv4_get_buf(&buf_size);
    if (buf == NULL) {
        return TIKU_KITS_NET_ERR_NOLINK;
    }

    our_ip = tiku_kits_net_ipv4_get_addr();

    udp_len = TIKU_KITS_NET_UDP_HDR_LEN + payload_len;
    total_len = TIKU_KITS_NET_IPV4_HDR_LEN + udp_len;

    /* --- Build IPv4 header --- */
    buf[0]  = 0x45;             /* ver=4, IHL=5 */
    buf[1]  = 0x00;             /* TOS */
    buf[2]  = (uint8_t)(total_len >> 8);
    buf[3]  = (uint8_t)(total_len & 0xFF);
    buf[4]  = 0x00; buf[5] = 0x00;   /* id=0 */
    buf[6]  = 0x00; buf[7] = 0x00;   /* flags+frag=0 */
    buf[8]  = TIKU_KITS_NET_TTL;     /* TTL */
    buf[9]  = TIKU_KITS_NET_IPV4_PROTO_UDP;  /* proto=UDP */
    buf[10] = 0x00; buf[11] = 0x00;  /* checksum placeholder */

    /* Source IP = our address */
    buf[12] = our_ip[0]; buf[13] = our_ip[1];
    buf[14] = our_ip[2]; buf[15] = our_ip[3];

    /* Destination IP */
    buf[16] = dst_addr[0]; buf[17] = dst_addr[1];
    buf[18] = dst_addr[2]; buf[19] = dst_addr[3];

    /* --- Build UDP header --- */
    udp = buf + TIKU_KITS_NET_IPV4_HDR_LEN;

    udp[TIKU_KITS_NET_UDP_OFF_SPORT]     = (uint8_t)(src_port >> 8);
    udp[TIKU_KITS_NET_UDP_OFF_SPORT + 1] = (uint8_t)(src_port & 0xFF);
    udp[TIKU_KITS_NET_UDP_OFF_DPORT]     = (uint8_t)(dst_port >> 8);
    udp[TIKU_KITS_NET_UDP_OFF_DPORT + 1] = (uint8_t)(dst_port & 0xFF);
    udp[TIKU_KITS_NET_UDP_OFF_LEN]       = (uint8_t)(udp_len >> 8);
    udp[TIKU_KITS_NET_UDP_OFF_LEN + 1]   = (uint8_t)(udp_len & 0xFF);
    udp[TIKU_KITS_NET_UDP_OFF_CHKSUM]     = 0;
    udp[TIKU_KITS_NET_UDP_OFF_CHKSUM + 1] = 0;

    /* Copy payload */
    if (payload_len > 0) {
        memcpy(udp + TIKU_KITS_NET_UDP_HDR_LEN, payload, payload_len);
    }

    /* Compute UDP checksum (over pseudo-header + UDP) */
    chk = udp_chksum(buf, udp, udp_len);
    /* RFC 768: if computed checksum is 0, transmit as 0xFFFF */
    if (chk == 0) {
        chk = 0xFFFF;
    }
    udp[TIKU_KITS_NET_UDP_OFF_CHKSUM]     = (uint8_t)(chk >> 8);
    udp[TIKU_KITS_NET_UDP_OFF_CHKSUM + 1] = (uint8_t)(chk & 0xFF);

    return tiku_kits_net_ipv4_output(buf, total_len);
}
