/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_dhcp.c - DHCP client implementation (RFC 2131)
 *
 * Poll-based DHCP client that runs the DORA exchange (Discover-Offer-
 * Request-Ack) to obtain an IPv4 address from a DHCP server.
 *
 * DHCP packets are built directly in net_buf (via ipv4_get_buf)
 * because they exceed the normal UDP max payload and require
 * source IP 0.0.0.0.  Responses are received via a standard UDP
 * port 68 binding and parsed in the receive callback.
 *
 * RAM budget:
 *   - Static state: ~42 bytes
 *   - MTU increase: +172 bytes (128 -> 300)
 *   - Total:        ~214 bytes additional
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

#include "tiku_kits_net_dhcp.h"
#include "tiku_kits_net_ipv4.h"
#include "tiku_kits_net_udp.h"
#include <kernel/process/tiku_process.h>
#include <kernel/timers/tiku_timer.h>
#include <kernel/timers/tiku_clock.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* STATIC STATE                                                              */
/*---------------------------------------------------------------------------*/

/** Current DHCP client state. */
static tiku_kits_net_dhcp_state_t dhcp_state;

/** Pending event from the UDP receive callback. */
static volatile tiku_kits_net_dhcp_evt_t pending_evt;

/** Lease information (valid when state == BOUND). */
static tiku_kits_net_dhcp_lease_t lease;

/** Client hardware address (6 bytes, used in CHADDR field). */
static uint8_t hw_addr[6];

/** Transaction ID for the current exchange. */
static uint32_t xid;

/** Retry counter for DISCOVER/REQUEST. */
static uint8_t retries;

/** Saved IP address before DHCP (restored on failure). */
static uint8_t saved_ip[4];

/*---------------------------------------------------------------------------*/
/* HELPERS: 32-BIT BYTE-ORDER                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read a 32-bit big-endian value from a byte buffer.
 */
static uint32_t
read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           ((uint32_t)p[3]);
}

/**
 * @brief Write a 32-bit value in big-endian to a byte buffer.
 */
static void
write_be32(uint8_t *p, uint32_t val)
{
    p[0] = (uint8_t)(val >> 24);
    p[1] = (uint8_t)(val >> 16);
    p[2] = (uint8_t)(val >>  8);
    p[3] = (uint8_t)(val);
}

/*---------------------------------------------------------------------------*/
/* OPTION PARSER                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parse DHCP options from a received message.
 *
 * Walks the TLV option list starting at @p opts, extracting the
 * message type, subnet mask, router, server ID, and lease time
 * into the static lease struct.
 *
 * @param opts     Pointer to the first option byte (after magic cookie)
 * @param opts_len Number of option bytes available
 * @return The DHCP message type (1-7), or 0 if not found.
 */
static uint8_t
parse_options(const uint8_t *opts, uint16_t opts_len)
{
    uint16_t i = 0;
    uint8_t  msg_type = 0;
    uint8_t  tag, len;

    while (i < opts_len) {
        tag = opts[i];

        /* End marker */
        if (tag == TIKU_KITS_NET_DHCP_OPT_END) {
            break;
        }

        /* Padding (no length byte) */
        if (tag == TIKU_KITS_NET_DHCP_OPT_PAD) {
            i++;
            continue;
        }

        /* TLV: need at least tag + length */
        if (i + 1 >= opts_len) {
            break;
        }
        len = opts[i + 1];
        if (i + 2 + len > opts_len) {
            break;  /* truncated option */
        }

        switch (tag) {
        case TIKU_KITS_NET_DHCP_OPT_MSG_TYPE:
            if (len >= 1) {
                msg_type = opts[i + 2];
            }
            break;

        case TIKU_KITS_NET_DHCP_OPT_SUBNET:
            if (len >= 4) {
                memcpy(lease.mask, &opts[i + 2], 4);
            }
            break;

        case TIKU_KITS_NET_DHCP_OPT_ROUTER:
            if (len >= 4) {
                memcpy(lease.gateway, &opts[i + 2], 4);
            }
            break;

        case TIKU_KITS_NET_DHCP_OPT_SERVER_ID:
            if (len >= 4) {
                memcpy(lease.server, &opts[i + 2], 4);
            }
            break;

        case TIKU_KITS_NET_DHCP_OPT_LEASE_TIME:
            if (len >= 4) {
                lease.lease_sec = read_be32(&opts[i + 2]);
            }
            break;

        default:
            /* Skip unknown options */
            break;
        }

        i += 2 + len;
    }

    return msg_type;
}

