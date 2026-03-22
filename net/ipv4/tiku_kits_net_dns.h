/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_dns.h - DNS stub resolver (A record only)
 *
 * Lightweight DNS stub resolver that queries a recursive DNS server
 * over UDP to resolve hostnames to IPv4 addresses.  Only A (address)
 * records are supported.  Designed for ultra-low-power MSP430
 * microcontrollers with static allocation and no heap usage.
 *
 * Architecture follows the same poll-based pattern as the NTP and
 * TFTP clients: the UDP receive callback stores incoming data into
 * static state, and the application calls dns_poll() from its own
 * context to parse the reply and drive state transitions.
 *
 * A small 2-entry cache avoids redundant queries for recently
 * resolved hostnames.  Cache entries are evicted by TTL expiry
 * or LRU (lowest remaining TTL) when full.
 *
 * Typical usage:
 * @code
 *   static const uint8_t dns_srv[] = {8, 8, 8, 8};
 *   tiku_kits_net_dns_init();
 *   tiku_kits_net_dns_set_server(dns_srv);
 *   tiku_kits_net_dns_resolve("pool.ntp.org");
 *   while (tiku_kits_net_dns_get_state()
 *          < TIKU_KITS_NET_DNS_STATE_DONE) {
 *       tiku_kits_net_dns_poll();
 *   }
 *   if (tiku_kits_net_dns_get_state()
 *       == TIKU_KITS_NET_DNS_STATE_DONE) {
 *       uint8_t addr[4];
 *       tiku_kits_net_dns_get_addr(addr);
 *   }
 * @endcode
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_NET_DNS_H_
#define TIKU_KITS_NET_DNS_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* DNS PROTOCOL CONSTANTS                                                    */
/*---------------------------------------------------------------------------*/

/** DNS server port (RFC 1035). */
#define TIKU_KITS_NET_DNS_PORT              53

/** Local UDP port for DNS client. */
#define TIKU_KITS_NET_DNS_LOCAL_PORT        49400

/** Maximum hostname length (single label limit, RFC 1035). */
#define TIKU_KITS_NET_DNS_MAX_HOSTNAME      63

/** Maximum number of poll() calls before timeout. */
#define TIKU_KITS_NET_DNS_MAX_RETRIES       3

/** Number of cache entries. */
#define TIKU_KITS_NET_DNS_CACHE_SIZE        2

/** DNS header length in bytes. */
#define TIKU_KITS_NET_DNS_HDR_LEN           12

