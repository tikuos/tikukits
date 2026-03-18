/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_udp.c - UDP datagram protocol (RFC 768)
 *
 * Implements port binding, datagram input with echo service,
 * datagram output with pseudo-header checksum, and the static
 * binding table.  All state is statically allocated.
 *
 * The echo service (port 7) uses the same in-place reply strategy
 * as ICMP: swap IPs, swap ports, zero the checksum, and call
 * ipv4_output on the same buffer.  This avoids a second buffer
 * copy and keeps RAM cost at zero beyond the 17-byte module state.
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

/**
 * @brief One entry in the static port binding table.
 *
 * A port value of 0 indicates a free slot.  The callback pointer
 * is only valid when port != 0.  Each entry is 4 bytes on MSP430
 * (2-byte port + 2-byte function pointer).
 */
typedef struct {
    uint16_t                     port;  /**< Bound port (host order), 0 = free */
    tiku_kits_net_udp_recv_cb_t  cb;    /**< Receive callback for this port */
} udp_bind_t;

/**
 * Static binding table.  Default 4 slots = 16 bytes of RAM.
 * Scanned linearly on each incoming datagram (O(MAX_BINDS), bounded).
 */
static udp_bind_t bind_table[TIKU_KITS_NET_UDP_MAX_BINDS];

/**
 * Re-entrancy guard.  Set to 1 while executing inside udp_input()
 * to prevent udp_send() from being called within a receive callback
 * (the shared net_buf holds the incoming packet at that point).
 */
static uint8_t in_rx;

/*---------------------------------------------------------------------------*/
/* UDP PSEUDO-HEADER CHECKSUM                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the UDP checksum including the IPv4 pseudo-header.
 *
 * The pseudo-header is a 12-byte virtual prefix constructed from the
 * IPv4 header: source IP (4), destination IP (4), zero (1),
 * protocol (1, always 17), and UDP length (2).  The checksum covers
 * this prefix concatenated with the entire UDP segment (header +
 * data).  This catches IP-level corruption that the IPv4 header
 * checksum alone would miss (e.g. a packet delivered to the wrong
 * host due to a bit flip in the destination address that happens
 * to produce a valid IPv4 header checksum).
 *
 * Uses the same ones-complement algorithm as the IPv4 checksum
 * (RFC 1071): accumulate 16-bit words in a 32-bit sum, handle an
 * odd trailing byte, fold carry, and complement.
 *
 * @param ip_hdr   Pointer to the IPv4 header (for src/dst IP)
 * @param udp      Pointer to the UDP header
 * @param udp_len  Length of the UDP segment (header + data)
 * @return 16-bit ones-complement checksum; 0 if valid on verify
 */
static uint16_t
udp_chksum(const uint8_t *ip_hdr, const uint8_t *udp,
           uint16_t udp_len)
{
    uint8_t pseudo[12];
    uint32_t sum = 0;
    uint16_t i;

    /* Build the 12-byte pseudo-header on the stack */
    memcpy(pseudo,     ip_hdr + TIKU_KITS_NET_IPV4_OFF_SRC, 4);
    memcpy(pseudo + 4, ip_hdr + TIKU_KITS_NET_IPV4_OFF_DST, 4);
    pseudo[8]  = 0;                          /* zero padding */
    pseudo[9]  = TIKU_KITS_NET_IPV4_PROTO_UDP;  /* protocol = 17 */
    pseudo[10] = (uint8_t)(udp_len >> 8);    /* UDP length (MSB) */
    pseudo[11] = (uint8_t)(udp_len & 0xFF);  /* UDP length (LSB) */

    /* Sum pseudo-header (always 12 bytes, even count -- no odd-byte) */
    for (i = 0; i < 12; i += 2) {
        sum += (uint16_t)((uint16_t)pseudo[i] << 8 | pseudo[i + 1]);
    }

    /* Sum the UDP header + payload */
    for (i = 0; i + 1 < udp_len; i += 2) {
        sum += (uint16_t)((uint16_t)udp[i] << 8 | udp[i + 1]);
    }
    /* Handle odd trailing byte (padded with zero on the right) */
    if (udp_len & 1) {
        sum += (uint16_t)((uint16_t)udp[udp_len - 1] << 8);
    }

    /* Fold 32-bit sum into 16 bits until no carry remains */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum & 0xFFFF);
}

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all binding slots and reset the re-entrancy guard.
 *
 * Sets every slot's port to 0 (the "free" sentinel) and callback
 * to NULL.  Safe to call at any time to wipe all bindings.
 */
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
/* PORT BINDING                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Register a callback for incoming datagrams on a port.
 *
 * Scans the table once to (a) reject duplicate bindings and
 * (b) record the first free slot.  If the port is available and
 * a slot is free, the binding is installed.  O(MAX_BINDS) scan.
 */
