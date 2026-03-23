/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_dns.c - DNS stub resolver implementation
 *
 * Implements a lightweight DNS stub resolver (A records only) over
 * UDP.  Uses the same poll-based architecture as the NTP client:
 * the UDP receive callback copies the response into a FRAM-backed
 * buffer and sets a flag, and dns_poll() parses the response from
 * application context.
 *
 * A 2-entry cache with TTL-based expiry and LRU eviction avoids
 * redundant queries for recently resolved hostnames.
 *
 * Memory footprint: ~60 bytes static SRAM + 102 bytes FRAM.
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

#include "tiku_kits_net_dns.h"
#include "tiku_kits_net_udp.h"
#include "tiku_kits_net_ipv4.h"
#include <tikukits/net/tiku_kits_net.h>
#include <kernel/timers/tiku_clock.h>
#include <arch/msp430/tiku_timer_arch.h>
#include <kernel/memory/tiku_mem.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL STATE (SRAM)                                                     */
/*---------------------------------------------------------------------------*/

/** Current resolver state. */
static tiku_kits_net_dns_state_t dns_state;

/** Configured DNS server address (4 bytes, network order). */
static uint8_t dns_server[4];

/** Non-zero once dns_set_server() has been called. */
static uint8_t dns_server_set;

/** Transaction ID for the current query. */
static uint16_t dns_txn_id;

/** Flag set by UDP callback when a response arrives. */
static volatile uint8_t dns_reply_ready;

/** Poll retry counter. */
static uint8_t dns_retries;

/** Resolved IPv4 address (valid in DONE state). */
static uint8_t dns_resolved_addr[4];

/** TTL from the resolved A record (valid in DONE state). */
static uint32_t dns_resolved_ttl;

/** RCODE from the most recent response. */
static uint8_t dns_rcode;

/** Hostname hash for the current query (for cache insertion). */
static uint16_t dns_query_hash;

/*---------------------------------------------------------------------------*/
/* DNS CACHE (SRAM)                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Single DNS cache entry.
 *
 * Stores the djb2 hash of the hostname, the resolved address,
 * the TTL, and the clock time when the entry was cached.
 */
struct dns_cache_entry {
    uint16_t hash;       /**< djb2 hash of the hostname */
    uint8_t  addr[4];    /**< Resolved IPv4 address */
    uint32_t ttl_sec;    /**< TTL in seconds */
    uint32_t cached_at;  /**< tiku_clock_time() when cached */
    uint8_t  valid;      /**< Non-zero if entry is valid */
};

/** 2-entry DNS cache. */
static struct dns_cache_entry dns_cache[TIKU_KITS_NET_DNS_CACHE_SIZE];

/*---------------------------------------------------------------------------*/
/* FRAM RECEIVE BUFFER                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief FRAM-backed receive buffer for DNS responses.
 *
 * 100 bytes is sufficient for a minimal DNS response containing
 * one A record: 12 (header) + ~20 (question echo) + ~30 (answer
 * with compressed name) = ~62 bytes typical.  The extra headroom
 * accommodates longer hostnames or additional answer records that
 * we skip past.
 */
__attribute__((section(".persistent"), aligned(2)))
static uint8_t dns_rx_buf[100];

/** Length of data in dns_rx_buf (set by UDP callback). */
static uint16_t dns_rx_len;

/*---------------------------------------------------------------------------*/
/* HOSTNAME HASH                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute a 16-bit hash of a hostname using djb2.
 *
 * Used as a compact key for cache lookups.  Collisions are
 * harmless -- a false hit returns a cached address that may
 * not match the requested name, but in practice the 2-entry
 * cache makes collisions unlikely for embedded use.
 *
 * @param hostname  Null-terminated hostname string
 * @return 16-bit hash value
 */
static uint16_t dns_hostname_hash(const char *hostname)
{
    uint32_t h = 5381;
    uint8_t c;

    while ((c = (uint8_t)*hostname++) != 0) {
        h = ((h << 5) + h) + c;   /* h * 33 + c */
    }
    return (uint16_t)(h & 0xFFFF);
}