/*---------------------------------------------------------------------------*/
/* UDP RECEIVE CALLBACK                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief UDP callback for DHCP responses on port 68.
 *
 * Validates the response (op=2, XID match, magic cookie), parses
 * options, and sets the appropriate pending event for poll().
 * Does NOT call udp_send (re-entrancy guard).
 *
 * Accepts two payload layouts:
 *   Standard (>= 241 B): magic cookie at offset 236, options at 240
 *   Compact  (>=  49 B): magic cookie at offset  44, options at  48
 *
 * The compact layout omits the 192-byte sname + file fields that
 * this client never inspects.  It exists because the eZ-FET debug
 * probe's backchannel UART buffer (~150 B) is too small to carry
 * a standard DHCP reply (~296 B on the wire).
 */
static void
dhcp_recv_cb(const uint8_t *src_addr, uint16_t src_port,
             const uint8_t *payload, uint16_t payload_len)
{
    uint8_t msg_type;
    uint32_t rx_xid;
    uint16_t magic_off;
    uint16_t opts_off;

    (void)src_addr;
    (void)src_port;

    /* Determine payload layout: standard or compact.
     * Standard: >= 241 bytes, cookie at 236, options at 240.
     * Compact:  >=  49 bytes, cookie at  44, options at  48. */
    if (payload_len >= TIKU_KITS_NET_DHCP_OFF_OPTIONS + 1) {
        magic_off = TIKU_KITS_NET_DHCP_OFF_MAGIC;
        opts_off  = TIKU_KITS_NET_DHCP_OFF_OPTIONS;
    } else if (payload_len >=
               TIKU_KITS_NET_DHCP_COMPACT_OPTIONS + 1) {
        magic_off = TIKU_KITS_NET_DHCP_COMPACT_MAGIC;
        opts_off  = TIKU_KITS_NET_DHCP_COMPACT_OPTIONS;
    } else {
        return;
    }

    /* Must be a BOOTP reply (op == 2) */
    if (payload[TIKU_KITS_NET_DHCP_OFF_OP] !=
        TIKU_KITS_NET_DHCP_OP_REPLY) {
        return;
    }

    /* Transaction ID must match */
    rx_xid = read_be32(&payload[TIKU_KITS_NET_DHCP_OFF_XID]);
    if (rx_xid != xid) {
        return;
    }

    /* Validate magic cookie: 99.130.83.99 */
    if (payload[magic_off]     != TIKU_KITS_NET_DHCP_MAGIC_0 ||
        payload[magic_off + 1] != TIKU_KITS_NET_DHCP_MAGIC_1 ||
        payload[magic_off + 2] != TIKU_KITS_NET_DHCP_MAGIC_2 ||
        payload[magic_off + 3] != TIKU_KITS_NET_DHCP_MAGIC_3) {
        return;
    }

    /* Extract YIADDR (the IP the server is offering/assigning) */
    memcpy(lease.ip, &payload[TIKU_KITS_NET_DHCP_OFF_YIADDR], 4);

    /* Parse options */
    msg_type = parse_options(
        &payload[opts_off],
        payload_len - opts_off);

    switch (msg_type) {
    case TIKU_KITS_NET_DHCP_MSG_OFFER:
        pending_evt = TIKU_KITS_NET_DHCP_EVT_OFFER;
        break;
    case TIKU_KITS_NET_DHCP_MSG_ACK:
        pending_evt = TIKU_KITS_NET_DHCP_EVT_ACK;
        break;
    case TIKU_KITS_NET_DHCP_MSG_NAK:
        pending_evt = TIKU_KITS_NET_DHCP_EVT_NAK;
        break;
    default:
        /* Ignore other message types */
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* PACKET BUILDER                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build and send a DHCP packet directly in net_buf.
 *
 * Constructs the full IPv4 + UDP + DHCP packet in the shared buffer:
 *   - IPv4: src=0.0.0.0, dst=255.255.255.255, proto=UDP
 *   - UDP:  sport=68, dport=67, checksum=0 (optional per RFC 768)
 *   - DHCP: fixed header + magic cookie + options
 *
 * @param msg_type  DHCP message type (DISCOVER, REQUEST, RELEASE)
 * @return TIKU_KITS_NET_OK on success, negative on error.
 */
static int8_t
dhcp_send(uint8_t msg_type)
{
    uint8_t *buf;
    uint16_t buf_size;
    uint8_t *dhcp;
    uint8_t *opts;
    uint16_t opt_pos;
    uint16_t udp_len;
    uint16_t total_len;
    uint16_t dhcp_payload_len;

    buf = tiku_kits_net_ipv4_get_buf(&buf_size);
    if (buf == (void *)0) {
        return TIKU_KITS_NET_ERR_NOLINK;
    }

    /* Zero the entire buffer (clears sname, file, padding) */
    memset(buf, 0, buf_size);

    /* Pointer to DHCP payload (after IPv4 + UDP headers) */
    dhcp = buf + TIKU_KITS_NET_IPV4_HDR_LEN + 8;  /* 20 + 8 = 28 */

    /* --- DHCP fixed header (236 bytes, mostly zero from memset) --- */
    dhcp[TIKU_KITS_NET_DHCP_OFF_OP]    = TIKU_KITS_NET_DHCP_OP_REQUEST;
    dhcp[TIKU_KITS_NET_DHCP_OFF_HTYPE] = 1;    /* Ethernet */
    dhcp[TIKU_KITS_NET_DHCP_OFF_HLEN]  = 6;    /* 6-byte addr */
    dhcp[TIKU_KITS_NET_DHCP_OFF_HOPS]  = 0;

    /* Transaction ID */
    write_be32(&dhcp[TIKU_KITS_NET_DHCP_OFF_XID], xid);

    /* Broadcast flag -- ensures server replies to 255.255.255.255 */
    dhcp[TIKU_KITS_NET_DHCP_OFF_FLAGS]     =
        (uint8_t)(TIKU_KITS_NET_DHCP_FLAG_BROADCAST >> 8);
    dhcp[TIKU_KITS_NET_DHCP_OFF_FLAGS + 1] =
        (uint8_t)(TIKU_KITS_NET_DHCP_FLAG_BROADCAST & 0xFF);

    /* For REQUEST, ciaddr stays 0 (we don't own the IP yet) */

    /* Client hardware address (6 bytes, rest zeroed by memset) */
    memcpy(&dhcp[TIKU_KITS_NET_DHCP_OFF_CHADDR], hw_addr, 6);

    /* Magic cookie: 99.130.83.99 */
    dhcp[TIKU_KITS_NET_DHCP_OFF_MAGIC]     = TIKU_KITS_NET_DHCP_MAGIC_0;
    dhcp[TIKU_KITS_NET_DHCP_OFF_MAGIC + 1] = TIKU_KITS_NET_DHCP_MAGIC_1;
    dhcp[TIKU_KITS_NET_DHCP_OFF_MAGIC + 2] = TIKU_KITS_NET_DHCP_MAGIC_2;
    dhcp[TIKU_KITS_NET_DHCP_OFF_MAGIC + 3] = TIKU_KITS_NET_DHCP_MAGIC_3;

    /* --- Options --- */
    opts = &dhcp[TIKU_KITS_NET_DHCP_OFF_OPTIONS];
    opt_pos = 0;

    /* Option 53: DHCP Message Type (always present) */
    opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_MSG_TYPE;
    opts[opt_pos++] = 1;
    opts[opt_pos++] = msg_type;

    if (msg_type == TIKU_KITS_NET_DHCP_MSG_DISCOVER) {
        /* Option 55: Parameter Request List */
        opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_PARAM_LIST;
        opts[opt_pos++] = 3;  /* requesting 3 options */
        opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_SUBNET;
        opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_ROUTER;
        opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_LEASE_TIME;
    }

    if (msg_type == TIKU_KITS_NET_DHCP_MSG_REQUEST) {
        /* Option 50: Requested IP Address */
        opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_REQUESTED_IP;
        opts[opt_pos++] = 4;
        memcpy(&opts[opt_pos], lease.ip, 4);
        opt_pos += 4;

        /* Option 54: Server Identifier */
        opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_SERVER_ID;
        opts[opt_pos++] = 4;
        memcpy(&opts[opt_pos], lease.server, 4);
        opt_pos += 4;
    }

    if (msg_type == TIKU_KITS_NET_DHCP_MSG_RELEASE) {
        /* For RELEASE, set ciaddr to the bound IP */
        memcpy(&dhcp[TIKU_KITS_NET_DHCP_OFF_CIADDR], lease.ip, 4);

        /* Option 54: Server Identifier */
        opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_SERVER_ID;
        opts[opt_pos++] = 4;
        memcpy(&opts[opt_pos], lease.server, 4);
        opt_pos += 4;
    }

    /* End marker */
    opts[opt_pos++] = TIKU_KITS_NET_DHCP_OPT_END;

    /* --- Compute lengths --- */
    dhcp_payload_len = TIKU_KITS_NET_DHCP_OFF_OPTIONS + opt_pos;
    udp_len = 8 + dhcp_payload_len;
    total_len = TIKU_KITS_NET_IPV4_HDR_LEN + udp_len;

    /* Ensure it fits in the buffer */
    if (total_len > buf_size) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    /* --- Build IPv4 header (20 bytes) --- */
    buf[0]  = 0x45;                          /* version=4, IHL=5 */
    buf[1]  = 0x00;                          /* TOS = 0 */
    buf[2]  = (uint8_t)(total_len >> 8);     /* total length */
    buf[3]  = (uint8_t)(total_len & 0xFF);
    buf[4]  = 0x00; buf[5] = 0x00;           /* identification */
    buf[6]  = 0x00; buf[7] = 0x00;           /* flags + frag */
    buf[8]  = TIKU_KITS_NET_TTL;             /* TTL */
    buf[9]  = TIKU_KITS_NET_IPV4_PROTO_UDP;  /* protocol = UDP */
    buf[10] = 0x00; buf[11] = 0x00;          /* checksum (ipv4_output fills) */

    /* Source IP: 0.0.0.0 (no address yet) */
    buf[12] = 0; buf[13] = 0; buf[14] = 0; buf[15] = 0;

    /* Destination IP: 255.255.255.255 (broadcast) */
    buf[16] = 255; buf[17] = 255; buf[18] = 255; buf[19] = 255;

    /* --- Build UDP header (8 bytes) --- */
    {
        uint8_t *udp = buf + TIKU_KITS_NET_IPV4_HDR_LEN;
        /* Source port = 68 (DHCP client) */
        udp[0] = (uint8_t)(TIKU_KITS_NET_DHCP_CLIENT_PORT >> 8);
        udp[1] = (uint8_t)(TIKU_KITS_NET_DHCP_CLIENT_PORT & 0xFF);
        /* Destination port = 67 (DHCP server) */
        udp[2] = (uint8_t)(TIKU_KITS_NET_DHCP_SERVER_PORT >> 8);
        udp[3] = (uint8_t)(TIKU_KITS_NET_DHCP_SERVER_PORT & 0xFF);
        /* UDP length */
        udp[4] = (uint8_t)(udp_len >> 8);
        udp[5] = (uint8_t)(udp_len & 0xFF);
        /* Checksum = 0 (optional per RFC 768 for broadcast) */
        udp[6] = 0; udp[7] = 0;
    }

    /* Transmit via ipv4_output (recomputes IPv4 header checksum) */
    return tiku_kits_net_ipv4_output(buf, total_len);
}

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_dhcp_init(void)
{
    tiku_kits_net_udp_unbind(TIKU_KITS_NET_DHCP_CLIENT_PORT);

    dhcp_state  = TIKU_KITS_NET_DHCP_STATE_IDLE;
    pending_evt = TIKU_KITS_NET_DHCP_EVT_NONE;
    retries     = 0;
    xid         = 0;

    memset(&lease, 0, sizeof(lease));
    memset(hw_addr, 0, sizeof(hw_addr));
    memset(saved_ip, 0, sizeof(saved_ip));
}

/*---------------------------------------------------------------------------*/
/* DHCP EXCHANGE                                                             */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_dhcp_start(const uint8_t *client_hw)
{
    static const uint8_t default_hw[] = TIKU_KITS_NET_DHCP_DEFAULT_HWADDR;
    static const uint8_t zero_ip[4] = {0, 0, 0, 0};
    const uint8_t *cur_ip;
    int8_t rc;

    if (dhcp_state == TIKU_KITS_NET_DHCP_STATE_DISCOVER_SENT ||
        dhcp_state == TIKU_KITS_NET_DHCP_STATE_REQUESTING) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Save current IP and set to 0.0.0.0 for DHCP */
    cur_ip = tiku_kits_net_ipv4_get_addr();
    memcpy(saved_ip, cur_ip, 4);
    tiku_kits_net_ipv4_set_addr(zero_ip);

    /* Store client hardware address */
    if (client_hw != (void *)0) {
        memcpy(hw_addr, client_hw, 6);
    } else {
        memcpy(hw_addr, default_hw, 6);
    }

    /* Generate a transaction ID from the hardware address */
    xid = ((uint32_t)hw_addr[0] << 24) |
           ((uint32_t)hw_addr[1] << 16) |
           ((uint32_t)hw_addr[2] <<  8) |
           ((uint32_t)hw_addr[3]);
    xid ^= 0x44484350UL;  /* XOR with "DHCP" */

    /* Clear previous lease data */
    memset(&lease, 0, sizeof(lease));
    pending_evt = TIKU_KITS_NET_DHCP_EVT_NONE;
    retries = 0;

    /* Bind client port 68 for receiving responses */
    rc = tiku_kits_net_udp_bind(TIKU_KITS_NET_DHCP_CLIENT_PORT,
                                 dhcp_recv_cb);
    if (rc != TIKU_KITS_NET_OK) {
        tiku_kits_net_ipv4_set_addr(saved_ip);
        return rc;
    }

    /* Send DHCPDISCOVER */
    rc = dhcp_send(TIKU_KITS_NET_DHCP_MSG_DISCOVER);
    if (rc != TIKU_KITS_NET_OK) {
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_DHCP_CLIENT_PORT);
        tiku_kits_net_ipv4_set_addr(saved_ip);
        return rc;
    }

    dhcp_state = TIKU_KITS_NET_DHCP_STATE_DISCOVER_SENT;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/
/* POLLING                                                                   */
/*---------------------------------------------------------------------------*/

tiku_kits_net_dhcp_evt_t
tiku_kits_net_dhcp_poll(void)
{
    tiku_kits_net_dhcp_evt_t evt;

    evt = pending_evt;
    pending_evt = TIKU_KITS_NET_DHCP_EVT_NONE;

    if (evt == TIKU_KITS_NET_DHCP_EVT_NONE) {
        /* No event -- retransmit or timeout.
         * RFC 2131 Section 4.1: client retransmits DISCOVER if no
         * OFFER is received.  We retransmit on each poll until
         * MAX_RETRIES is exhausted. */
        retries++;
        if (retries >= TIKU_KITS_NET_DHCP_MAX_RETRIES) {
            tiku_kits_net_udp_unbind(TIKU_KITS_NET_DHCP_CLIENT_PORT);
            tiku_kits_net_ipv4_set_addr(saved_ip);
            dhcp_state = TIKU_KITS_NET_DHCP_STATE_ERROR;
            return TIKU_KITS_NET_DHCP_EVT_TIMEOUT;
        }

        /* Retransmit while waiting for a response.
         * RFC 2131 Section 4.1: client retransmits if no response.
         *
         * CRITICAL: only retransmit if the SLIP receive buffer is
         * idle (no partial frame being decoded).  dhcp_send() writes
         * to net_buf via ipv4_get_buf(), which would destroy any
         * partially-received OFFER or ACK in the shared buffer.
         * This is the key guard against the half-duplex buffer
         * conflict on constrained systems with a single net_buf. */
        if (tiku_kits_net_ipv4_rx_idle()) {
            if (dhcp_state ==
                TIKU_KITS_NET_DHCP_STATE_DISCOVER_SENT) {
                dhcp_send(TIKU_KITS_NET_DHCP_MSG_DISCOVER);
            } else if (dhcp_state ==
                       TIKU_KITS_NET_DHCP_STATE_REQUESTING) {
                dhcp_send(TIKU_KITS_NET_DHCP_MSG_REQUEST);
            }
        }

        return TIKU_KITS_NET_DHCP_EVT_NONE;
    }

    switch (evt) {

    case TIKU_KITS_NET_DHCP_EVT_OFFER:
        /* OFFER received -- send DHCPREQUEST for the offered IP */
        if (dhcp_state == TIKU_KITS_NET_DHCP_STATE_DISCOVER_SENT) {
            retries = 0;
            dhcp_send(TIKU_KITS_NET_DHCP_MSG_REQUEST);
            dhcp_state = TIKU_KITS_NET_DHCP_STATE_REQUESTING;
        }
        break;

    case TIKU_KITS_NET_DHCP_EVT_ACK:
        /* ACK received -- lease is active */
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_DHCP_CLIENT_PORT);
        tiku_kits_net_ipv4_set_addr(lease.ip);
        dhcp_state = TIKU_KITS_NET_DHCP_STATE_BOUND;
        break;

    case TIKU_KITS_NET_DHCP_EVT_NAK:
        /* NAK -- server rejected our request */
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_DHCP_CLIENT_PORT);
        tiku_kits_net_ipv4_set_addr(saved_ip);
        dhcp_state = TIKU_KITS_NET_DHCP_STATE_ERROR;
        break;

    default:
        break;
    }

    return evt;
}

