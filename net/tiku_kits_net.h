/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net.h - Common networking types and definitions
 *
 * Provides shared return codes, configuration macros, byte-order
 * helpers, and the link-layer abstraction used by all TikuKits
 * networking sub-modules (SLIP, IPv4, ICMP, UDP, etc.).
 *
 * All networking code is designed for ultra-low-power embedded
 * targets with severe memory constraints.  The single shared packet
 * buffer (TIKU_KITS_NET_MTU bytes) and static allocation policy
 * keep total RAM overhead well under 200 bytes.  No heap is used.
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

#ifndef TIKU_KITS_NET_H_
#define TIKU_KITS_NET_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_STATUS Networking Status Codes
 * @brief Return codes shared by all TikuKits networking sub-modules.
 *
 * Every public networking function returns one of these codes.  Zero
 * indicates success; negative values indicate distinct error classes.
 * Sub-modules must never define their own return codes -- all codes
 * live here so that application code can handle errors uniformly.
 *
 * Example:
 * @code
 *   int8_t rc = tiku_kits_net_ipv4_output(buf, len);
 *   if (rc != TIKU_KITS_NET_OK) {
 *       // handle error
 *   }
 * @endcode
 * @{
 */
#define TIKU_KITS_NET_OK            0   /**< Operation succeeded */
#define TIKU_KITS_NET_ERR_NULL    (-1)  /**< NULL pointer argument */
#define TIKU_KITS_NET_ERR_PARAM   (-2)  /**< Invalid parameter */
#define TIKU_KITS_NET_ERR_OVERFLOW (-3) /**< Buffer overflow (frame too large) */
#define TIKU_KITS_NET_ERR_SHORT   (-4)  /**< Packet too short for header */
#define TIKU_KITS_NET_ERR_CHKSUM  (-5)  /**< Checksum verification failed */
#define TIKU_KITS_NET_ERR_NOLINK  (-6)  /**< No link-layer backend set */
#define TIKU_KITS_NET_ERR_PROTO   (-7)  /**< Unsupported protocol */
#define TIKU_KITS_NET_ERR_SLIP    (-8)  /**< SLIP framing error */
/** @brief Operation timed out waiting for a response. */
#define TIKU_KITS_NET_ERR_TIMEOUT (-9)  /**< Timeout (no reply) */
/** @} */

/*---------------------------------------------------------------------------*/
/* CONFIGURATION (compile-time overrideable)                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Enable the DHCP client (default: disabled).
 *
 * When enabled, the MTU is automatically raised to at least 300
 * bytes so that DHCP packets (236-byte fixed header + options)
 * fit within the shared net_buf.  This adds ~172 bytes of RAM
 * compared to the default 128-byte MTU.
 *
 * Set to 1 at compile time to enable:
 * @code
 *   #define TIKU_KITS_NET_DHCP_ENABLE 1
 * @endcode
 */
#ifndef TIKU_KITS_NET_DHCP_ENABLE
#define TIKU_KITS_NET_DHCP_ENABLE   0
#endif

/**
 * @brief Enable the TCP transport layer (default: disabled).
 *
 * When enabled, the net process initialises the TCP subsystem and
 * dispatches protocol-6 packets to the TCP input handler.  TCP
 * uses FRAM-backed buffers for retransmission and receive data;
 * see tiku_kits_net_tcp.h for configuration macros and memory
 * budget details.
 *
 * Set to 1 at compile time to enable:
 * @code
 *   #define TIKU_KITS_NET_TCP_ENABLE 1
 * @endcode
 */
#ifndef TIKU_KITS_NET_TCP_ENABLE
#define TIKU_KITS_NET_TCP_ENABLE    0
#endif

/**
 * @brief Maximum transmission unit (bytes).
 *
 * 128 fits comfortably in MSP430 SRAM and keeps total RAM usage
 * around 172 bytes (~8.4% of 2 KB).  This is the capacity of the
 * single shared packet buffer used for both RX and TX (half-duplex).
 * All upper-layer protocols derive their maximum payload from this
 * value (e.g. ICMP max payload = MTU - 20 - 8 = 100 bytes).
 *
 * When DHCP is enabled, the MTU is automatically raised to 300
 * to accommodate the 236-byte DHCP header plus options.  The user
 * may still override this by defining TIKU_KITS_NET_MTU before
 * including this header.
 *
 * Override at compile time to adjust:
 * @code
 *   #define TIKU_KITS_NET_MTU 256
 *   #include "tiku_kits_net.h"
 * @endcode
 */
#if TIKU_KITS_NET_DHCP_ENABLE && !defined(TIKU_KITS_NET_MTU)
#define TIKU_KITS_NET_MTU           300
#endif

#ifndef TIKU_KITS_NET_MTU
#define TIKU_KITS_NET_MTU           128
#endif

/**
 * @brief Escape NUL (0x00) bytes in SLIP frames (default: enabled).
 *
 * The eZ-FET backchannel firmware resets the MSP430 target when it
 * receives two consecutive NUL bytes.  With this enabled, the SLIP
 * encoder replaces 0x00 with [ESC, 0xDE] and the decoder reverses
 * it.  This is a non-standard SLIP extension.
 *
 * **Disable (set to 0) when using an external UART adapter (FT232,
 * CP2102) with the Linux kernel SLIP driver (slattach).**  The
 * kernel's SLIP decoder does not understand the NUL escape and
 * will corrupt every 0x00 byte in the packet (turning it into
 * 0xDE), making IP packets unparseable.
 *
 * Set to 0 at compile time to disable:
 * @code
 *   -DTIKU_KITS_NET_SLIP_ESC_NUL_ENABLE=0
 * @endcode
 */
#ifndef TIKU_KITS_NET_SLIP_ESC_NUL_ENABLE
#define TIKU_KITS_NET_SLIP_ESC_NUL_ENABLE  1
#endif

/**
 * @brief Our IPv4 address (default: 172.16.7.2).
 *
 * Stored as four initialiser bytes in network order.  The default
 * avoids 192.x addresses because 192 == 0xC0 == SLIP END, which
 * would complicate SLIP encoding of the IP header.
 */
#ifndef TIKU_KITS_NET_IP_ADDR
#define TIKU_KITS_NET_IP_ADDR       {172, 16, 7, 2}
#endif

/**
 * @brief Default time-to-live for outgoing packets.
 *
 * 64 is the standard TTL for most operating systems and is more
 * than sufficient for a point-to-point SLIP link with no routing.
 */
#ifndef TIKU_KITS_NET_TTL
#define TIKU_KITS_NET_TTL           64
#endif

/**
 * @brief Net process I/O poll interval (ticks).
 *
 * The net process protothread wakes every POLL_TICKS to drain UART
 * bytes through the SLIP decoder.  TIKU_CLOCK_SECOND/20 gives
 * approximately 50 ms, balancing responsiveness against CPU wake-ups.
 */
#ifndef TIKU_KITS_NET_POLL_TICKS
#define TIKU_KITS_NET_POLL_TICKS    (TIKU_CLOCK_SECOND / 20)
#endif

/*---------------------------------------------------------------------------*/
/* COMMON DATA TYPES                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_net_ip4_addr
 * @brief IPv4 address stored as four bytes in network order.
 *
 * Using a byte array avoids alignment issues on 16-bit MSP430 and
 * makes network byte order handling trivial -- the bytes are already
 * in network order and can be copied directly into packet headers
 * via memcpy without any byte-swapping.
 */
typedef struct tiku_kits_net_ip4_addr {
    uint8_t b[4];  /**< Address octets in network order */
} tiku_kits_net_ip4_addr_t;

/*---------------------------------------------------------------------------*/
/* BYTE-ORDER HELPERS                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Convert a 16-bit value from host to network byte order.
 *
 * MSP430 is little-endian; network order is big-endian, so we swap.
 * Implemented as an inline function rather than a macro to ensure
 * the argument is evaluated exactly once.
 *
 * @param h  16-bit value in host byte order
 * @return The same value in network (big-endian) byte order
 */
static inline uint16_t
tiku_kits_net_htons(uint16_t h)
{
    return (uint16_t)((h >> 8) | (h << 8));
}

/**
 * @brief Convert a 16-bit value from network to host byte order.
 *
 * Symmetric with htons; both perform an identical byte swap on
 * little-endian targets.
 *
 * @param n  16-bit value in network byte order
 * @return The same value in host (little-endian) byte order
 */
static inline uint16_t
tiku_kits_net_ntohs(uint16_t n)
{
    return (uint16_t)((n >> 8) | (n << 8));
}

/*---------------------------------------------------------------------------*/
/* LINK-LAYER ABSTRACTION                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_net_link
 * @brief Link-layer backend descriptor.
 *
 * Abstracts the underlying transport so that the IPv4 layer is
 * independent of the physical link.  The default implementation
 * uses SLIP over UART (tiku_kits_net_slip_link), but any transport
 * (wireless, SPI, loopback) can be plugged in by filling one of
 * these structs and passing it to tiku_kits_net_ipv4_set_link().
 *
 * The design mirrors tiku_cli_io_t -- a function-pointer struct
 * that decouples I/O from processing logic.
 *
 * Example (registering a custom link):
 * @code
 *   static const tiku_kits_net_link_t my_link = {
 *       .send    = my_send_func,
 *       .poll_rx = my_poll_func,
 *       .name    = "custom"
 *   };
 *   tiku_kits_net_ipv4_set_link(&my_link);
 * @endcode
 */
typedef struct tiku_kits_net_link {
    /**
     * @brief Transmit a framed packet over the link.
     *
     * The link layer is responsible for any framing (e.g. SLIP
     * encoding) before writing bytes to the hardware.  The caller
     * retains ownership of @p pkt.
     *
     * @param pkt  Pointer to the complete IP packet
     * @param len  Packet length in bytes
     * @return TIKU_KITS_NET_OK on success, negative error code otherwise
     */
    int8_t (*send)(const uint8_t *pkt, uint16_t len);

    /**
     * @brief Poll the hardware for incoming bytes (non-blocking).
     *
     * Reads all available bytes from the hardware and feeds them
     * into the link-layer decoder.  When a complete frame has been
     * assembled in @p buf, sets *pos to the frame length and
     * returns 1.  May be called repeatedly in a tight loop until
     * it returns 0 (no more complete frames available).
     *
     * @param buf       Receive buffer (caller-owned)
     * @param buf_size  Buffer capacity (MTU)
     * @param pos       In/out: current write position in buf;
     *                  set to frame length on return of 1
     * @return 1 if a complete frame is ready, 0 otherwise
     */
    uint8_t (*poll_rx)(uint8_t *buf, uint16_t buf_size, uint16_t *pos);

    const char *name;  /**< Human-readable name (e.g. "SLIP") */
} tiku_kits_net_link_t;

#endif /* TIKU_KITS_NET_H_ */
