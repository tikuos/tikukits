/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_slip.c - SLIP (RFC 1055) encode/decode over UART
 *
 * Platform-independent SLIP framing implementation.  TX is a
 * blocking encoder that escapes special bytes (END, ESC, and
 * optionally NUL) and writes one byte at a time via
 * tiku_uart_putc().  RX is a non-blocking byte-at-a-time state
 * machine that can be called repeatedly from a poll loop to
 * reassemble a complete IP frame from UART bytes.
 *
 * The decoder uses two static variables (state and overflow flag)
 * that persist across poll_rx calls, allowing partial frames to
 * be assembled over multiple invocations.  When a frame exceeds
 * the caller's buffer capacity, the overflow flag is set and the
 * remaining bytes are discarded until the next END delimiter
 * resets the decoder for a fresh frame.
 *
 * The pre-defined tiku_kits_net_slip_link struct at the bottom
 * packages the send and poll_rx functions into a link-layer
 * backend that can be plugged into the IPv4 layer via
 * tiku_kits_net_ipv4_set_link().
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
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the SLIP decoder to its initial state.
 *
 * Clears the state machine to NORMAL and resets the overflow flag.
 * The UART hardware is NOT initialised here -- that is handled by
 * the arch-level boot sequence before the net process starts.
 */
void
tiku_kits_net_slip_init(void)
{
    slip_state = SLIP_RX_NORMAL;
    slip_overflow = 0;
}

/*---------------------------------------------------------------------------*/
/* TRANSMIT                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief SLIP-encode and transmit a packet over UART.
 *
 * Sends a leading END byte to flush any line noise, then iterates
 * over the packet replacing special bytes with their two-byte escape
 * sequences (RFC 1055):
 *   - END (0xC0)  ->  ESC (0xDB) + ESC_END (0xDC)
 *   - ESC (0xDB)  ->  ESC (0xDB) + ESC_ESC (0xDD)
 *   - NUL (0x00)  ->  ESC (0xDB) + ESC_NUL (0xDE)  [if enabled]
 *
 * A trailing END byte marks the frame boundary.  All bytes are
 * written synchronously via tiku_uart_putc(), so this function
 * blocks until the entire frame has been queued to the UART.
 *
 * The NUL escape is a non-standard extension that works around
 * an eZ-FET firmware bug where two consecutive 0x00 bytes on the
 * backchannel UART trigger a target reset.  It is controlled by
 * TIKU_KITS_NET_SLIP_ESC_NUL_ENABLE and must be disabled when
 * using an external UART adapter with the Linux kernel SLIP driver.
 */
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
/* RECEIVE                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Non-blocking SLIP receive: decode available UART bytes.
 *
 * Drains all bytes currently available in the UART ring buffer
 * and feeds them through a two-state machine:
 *
 *   SLIP_RX_NORMAL -- the default state.  An END byte signals a
 *   frame boundary; an ESC byte transitions to SLIP_RX_ESC; all
 *   other bytes are appended to the output buffer.
 *
 *   SLIP_RX_ESC -- the previous byte was ESC.  The current byte
 *   is decoded (ESC_END -> END, ESC_ESC -> ESC, ESC_NUL -> NUL)
 *   and appended.  Any other byte after ESC is treated as a
 *   protocol violation and stored literally.
 *
 * If the decoded frame exceeds @p buf_size, the overflow flag is
 * set and remaining bytes are discarded.  The overflow flag is
 * cleared when the next END delimiter arrives, allowing the
 * decoder to recover for subsequent frames.
 *
 * Returns 1 when a complete, non-empty frame is ready in @p buf
 * (with *pos set to the frame length).  Returns 0 if no complete
 * frame is available yet -- call again when more UART bytes arrive.
 */
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

/**
 * Pre-defined SLIP link-layer backend.  Pass to
 * tiku_kits_net_ipv4_set_link() to route all IPv4 I/O through
 * SLIP over the backchannel UART.  The net process registers
 * this backend during its one-time init sequence.
 */
const tiku_kits_net_link_t tiku_kits_net_slip_link = {
    .send    = tiku_kits_net_slip_send,
    .poll_rx = tiku_kits_net_slip_poll_rx,
    .name    = "SLIP"
};