/*---------------------------------------------------------------------------*/
/* STATE AND LEASE QUERY                                                     */
/*---------------------------------------------------------------------------*/

tiku_kits_net_dhcp_state_t
tiku_kits_net_dhcp_get_state(void)
{
    return dhcp_state;
}

/*---------------------------------------------------------------------------*/

const tiku_kits_net_dhcp_lease_t *
tiku_kits_net_dhcp_get_lease(void)
{
    if (dhcp_state != TIKU_KITS_NET_DHCP_STATE_BOUND) {
        return (void *)0;
    }
    return &lease;
}

/*---------------------------------------------------------------------------*/
/* RELEASE                                                                   */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_dhcp_release(void)
{
    if (dhcp_state == TIKU_KITS_NET_DHCP_STATE_BOUND) {
        /* Rebind port briefly to send the release */
        tiku_kits_net_udp_bind(TIKU_KITS_NET_DHCP_CLIENT_PORT,
                                dhcp_recv_cb);
        dhcp_send(TIKU_KITS_NET_DHCP_MSG_RELEASE);
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_DHCP_CLIENT_PORT);
    }

    dhcp_state  = TIKU_KITS_NET_DHCP_STATE_IDLE;
    pending_evt = TIKU_KITS_NET_DHCP_EVT_NONE;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/