int8_t
tiku_kits_net_udp_bind(uint16_t port, tiku_kits_net_udp_recv_cb_t cb)
{
    uint8_t i;
    int8_t  free_slot = -1;

    /* Reject NULL callback and reserved port 0 */
    if (cb == (tiku_kits_net_udp_recv_cb_t)0 || port == 0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    for (i = 0; i < TIKU_KITS_NET_UDP_MAX_BINDS; i++) {
        /* Reject duplicate: port already has a binding */
        if (bind_table[i].port == port) {
            return TIKU_KITS_NET_ERR_PARAM;
        }
        /* Remember the first free slot for later use */
        if (bind_table[i].port == 0 && free_slot < 0) {
            free_slot = (int8_t)i;
        }
    }

    /* No free slot found -- table is full */
    if (free_slot < 0) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    bind_table[free_slot].port = port;
    bind_table[free_slot].cb   = cb;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove a port binding and free the slot.
 *
 * Linear scan for the matching port.  Zeroes the port field
 * (the "free" sentinel) and NULLs the callback pointer.
 */
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
/* INPUT                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Validate and dispatch an incoming UDP datagram.
 *
 * Three validation steps (silent drop on any failure):
 *   1. At least 8 bytes of UDP data present after the IPv4 header
 *   2. UDP length field >= 8 and <= actual bytes available
 *   3. If checksum field != 0, verify via pseudo-header checksum
 *
 * After validation, the destination port is checked:
 *   - Echo port 7 (if enabled): in-place reply (swap IPs, swap
 *     ports, zero checksum, transmit via ipv4_output)
 *   - Binding table: linear scan for matching dport, invoke
 *     callback with the in_rx guard set
 *   - No match: silently dropped
 */
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

    /* Point to the start of the UDP header (right after IPv4) */
    udp = buf + ihl_len;
    udp_total = len - ihl_len;

    /* --- Validation step 1: minimum UDP header length --- */
    if (udp_total < TIKU_KITS_NET_UDP_HDR_LEN) {
        return;
    }

    /* --- Validation step 2: length field consistency ---
     * The length field must be at least 8 (header only) and must
     * not exceed the actual number of bytes available after the
     * IPv4 header.  This catches both truncated packets and
     * fabricated length fields. */
    udp_len = (uint16_t)((uint16_t)udp[TIKU_KITS_NET_UDP_OFF_LEN] << 8 |
                          udp[TIKU_KITS_NET_UDP_OFF_LEN + 1]);
    if (udp_len < TIKU_KITS_NET_UDP_HDR_LEN || udp_len > udp_total) {
        return;
    }

    /* --- Validation step 3: checksum (if present) ---
     * RFC 768: a checksum of 0 means "not computed" and must not
     * be verified.  A non-zero checksum is verified against the
     * pseudo-header; the result should fold to 0 for a valid
     * datagram. */
    chk = (uint16_t)((uint16_t)udp[TIKU_KITS_NET_UDP_OFF_CHKSUM] << 8 |
                      udp[TIKU_KITS_NET_UDP_OFF_CHKSUM + 1]);
    if (chk != 0) {
        if (udp_chksum(buf, udp, udp_len) != 0) {
            return;
        }
    }

    /* Extract source and destination ports (network -> host order) */
    sport = (uint16_t)((uint16_t)udp[TIKU_KITS_NET_UDP_OFF_SPORT] << 8 |
                        udp[TIKU_KITS_NET_UDP_OFF_SPORT + 1]);
    dport = (uint16_t)((uint16_t)udp[TIKU_KITS_NET_UDP_OFF_DPORT] << 8 |
                        udp[TIKU_KITS_NET_UDP_OFF_DPORT + 1]);

    payload_len = udp_len - TIKU_KITS_NET_UDP_HDR_LEN;

    /* ---------------------------------------------------------------
     * Echo service (port 7, RFC 862)
     *
     * Handled BEFORE the binding table lookup so that echo always
     * works regardless of table state and does not consume a slot.
     * Uses the same in-place reply pattern as ICMP echo:
     *   1. Swap source and destination IP addresses
     *   2. Swap source and destination ports
     *   3. Zero the checksum (optional per RFC 768)
     *   4. Transmit via ipv4_output
     * The payload is left untouched -- it is echoed verbatim.
     * --------------------------------------------------------------- */
#if TIKU_KITS_NET_UDP_ECHO_ENABLE
    if (dport == TIKU_KITS_NET_UDP_ECHO_PORT) {
        /* Swap source and destination IP addresses using a 4-byte
         * temporary (same approach as ICMP echo) */
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

        /* Swap source and destination ports.  Save the original
         * source port, copy dport into sport position, then write
         * the saved sport into dport position. */
        tmp16 = sport;
        udp[TIKU_KITS_NET_UDP_OFF_SPORT]     = udp[TIKU_KITS_NET_UDP_OFF_DPORT];
        udp[TIKU_KITS_NET_UDP_OFF_SPORT + 1] = udp[TIKU_KITS_NET_UDP_OFF_DPORT + 1];
        udp[TIKU_KITS_NET_UDP_OFF_DPORT]     = (uint8_t)(tmp16 >> 8);
        udp[TIKU_KITS_NET_UDP_OFF_DPORT + 1] = (uint8_t)(tmp16 & 0xFF);

        /* Zero the checksum field -- optional per RFC 768, saves
         * the cost of recomputing the pseudo-header checksum for
         * the echo reply */
        udp[TIKU_KITS_NET_UDP_OFF_CHKSUM]     = 0;
        udp[TIKU_KITS_NET_UDP_OFF_CHKSUM + 1] = 0;

        /* Transmit the echo reply (ipv4_output recomputes the
         * IPv4 header checksum to account for swapped addresses) */
        tiku_kits_net_ipv4_output(buf, len);
        return;
    }
#endif /* TIKU_KITS_NET_UDP_ECHO_ENABLE */

    /* ---------------------------------------------------------------
     * Binding table dispatch
     *
     * Set the in_rx guard before invoking the callback to prevent
     * the callback from calling udp_send (which would overwrite
     * the buffer that still holds the incoming packet).  Clear the
     * guard after the callback returns.
     * --------------------------------------------------------------- */
    in_rx = 1;
    for (i = 0; i < TIKU_KITS_NET_UDP_MAX_BINDS; i++) {
        if (bind_table[i].port == dport && bind_table[i].cb != 0) {
            bind_table[i].cb(
                TIKU_KITS_NET_IPV4_SRC(buf),    /* sender IP */
                sport,                           /* sender port */
                udp + TIKU_KITS_NET_UDP_HDR_LEN, /* payload start */
                payload_len);                    /* payload bytes */
            in_rx = 0;
            return;
        }
    }
    in_rx = 0;

    /* No matching binding -- silently drop (no ICMP unreachable) */
}

/*---------------------------------------------------------------------------*/
/* OUTPUT                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Construct and transmit a UDP datagram.
 *
 * Builds the complete packet (IPv4 header + UDP header + payload)
 * in the shared net_buf obtained from ipv4_get_buf().  The IPv4
 * header is a minimal 20-byte header (IHL=5, no options).  The
 * UDP checksum is computed over the pseudo-header + UDP segment
 * per RFC 768; if the result is 0 it is transmitted as 0xFFFF
 * (RFC 768 requirement to distinguish "no checksum" from "valid
 * checksum that happens to be zero").
 *
 * ipv4_output() handles the IPv4 header checksum recomputation
 * and link-layer transmission.
 */
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

    /* Re-entrancy check: reject if called from a receive callback */
    if (in_rx) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Parameter validation */
    if (dst_addr == NULL) {
        return TIKU_KITS_NET_ERR_NULL;
    }
    if (payload_len > TIKU_KITS_NET_UDP_MAX_PAYLOAD) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }
    if (payload_len > 0 && payload == NULL) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    /* Acquire the shared buffer and our IP address */
    buf = tiku_kits_net_ipv4_get_buf(&buf_size);
    if (buf == NULL) {
        return TIKU_KITS_NET_ERR_NOLINK;
    }
    our_ip = tiku_kits_net_ipv4_get_addr();

    /* Compute lengths */
    udp_len = TIKU_KITS_NET_UDP_HDR_LEN + payload_len;
    total_len = TIKU_KITS_NET_IPV4_HDR_LEN + udp_len;

    /* --- Build the 20-byte IPv4 header (IHL=5, no options) ---
     * The header checksum field is left at 0 here; ipv4_output()
     * will recompute it before transmitting. */
    buf[0]  = 0x45;                          /* version=4, IHL=5 */
    buf[1]  = 0x00;                          /* TOS = 0 */
    buf[2]  = (uint8_t)(total_len >> 8);     /* total length (MSB) */
    buf[3]  = (uint8_t)(total_len & 0xFF);   /* total length (LSB) */
    buf[4]  = 0x00; buf[5] = 0x00;           /* identification = 0 */
    buf[6]  = 0x00; buf[7] = 0x00;           /* flags + frag = 0 */
    buf[8]  = TIKU_KITS_NET_TTL;             /* time to live */
    buf[9]  = TIKU_KITS_NET_IPV4_PROTO_UDP;  /* protocol = 17 (UDP) */
    buf[10] = 0x00; buf[11] = 0x00;          /* checksum placeholder */

    /* Source IP = our configured address */
    buf[12] = our_ip[0]; buf[13] = our_ip[1];
    buf[14] = our_ip[2]; buf[15] = our_ip[3];

    /* Destination IP = caller-provided address */
    buf[16] = dst_addr[0]; buf[17] = dst_addr[1];
    buf[18] = dst_addr[2]; buf[19] = dst_addr[3];

    /* --- Build the 8-byte UDP header ---
     * Ports and length are written in network byte order (MSB first).
     * Checksum is zeroed before computation. */
    udp = buf + TIKU_KITS_NET_IPV4_HDR_LEN;

    udp[TIKU_KITS_NET_UDP_OFF_SPORT]     = (uint8_t)(src_port >> 8);
    udp[TIKU_KITS_NET_UDP_OFF_SPORT + 1] = (uint8_t)(src_port & 0xFF);
    udp[TIKU_KITS_NET_UDP_OFF_DPORT]     = (uint8_t)(dst_port >> 8);
    udp[TIKU_KITS_NET_UDP_OFF_DPORT + 1] = (uint8_t)(dst_port & 0xFF);
    udp[TIKU_KITS_NET_UDP_OFF_LEN]       = (uint8_t)(udp_len >> 8);
    udp[TIKU_KITS_NET_UDP_OFF_LEN + 1]   = (uint8_t)(udp_len & 0xFF);
    udp[TIKU_KITS_NET_UDP_OFF_CHKSUM]     = 0;
    udp[TIKU_KITS_NET_UDP_OFF_CHKSUM + 1] = 0;

    /* Copy the application payload into the buffer */
    if (payload_len > 0) {
        memcpy(udp + TIKU_KITS_NET_UDP_HDR_LEN, payload, payload_len);
    }

    /* Compute the UDP checksum over pseudo-header + UDP segment.
     * RFC 768: a computed checksum of 0 must be transmitted as
     * 0xFFFF so that a receiver can distinguish "valid checksum
     * that happens to be zero" from "checksum not computed". */
    chk = udp_chksum(buf, udp, udp_len);
    if (chk == 0) {
        chk = 0xFFFF;
    }
    udp[TIKU_KITS_NET_UDP_OFF_CHKSUM]     = (uint8_t)(chk >> 8);
    udp[TIKU_KITS_NET_UDP_OFF_CHKSUM + 1] = (uint8_t)(chk & 0xFF);

    /* Transmit: ipv4_output recomputes the IPv4 header checksum
     * and passes the packet to the active link layer */
    return tiku_kits_net_ipv4_output(buf, total_len);
}
