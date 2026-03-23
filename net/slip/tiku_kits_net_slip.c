/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_slip.c - SLIP (RFC 1055) encode/decode over UART
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

#include "tiku_kits_net_slip.h"
#include <arch/msp430/tiku_uart_arch.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* RX STATE MACHINE                                                          */
/*---------------------------------------------------------------------------*/

/** Decoder states */
enum slip_rx_state {
    SLIP_RX_NORMAL = 0,  /**< Normal byte reception */
    SLIP_RX_ESC          /**< Previous byte was ESC */
};

/** Current decoder state (persists across poll_rx calls) */
static enum slip_rx_state slip_state;

/** Overflow flag: set when frame exceeds buffer, cleared on next END */
static uint8_t slip_overflow;

/*---------------------------------------------------------------------------*/
/* PUBLIC FUNCTIONS                                                          */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_slip_init(void)
{
    slip_state = SLIP_RX_NORMAL;
    slip_overflow = 0;
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_slip_send(const uint8_t *pkt, uint16_t len)
{
    uint16_t i;

    if (pkt == NULL) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    /* Leading END flushes any garbage on the line */
    tiku_uart_putc((char)TIKU_KITS_NET_SLIP_END);

    for (i = 0; i < len; i++) {
        switch (pkt[i]) {
        case TIKU_KITS_NET_SLIP_END:
            tiku_uart_putc((char)TIKU_KITS_NET_SLIP_ESC);
            tiku_uart_putc((char)TIKU_KITS_NET_SLIP_ESC_END);
            break;
        case TIKU_KITS_NET_SLIP_ESC:
            tiku_uart_putc((char)TIKU_KITS_NET_SLIP_ESC);
            tiku_uart_putc((char)TIKU_KITS_NET_SLIP_ESC_ESC);
            break;
#if TIKU_KITS_NET_SLIP_ESC_NUL_ENABLE
        case 0x00:
            /* eZ-FET workaround: two consecutive 0x00 bytes on the
             * backchannel UART trigger a target reset in eZ-FET
             * firmware v31501001.  Escaping NUL prevents this.
             * Disabled when using FT232/slattach (kernel SLIP). */
            tiku_uart_putc((char)TIKU_KITS_NET_SLIP_ESC);
            tiku_uart_putc((char)TIKU_KITS_NET_SLIP_ESC_NUL);
            break;
#endif
        default:
            tiku_uart_putc((char)pkt[i]);
            break;
        }
    }

    /* Trailing END marks the frame boundary */
    tiku_uart_putc((char)TIKU_KITS_NET_SLIP_END);

    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

uint8_t
tiku_kits_net_slip_poll_rx(uint8_t *buf, uint16_t buf_size, uint16_t *pos)
{
    int ch;
    uint8_t byte;

    while (tiku_uart_rx_ready()) {
        ch = tiku_uart_getc();
        if (ch < 0) {
            break;
        }
        byte = (uint8_t)ch;

        switch (slip_state) {
        case SLIP_RX_ESC:
            slip_state = SLIP_RX_NORMAL;
            if (byte == TIKU_KITS_NET_SLIP_ESC_END) {
                byte = TIKU_KITS_NET_SLIP_END;
            } else if (byte == TIKU_KITS_NET_SLIP_ESC_ESC) {
                byte = TIKU_KITS_NET_SLIP_ESC;
            } else if (byte == TIKU_KITS_NET_SLIP_ESC_NUL) {
                byte = 0x00;
            } else {
                /* Protocol violation — treat as literal byte */
            }
            /* Fall through to store the decoded byte */
            if (!slip_overflow && *pos < buf_size) {
                buf[*pos] = byte;
                (*pos)++;
            } else {
                slip_overflow = 1;
            }
            break;

        case SLIP_RX_NORMAL:
        default:
            if (byte == TIKU_KITS_NET_SLIP_END) {
                /* Frame boundary */
                if (*pos > 0 && !slip_overflow) {
                    /* Complete frame ready */
                    slip_overflow = 0;
                    return 1;
                }
                /* Empty frame or overflow — reset for next frame */
                *pos = 0;
                slip_overflow = 0;
            } else if (byte == TIKU_KITS_NET_SLIP_ESC) {
                slip_state = SLIP_RX_ESC;
            } else {
                if (!slip_overflow && *pos < buf_size) {
                    buf[*pos] = byte;
                    (*pos)++;
                } else {
                    slip_overflow = 1;
                }
            }
            break;
        }
    }

    return 0;
}

/*---------------------------------------------------------------------------*/
/* LINK BACKEND DEFINITION                                                   */
/*---------------------------------------------------------------------------*/

const tiku_kits_net_link_t tiku_kits_net_slip_link = {
    .send    = tiku_kits_net_slip_send,
    .poll_rx = tiku_kits_net_slip_poll_rx,
    .name    = "SLIP"
};