/*---------------------------------------------------------------------------*/
/* HOSTNAME ENCODING                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encode a hostname into DNS wire format.
 *
 * Converts "pool.ntp.org" into the label sequence
 * \\x04pool\\x03ntp\\x03org\\x00.  Validates that no single
 * label exceeds 63 bytes, no empty labels exist, and the total
 * encoded length does not exceed DNS_MAX_HOSTNAME.
 *
 * @param hostname  Null-terminated dot-separated hostname
 * @param out       Output buffer (must be at least
 *                  DNS_MAX_HOSTNAME + 2 bytes)
 * @return Encoded length in bytes (including trailing 0x00),
 *         or 0 on validation error.
 */
static uint16_t dns_encode_hostname(const char *hostname,
                                     uint8_t *out)
{
    uint16_t pos = 0;
    const char *p = hostname;
    const char *dot;
    uint8_t label_len;

    while (*p != '\0') {
        /* Find the next dot or end of string */
        dot = p;
        while (*dot != '\0' && *dot != '.') {
            dot++;
        }

        label_len = (uint8_t)(dot - p);

        /* Validate: no empty labels, no label > 63 bytes */
        if (label_len == 0 || label_len > 63) {
            return 0;
        }

        /* Check total length (pos + 1 length byte + label + 1 terminator) */
        if (pos + 1 + label_len + 1 > TIKU_KITS_NET_DNS_MAX_HOSTNAME) {
            return 0;
        }

        /* Write length byte and label */
        out[pos++] = label_len;
        memcpy(&out[pos], p, label_len);
        pos += label_len;

        /* Advance past the dot (if present) */
        p = dot;
        if (*p == '.') {
            p++;
        }
    }

    /* Trailing zero-length label */
    out[pos++] = 0x00;
    return pos;
}

/*---------------------------------------------------------------------------*/
/* 32-BIT BIG-ENDIAN READER                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read a 32-bit big-endian value from a byte buffer.
 *
 * DNS TTL fields are in network (big-endian) byte order.
 * MSP430 is little-endian, so we manually reassemble.
 *
 * @param p  Pointer to 4 bytes in big-endian order
 * @return 32-bit value in host byte order
 */
static uint32_t dns_read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           ((uint32_t)p[3]);
}

/*---------------------------------------------------------------------------*/
/* UDP RECEIVE CALLBACK                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief UDP callback for DNS responses.
 *
 * Called by the UDP layer when a packet arrives on our bound port.
 * Performs minimal validation (length, transaction ID, QR bit)
 * and copies the payload into the FRAM-backed dns_rx_buf.
 * Full parsing is deferred to dns_poll() in application context.
 *
 * Does NOT call udp_send() -- just sets a flag for poll().
 */
static void dns_udp_recv(const uint8_t *src_addr,
                          uint16_t       src_port,
                          const uint8_t *payload,
                          uint16_t       payload_len)
{
    uint16_t rx_id;
    uint16_t flags;

    (void)src_addr;
    (void)src_port;

    /* Need at least a 12-byte DNS header */
    if (payload == (void *)0 ||
        payload_len < TIKU_KITS_NET_DNS_HDR_LEN) {
        return;
    }

    /* Check transaction ID matches our query */
    rx_id = (uint16_t)((uint16_t)payload[TIKU_KITS_NET_DNS_OFF_ID]
                       << 8 |
                       payload[TIKU_KITS_NET_DNS_OFF_ID + 1]);
    if (rx_id != dns_txn_id) {
        return;
    }

    /* Check QR bit (must be a response) */
    flags = (uint16_t)((uint16_t)payload[TIKU_KITS_NET_DNS_OFF_FLAGS]
                       << 8 |
                       payload[TIKU_KITS_NET_DNS_OFF_FLAGS + 1]);
    if ((flags & TIKU_KITS_NET_DNS_FLAG_QR) == 0) {
        return;
    }

    /* Copy into FRAM buffer (truncate if larger than buffer) */
    dns_rx_len = payload_len;
    if (dns_rx_len > sizeof(dns_rx_buf)) {
        dns_rx_len = sizeof(dns_rx_buf);
    }
    {
        uint16_t saved = tiku_mpu_unlock_nvm();
        memcpy(dns_rx_buf, payload, dns_rx_len);
        tiku_mpu_lock_nvm(saved);
    }

    /* Signal poll() that a valid reply arrived */
    dns_reply_ready = 1;
}

