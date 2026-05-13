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
/* Helpers                                                                   */
/*---------------------------------------------------------------------------*/

/* Build an EthII header at the start of `out`: dst (6) + src (6) +
 * etype-BE (2). Returns 14. */
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

/* Returns 1 if the IPv4 destination address starts on a byte that
 * suggests a broadcast/multicast — 255.255.255.255 (limited bcast)
 * or 224.0.0.0/4 (multicast). v1 also forces broadcast for any dst
 * whose host bits are all-1 since we don't know the subnet mask. */
static int
ipv4_dst_is_broadcast(const uint8_t ipv4_pkt[4])
{
    if (ipv4_pkt[0] == 0xFFU && ipv4_pkt[1] == 0xFFU
        && ipv4_pkt[2] == 0xFFU && ipv4_pkt[3] == 0xFFU) return 1;
    if ((ipv4_pkt[0] & 0xF0U) == 0xE0U) return 1;  /* 224.0.0.0/4 */
    return 0;
}

/* Build an ARP reply telling `requester` that `our_ip` is at `our_mac`.
 * Returns 42 (14 EthII + 28 ARP body). */
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

/* Parse an incoming ARP frame and, if it's a request asking for our
 * configured IP, send back a reply. Returns 1 if the frame was an
 * ARP request we answered, 0 otherwise (caller can treat as dropped). */
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
        /* Reply if it's "who has <our IP>". Else drop. ARP reply
         * frames (op=2) are also dropped here for now — they'll feed
         * the unicast ARP cache in phase 5.A.1. */
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
        if (rx_stage_len != 0U) {
            rx_dropped_full += 1U;
            return;
        }
        for (i = 0U; i < ip_len; ++i) {
            rx_stage[i] = frame[ETH_HDR_LEN + i];
        }
        rx_stage_len = ip_len;
    }
}

/*---------------------------------------------------------------------------*/
/* tiku_kits_net_link_t implementation                                       */
/*---------------------------------------------------------------------------*/

/* Outbound: prepend EthII + ship via whd_tx_eth. For v1, the
 * destination MAC is broadcast for limited-broadcast / multicast
 * destinations (DHCP, IGMP query response, etc.); unicast unknown
 * peers fail with NOLINK until an ARP cache lands in 5.A.1. */
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
     * packet. v1 only sends broadcast-class traffic; unicast peers
     * fall through to NOLINK until the ARP cache is wired in. */
    if (ipv4_dst_is_broadcast(&pkt[16])) {
        for (i = 0U; i < 6U; ++i) dst[i] = 0xFFU;
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

/* Inbound: copy any staged IP packet to the caller's buffer. The
 * kit calls this in a loop until we return 0, so single-slot
 * staging is fine — backlog is just deferred to the next poll
 * (with the cost of a drop counter for slot-full events). */
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
