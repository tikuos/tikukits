/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_ipv4.c - IPv4 processing and net process protothread
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
#include <tikukits/net/slip/tiku_kits_net_slip.h>
#include <kernel/process/tiku_process.h>
#include <kernel/timers/tiku_timer.h>
#include <arch/msp430/tiku_device_select.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* STATIC DATA                                                               */
/*---------------------------------------------------------------------------*/

/** Single static packet buffer (half-duplex: RX -> process -> TX -> reset) */
static uint8_t net_buf[TIKU_KITS_NET_MTU];
static uint16_t net_buf_len;

/** Our IPv4 address */
static const tiku_kits_net_ip4_addr_t our_addr = {TIKU_KITS_NET_IP_ADDR};

/** Active link-layer backend */
static const tiku_kits_net_link_t *active_link;

/** Periodic timer for I/O polling */
static struct tiku_timer net_timer;

/*---------------------------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                                          */
/*---------------------------------------------------------------------------*/

uint16_t
tiku_kits_net_ipv4_chksum(const uint8_t *data, uint16_t len)
{
    uint32_t sum = 0;
    uint16_t i;

    /* Sum consecutive 16-bit words */
    for (i = 0; i + 1 < len; i += 2) {
        sum += (uint16_t)((uint16_t)data[i] << 8 | data[i + 1]);
    }

    /* Handle odd trailing byte */
    if (len & 1) {
        sum += (uint16_t)((uint16_t)data[len - 1] << 8);
    }

    /* Fold 32-bit sum into 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum & 0xFFFF);
}

/*---------------------------------------------------------------------------*/

void
tiku_kits_net_ipv4_set_link(const tiku_kits_net_link_t *link)
{
    active_link = link;
}

/*---------------------------------------------------------------------------*/

void
tiku_kits_net_ipv4_input(uint8_t *buf, uint16_t len)
{
    uint16_t ihl_bytes;
    uint16_t hdr_chksum;

    if (len > 0 && buf[0] == 0x45) {
        TIKU_BOARD_LED2_ON();
    } else {
        TIKU_BOARD_LED2_OFF();
    }

    /* Check minimum length */
    if (len < TIKU_KITS_NET_IPV4_HDR_LEN) {
        return;
    }

    /* Validate version == 4 */
    if (TIKU_KITS_NET_IPV4_VER(buf) != 4) {
        return;
    }

    /* Validate IHL >= 5 (minimum 20 bytes) */
    ihl_bytes = (uint16_t)(TIKU_KITS_NET_IPV4_IHL(buf) * 4);
    if (ihl_bytes < TIKU_KITS_NET_IPV4_HDR_LEN) {
        return;
    }

    /* Verify header checksum */
    hdr_chksum = tiku_kits_net_ipv4_chksum(buf, ihl_bytes);
    if (hdr_chksum != 0) {
        return;
    }

    /* Check destination IP matches ours */
    if (buf[TIKU_KITS_NET_IPV4_OFF_DST]     != our_addr.b[0] ||
        buf[TIKU_KITS_NET_IPV4_OFF_DST + 1] != our_addr.b[1] ||
        buf[TIKU_KITS_NET_IPV4_OFF_DST + 2] != our_addr.b[2] ||
        buf[TIKU_KITS_NET_IPV4_OFF_DST + 3] != our_addr.b[3]) {
        return;
    }

    /* Dispatch by protocol */
    switch (TIKU_KITS_NET_IPV4_PROTO(buf)) {
    case TIKU_KITS_NET_IPV4_PROTO_ICMP:
        tiku_kits_net_icmp_input(buf, len, ihl_bytes);
        break;
    default:
        /* Unsupported protocol — silently drop */
        break;
    }
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_ipv4_output(uint8_t *buf, uint16_t len)
{
    uint16_t ihl_bytes;
    uint16_t chksum;

    if (active_link == NULL || active_link->send == NULL) {
        return TIKU_KITS_NET_ERR_NOLINK;
    }

    ihl_bytes = (uint16_t)(TIKU_KITS_NET_IPV4_IHL(buf) * 4);

    /* Zero the checksum field and recompute */
    buf[TIKU_KITS_NET_IPV4_OFF_CHKSUM]     = 0;
    buf[TIKU_KITS_NET_IPV4_OFF_CHKSUM + 1] = 0;
    chksum = tiku_kits_net_ipv4_chksum(buf, ihl_bytes);
    buf[TIKU_KITS_NET_IPV4_OFF_CHKSUM]     = (uint8_t)(chksum >> 8);
    buf[TIKU_KITS_NET_IPV4_OFF_CHKSUM + 1] = (uint8_t)(chksum & 0xFF);

    return active_link->send(buf, len);
}

/*---------------------------------------------------------------------------*/
/* NET PROCESS                                                               */
/*---------------------------------------------------------------------------*/

TIKU_PROCESS(tiku_kits_net_process, "net");

TIKU_PROCESS_THREAD(tiku_kits_net_process, ev, data)
{
    (void)data;

    TIKU_PROCESS_BEGIN();

    /* ---- One-time init ---- */
    tiku_kits_net_slip_init();
    tiku_kits_net_ipv4_set_link(&tiku_kits_net_slip_link);
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

        /* Drain UART bytes via the link-layer decoder */
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
 * global autostart array — that would clash with CLI or any other
 * app.  Instead, the APP=net Makefile target provides a thin wrapper
 * that registers the net process via TIKU_AUTOSTART_PROCESSES.
 */