/*---------------------------------------------------------------------------*/
/* CACHE LOOKUP                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Look up a hostname hash in the cache.
 *
 * Checks each cache entry for a matching hash and validates
 * that the TTL has not expired using tiku_clock_time().
 *
 * @param hash      djb2 hash of the hostname
 * @param addr_out  Output: 4-byte IPv4 address if found
 * @return TIKU_KITS_NET_OK if found and valid,
 *         TIKU_KITS_NET_ERR_PARAM if not found or expired.
 */
static int8_t dns_cache_lookup(uint16_t hash, uint8_t *addr_out)
{
    uint8_t i;
    tiku_clock_time_t now = tiku_clock_time();
    tiku_clock_time_t elapsed;

    for (i = 0; i < TIKU_KITS_NET_DNS_CACHE_SIZE; i++) {
        if (dns_cache[i].valid && dns_cache[i].hash == hash) {
            /* Check TTL expiry */
            elapsed =
                (now - dns_cache[i].cached_at) / TIKU_CLOCK_SECOND;
            if (elapsed < dns_cache[i].ttl_sec) {
                memcpy(addr_out, dns_cache[i].addr, 4);
                return TIKU_KITS_NET_OK;
            }
            /* Expired -- invalidate */
            dns_cache[i].valid = 0;
        }
    }
    return TIKU_KITS_NET_ERR_PARAM;
}

/*---------------------------------------------------------------------------*/
/* CACHE INSERT                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a resolved address into the cache.
 *
 * If a free slot exists, uses it.  Otherwise evicts the entry
 * with the lowest remaining TTL (LRU by time-to-live).
 *
 * @param hash  djb2 hash of the hostname
 * @param addr  4-byte IPv4 address to cache
 * @param ttl   TTL in seconds from the A record
 */
static void dns_cache_insert(uint16_t hash, const uint8_t *addr,
                              uint32_t ttl)
{
    uint8_t i;
    uint8_t victim = 0;
    uint32_t min_remaining = 0xFFFFFFFF;
    tiku_clock_time_t now = tiku_clock_time();
    tiku_clock_time_t elapsed;
    uint32_t remaining;

    /* Look for a free slot or the LRU victim */
    for (i = 0; i < TIKU_KITS_NET_DNS_CACHE_SIZE; i++) {
        if (!dns_cache[i].valid) {
            victim = i;
            break;
        }
        /* Compute remaining TTL */
        elapsed =
            (now - dns_cache[i].cached_at) / TIKU_CLOCK_SECOND;
        remaining = 0;
        if (elapsed < dns_cache[i].ttl_sec) {
            remaining = dns_cache[i].ttl_sec - (uint32_t)elapsed;
        }
        if (remaining < min_remaining) {
            min_remaining = remaining;
            victim = i;
        }
    }

    dns_cache[victim].hash = hash;
    memcpy(dns_cache[victim].addr, addr, 4);
    dns_cache[victim].ttl_sec = ttl;
    dns_cache[victim].cached_at = now;
    dns_cache[victim].valid = 1;
}

/*---------------------------------------------------------------------------*/
/* DNS NAME FIELD SKIPPER                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Skip a DNS name field in a response buffer.
 *
 * Handles both uncompressed labels (length byte + data, ending
 * with 0x00) and compressed pointers (two bytes starting with
 * 0xC0).  Returns the position immediately after the name.
 *
 * @param buf     DNS response buffer
 * @param pos     Current position in buffer
 * @param buflen  Total buffer length
 * @return New position after the name, or 0 on error.
 */
static uint16_t dns_skip_name(const uint8_t *buf, uint16_t pos,
                               uint16_t buflen)
{
    while (pos < buflen) {
        uint8_t len = buf[pos];

        /* Compression pointer (top 2 bits set) */
        if ((len & 0xC0) == 0xC0) {
            /* Pointer is 2 bytes; skip them */
            if (pos + 2 > buflen) {
                return 0;
            }
            return pos + 2;
        }

        /* Zero-length label = end of name */
        if (len == 0) {
            return pos + 1;
        }

        /* Regular label: skip length byte + label data */
        pos += 1 + len;
    }

    /* Ran off the end of the buffer */
    return 0;
}