/* DHCP PROCESS                                                              */
/*---------------------------------------------------------------------------*/

TIKU_PROCESS(tiku_kits_net_dhcp_process, "dhcp");

static struct tiku_timer dhcp_timer;

/** Boot delay before starting DHCP (seconds).
 *  Must be > 2.5s so that the SLIP host's serial port setup
 *  (2.0s boot wait + 0.5s flush) completes before the first
 *  DISCOVER is sent. */
#define DHCP_BOOT_DELAY_SEC  4

/** Poll interval while waiting for DHCP responses (ticks). */
#define DHCP_POLL_TICKS      (TIKU_CLOCK_SECOND)

TIKU_PROCESS_THREAD(tiku_kits_net_dhcp_process, ev, data)
{
    (void)data;

    TIKU_PROCESS_BEGIN();

    /* Wait for the net process to initialise SLIP and the link */
    tiku_timer_set_event(&dhcp_timer,
                          TIKU_CLOCK_SECOND * DHCP_BOOT_DELAY_SEC);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /* Start DHCP with the default hardware address */
    tiku_kits_net_dhcp_init();
    if (tiku_kits_net_dhcp_start((void *)0) != TIKU_KITS_NET_OK) {
        TIKU_PROCESS_EXIT();
    }

    /* Poll until BOUND or ERROR */
    while (dhcp_state == TIKU_KITS_NET_DHCP_STATE_DISCOVER_SENT ||
           dhcp_state == TIKU_KITS_NET_DHCP_STATE_REQUESTING) {

        tiku_timer_set_event(&dhcp_timer, DHCP_POLL_TICKS);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

        tiku_kits_net_dhcp_poll();
    }

    TIKU_PROCESS_END();
}
