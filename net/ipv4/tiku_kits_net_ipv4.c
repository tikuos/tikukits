/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_ipv4.c - IPv4 processing and net process protothread
 *
 * Contains the RFC 1071 checksum, IPv4 input validation pipeline,
 * output with checksum recomputation, buffer accessors for upper
 * layers, and the main net process protothread that drives the
 * poll-based I/O loop.
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

#include "tiku_kits_net_ipv4.h"
#include "tiku_kits_net_icmp.h"
#include "tiku_kits_net_udp.h"
#include <tikukits/net/slip/tiku_kits_net_slip.h>
#include <kernel/process/tiku_process.h>
#include <kernel/timers/tiku_timer.h>
#include <arch/msp430/tiku_device_select.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* STATIC DATA                                                               */
/*---------------------------------------------------------------------------*/

/**
 * Single static packet buffer used for both RX and TX (half-duplex).
 * The processing cycle is: link RX fills the buffer, ipv4_input
 * validates and dispatches, the handler may modify the buffer
 * in-place for a reply, ipv4_output transmits, then the buffer is
 * reset for the next frame.
 */
static uint8_t net_buf[TIKU_KITS_NET_MTU];

/** Current frame length in net_buf (reset to 0 after processing) */
static uint16_t net_buf_len;

/** Our IPv4 address, initialised from the TIKU_KITS_NET_IP_ADDR macro.
 *  Mutable so that DHCP (or other runtime config) can update it. */
static tiku_kits_net_ip4_addr_t our_addr = {TIKU_KITS_NET_IP_ADDR};

/** Active link-layer backend (set by ipv4_set_link, NULL at startup) */
static const tiku_kits_net_link_t *active_link;

/** Periodic timer that drives the net process poll loop */
static struct tiku_timer net_timer;

/*---------------------------------------------------------------------------*/
/* CHECKSUM                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief RFC 1071 ones-complement checksum
 *
 * Sums consecutive 16-bit words from @p data in a 32-bit accumulator,
 * handles an odd trailing byte, then folds the carry bits down into
 * 16 bits and returns the ones-complement.
 */
uint16_t
tiku_kits_net_ipv4_chksum(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    uint16_t i;

    /* Sum consecutive 16-bit words (big-endian) */
    for (i = 0; i + 1 < len; i += 2) {
        sum += (uint16_t)((uint16_t)data[i] << 8 | data[i + 1]);
    }

    /* Handle odd trailing byte (padded with zero on the right) */
    if (len & 1) {
        sum += (uint16_t)((uint16_t)data[len - 1] << 8);
    }

    /* Fold 32-bit sum into 16 bits until no carry remains */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum & 0xFFFF);
}

/*---------------------------------------------------------------------------*/
/* LINK MANAGEMENT                                                           */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_ipv4_set_link(const tiku_kits_net_link_t *link)
{
    active_link = link;
}

/*---------------------------------------------------------------------------*/
/* INPUT PIPELINE                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Validate and dispatch an incoming IPv4 packet
 *
 * Performs five sequential checks (any failure silently drops the
 * packet).  On success, dispatches to the appropriate upper-layer
 * handler based on the protocol field.  The LED2 toggling provides
 * a visual heartbeat on the LaunchPad for debugging.
 */