/*---------------------------------------------------------------------------*/
/* DNS RESPONSE PARSER                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parse a DNS response from dns_rx_buf.
 *
 * Validates RCODE, checks for truncation (TC bit), reads the
 * answer count, skips the question section, then iterates
 * through answer RRs looking for the first A record (TYPE=1,
 * CLASS=1, RDLENGTH=4).  Extracts the 4-byte address and TTL.
 *
 * @return TIKU_KITS_NET_OK on success (address extracted),
 *         TIKU_KITS_NET_ERR_PARAM on parse or protocol error.
 */
static int8_t dns_parse_response(void)
{
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t pos;
    uint16_t i;
    uint16_t rr_type;
    uint16_t rr_class;
    uint32_t rr_ttl;
    uint16_t rdlength;

    if (dns_rx_len < TIKU_KITS_NET_DNS_HDR_LEN) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Read flags */
    flags = (uint16_t)(
        (uint16_t)dns_rx_buf[TIKU_KITS_NET_DNS_OFF_FLAGS] << 8 |
        dns_rx_buf[TIKU_KITS_NET_DNS_OFF_FLAGS + 1]);

    /* Check RCODE */
    dns_rcode = (uint8_t)(flags & TIKU_KITS_NET_DNS_RCODE_MASK);
    if (dns_rcode != TIKU_KITS_NET_DNS_RCODE_OK) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Check TC (truncation) -- we cannot handle truncated replies */
    if (flags & TIKU_KITS_NET_DNS_FLAG_TC) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Read question and answer counts */
    qdcount = (uint16_t)(
        (uint16_t)dns_rx_buf[TIKU_KITS_NET_DNS_OFF_QDCOUNT] << 8 |
        dns_rx_buf[TIKU_KITS_NET_DNS_OFF_QDCOUNT + 1]);
    ancount = (uint16_t)(
        (uint16_t)dns_rx_buf[TIKU_KITS_NET_DNS_OFF_ANCOUNT] << 8 |
        dns_rx_buf[TIKU_KITS_NET_DNS_OFF_ANCOUNT + 1]);

    if (ancount == 0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Skip past the header */
    pos = TIKU_KITS_NET_DNS_HDR_LEN;

    /* Skip the question section (qdcount questions) */
    for (i = 0; i < qdcount; i++) {
        pos = dns_skip_name(dns_rx_buf, pos, dns_rx_len);
        if (pos == 0) {
            return TIKU_KITS_NET_ERR_PARAM;
        }
        /* Skip QTYPE (2) + QCLASS (2) */
        pos += 4;
        if (pos > dns_rx_len) {
            return TIKU_KITS_NET_ERR_PARAM;
        }
    }

    /* Iterate through answer RRs looking for first A record */
    for (i = 0; i < ancount; i++) {
        /* Skip the name field */
        pos = dns_skip_name(dns_rx_buf, pos, dns_rx_len);
        if (pos == 0) {
            return TIKU_KITS_NET_ERR_PARAM;
        }

        /* Need at least 10 bytes: TYPE(2) + CLASS(2) + TTL(4) + RDLENGTH(2) */
        if (pos + 10 > dns_rx_len) {
            return TIKU_KITS_NET_ERR_PARAM;
        }

        rr_type = (uint16_t)(
            (uint16_t)dns_rx_buf[pos] << 8 |
            dns_rx_buf[pos + 1]);
        rr_class = (uint16_t)(
            (uint16_t)dns_rx_buf[pos + 2] << 8 |
            dns_rx_buf[pos + 3]);
        rr_ttl = dns_read_be32(&dns_rx_buf[pos + 4]);
        rdlength = (uint16_t)(
            (uint16_t)dns_rx_buf[pos + 8] << 8 |
            dns_rx_buf[pos + 9]);
        pos += 10;

        /* Check if this is an A record (TYPE=1, CLASS=1, RDLENGTH=4) */
        if (rr_type == TIKU_KITS_NET_DNS_QTYPE_A &&
            rr_class == TIKU_KITS_NET_DNS_QCLASS_IN &&
            rdlength == 4) {
            if (pos + 4 > dns_rx_len) {
                return TIKU_KITS_NET_ERR_PARAM;
            }
            memcpy(dns_resolved_addr, &dns_rx_buf[pos], 4);
            dns_resolved_ttl = rr_ttl;
            return TIKU_KITS_NET_OK;
        }

        /* Skip RDATA for non-A records */
        pos += rdlength;
        if (pos > dns_rx_len) {
            return TIKU_KITS_NET_ERR_PARAM;
        }
    }

    /* No A record found in any answer */
    return TIKU_KITS_NET_ERR_PARAM;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

void tiku_kits_net_dns_init(void)
{
    dns_state = TIKU_KITS_NET_DNS_STATE_IDLE;
    memset(dns_server, 0, sizeof(dns_server));
    dns_server_set = 0;
    dns_txn_id = 0x1234;
    dns_reply_ready = 0;
    dns_retries = 0;
    memset(dns_resolved_addr, 0, sizeof(dns_resolved_addr));
    dns_resolved_ttl = 0;
    dns_rcode = 0;
    dns_query_hash = 0;
    dns_rx_len = 0;

    /* Flush cache */
    tiku_kits_net_dns_cache_flush();

    /* Ensure port is unbound from any previous session */
    tiku_kits_net_udp_unbind(TIKU_KITS_NET_DNS_LOCAL_PORT);
}

/*---------------------------------------------------------------------------*/

int8_t tiku_kits_net_dns_set_server(const uint8_t *addr)
{
    if (addr == (void *)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    memcpy(dns_server, addr, 4);
    dns_server_set = 1;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

int8_t tiku_kits_net_dns_resolve(const char *hostname)
{
    uint8_t qname[TIKU_KITS_NET_DNS_MAX_HOSTNAME + 2];
    uint16_t qname_len;
    uint16_t hash;
    uint8_t pkt[TIKU_KITS_NET_DNS_HDR_LEN +
                TIKU_KITS_NET_DNS_MAX_HOSTNAME + 2 + 4];
    uint16_t pkt_len;
    int8_t rc;

    if (hostname == (void *)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    if (!dns_server_set) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Compute hostname hash for cache lookup and later insertion */
    hash = dns_hostname_hash(hostname);
    dns_query_hash = hash;

    /* Check cache -- if hit, go directly to DONE */
    if (dns_cache_lookup(hash, dns_resolved_addr) == TIKU_KITS_NET_OK) {
        dns_state = TIKU_KITS_NET_DNS_STATE_DONE;
        return TIKU_KITS_NET_OK;
    }

    /* Encode hostname into DNS wire format */
    qname_len = dns_encode_hostname(hostname, qname);
    if (qname_len == 0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Reset state for new query */
    dns_reply_ready = 0;
    dns_retries = 0;
    memset(dns_resolved_addr, 0, sizeof(dns_resolved_addr));
    dns_resolved_ttl = 0;

    /* Advance transaction ID */
    dns_txn_id++;

    /*
     * Build DNS query packet on the stack.
     *
     * Header (12 bytes):
     *   ID      = dns_txn_id
     *   FLAGS   = RD (recursion desired)
     *   QDCOUNT = 1
     *   ANCOUNT = 0
     *   NSCOUNT = 0
     *   ARCOUNT = 0
     *
     * Question section:
     *   QNAME  = encoded hostname
     *   QTYPE  = A (1)
     *   QCLASS = IN (1)
     */
    memset(pkt, 0, sizeof(pkt));

    /* Transaction ID (big-endian) */
    pkt[TIKU_KITS_NET_DNS_OFF_ID]     =
        (uint8_t)(dns_txn_id >> 8);
    pkt[TIKU_KITS_NET_DNS_OFF_ID + 1] =
        (uint8_t)(dns_txn_id & 0xFF);

    /* Flags: RD=1 */
    pkt[TIKU_KITS_NET_DNS_OFF_FLAGS]     =
        (uint8_t)(TIKU_KITS_NET_DNS_FLAG_RD >> 8);
    pkt[TIKU_KITS_NET_DNS_OFF_FLAGS + 1] =
        (uint8_t)(TIKU_KITS_NET_DNS_FLAG_RD & 0xFF);

    /* QDCOUNT = 1 */
    pkt[TIKU_KITS_NET_DNS_OFF_QDCOUNT]     = 0;
    pkt[TIKU_KITS_NET_DNS_OFF_QDCOUNT + 1] = 1;

    /* Copy encoded QNAME after header */
    pkt_len = TIKU_KITS_NET_DNS_HDR_LEN;
    memcpy(&pkt[pkt_len], qname, qname_len);
    pkt_len += qname_len;

    /* QTYPE = A (1), big-endian */
    pkt[pkt_len++] = 0;
    pkt[pkt_len++] = (uint8_t)TIKU_KITS_NET_DNS_QTYPE_A;

    /* QCLASS = IN (1), big-endian */
    pkt[pkt_len++] = 0;
    pkt[pkt_len++] = (uint8_t)TIKU_KITS_NET_DNS_QCLASS_IN;

    /* Bind local port to receive response */
    rc = tiku_kits_net_udp_bind(TIKU_KITS_NET_DNS_LOCAL_PORT,
                                 dns_udp_recv);
    if (rc != TIKU_KITS_NET_OK) {
        dns_state = TIKU_KITS_NET_DNS_STATE_ERROR;
        return rc;
    }

    /* Send via UDP to DNS server port 53 */
    rc = tiku_kits_net_udp_send(
             dns_server,
             TIKU_KITS_NET_DNS_PORT,
             TIKU_KITS_NET_DNS_LOCAL_PORT,
             pkt,
             pkt_len);

    if (rc != TIKU_KITS_NET_OK) {
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_DNS_LOCAL_PORT);
        dns_state = TIKU_KITS_NET_DNS_STATE_ERROR;
        return rc;
    }

    dns_state = TIKU_KITS_NET_DNS_STATE_SENT;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

int8_t tiku_kits_net_dns_poll(void)
{
    int8_t rc;

    if (dns_state != TIKU_KITS_NET_DNS_STATE_SENT) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    if (dns_reply_ready) {
        rc = dns_parse_response();

        if (rc == TIKU_KITS_NET_OK) {
            /* Cache using the hash saved at resolve() time */
            dns_cache_insert(dns_query_hash, dns_resolved_addr,
                             dns_resolved_ttl);
        }

        /* Clean up regardless of parse result */
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_DNS_LOCAL_PORT);
        dns_reply_ready = 0;

        if (rc == TIKU_KITS_NET_OK) {
            dns_state = TIKU_KITS_NET_DNS_STATE_DONE;
            return TIKU_KITS_NET_OK;
        }

        dns_state = TIKU_KITS_NET_DNS_STATE_ERROR;
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* No reply yet -- check retry count for timeout.
     * Each poll() call without a reply counts as one retry.
     * The caller is expected to poll periodically (e.g. every
     * 1 second), so MAX_RETRIES ~ seconds to wait. */
    dns_retries++;
    if (dns_retries >= TIKU_KITS_NET_DNS_MAX_RETRIES) {
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_DNS_LOCAL_PORT);
        dns_state = TIKU_KITS_NET_DNS_STATE_ERROR;
        return TIKU_KITS_NET_ERR_TIMEOUT;
    }

    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

tiku_kits_net_dns_state_t tiku_kits_net_dns_get_state(void)
{
    return dns_state;
}

/*---------------------------------------------------------------------------*/

int8_t tiku_kits_net_dns_get_addr(uint8_t *addr_out)
{
    if (addr_out == (void *)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    if (dns_state != TIKU_KITS_NET_DNS_STATE_DONE) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    memcpy(addr_out, dns_resolved_addr, 4);
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

uint32_t tiku_kits_net_dns_get_ttl(void)
{
    if (dns_state != TIKU_KITS_NET_DNS_STATE_DONE) {
        return 0;
    }
    return dns_resolved_ttl;
}

/*---------------------------------------------------------------------------*/

int8_t tiku_kits_net_dns_abort(void)
{
    tiku_kits_net_udp_unbind(TIKU_KITS_NET_DNS_LOCAL_PORT);
    dns_state = TIKU_KITS_NET_DNS_STATE_IDLE;
    dns_reply_ready = 0;
    dns_retries = 0;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

void tiku_kits_net_dns_cache_flush(void)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_DNS_CACHE_SIZE; i++) {
        dns_cache[i].valid = 0;
    }
}
