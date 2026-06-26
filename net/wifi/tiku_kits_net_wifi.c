/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_wifi.c - WiFi link backend (CYW43439) for tikukits/net
 *
 * Adapter between the kit's `tiku_kits_net_link_t` contract and the
 * driver's `whd_tx_eth` / `whd_register_rx_callback` API. See the
 * header for the full RESPONSIBILITIES list.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_net_wifi.h"
#include "../ipv4/tiku_kits_net_ipv4.h"
#include <interfaces/wireless/tiku_wireless.h>
#include "drivers/wifi/cyw43/whd.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

#define ETH_HDR_LEN          14U
#define ETHERTYPE_IPV4   0x0800U
#define ETHERTYPE_ARP    0x0806U

#define ARP_OP_REQUEST   0x0001U
#define ARP_OP_REPLY     0x0002U

/* ARP request/reply frame size: 14 EthII + 28 ARP body = 42 bytes.
 * The chip pads to the 64-byte 802.3 minimum on the air; we don't
 * need to pre-pad here (whd_tx_eth ships exactly what we hand it).  */
#define ARP_FRAME_BYTES  42U

/* Single staging frame between the driver RX callback (runner ctx)
 * and the kit's poll_rx (net-proc ctx). Sized to hold any v4 frame
 * the kit will produce. WHD delivers up to 1514 B; we cap at the
 * kit's MTU so the kit's buf_size on poll_rx is enough. */
#define RX_STAGE_BYTES       TIKU_KITS_NET_MTU

/*---------------------------------------------------------------------------*/
/* RX staging buffer                                                         */
/*---------------------------------------------------------------------------*/
/*
 * Single-slot pipe: the driver callback writes one frame at a time,
 * the kit polls and consumes it. Capacity-of-one is sufficient
 * because the kit's net process serialises RX/process/TX. A backlog
 * shows up as `rx_dropped++` so it's observable.
 */
static uint8_t  rx_stage[RX_STAGE_BYTES];
static uint16_t rx_stage_len;       /* 0 = empty */
static uint32_t rx_dropped_full;    /* slot full when new frame arrived */
static uint32_t rx_dropped_other;   /* non-v4 / malformed             */
static uint32_t rx_delivered;       /* slots successfully drained     */

/*---------------------------------------------------------------------------*/
/* Passive ARP cache                                                         */
/*---------------------------------------------------------------------------*/
/*
 * Every IPv4 frame carries (sender_ip, sender_mac). Cache that pair
 * implicitly so unicast TX can resolve dst_ip -> dst_mac without
 * sending our own ARP request. Replies to incoming traffic always
 * find a hit because the destination IS the previous sender. Full
 * outbound-first ARP discovery (gateway we've never heard from)
 * stays on the 5.A.1 list.
 */
#define ARP_CACHE_SIZE 4

typedef struct {
    uint8_t ip[4];
    uint8_t mac[6];
    uint8_t used;
} arp_entry_t;

static arp_entry_t arp_cache[ARP_CACHE_SIZE];
static uint8_t     arp_cache_next;

/**
 * @brief Insert or refresh an (ip, mac) pair in the ARP cache
 *
 * Linear scan over the 4-entry cache; if the IP already has a slot,
 * the MAC is updated in-place. Otherwise the next round-robin slot
 * is overwritten and the cursor advances.
 *
 * @param ip    4-byte IPv4 address (network order)
 * @param mac   6-byte Ethernet address
 */
static void
arp_cache_learn(const uint8_t ip[4], const uint8_t mac[6])
{
    uint8_t i, j;
    for (i = 0U; i < ARP_CACHE_SIZE; ++i) {
        if (arp_cache[i].used
            && arp_cache[i].ip[0] == ip[0] && arp_cache[i].ip[1] == ip[1]
            && arp_cache[i].ip[2] == ip[2] && arp_cache[i].ip[3] == ip[3]) {
            for (j = 0U; j < 6U; ++j) arp_cache[i].mac[j] = mac[j];
            return;
        }
    }
    arp_cache[arp_cache_next].used = 1U;
    for (j = 0U; j < 4U; ++j) arp_cache[arp_cache_next].ip[j]  = ip[j];
    for (j = 0U; j < 6U; ++j) arp_cache[arp_cache_next].mac[j] = mac[j];
    arp_cache_next = (uint8_t)((arp_cache_next + 1U) % ARP_CACHE_SIZE);
}