/*---------------------------------------------------------------------------*/
/* DNS HEADER BYTE OFFSETS (RFC 1035 Section 4.1.1)                          */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_DNS_OFFSETS DNS Header Byte Offsets
 * @brief Offsets into the 12-byte DNS header.
 * @{
 */
#define TIKU_KITS_NET_DNS_OFF_ID            0   /**< Transaction ID (16b) */
#define TIKU_KITS_NET_DNS_OFF_FLAGS         2   /**< Flags (16b) */
#define TIKU_KITS_NET_DNS_OFF_QDCOUNT       4   /**< Question count (16b) */
#define TIKU_KITS_NET_DNS_OFF_ANCOUNT       6   /**< Answer count (16b) */
#define TIKU_KITS_NET_DNS_OFF_NSCOUNT       8   /**< Authority count (16b) */
#define TIKU_KITS_NET_DNS_OFF_ARCOUNT      10   /**< Additional count (16b) */
/** @} */

/*---------------------------------------------------------------------------*/
/* DNS FLAG VALUES                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_DNS_FLAGS DNS Flag Values
 * @brief Bit masks and values for the 16-bit flags field.
 * @{
 */
#define TIKU_KITS_NET_DNS_FLAG_QR       0x8000  /**< Query/Response bit */
#define TIKU_KITS_NET_DNS_FLAG_RD       0x0100  /**< Recursion Desired */
#define TIKU_KITS_NET_DNS_FLAG_TC       0x0200  /**< Truncation */
#define TIKU_KITS_NET_DNS_RCODE_MASK    0x000F  /**< Response code mask */
#define TIKU_KITS_NET_DNS_RCODE_OK      0       /**< No error */
#define TIKU_KITS_NET_DNS_RCODE_NXDOMAIN 3      /**< Name does not exist */
/** @} */

/*---------------------------------------------------------------------------*/
/* DNS QUERY TYPE AND CLASS                                                  */
/*---------------------------------------------------------------------------*/

/** QTYPE for A (host address) records. */
#define TIKU_KITS_NET_DNS_QTYPE_A       1

/** QCLASS for Internet. */
#define TIKU_KITS_NET_DNS_QCLASS_IN     1

/*---------------------------------------------------------------------------*/
/* STATE TYPES                                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief DNS resolver state.
 *
 * Progresses from IDLE -> SENT -> DONE or ERROR.
 */
typedef enum {
    /** No query active. */
    TIKU_KITS_NET_DNS_STATE_IDLE,
    /** Query sent, awaiting reply. */
    TIKU_KITS_NET_DNS_STATE_SENT,
    /** Reply received, address available. */
    TIKU_KITS_NET_DNS_STATE_DONE,
    /** Query failed (timeout, NXDOMAIN, parse error). */
    TIKU_KITS_NET_DNS_STATE_ERROR
} tiku_kits_net_dns_state_t;

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the DNS resolver.
 *
 * Clears all state, flushes the cache, and resets the transaction
 * ID counter.  Safe to call again to forcibly reset.
 */
void tiku_kits_net_dns_init(void);

/**
 * @brief Set the DNS server address.
 *
 * Copies 4 bytes from @p addr.  Must be called before
 * dns_resolve().
 *
 * @param addr  DNS server IPv4 address (4 bytes, network order)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NULL if @p addr is NULL.
 */
int8_t tiku_kits_net_dns_set_server(const uint8_t *addr);

/**
 * @brief Start a DNS A-record query for @p hostname.
 *
 * Checks the cache first; if a valid entry exists, transitions
 * directly to DONE without sending a packet.  Otherwise builds
 * a DNS query, binds the local UDP port, sends the packet, and
 * sets the state to SENT.
 *
 * After calling resolve(), poll dns_poll() until get_state()
 * returns DONE or ERROR.
 *
 * @param hostname  Dot-separated hostname (e.g. "pool.ntp.org")
 * @return TIKU_KITS_NET_OK on success (query sent or cache hit),
 *         TIKU_KITS_NET_ERR_NULL if @p hostname is NULL,
 *         TIKU_KITS_NET_ERR_PARAM if hostname is invalid or
 *         no server has been set,
 *         negative error code if UDP send fails.
 */
int8_t tiku_kits_net_dns_resolve(const char *hostname);

/**
 * @brief Poll for DNS reply and update state.
 *
 * Checks if a reply has been received by the UDP callback.
 * If so, parses the DNS response, extracts the first A record,
 * caches it, unbinds the port, and transitions to DONE.
 *
 * If no reply has arrived, increments the retry counter.  When
 * retries reach DNS_MAX_RETRIES, unbinds the port and
 * transitions to ERROR.
 *
 * Must be called from application context (not from inside a
 * UDP receive callback).
 *
 * @return TIKU_KITS_NET_OK if reply processed (state == DONE),
 *         TIKU_KITS_NET_ERR_PARAM if no query is active,
 *         TIKU_KITS_NET_ERR_TIMEOUT if max retries exhausted.
 */
int8_t tiku_kits_net_dns_poll(void);

/**
 * @brief Get the current resolver state.
 *
 * @return Current DNS state (IDLE, SENT, DONE, or ERROR).
 */
tiku_kits_net_dns_state_t tiku_kits_net_dns_get_state(void);

/**
 * @brief Get the resolved IPv4 address.
 *
 * Only valid when get_state() returns DONE.
 *
 * @param addr_out  Output: 4-byte IPv4 address (network order)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NULL if @p addr_out is NULL,
 *         TIKU_KITS_NET_ERR_PARAM if no address is available.
 */
int8_t tiku_kits_net_dns_get_addr(uint8_t *addr_out);

/**
 * @brief Get the TTL from the resolved A record.
 *
 * Only valid when get_state() returns DONE.
 *
 * @return TTL in seconds, or 0 if no result available.
 */
uint32_t tiku_kits_net_dns_get_ttl(void);

/**
 * @brief Abort any active DNS query.
 *
 * Unbinds the DNS port and resets state to IDLE.
 *
 * @return TIKU_KITS_NET_OK always.
 */
int8_t tiku_kits_net_dns_abort(void);

/**
 * @brief Flush all cached DNS entries.
 *
 * Marks every cache slot as invalid.  Subsequent resolve()
 * calls will always send a query on the wire.
 */
void tiku_kits_net_dns_cache_flush(void);

#endif /* TIKU_KITS_NET_DNS_H_ */
