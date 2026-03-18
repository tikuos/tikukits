/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_slip.h - SLIP (RFC 1055) framing over UART
 *
 * Encodes and decodes IP packets using the Serial Line Internet
 * Protocol.  TX is blocking (one byte at a time via tiku_uart_putc);
 * RX is a non-blocking byte-at-a-time state machine driven by the
 * net process poll loop.
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

#ifndef TIKU_KITS_NET_SLIP_H_
#define TIKU_KITS_NET_SLIP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"

/*---------------------------------------------------------------------------*/
/* SLIP CONSTANTS (RFC 1055)                                                 */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_NET_SLIP_END      0xC0  /**< Frame delimiter */
#define TIKU_KITS_NET_SLIP_ESC      0xDB  /**< Escape character */
#define TIKU_KITS_NET_SLIP_ESC_END  0xDC  /**< Escaped END byte */
#define TIKU_KITS_NET_SLIP_ESC_ESC  0xDD  /**< Escaped ESC byte */
#define TIKU_KITS_NET_SLIP_ESC_NUL  0xDE  /**< Escaped NUL byte (eZ-FET workaround) */

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the SLIP decoder state.
 *
 * Call once at startup.  Does NOT call tiku_uart_init() (that is
 * handled during boot).
 */
void tiku_kits_net_slip_init(void);

/**
 * @brief SLIP-encode and transmit a packet over UART.
 *
 * Sends END, escapes each byte (END -> ESC ESC_END, ESC -> ESC
 * ESC_ESC), then sends a trailing END.  Blocking per-byte via
 * tiku_uart_putc().
 *
 * @param pkt  Pointer to the IP packet
 * @param len  Packet length in bytes
 * @return TIKU_KITS_NET_OK on success, negative error code otherwise
 */
int8_t tiku_kits_net_slip_send(const uint8_t *pkt, uint16_t len);

/**
 * @brief Non-blocking SLIP RX: decode available UART bytes.
 *
 * Reads all bytes available from tiku_uart_rx_ready()/getc() and
 * feeds them through the SLIP state machine.  When a complete frame
 * has been assembled in @p buf, sets *pos to the frame length and
 * returns 1.
 *
 * If the frame exceeds @p buf_size, it is silently discarded and
 * decoding resets for the next frame.
 *
 * @param buf       Receive buffer
 * @param buf_size  Buffer capacity (typically TIKU_KITS_NET_MTU)
 * @param pos       In/out: current write position in buf
 * @return 1 if a complete frame is ready, 0 otherwise
 */
uint8_t tiku_kits_net_slip_poll_rx(uint8_t *buf, uint16_t buf_size,
                                   uint16_t *pos);

/*---------------------------------------------------------------------------*/
/* LINK BACKEND                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Pre-defined SLIP link-layer backend.
 *
 * Pass to tiku_kits_net_ipv4_set_link() to use SLIP over UART.
 */
extern const tiku_kits_net_link_t tiku_kits_net_slip_link;

#endif /* TIKU_KITS_NET_SLIP_H_ */
