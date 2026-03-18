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
 * networking sub-modules (SLIP, IPv4, ICMP, etc.).
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
/** @} */

/*---------------------------------------------------------------------------*/
/* CONFIGURATION (compile-time overrideable)                                 */
/*---------------------------------------------------------------------------*/

/** Maximum transmission unit (bytes).  128 fits in SRAM and keeps
 *  total RAM usage around 172 bytes (~8.4% of 2 KB). */
#ifndef TIKU_KITS_NET_MTU
#define TIKU_KITS_NET_MTU           128
#endif

/** Our IPv4 address (default: 172.16.7.2).
 *  Avoids 192.x (0xC0 = SLIP END) to simplify SLIP encoding. */
#ifndef TIKU_KITS_NET_IP_ADDR
#define TIKU_KITS_NET_IP_ADDR       {172, 16, 7, 2}
#endif

/** Default time-to-live for outgoing packets. */
#ifndef TIKU_KITS_NET_TTL
#define TIKU_KITS_NET_TTL           64
#endif

/** Net process I/O poll interval (ticks).  TIKU_CLOCK_SECOND/20 ~ 50 ms. */
#ifndef TIKU_KITS_NET_POLL_TICKS
#define TIKU_KITS_NET_POLL_TICKS    (TIKU_CLOCK_SECOND / 20)
#endif

/*---------------------------------------------------------------------------*/
/* COMMON DATA TYPES                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief IPv4 address stored as four bytes.
 *
 * Using a byte array avoids alignment issues on 16-bit MSP430 and
 * makes network byte order handling trivial (bytes are already in
 * network order).
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
 */
static inline uint16_t
tiku_kits_net_htons(uint16_t h)
{
    return (uint16_t)((h >> 8) | (h << 8));
}

/**
 * @brief Convert a 16-bit value from network to host byte order.
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
 * @brief Link-layer backend descriptor.
 *
 * Modeled on tiku_cli_io_t.  IPv4 calls send() to transmit a
 * complete IP packet; the net process calls poll_rx() to feed bytes
 * from the hardware into the link-layer decoder.
 *
 * To add a new link layer (e.g. wireless), fill one of these structs
 * and pass it to tiku_kits_net_ipv4_set_link().
 */
typedef struct tiku_kits_net_link {
    /**
     * @brief Transmit a framed packet over the link.
     * @param pkt  Pointer to the IP packet
     * @param len  Packet length in bytes
     * @return TIKU_KITS_NET_OK on success, negative error code otherwise
     */
    int8_t (*send)(const uint8_t *pkt, uint16_t len);

    /**
     * @brief Poll the hardware for incoming bytes (non-blocking).
     *
     * Reads all available bytes from the hardware and feeds them into
     * the link-layer decoder.  When a complete frame has been
     * assembled in @p buf, sets *pos to the frame length and returns 1.
     *
     * @param buf       Receive buffer
     * @param buf_size  Buffer capacity (MTU)
     * @param pos       In/out: current write position in buf
     * @return 1 if a complete frame is ready, 0 otherwise
     */
    uint8_t (*poll_rx)(uint8_t *buf, uint16_t buf_size, uint16_t *pos);

    const char *name;  /**< Human-readable name (e.g. "SLIP") */
} tiku_kits_net_link_t;

#endif /* TIKU_KITS_NET_H_ */