void
tiku_kits_net_ipv4_input(uint8_t *buf, uint16_t len)
{
    uint16_t ihl_bytes;
    uint16_t hdr_chksum;
    uint16_t total_len;
    uint16_t frag;

    /* Debug LED: LED2 on for valid-looking IPv4 frames */
    if (len > 0 && buf[0] == 0x45) {
        TIKU_BOARD_LED2_ON();
    } else {
        TIKU_BOARD_LED2_OFF();
    }

    /* Check 1: minimum length for an IPv4 header */
    if (len < TIKU_KITS_NET_IPV4_HDR_LEN) {
        return;
    }

    /* Check 2: IP version must be 4 */
    if (TIKU_KITS_NET_IPV4_VER(buf) != 4) {
        return;
    }

    /* Check 3: IHL >= 5 (minimum 20-byte header) */
    ihl_bytes = (uint16_t)(TIKU_KITS_NET_IPV4_IHL(buf) * 4);
    if (ihl_bytes < TIKU_KITS_NET_IPV4_HDR_LEN) {
        return;
    }

    /* Check 4: IHL must not exceed frame (prevents buffer over-read) */
    if (ihl_bytes > len) {
        return;
    }

    /* Check 5: header checksum must validate to zero */
    hdr_chksum = tiku_kits_net_ipv4_chksum(buf, ihl_bytes);
    if (hdr_chksum != 0) {
        return;
    }

    /* Check 6: total_len must be plausible
     *   - At least large enough for the header (ihl_bytes)
     *   - Must not exceed the actual frame length */
    total_len = TIKU_KITS_NET_IPV4_TOTLEN(buf);
    if (total_len < ihl_bytes || total_len > len) {
        return;
    }

    /* Check 7: fragmented packets are not supported
     *   - MF (More Fragments) bit set → fragment
     *   - Non-zero fragment offset   → fragment */
    frag = (uint16_t)((uint16_t)buf[TIKU_KITS_NET_IPV4_OFF_FRAG] << 8 |
                       buf[TIKU_KITS_NET_IPV4_OFF_FRAG + 1]);
    if ((frag & 0x2000) || (frag & 0x1FFF)) {
        return;
    }

    /* Check 8: destination IP must match ours or be broadcast.
     * Broadcast (255.255.255.255) is accepted for DHCP responses
     * and any future broadcast-based protocols. */
    if (!(buf[TIKU_KITS_NET_IPV4_OFF_DST]     == 255 &&
          buf[TIKU_KITS_NET_IPV4_OFF_DST + 1] == 255 &&
          buf[TIKU_KITS_NET_IPV4_OFF_DST + 2] == 255 &&
          buf[TIKU_KITS_NET_IPV4_OFF_DST + 3] == 255) &&
        (buf[TIKU_KITS_NET_IPV4_OFF_DST]     != our_addr.b[0] ||
         buf[TIKU_KITS_NET_IPV4_OFF_DST + 1] != our_addr.b[1] ||
         buf[TIKU_KITS_NET_IPV4_OFF_DST + 2] != our_addr.b[2] ||
         buf[TIKU_KITS_NET_IPV4_OFF_DST + 3] != our_addr.b[3])) {
        return;
    }

    /* Protocol dispatch -- use total_len (trims any link-layer padding) */
    switch (TIKU_KITS_NET_IPV4_PROTO(buf)) {
    case TIKU_KITS_NET_IPV4_PROTO_ICMP:
        tiku_kits_net_icmp_input(buf, total_len, ihl_bytes);
        break;
    case TIKU_KITS_NET_IPV4_PROTO_UDP:
        tiku_kits_net_udp_input(buf, total_len, ihl_bytes);
        break;
    default:
        /* Unsupported protocol -- silently drop */
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* OUTPUT                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Recompute IPv4 header checksum and transmit via link layer
 *
 * Zeros the checksum field, recomputes over the header, writes the
 * result back, then calls the link backend's send function.
 */
int8_t
tiku_kits_net_ipv4_output(uint8_t *buf, uint16_t len)
{
    uint16_t ihl_bytes;
    uint16_t chksum;

    if (active_link == NULL || active_link->send == NULL) {
        return TIKU_KITS_NET_ERR_NOLINK;
    }

    ihl_bytes = (uint16_t)(TIKU_KITS_NET_IPV4_IHL(buf) * 4);

    /* Zero the checksum field before recomputing */
    buf[TIKU_KITS_NET_IPV4_OFF_CHKSUM]     = 0;
    buf[TIKU_KITS_NET_IPV4_OFF_CHKSUM + 1] = 0;
    chksum = tiku_kits_net_ipv4_chksum(buf, ihl_bytes);
    buf[TIKU_KITS_NET_IPV4_OFF_CHKSUM]     = (uint8_t)(chksum >> 8);
    buf[TIKU_KITS_NET_IPV4_OFF_CHKSUM + 1] = (uint8_t)(chksum & 0xFF);

    return active_link->send(buf, len);
}

/*---------------------------------------------------------------------------*/
/* BUFFER ACCESSORS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return a pointer to the shared packet buffer for TX use.
 *
 * Used by upper-layer send functions (e.g. udp_send, dhcp_send)
 * to construct outgoing packets directly in the shared buffer,
 * avoiding an extra copy.  Resets the RX decode position so that
 * any partially-received SLIP frame is discarded -- the buffer
 * is half-duplex and cannot hold RX data while being used for TX.
 *
 * The caller must not be inside a receive callback (the buffer
 * already holds the incoming packet at that point).
 */
uint8_t *
tiku_kits_net_ipv4_get_buf(uint16_t *size)
{
    /* Discard any partial SLIP decode in progress.  Without this,
     * a TX operation (e.g. DHCP retransmit) that memsets net_buf
     * would corrupt the SLIP decoder's partial frame state, causing
     * the next poll_rx to produce a garbage "complete" frame. */
    net_buf_len = 0;

    if (size != NULL) {
        *size = TIKU_KITS_NET_MTU;
    }
    return net_buf;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return a pointer to our 4-byte IPv4 address
 *
 * Points into the static our_addr structure, which is initialised
 * at compile time from TIKU_KITS_NET_IP_ADDR.  The bytes are in
 * network order and can be copied directly into packet headers.
 */
const uint8_t *
tiku_kits_net_ipv4_get_addr(void)
{
    return our_addr.b;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Update our IPv4 address at runtime.
 *
 * Used by DHCP (or other runtime configuration) to set the IP
 * address after obtaining a lease.  The 4-byte address is in
 * network order.  Passing NULL is a no-op.
 */
void
tiku_kits_net_ipv4_set_addr(const uint8_t *addr)
{
    if (addr != (void *)0) {
        our_addr.b[0] = addr[0];
        our_addr.b[1] = addr[1];
        our_addr.b[2] = addr[2];
        our_addr.b[3] = addr[3];
    }
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return 1 if no partial SLIP frame is being decoded.
 *
 * When net_buf_len > 0, the SLIP decoder has partially filled
 * net_buf with an incoming frame.  Writing to net_buf at this
 * point (e.g. for a retransmit) would corrupt the partial frame.
 */
uint8_t
tiku_kits_net_ipv4_rx_idle(void)
{
    return (net_buf_len == 0) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
/* NET PROCESS                                                               */
/*---------------------------------------------------------------------------*/

TIKU_PROCESS(tiku_kits_net_process, "net");

/**
 * Net process protothread.
 *
 * One-time init: reset SLIP decoder, register the SLIP link backend,
 * initialise the UDP binding table, configure LEDs, flush any stale
 * UART bytes, and start the periodic poll timer.
 *
 * Main loop: on each timer event, drain all complete SLIP frames from
 * the UART and feed each through the IPv4 input pipeline.  The timer
 * resets at the bottom of each iteration for continuous polling.
 */
TIKU_PROCESS_THREAD(tiku_kits_net_process, ev, data)
{
    (void)data;

    TIKU_PROCESS_BEGIN();

    /* ---- One-time init ---- */
    tiku_kits_net_slip_init();
    tiku_kits_net_ipv4_set_link(&tiku_kits_net_slip_link);
    tiku_kits_net_udp_init();
    net_buf_len = 0;
    TIKU_BOARD_LED1_INIT();
    TIKU_BOARD_LED2_INIT();
    TIKU_BOARD_LED1_OFF();
    TIKU_BOARD_LED2_OFF();

    /* Drain any stale bytes from the UART ring buffer.
     *
     * During boot the eZ-FET backchannel and GPIO-to-UART pin
     * transitions can inject noise bytes into the RX ring buffer.
     * If any of these is 0xDB (SLIP ESC), the SLIP decoder enters
     * ESC state and misinterprets the next END marker as data,
     * corrupting the first received frame.  Flushing the ring
     * buffer here guarantees the decoder starts with a clean slate. */
    while (tiku_uart_rx_ready()) {
        (void)tiku_uart_getc();
    }

    tiku_timer_set_event(&net_timer, TIKU_KITS_NET_POLL_TICKS);

    /* ---- Main loop ---- */
    while (1) {
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

        /* Drain all complete SLIP frames from the UART */
        while (active_link != NULL &&
               active_link->poll_rx(net_buf, TIKU_KITS_NET_MTU,
                                    &net_buf_len)) {
            TIKU_BOARD_LED1_TOGGLE();
            tiku_kits_net_ipv4_input(net_buf, net_buf_len);
            net_buf_len = 0;
        }

        tiku_timer_reset(&net_timer);
    }

    TIKU_PROCESS_END();
}

/*
 * NOTE: No TIKU_AUTOSTART_PROCESSES here.  The net process lives in
 * the tikukits library (always compiled), so it must not define the
 * global autostart array -- that would clash with CLI or any other
 * app.  Instead, the APP=net Makefile target provides a thin wrapper
 * (apps/net/tiku_app_net.c) that registers the net process via
 * TIKU_AUTOSTART_PROCESSES.
 */