/**
 * @brief Look up the MAC for an IPv4 destination in the ARP cache
 *
 * Linear scan over the 4-entry cache. On hit, the cached MAC is
 * copied into mac_out.
 *
 * @param ip       4-byte IPv4 destination to resolve
 * @param mac_out  6-byte buffer that receives the cached MAC on hit
 * @return 1 if the address was found (mac_out populated), 0 on miss
 */
static int
arp_cache_lookup(const uint8_t ip[4], uint8_t mac_out[6])
{
    uint8_t i, j;
    for (i = 0U; i < ARP_CACHE_SIZE; ++i) {
        if (arp_cache[i].used
            && arp_cache[i].ip[0] == ip[0] && arp_cache[i].ip[1] == ip[1]
            && arp_cache[i].ip[2] == ip[2] && arp_cache[i].ip[3] == ip[3]) {
            for (j = 0U; j < 6U; ++j) mac_out[j] = arp_cache[i].mac[j];
            return 1;
        }
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
/* Helpers                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Write a 14-byte Ethernet II header at the start of `out`
 *
 * Layout: dst (6) + src (6) + etype big-endian (2). Caller is
 * responsible for ensuring `out` has at least ETH_HDR_LEN bytes.
 *
 * @param out    Destination buffer (>= 14 bytes)
 * @param dst    Destination MAC (6 bytes)
 * @param src    Source MAC (6 bytes)
 * @param etype  Ethertype in host byte order
 * @return Number of bytes written (always 14)
 */
static uint16_t
eth_hdr_build(uint8_t *out, const uint8_t dst[6], const uint8_t src[6],
              uint16_t etype)
{
    uint8_t i;
    for (i = 0U; i < 6U; ++i) out[i]      = dst[i];
    for (i = 0U; i < 6U; ++i) out[6U + i] = src[i];
    out[12] = (uint8_t)((etype >> 8) & 0xFFU);
    out[13] = (uint8_t)( etype       & 0xFFU);
    return ETH_HDR_LEN;
}

/**
 * @brief Test whether an IPv4 destination requires a broadcast MAC
 *
 * Returns true for 255.255.255.255 (limited broadcast) and for
 * 224.0.0.0/4 (multicast). v1 also forces broadcast for any dst
 * whose host bits are all-1 since we don't know the subnet mask.
 *
 * @param ipv4_pkt  4-byte IPv4 destination address
 * @return 1 if a broadcast/multicast MAC should be used, 0 otherwise
 */
static int
ipv4_dst_is_broadcast(const uint8_t ipv4_pkt[4])
{
    if (ipv4_pkt[0] == 0xFFU && ipv4_pkt[1] == 0xFFU
        && ipv4_pkt[2] == 0xFFU && ipv4_pkt[3] == 0xFFU) return 1;
    if ((ipv4_pkt[0] & 0xF0U) == 0xE0U) return 1;  /* 224.0.0.0/4 */
    return 0;
}

/**
 * @brief Build a complete ARP reply frame
 *
 * Tells `requester` that `our_ip` is reachable at `our_mac`. The
 * resulting frame is 42 bytes: 14-byte EthII header + 28-byte ARP
 * body (htype Eth, ptype IPv4, hlen 6, plen 4, op REPLY, then
 * sender/target MAC+IP pairs).
 *
 * @param out            Destination buffer (>= ARP_FRAME_BYTES)
 * @param our_mac        Our MAC address (sender in the reply)
 * @param our_ip         Our IPv4 address (sender in the reply)
 * @param requester_mac  MAC of the host that issued the ARP request
 * @param requester_ip   IPv4 of the host that issued the ARP request
 * @return Number of bytes written (always 42)
 */
static uint16_t
arp_reply_build(uint8_t *out,
                const uint8_t our_mac[6],
                const uint8_t our_ip[4],
                const uint8_t requester_mac[6],
                const uint8_t requester_ip[4])
{
    uint8_t  i;
    uint16_t n = eth_hdr_build(out, requester_mac, our_mac, ETHERTYPE_ARP);
    out[n + 0]  = 0x00U; out[n + 1]  = 0x01U;            /* htype Eth */
    out[n + 2]  = 0x08U; out[n + 3]  = 0x00U;            /* ptype IPv4*/
    out[n + 4]  = 0x06U;                                 /* hlen 6    */
    out[n + 5]  = 0x04U;                                 /* plen 4    */
    out[n + 6]  = (uint8_t)(ARP_OP_REPLY >> 8);
    out[n + 7]  = (uint8_t)(ARP_OP_REPLY & 0xFFU);
    for (i = 0U; i < 6U; ++i) out[n + 8U  + i] = our_mac[i];
    for (i = 0U; i < 4U; ++i) out[n + 14U + i] = our_ip[i];
    for (i = 0U; i < 6U; ++i) out[n + 18U + i] = requester_mac[i];
    for (i = 0U; i < 4U; ++i) out[n + 24U + i] = requester_ip[i];
    return (uint16_t)(n + 28U);
}

/**
 * @brief Parse an incoming ARP frame and answer if it targets us
 *
 * Validates that the frame is a well-formed IPv4-over-Ethernet ARP
 * request (htype=1, ptype=0x0800, hlen=6, plen=4) and that the
 * target IP matches the kit's configured address; if so, sends an
 * ARP reply via whd_tx_eth. Non-request opcodes and requests
 * targeting other hosts are ignored — the AP forwards ARPs broadcast,
 * so seeing one for someone else is expected.
 *
 * @param frame  Pointer to the start of the EthII frame
 * @param len    Total length of the frame in bytes
 * @return 1 if an ARP request was answered, 0 otherwise (caller may
 *         treat as dropped)
 */
static int
arp_handle_in(const uint8_t *frame, uint16_t len)
{
    const uint8_t *body;
    uint16_t       op;
    const uint8_t *kit_ip;
    uint8_t        i;
    uint8_t        reply[ARP_FRAME_BYTES];
    tiku_wireless_status_t st;

    if (len < ARP_FRAME_BYTES) return 0;
    body = frame + ETH_HDR_LEN;

    /* htype=1, ptype=0x0800, hlen=6, plen=4 — anything else, drop. */
    if (body[0] != 0x00U || body[1] != 0x01U
        || body[2] != 0x08U || body[3] != 0x00U
        || body[4] != 0x06U || body[5] != 0x04U) {
        return 0;
    }
    op = (uint16_t)((body[6] << 8) | body[7]);
    if (op != ARP_OP_REQUEST) return 0;

    /* Target IP is at body+24..27. Compare against the kit's configured
     * address; if no match, ignore (the AP forwards ARPs broadcast). */
    kit_ip = tiku_kits_net_ipv4_get_addr();
    if (kit_ip == (const uint8_t *)0) return 0;
    for (i = 0U; i < 4U; ++i) {
        if (body[24U + i] != kit_ip[i]) return 0;
    }

    if (tiku_wireless_status(&st) != 0 || st.up == 0U) return 0;

    /* requester_mac = body[8..13], requester_ip = body[14..17] */
    (void)arp_reply_build(reply, st.mac, kit_ip,
                          &body[8], &body[14]);
    (void)whd_tx_eth(reply, ARP_FRAME_BYTES);
    return 1;
}

/*---------------------------------------------------------------------------*/
/* whd_register_rx_callback hook — runs in runner-process context.           */
/*---------------------------------------------------------------------------*/

/**
 * @brief WHD driver RX callback (runner-process context)
 *
 * Strips the EthII header, learns sender (ip, mac) pairs into the
 * passive ARP cache, answers ARP requests targeting us, and stages a
 * single IPv4 packet for the kit's poll_rx to consume. ARP and
 * non-IPv4 ethertypes are handled (or counted as dropped) inline; an
 * IPv4 frame that arrives while the staging slot is already full
 * bumps rx_dropped_full so backlog is observable.
 *
 * @param frame  Pointer to the start of the EthII frame
 * @param len    Total length of the frame in bytes
 * @param ctx    Opaque callback context (unused, passed by WHD)
 */
static void
wifi_rx_cb(const uint8_t *frame, uint16_t len, void *ctx)
{
    uint16_t etype;
    (void)ctx;

    if (len < ETH_HDR_LEN) {
        rx_dropped_other += 1U;
        return;
    }
    etype = (uint16_t)((frame[12] << 8) | frame[13]);

    if (etype == ETHERTYPE_ARP) {
        /* Learn from any ARP traffic we see (request OR reply) — the
         * ARP body carries sender_mac + sender_ip at body[8..17]. */
        if (len >= ETH_HDR_LEN + 28U) {
            const uint8_t *body = frame + ETH_HDR_LEN;
            arp_cache_learn(&body[14], &body[8]);
        }
        /* Reply if it's "who has <our IP>". Otherwise just leave the
         * cached MAC in place. */
        (void)arp_handle_in(frame, len);
        return;
    }

    if (etype != ETHERTYPE_IPV4) {
        rx_dropped_other += 1U;
        return;
    }

    {
        uint16_t ip_len = (uint16_t)(len - ETH_HDR_LEN);
        uint16_t i;

        if (ip_len > RX_STAGE_BYTES) {
            rx_dropped_other += 1U;
            return;
        }

        /* Passive ARP learn: IPv4 header has sender IP at offset 12
         * (12..15) within the IP packet; Ethernet src MAC is at
         * frame[6..11]. */
        if (ip_len >= 20U) {
            arp_cache_learn(frame + ETH_HDR_LEN + 12U,
                            frame + 6U);
        }

        /* Copy out of the const, borrowed frame into mutable scratch, then push
         * straight into the IP stack (ipv4_input may rewrite the buffer in
         * place). Done here in the runner's RX callback -- exactly like the ARP
         * reply above -- so RX works with NO poll loop draining a stage: the
         * shell+net build owns RX in the shell and never runs the net process's
         * active_link->poll_rx() loop. The standalone net app is unaffected
         * (its poll_rx() simply finds the stage empty). */
        for (i = 0U; i < ip_len; ++i) {
            rx_stage[i] = frame[ETH_HDR_LEN + i];
        }
        rx_delivered += 1U;
        tiku_kits_net_ipv4_input(rx_stage, ip_len);
    }
}

/*---------------------------------------------------------------------------*/
/* tiku_kits_net_link_t implementation                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief `tiku_kits_net_link_t.send` impl — outbound IPv4 packet
 *
 * Prepends an EthII header (broadcast MAC for limited-broadcast /
 * multicast destinations, otherwise the cached MAC from the passive
 * ARP cache) and ships the result via whd_tx_eth. For v1, unicast
 * peers we have never heard from fail with NOLINK until outbound-
 * first ARP discovery lands in 5.A.1.
 *
 * @param pkt  IPv4 packet (>= 20 bytes; dst at bytes 16..19)
 * @param len  Length of the IPv4 packet in bytes
 * @return TIKU_KITS_NET_OK on success, otherwise a TIKU_KITS_NET_ERR_*
 *         code mapped from the underlying driver result
 */
static int8_t
wifi_send(const uint8_t *pkt, uint16_t len)
{
    tiku_wireless_status_t st;
    uint8_t   eth[ETH_HDR_LEN + TIKU_KITS_NET_MTU];
    uint8_t   dst[6];
    uint16_t  i, total;
    int       rc;

    if (pkt == (const uint8_t *)0 || len < 20U) {
        return TIKU_KITS_NET_ERR_PARAM;
    }
    if ((uint32_t)len + ETH_HDR_LEN > sizeof eth) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }
    if (tiku_wireless_status(&st) != 0 || st.up == 0U) {
        return TIKU_KITS_NET_ERR_NOLINK;
    }

    /* Destination MAC selection: IP dst at bytes 16..19 of an IPv4
     * packet. Broadcast/multicast -> broadcast MAC. Unicast hits
     * the passive ARP cache populated by wifi_rx_cb; cache miss
     * still returns NOLINK (outbound-first ARP discovery is the
     * remaining 5.A.1 piece). */
    if (ipv4_dst_is_broadcast(&pkt[16])) {
        for (i = 0U; i < 6U; ++i) dst[i] = 0xFFU;
    } else if (arp_cache_lookup(&pkt[16], dst)) {
        /* dst now holds the cached MAC for pkt[16..19]. */
    } else {
        return TIKU_KITS_NET_ERR_NOLINK;
    }

    (void)eth_hdr_build(eth, dst, st.mac, ETHERTYPE_IPV4);
    for (i = 0U; i < len; ++i) eth[ETH_HDR_LEN + i] = pkt[i];
    total = (uint16_t)(ETH_HDR_LEN + len);

    rc = whd_tx_eth(eth, total);
    if (rc == TIKU_DRV_OK)            return TIKU_KITS_NET_OK;
    if (rc == TIKU_DRV_ERR_INVALID)   return TIKU_KITS_NET_ERR_PARAM;
    if (rc == TIKU_DRV_ERR_TIMEOUT)   return TIKU_KITS_NET_ERR_TIMEOUT;
    return TIKU_KITS_NET_ERR_NOLINK;
}

/**
 * @brief `tiku_kits_net_link_t.poll_rx` impl — drain the staging slot
 *
 * Copies any staged IP packet into the caller's buffer. The kit calls
 * this in a loop until we return 0, so single-slot staging is fine —
 * backlog is just deferred to the next poll (with the cost of a drop
 * counter for slot-full events). A caller buffer that's too small is
 * also reported as zero-frame so corrupt delivery is avoided.
 *
 * @param buf       Destination buffer for the staged IP packet
 * @param buf_size  Capacity of `buf` in bytes
 * @param pos       Out: length of the delivered frame in bytes
 * @return 1 if a frame was delivered, 0 otherwise (empty slot, bad
 *         arguments, or oversized frame dropped)
 */
static uint8_t
wifi_poll_rx(uint8_t *buf, uint16_t buf_size, uint16_t *pos)
{
    uint16_t n, i;
    if (buf == (uint8_t *)0 || pos == (uint16_t *)0) return 0U;
    if (rx_stage_len == 0U) return 0U;

    n = rx_stage_len;
    if (n > buf_size) {
        /* Caller buffer too small — drop the frame to keep the slot
         * unblocked; signal as zero-frame to avoid corrupt delivery. */
        rx_stage_len = 0U;
        rx_dropped_other += 1U;
        return 0U;
    }
    for (i = 0U; i < n; ++i) buf[i] = rx_stage[i];
    *pos = n;
    rx_stage_len = 0U;
    rx_delivered += 1U;
    return 1U;
}

/*---------------------------------------------------------------------------*/
/* Public                                                                    */
/*---------------------------------------------------------------------------*/

const tiku_kits_net_link_t tiku_kits_net_wifi_link = {
    .send    = wifi_send,
    .poll_rx = wifi_poll_rx,
    .name    = "WiFi"
};

/**
 * @brief Initialise the WiFi link adapter
 *
 * Clears the RX staging slot and drop counters, registers our RX
 * callback with the WHD driver, and installs this link with the IPv4
 * kit via tiku_kits_net_ipv4_set_link so outbound packets flow
 * through wifi_send.
 *
 * @return TIKU_KITS_NET_OK on success, TIKU_KITS_NET_ERR_NOLINK if
 *         the driver rejected the callback registration
 */
int8_t
tiku_kits_net_wifi_init(void)
{
    int rc;

    rx_stage_len      = 0U;
    rx_dropped_full   = 0UL;
    rx_dropped_other  = 0UL;
    rx_delivered      = 0UL;

    rc = whd_register_rx_callback(wifi_rx_cb, (void *)0);
    if (rc != TIKU_DRV_OK) return TIKU_KITS_NET_ERR_NOLINK;

    tiku_kits_net_ipv4_set_link(&tiku_kits_net_wifi_link);
    return TIKU_KITS_NET_OK;
}
