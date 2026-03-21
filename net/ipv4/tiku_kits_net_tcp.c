/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_tcp.c - TCP transport protocol (RFC 793)
 *
 * Implements connection management, the TCP state machine, data
 * transfer with FRAM-backed retransmission and receive buffers,
 * MSS negotiation, and retransmission with exponential backoff.
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

#if TIKU_KITS_NET_TCP_ENABLE

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_net_tcp.h"
#include "tiku_kits_net_ipv4.h"
#include <kernel/memory/tiku_mem.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* FRAM-BACKED STORAGE                                                       */
/*---------------------------------------------------------------------------*/

/**
 * TX segment pool backing array in FRAM.  Each block stores one
 * retransmission segment (metadata + payload).
 */
__attribute__((section(".persistent"), aligned(2)))
static uint8_t tcp_tx_pool_buf[TIKU_KITS_NET_TCP_TX_POOL_COUNT *
                                TIKU_KITS_NET_TCP_TX_SEG_BLOCK];

/**
 * Per-connection RX ring buffers in FRAM.  Indexed by connection
 * table slot number.
 */
__attribute__((section(".persistent"), aligned(2)))
static uint8_t tcp_rx_bufs[TIKU_KITS_NET_TCP_MAX_CONNS]
                           [TIKU_KITS_NET_TCP_RX_BUF_SIZE];

/*---------------------------------------------------------------------------*/
/* STATIC DATA (SRAM)                                                        */
/*---------------------------------------------------------------------------*/

/** Connection table -- one slot per simultaneous connection */
static tiku_kits_net_tcp_conn_t conn_table[TIKU_KITS_NET_TCP_MAX_CONNS];

/** Listener table entry */
typedef struct {
    uint16_t                      port;      /**< Listening port (0 = free) */
    tiku_kits_net_tcp_recv_cb_t   recv_cb;
    tiku_kits_net_tcp_event_cb_t  event_cb;
} tcp_listener_t;

/** Listener table */
static tcp_listener_t listeners[TIKU_KITS_NET_TCP_MAX_LISTENERS];

/** TX segment pool control block */
static tiku_pool_t seg_pool;

/** ISS counter */
static uint32_t iss_counter;

/*---------------------------------------------------------------------------*/
/* TX SEGMENT DESCRIPTOR                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief TX segment descriptor stored in FRAM pool blocks.
 *
 * Each block is TIKU_KITS_NET_TCP_TX_SEG_BLOCK bytes.  The first
 * 10 bytes are metadata; the remainder is payload data for
 * retransmission.
 */
typedef struct tcp_txseg {
    struct tcp_txseg *next;      /**< Next in TX queue (2B on MSP430) */
    uint32_t          seq;       /**< Starting sequence number */
    uint16_t          data_len;  /**< Payload length */
    uint8_t           flags;     /**< TCP flags (SYN, FIN) */
    uint8_t           _pad;      /**< Alignment padding */
    /* uint8_t payload[MSS] follows -- accessed via (uint8_t *)(seg + 1) */
} tcp_txseg_t;

/*---------------------------------------------------------------------------*/
/* NVM WRITE HELPER                                                          */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* POOL HELPERS (MPU-aware)                                                  */
/*---------------------------------------------------------------------------*/

static void *
seg_alloc(void)
{
    void *p;
    uint16_t saved = tiku_mpu_unlock_nvm();
    p = tiku_pool_alloc(&seg_pool);
    tiku_mpu_lock_nvm(saved);
    return p;
}

static void
seg_free(void *p)
{
    uint16_t saved = tiku_mpu_unlock_nvm();
    tiku_pool_free(&seg_pool, p);
    tiku_mpu_lock_nvm(saved);
}

/*---------------------------------------------------------------------------*/
/* TCP PSEUDO-HEADER CHECKSUM                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute TCP checksum including the IPv4 pseudo-header.
 *
 * Same algorithm as UDP (RFC 1071), with protocol field = 6.
 */
static uint16_t
tcp_chksum(const uint8_t *ip_hdr, const uint8_t *tcp,
           uint16_t tcp_len)
{
    uint8_t pseudo[12];
    uint32_t sum = 0;
    uint16_t i;

    /* Build 12-byte pseudo-header */
    memcpy(pseudo,     ip_hdr + 12, 4);  /* source IP */
    memcpy(pseudo + 4, ip_hdr + 16, 4);  /* dest IP */
    pseudo[8]  = 0;
    pseudo[9]  = 6;                       /* protocol = TCP */
    pseudo[10] = (uint8_t)(tcp_len >> 8);
    pseudo[11] = (uint8_t)(tcp_len & 0xFF);

    /* Sum pseudo-header */
    for (i = 0; i < 12; i += 2) {
        sum += (uint16_t)((uint16_t)pseudo[i] << 8 | pseudo[i + 1]);
    }

    /* Sum TCP header + payload */
    for (i = 0; i + 1 < tcp_len; i += 2) {
        sum += (uint16_t)((uint16_t)tcp[i] << 8 | tcp[i + 1]);
    }
    if (tcp_len & 1) {
        sum += (uint16_t)((uint16_t)tcp[tcp_len - 1] << 8);
    }

    /* Fold and complement */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum & 0xFFFF);
}

/*---------------------------------------------------------------------------*/
/* RX RING BUFFER HELPERS                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Bytes available to read from the RX ring buffer.
 */
static uint16_t
rx_available(const tiku_kits_net_tcp_conn_t *c)
{
    if (c->rx_head >= c->rx_tail) {
        return c->rx_head - c->rx_tail;
    }
    return TIKU_KITS_NET_TCP_RX_BUF_SIZE - c->rx_tail + c->rx_head;
}

/**
 * @brief Free space in the RX ring buffer.
 */
static uint16_t
rx_free(const tiku_kits_net_tcp_conn_t *c)
{
    return (TIKU_KITS_NET_TCP_RX_BUF_SIZE - 1) - rx_available(c);
}

/**
 * @brief Write data into the RX ring buffer (FRAM).
 *
 * Wraps around at RX_BUF_SIZE.  Caller must ensure len <= rx_free().
 * MPU is unlocked/locked around the FRAM writes.
 */
static void
rx_write(tiku_kits_net_tcp_conn_t *c, const uint8_t *data,
         uint16_t len)
{
    uint16_t first, saved;
    uint16_t head = c->rx_head;

    if (len == 0) {
        return;
    }

    saved = tiku_mpu_unlock_nvm();

    first = TIKU_KITS_NET_TCP_RX_BUF_SIZE - head;
    if (first > len) {
        first = len;
    }
    memcpy(c->rx_buf + head, data, first);
    if (len > first) {
        memcpy(c->rx_buf, data + first, len - first);
    }

    tiku_mpu_lock_nvm(saved);

    c->rx_head = (head + len) % TIKU_KITS_NET_TCP_RX_BUF_SIZE;
}

/*---------------------------------------------------------------------------*/
/* CONNECTION TABLE HELPERS                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Allocate a free connection slot.
 *
 * @param idx  Output: index into conn_table (for RX buffer assignment)
 * @return Pointer to the allocated slot, or NULL if full
 */
static tiku_kits_net_tcp_conn_t *
tcp_alloc_conn(uint8_t *idx)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_TCP_MAX_CONNS; i++) {
        if (!conn_table[i].active) {
            memset(&conn_table[i], 0, sizeof(tiku_kits_net_tcp_conn_t));
            conn_table[i].active = 1;
            conn_table[i].rx_buf = tcp_rx_bufs[i];
            if (idx) {
                *idx = i;
            }
            return &conn_table[i];
        }
    }
    return (tiku_kits_net_tcp_conn_t *)0;
}

/**
 * @brief Free a connection slot and all its TX segments.
 */
static void
tcp_free_conn(tiku_kits_net_tcp_conn_t *c)
{
    tcp_txseg_t *seg;
    tcp_txseg_t *next;

    /* Free all TX segments */
    seg = (tcp_txseg_t *)c->tx_head;
    while (seg) {
        next = seg->next;
        seg_free(seg);
        seg = next;
    }

    c->active = 0;
    c->state = TIKU_KITS_NET_TCP_STATE_CLOSED;
    c->tx_head = (void *)0;
    c->tx_tail = (void *)0;
    c->tx_count = 0;
}

/**
 * @brief Find a connection matching a 4-tuple.
 */
static tiku_kits_net_tcp_conn_t *
tcp_find_conn(const uint8_t *remote_ip, uint16_t remote_port,
              uint16_t local_port)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_TCP_MAX_CONNS; i++) {
        tiku_kits_net_tcp_conn_t *c = &conn_table[i];
        if (!c->active) {
            continue;
        }
        if (c->local_port == local_port &&
            c->remote_port == remote_port &&
            c->remote_addr.b[0] == remote_ip[0] &&
            c->remote_addr.b[1] == remote_ip[1] &&
            c->remote_addr.b[2] == remote_ip[2] &&
            c->remote_addr.b[3] == remote_ip[3]) {
            return c;
        }
    }
    return (tiku_kits_net_tcp_conn_t *)0;
}

/*---------------------------------------------------------------------------*/
/* LISTENER TABLE HELPERS                                                    */
/*---------------------------------------------------------------------------*/

static tcp_listener_t *
tcp_find_listener(uint16_t port)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_TCP_MAX_LISTENERS; i++) {
        if (listeners[i].port == port) {
            return &listeners[i];
        }
    }
    return (tcp_listener_t *)0;
}

/*---------------------------------------------------------------------------*/
/* ISS GENERATOR                                                             */
/*---------------------------------------------------------------------------*/

static uint32_t
tcp_gen_iss(void)
{
    iss_counter += 64000U;
    return iss_counter;
}

/*---------------------------------------------------------------------------*/
/* MSS OPTION PARSING                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parse MSS from TCP options in a SYN segment.
 *
 * Scans the options region (bytes 20..hdr_len-1) for the MSS
 * option (kind=2, len=4).  Returns the MSS value if found,
 * or TIKU_KITS_NET_TCP_DEFAULT_MSS (536) if not.
 */
static uint16_t
tcp_parse_mss(const uint8_t *tcp, uint16_t hdr_len)
{
    uint16_t off = TIKU_KITS_NET_TCP_HDR_LEN;

    while (off < hdr_len) {
        uint8_t kind = tcp[off];

        if (kind == TIKU_KITS_NET_TCP_OPT_END) {
            break;
        }
        if (kind == TIKU_KITS_NET_TCP_OPT_NOP) {
            off++;
            continue;
        }
        if (off + 1 >= hdr_len) {
            break;
        }

        uint8_t opt_len = tcp[off + 1];
        if (opt_len < 2 || off + opt_len > hdr_len) {
            break;
        }

        if (kind == TIKU_KITS_NET_TCP_OPT_MSS_KIND &&
            opt_len == TIKU_KITS_NET_TCP_OPT_MSS_LEN &&
            off + 4 <= hdr_len) {
            return (uint16_t)((uint16_t)tcp[off + 2] << 8 |
                               tcp[off + 3]);
        }

        off += opt_len;
    }

    return TIKU_KITS_NET_TCP_DEFAULT_MSS;
}

/*---------------------------------------------------------------------------*/
/* SEGMENT CONSTRUCTION AND OUTPUT                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build and send a TCP segment.
 *
 * Constructs a complete IPv4 + TCP packet in the shared net_buf.
 * For SYN segments, includes the MSS option (4 bytes, making the
 * TCP header 24 bytes with data offset = 6).
 *
 * The sequence number used is conn->snd_nxt at call time; the
 * caller is responsible for advancing snd_nxt after calling.
 *
 * @param c          Connection
 * @param flags      TCP flags to set
 * @param seq        Sequence number to use
 * @param data       Payload (may be NULL if data_len == 0)
 * @param data_len   Payload length
 * @return TIKU_KITS_NET_OK on success, negative on error
 */
static int8_t
tcp_send_segment(tiku_kits_net_tcp_conn_t *c, uint8_t flags,
                 uint32_t seq, const uint8_t *data, uint16_t data_len)
{
    uint8_t *buf;
    uint8_t *tcp;
    const uint8_t *our_ip;
    uint16_t tcp_hdr_len;
    uint16_t tcp_total;
    uint16_t total_len;
    uint16_t chk;

    buf = tiku_kits_net_ipv4_get_buf((void *)0);
    if (buf == (void *)0) {
        return TIKU_KITS_NET_ERR_NOLINK;
    }
    our_ip = tiku_kits_net_ipv4_get_addr();

    /* TCP header length: 24 for SYN (with MSS), 20 otherwise */
    tcp_hdr_len = (flags & TIKU_KITS_NET_TCP_FLAG_SYN)
                  ? TIKU_KITS_NET_TCP_SYN_HDR_LEN
                  : TIKU_KITS_NET_TCP_HDR_LEN;
    tcp_total = tcp_hdr_len + data_len;
    total_len = 20 + tcp_total;  /* IPv4 header (20) + TCP */

    if (total_len > TIKU_KITS_NET_MTU) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    /* --- IPv4 header (20 bytes) --- */
    buf[0]  = 0x45;                            /* ver=4, IHL=5 */
    buf[1]  = 0x00;                            /* TOS */
    buf[2]  = (uint8_t)(total_len >> 8);
    buf[3]  = (uint8_t)(total_len & 0xFF);
    buf[4]  = 0x00; buf[5] = 0x00;             /* identification */
    buf[6]  = 0x00; buf[7] = 0x00;             /* flags + frag */
    buf[8]  = TIKU_KITS_NET_TTL;
    buf[9]  = 6;                               /* protocol = TCP */
    buf[10] = 0x00; buf[11] = 0x00;            /* checksum placeholder */
    memcpy(buf + 12, our_ip, 4);               /* source IP */
    memcpy(buf + 16, c->remote_addr.b, 4);     /* dest IP */

    /* --- TCP header --- */
    tcp = buf + 20;

    /* Source port */
    tcp[0] = (uint8_t)(c->local_port >> 8);
    tcp[1] = (uint8_t)(c->local_port & 0xFF);
    /* Destination port */
    tcp[2] = (uint8_t)(c->remote_port >> 8);
    tcp[3] = (uint8_t)(c->remote_port & 0xFF);
    /* Sequence number */
    tcp[4] = (uint8_t)(seq >> 24);
    tcp[5] = (uint8_t)(seq >> 16);
    tcp[6] = (uint8_t)(seq >> 8);
    tcp[7] = (uint8_t)(seq);
    /* ACK number */
    tcp[8]  = (uint8_t)(c->rcv_nxt >> 24);
    tcp[9]  = (uint8_t)(c->rcv_nxt >> 16);
    tcp[10] = (uint8_t)(c->rcv_nxt >> 8);
    tcp[11] = (uint8_t)(c->rcv_nxt);
    /* Data offset (upper 4 bits) + reserved */
    tcp[12] = (uint8_t)((tcp_hdr_len / 4) << 4);
    /* Flags */
    tcp[13] = flags;
    /* Window */
    tcp[14] = (uint8_t)(c->rcv_wnd >> 8);
    tcp[15] = (uint8_t)(c->rcv_wnd & 0xFF);
    /* Checksum placeholder */
    tcp[16] = 0; tcp[17] = 0;
    /* Urgent pointer */
    tcp[18] = 0; tcp[19] = 0;

    /* MSS option on SYN */
    if (flags & TIKU_KITS_NET_TCP_FLAG_SYN) {
        uint16_t mss = TIKU_KITS_NET_TCP_MSS;
        tcp[20] = TIKU_KITS_NET_TCP_OPT_MSS_KIND;
        tcp[21] = TIKU_KITS_NET_TCP_OPT_MSS_LEN;
        tcp[22] = (uint8_t)(mss >> 8);
        tcp[23] = (uint8_t)(mss & 0xFF);
    }

    /* Copy payload */
    if (data_len > 0 && data != (void *)0) {
        memcpy(tcp + tcp_hdr_len, data, data_len);
    }

    /* TCP checksum */
    chk = tcp_chksum(buf, tcp, tcp_total);
    tcp[16] = (uint8_t)(chk >> 8);
    tcp[17] = (uint8_t)(chk & 0xFF);

    return tiku_kits_net_ipv4_output(buf, total_len);
}

/*---------------------------------------------------------------------------*/
/* RST RESPONSE FOR UNMATCHED SEGMENTS                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send a RST in response to an unexpected segment.
 *
 * Extracts all needed fields from the incoming packet before
 * overwriting net_buf to construct the RST.
 */
static void
tcp_send_rst_reply(uint8_t *pkt, uint16_t pkt_len, uint16_t ihl_len)
{
    uint8_t *tcp_in = pkt + ihl_len;
    uint8_t  in_flags = TIKU_KITS_NET_TCP_FLAGS(tcp_in);
    uint32_t in_seq = TIKU_KITS_NET_TCP_SEQ(tcp_in);
    uint32_t in_ack = TIKU_KITS_NET_TCP_ACKNUM(tcp_in);
    uint16_t in_sport = TIKU_KITS_NET_TCP_SPORT(tcp_in);
    uint16_t in_dport = TIKU_KITS_NET_TCP_DPORT(tcp_in);
    uint16_t in_doff = (uint16_t)(TIKU_KITS_NET_TCP_DOFF(tcp_in) * 4);
    uint16_t in_tcp_total = pkt_len - ihl_len;
    uint16_t in_data_len;
    uint8_t  in_src[4], in_dst[4];
    uint8_t *buf, *tcp;
    uint16_t total_len;
    uint32_t rst_seq, rst_ack;
    uint8_t  rst_flags;
    uint16_t chk;
    uint32_t seg_len;

    /* Don't RST a RST */
    if (in_flags & TIKU_KITS_NET_TCP_FLAG_RST) {
        return;
    }

    /* Save addresses before overwriting net_buf */
    memcpy(in_src, pkt + 12, 4);
    memcpy(in_dst, pkt + 16, 4);
    in_data_len = (in_doff <= in_tcp_total)
                  ? (in_tcp_total - in_doff) : 0;

    /* Compute RST seq/ack per RFC 793 */
    if (in_flags & TIKU_KITS_NET_TCP_FLAG_ACK) {
        rst_seq = in_ack;
        rst_ack = 0;
        rst_flags = TIKU_KITS_NET_TCP_FLAG_RST;
    } else {
        rst_seq = 0;
        seg_len = (uint32_t)in_data_len;
        if (in_flags & TIKU_KITS_NET_TCP_FLAG_SYN) {
            seg_len++;
        }
        if (in_flags & TIKU_KITS_NET_TCP_FLAG_FIN) {
            seg_len++;
        }
        rst_ack = in_seq + seg_len;
        rst_flags = TIKU_KITS_NET_TCP_FLAG_RST |
                    TIKU_KITS_NET_TCP_FLAG_ACK;
    }

    /* Build RST packet */
    buf = tiku_kits_net_ipv4_get_buf((void *)0);
    total_len = 20 + TIKU_KITS_NET_TCP_HDR_LEN;

    /* IPv4 header */
    buf[0] = 0x45; buf[1] = 0x00;
    buf[2] = (uint8_t)(total_len >> 8);
    buf[3] = (uint8_t)(total_len & 0xFF);
    buf[4] = 0; buf[5] = 0;
    buf[6] = 0; buf[7] = 0;
    buf[8] = TIKU_KITS_NET_TTL;
    buf[9] = 6;
    buf[10] = 0; buf[11] = 0;
    memcpy(buf + 12, in_dst, 4);   /* our IP as source */
    memcpy(buf + 16, in_src, 4);   /* their IP as dest */

    /* TCP header */
    tcp = buf + 20;
    tcp[0] = (uint8_t)(in_dport >> 8);
    tcp[1] = (uint8_t)(in_dport & 0xFF);
    tcp[2] = (uint8_t)(in_sport >> 8);
    tcp[3] = (uint8_t)(in_sport & 0xFF);
    tcp[4] = (uint8_t)(rst_seq >> 24);
    tcp[5] = (uint8_t)(rst_seq >> 16);
    tcp[6] = (uint8_t)(rst_seq >> 8);
    tcp[7] = (uint8_t)(rst_seq);
    tcp[8]  = (uint8_t)(rst_ack >> 24);
    tcp[9]  = (uint8_t)(rst_ack >> 16);
    tcp[10] = (uint8_t)(rst_ack >> 8);
    tcp[11] = (uint8_t)(rst_ack);
    tcp[12] = 0x50;    /* data offset = 5 */
    tcp[13] = rst_flags;
    tcp[14] = 0; tcp[15] = 0;   /* window = 0 */
    tcp[16] = 0; tcp[17] = 0;   /* checksum placeholder */
    tcp[18] = 0; tcp[19] = 0;   /* urgent pointer */

    chk = tcp_chksum(buf, tcp, TIKU_KITS_NET_TCP_HDR_LEN);
    tcp[16] = (uint8_t)(chk >> 8);
    tcp[17] = (uint8_t)(chk & 0xFF);

    tiku_kits_net_ipv4_output(buf, total_len);
}

/*---------------------------------------------------------------------------*/
/* TX SEGMENT QUEUE MANAGEMENT                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Store a segment in the TX pool for retransmission.
 */
static tcp_txseg_t *
tcp_tx_enqueue(tiku_kits_net_tcp_conn_t *c, uint32_t seq,
               uint8_t flags, const uint8_t *data, uint16_t data_len)
{
    tcp_txseg_t *seg;
    uint16_t saved;

    seg = (tcp_txseg_t *)seg_alloc();
    if (seg == (void *)0) {
        return (tcp_txseg_t *)0;
    }

    /* Write metadata and payload to FRAM */
    saved = tiku_mpu_unlock_nvm();
    seg->next = (tcp_txseg_t *)0;
    seg->seq = seq;
    seg->data_len = data_len;
    seg->flags = flags;
    seg->_pad = 0;
    if (data_len > 0 && data != (void *)0) {
        memcpy((uint8_t *)(seg + 1), data, data_len);
    }
    tiku_mpu_lock_nvm(saved);

    /* Append to TX queue */
    if (c->tx_tail != (void *)0) {
        saved = tiku_mpu_unlock_nvm();
        ((tcp_txseg_t *)c->tx_tail)->next = seg;
        tiku_mpu_lock_nvm(saved);
    } else {
        c->tx_head = seg;
    }
    c->tx_tail = seg;
    c->tx_count++;

    return seg;
}

/**
 * @brief Free TX segments that have been ACK'd.
 *
 * Walks the TX queue from the head and frees all segments whose
 * entire sequence range falls below @p ack_num.
 */
static void
tcp_tx_ack(tiku_kits_net_tcp_conn_t *c, uint32_t ack_num)
{
    tcp_txseg_t *seg;
    tcp_txseg_t *next;
    uint32_t seg_end;

    seg = (tcp_txseg_t *)c->tx_head;
    while (seg) {
        /* Sequence space consumed by this segment */
        seg_end = seg->seq + (uint32_t)seg->data_len;
        if (seg->flags & TIKU_KITS_NET_TCP_FLAG_SYN) {
            seg_end++;
        }
        if (seg->flags & TIKU_KITS_NET_TCP_FLAG_FIN) {
            seg_end++;
        }

        if (TIKU_KITS_NET_TCP_SEQ_LEQ(seg_end, ack_num)) {
            next = seg->next;
            seg_free(seg);
            c->tx_count--;
            seg = next;
        } else {
            break;  /* Remaining segments not yet ACK'd */
        }
    }
    c->tx_head = seg;
    if (seg == (void *)0) {
        c->tx_tail = (void *)0;
    }
}

/**
 * @brief Retransmit the head TX segment.
 */
static void
tcp_retransmit_head(tiku_kits_net_tcp_conn_t *c)
{
    tcp_txseg_t *seg = (tcp_txseg_t *)c->tx_head;

    if (seg == (void *)0) {
        return;
    }

    tcp_send_segment(c,
                     TIKU_KITS_NET_TCP_FLAG_ACK | seg->flags,
                     seg->seq,
                     (uint8_t *)(seg + 1),
                     seg->data_len);
}

/*---------------------------------------------------------------------------*/
/* STATE MACHINE: PROCESS INCOMING SEGMENT                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an incoming segment for an existing connection.
 *
 * Follows RFC 793 Section 3.9 processing order:
 *   1. Sequence number check
 *   2. RST check
 *   3. SYN check (error in established states)
 *   4. ACK check and processing
 *   5. Data processing
 *   6. FIN processing
 */
static void
tcp_process(tiku_kits_net_tcp_conn_t *c,
            uint32_t seg_seq, uint32_t seg_ack,
            uint8_t flags, uint16_t seg_wnd,
            const uint8_t *data, uint16_t data_len)
{
    /* ---------------------------------------------------------------
     * 1. RST processing (checked first for all states)
     * --------------------------------------------------------------- */
    if (flags & TIKU_KITS_NET_TCP_FLAG_RST) {
        if (c->state == TIKU_KITS_NET_TCP_STATE_SYN_RCVD ||
            c->state == TIKU_KITS_NET_TCP_STATE_SYN_SENT) {
            /* Connection refused */
            if (c->event_cb) {
                c->event_cb(c, TIKU_KITS_NET_TCP_EVT_ABORTED);
            }
            tcp_free_conn(c);
            return;
        }
        if (c->state >= TIKU_KITS_NET_TCP_STATE_ESTABLISHED) {
            if (c->event_cb) {
                c->event_cb(c, TIKU_KITS_NET_TCP_EVT_ABORTED);
            }
            tcp_free_conn(c);
            return;
        }
        return;
    }

    /* ---------------------------------------------------------------
     * 2. SYN_SENT state -- waiting for SYN+ACK
     * --------------------------------------------------------------- */
    if (c->state == TIKU_KITS_NET_TCP_STATE_SYN_SENT) {
        if (!(flags & TIKU_KITS_NET_TCP_FLAG_SYN)) {
            return;  /* Expecting SYN, drop */
        }
        if (flags & TIKU_KITS_NET_TCP_FLAG_ACK) {
            /* SYN+ACK: validate ACK */
            if (!TIKU_KITS_NET_TCP_SEQ_GT(seg_ack, c->snd_una) ||
                TIKU_KITS_NET_TCP_SEQ_GT(seg_ack, c->snd_nxt)) {
                return;  /* Bad ACK */
            }
            /* Complete handshake */
            c->rcv_nxt = seg_seq + 1;
            c->snd_una = seg_ack;
            c->snd_wnd = seg_wnd;

            /* ACK the SYN segment in TX queue */
            tcp_tx_ack(c, seg_ack);

            c->state = TIKU_KITS_NET_TCP_STATE_ESTABLISHED;
            c->rto_ticks = TIKU_KITS_NET_TCP_RTO_INIT;
            c->rto_counter = 0;
            c->retries = 0;

            /* Send ACK */
            tcp_send_segment(c, TIKU_KITS_NET_TCP_FLAG_ACK,
                             c->snd_nxt, (void *)0, 0);

            if (c->event_cb) {
                c->event_cb(c, TIKU_KITS_NET_TCP_EVT_CONNECTED);
            }
        } else {
            /* Simultaneous open: SYN without ACK */
            c->rcv_nxt = seg_seq + 1;
            c->state = TIKU_KITS_NET_TCP_STATE_SYN_RCVD;
            tcp_send_segment(c,
                             TIKU_KITS_NET_TCP_FLAG_SYN |
                             TIKU_KITS_NET_TCP_FLAG_ACK,
                             c->snd_una,  /* resend our ISS */
                             (void *)0, 0);
        }
        return;
    }

    /* ---------------------------------------------------------------
     * 3. Sequence number check (for all states except SYN_SENT)
     *
     * Simplified: we only accept in-order segments where
     * seg_seq == rcv_nxt.  Out-of-order segments are dropped
     * and a duplicate ACK is sent.
     * --------------------------------------------------------------- */
    if (c->state >= TIKU_KITS_NET_TCP_STATE_SYN_RCVD) {
        /* For ACK-only segments (no data, no SYN/FIN), the seq
         * should be rcv_nxt.  For data segments, seg_seq must
         * equal rcv_nxt (in-order only). */
        if (data_len > 0 && seg_seq != c->rcv_nxt) {
            /* Out of order -- send dup ACK */
            tcp_send_segment(c, TIKU_KITS_NET_TCP_FLAG_ACK,
                             c->snd_nxt, (void *)0, 0);
            return;
        }
    }

    /* ---------------------------------------------------------------
     * 4. SYN check (error in established states)
     * --------------------------------------------------------------- */
    if ((flags & TIKU_KITS_NET_TCP_FLAG_SYN) &&
        c->state >= TIKU_KITS_NET_TCP_STATE_ESTABLISHED) {
        /* SYN in established connection is an error -- RST */
        if (c->event_cb) {
            c->event_cb(c, TIKU_KITS_NET_TCP_EVT_ABORTED);
        }
        tcp_free_conn(c);
        return;
    }

    /* ---------------------------------------------------------------
     * 5. ACK processing
     * --------------------------------------------------------------- */
    if (flags & TIKU_KITS_NET_TCP_FLAG_ACK) {
        switch (c->state) {
        case TIKU_KITS_NET_TCP_STATE_SYN_RCVD:
            /* ACK of our SYN+ACK -> ESTABLISHED */
            if (TIKU_KITS_NET_TCP_SEQ_LEQ(c->snd_una, seg_ack) &&
                TIKU_KITS_NET_TCP_SEQ_LEQ(seg_ack, c->snd_nxt)) {
                c->snd_una = seg_ack;
                tcp_tx_ack(c, seg_ack);
                c->state = TIKU_KITS_NET_TCP_STATE_ESTABLISHED;
                c->snd_wnd = seg_wnd;
                c->rto_ticks = TIKU_KITS_NET_TCP_RTO_INIT;
                c->rto_counter = 0;
                c->retries = 0;
                if (c->event_cb) {
                    c->event_cb(c,
                                TIKU_KITS_NET_TCP_EVT_CONNECTED);
                }
            }
            break;

        case TIKU_KITS_NET_TCP_STATE_ESTABLISHED:
        case TIKU_KITS_NET_TCP_STATE_CLOSE_WAIT:
            /* Normal ACK processing */
            if (TIKU_KITS_NET_TCP_SEQ_GT(seg_ack, c->snd_una) &&
                TIKU_KITS_NET_TCP_SEQ_LEQ(seg_ack, c->snd_nxt)) {
                c->snd_una = seg_ack;
                tcp_tx_ack(c, seg_ack);
                c->snd_wnd = seg_wnd;
                /* Reset retransmit timer on new ACK */
                c->rto_ticks = TIKU_KITS_NET_TCP_RTO_INIT;
                c->rto_counter = 0;
                c->retries = 0;
            }
            break;

        case TIKU_KITS_NET_TCP_STATE_FIN_WAIT_1:
            /* ACK processing + check if our FIN is ACK'd */
            if (TIKU_KITS_NET_TCP_SEQ_GT(seg_ack, c->snd_una) &&
                TIKU_KITS_NET_TCP_SEQ_LEQ(seg_ack, c->snd_nxt)) {
                c->snd_una = seg_ack;
                tcp_tx_ack(c, seg_ack);
                c->snd_wnd = seg_wnd;
                c->rto_counter = 0;
                c->retries = 0;
            }
            /* If FIN is ACK'd (snd_una == snd_nxt) */
            if (c->snd_una == c->snd_nxt) {
                c->state = TIKU_KITS_NET_TCP_STATE_FIN_WAIT_2;
            }
            break;

        case TIKU_KITS_NET_TCP_STATE_CLOSING:
            if (TIKU_KITS_NET_TCP_SEQ_GT(seg_ack, c->snd_una) &&
                TIKU_KITS_NET_TCP_SEQ_LEQ(seg_ack, c->snd_nxt)) {
                c->snd_una = seg_ack;
                tcp_tx_ack(c, seg_ack);
            }
            if (c->snd_una == c->snd_nxt) {
                c->state = TIKU_KITS_NET_TCP_STATE_TIME_WAIT;
                c->tw_counter = TIKU_KITS_NET_TCP_TIME_WAIT_TICKS;
            }
            break;

        case TIKU_KITS_NET_TCP_STATE_LAST_ACK:
            /* ACK of our FIN -> CLOSED */
            if (TIKU_KITS_NET_TCP_SEQ_LEQ(seg_ack, c->snd_nxt)) {
                if (c->event_cb) {
                    c->event_cb(c, TIKU_KITS_NET_TCP_EVT_CLOSED);
                }
                tcp_free_conn(c);
                return;
            }
            break;

        case TIKU_KITS_NET_TCP_STATE_TIME_WAIT:
            /* ACK in TIME_WAIT: restart the timer */
            c->tw_counter = TIKU_KITS_NET_TCP_TIME_WAIT_TICKS;
            break;

        default:
            break;
        }
    }

    /* ---------------------------------------------------------------
     * 6. Data processing (ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2)
     * --------------------------------------------------------------- */
    if (data_len > 0 &&
        (c->state == TIKU_KITS_NET_TCP_STATE_ESTABLISHED ||
         c->state == TIKU_KITS_NET_TCP_STATE_FIN_WAIT_1 ||
         c->state == TIKU_KITS_NET_TCP_STATE_FIN_WAIT_2)) {

        uint16_t space = rx_free(c);
        uint16_t accept = (data_len <= space) ? data_len : space;

        if (accept > 0) {
            rx_write(c, data, accept);
            c->rcv_nxt += accept;
            c->rcv_wnd = rx_free(c);
        }

        /* Send ACK for received data */
        tcp_send_segment(c, TIKU_KITS_NET_TCP_FLAG_ACK,
                         c->snd_nxt, (void *)0, 0);

        if (accept > 0 && c->recv_cb) {
            c->recv_cb(c, rx_available(c));
        }
    }

    /* ---------------------------------------------------------------
     * 7. FIN processing
     * --------------------------------------------------------------- */
    if (flags & TIKU_KITS_NET_TCP_FLAG_FIN) {
        /* FIN consumes one sequence number */
        c->rcv_nxt++;
        c->rcv_wnd = rx_free(c);

        /* Send ACK for the FIN */
        tcp_send_segment(c, TIKU_KITS_NET_TCP_FLAG_ACK,
                         c->snd_nxt, (void *)0, 0);

        switch (c->state) {
        case TIKU_KITS_NET_TCP_STATE_SYN_RCVD:
        case TIKU_KITS_NET_TCP_STATE_ESTABLISHED:
            c->state = TIKU_KITS_NET_TCP_STATE_CLOSE_WAIT;
            /* Notify app that peer closed */
            if (c->event_cb) {
                c->event_cb(c, TIKU_KITS_NET_TCP_EVT_CLOSED);
            }
            break;

        case TIKU_KITS_NET_TCP_STATE_FIN_WAIT_1:
            /* Simultaneous close */
            if (c->snd_una == c->snd_nxt) {
                /* Our FIN was ACK'd too */
                c->state = TIKU_KITS_NET_TCP_STATE_TIME_WAIT;
                c->tw_counter =
                    TIKU_KITS_NET_TCP_TIME_WAIT_TICKS;
            } else {
                c->state = TIKU_KITS_NET_TCP_STATE_CLOSING;
            }
            break;

        case TIKU_KITS_NET_TCP_STATE_FIN_WAIT_2:
            c->state = TIKU_KITS_NET_TCP_STATE_TIME_WAIT;
            c->tw_counter = TIKU_KITS_NET_TCP_TIME_WAIT_TICKS;
            break;

        case TIKU_KITS_NET_TCP_STATE_TIME_WAIT:
            /* Retransmitted FIN: restart timer, re-ACK */
            c->tw_counter = TIKU_KITS_NET_TCP_TIME_WAIT_TICKS;
            break;

        default:
            break;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* BUILT-IN ECHO SERVICE (PORT 7)                                            */
/*---------------------------------------------------------------------------*/

#if TIKU_KITS_NET_TCP_ECHO_ENABLE
/**
 * @brief Echo service receive callback.
 *
 * Reads all available data from the RX ring buffer and sends it
 * back on the same connection.  Same concept as the UDP echo
 * service on port 7 (RFC 862).
 */
static void
tcp_echo_recv(struct tiku_kits_net_tcp_conn *conn, uint16_t available)
{
    uint8_t buf[TIKU_KITS_NET_TCP_MSS];
    (void)available;
    uint16_t n;

    n = tiku_kits_net_tcp_read(conn, buf, sizeof(buf));
    if (n > 0) {
        tiku_kits_net_tcp_send(conn, buf, n);
    }
}

/**
 * @brief Echo service event callback (no-op).
 */
static void
tcp_echo_event(struct tiku_kits_net_tcp_conn *conn, uint8_t event)
{
    (void)conn;
    (void)event;
}
#endif /* TIKU_KITS_NET_TCP_ECHO_ENABLE */

/*---------------------------------------------------------------------------*/
/* PUBLIC API: INIT                                                          */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_tcp_init(void)
{
    uint8_t i;
    uint16_t saved;

    /* Clear connection table */
    memset(conn_table, 0, sizeof(conn_table));

    /* Clear listener table */
    memset(listeners, 0, sizeof(listeners));

    /* Reset ISS counter */
    iss_counter = 0x10000;

    /* Create TX segment pool over FRAM backing array.
     * MPU must be unlocked for pool creation (writes freelist
     * pointers into FRAM blocks). */
    saved = tiku_mpu_unlock_nvm();
    tiku_pool_create(&seg_pool,
                     tcp_tx_pool_buf,
                     TIKU_KITS_NET_TCP_TX_SEG_BLOCK,
                     TIKU_KITS_NET_TCP_TX_POOL_COUNT,
                     0x0C);
    tiku_mpu_lock_nvm(saved);

    /* Clear RX ring buffers in FRAM */
    saved = tiku_mpu_unlock_nvm();
    for (i = 0; i < TIKU_KITS_NET_TCP_MAX_CONNS; i++) {
        memset(tcp_rx_bufs[i], 0, TIKU_KITS_NET_TCP_RX_BUF_SIZE);
    }
    tiku_mpu_lock_nvm(saved);

#if TIKU_KITS_NET_TCP_ECHO_ENABLE
    tiku_kits_net_tcp_listen(TIKU_KITS_NET_TCP_ECHO_PORT,
                              tcp_echo_recv, tcp_echo_event);
#endif
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API: LISTEN / UNLISTEN                                             */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_tcp_listen(uint16_t port,
                          tiku_kits_net_tcp_recv_cb_t recv_cb,
                          tiku_kits_net_tcp_event_cb_t event_cb)
{
    uint8_t i;
    int8_t free_slot = -1;

    if (port == 0 || recv_cb == (void *)0 || event_cb == (void *)0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    for (i = 0; i < TIKU_KITS_NET_TCP_MAX_LISTENERS; i++) {
        if (listeners[i].port == port) {
            return TIKU_KITS_NET_ERR_PARAM;  /* Already listening */
        }
        if (listeners[i].port == 0 && free_slot < 0) {
            free_slot = (int8_t)i;
        }
    }

    if (free_slot < 0) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    listeners[free_slot].port = port;
    listeners[free_slot].recv_cb = recv_cb;
    listeners[free_slot].event_cb = event_cb;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_tcp_unlisten(uint16_t port)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_TCP_MAX_LISTENERS; i++) {
        if (listeners[i].port == port) {
            listeners[i].port = 0;
            listeners[i].recv_cb = (void *)0;
            listeners[i].event_cb = (void *)0;
            return TIKU_KITS_NET_OK;
        }
    }
    return TIKU_KITS_NET_ERR_PARAM;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API: CONNECT                                                       */
/*---------------------------------------------------------------------------*/

tiku_kits_net_tcp_conn_t *
tiku_kits_net_tcp_connect(const uint8_t *dst_addr,
                           uint16_t dst_port,
                           uint16_t src_port,
                           tiku_kits_net_tcp_recv_cb_t recv_cb,
                           tiku_kits_net_tcp_event_cb_t event_cb)
{
    tiku_kits_net_tcp_conn_t *c;
    uint8_t idx;
    uint32_t iss;
    if (dst_addr == (void *)0 || recv_cb == (void *)0 ||
        event_cb == (void *)0) {
        return (tiku_kits_net_tcp_conn_t *)0;
    }

    c = tcp_alloc_conn(&idx);
    if (c == (void *)0) {
        return (tiku_kits_net_tcp_conn_t *)0;
    }

    /* Set up connection */
    memcpy(c->remote_addr.b, dst_addr, 4);
    c->local_port = src_port;
    c->remote_port = dst_port;
    c->recv_cb = recv_cb;
    c->event_cb = event_cb;

    /* ISS and send variables */
    iss = tcp_gen_iss();
    c->snd_una = iss;
    c->snd_nxt = iss + 1;  /* SYN consumes 1 seq */
    c->snd_wnd = 0;
    c->snd_mss = TIKU_KITS_NET_TCP_MSS;
    c->peer_mss = TIKU_KITS_NET_TCP_DEFAULT_MSS;

    /* Receive variables */
    c->rcv_nxt = 0;
    c->rcv_wnd = TIKU_KITS_NET_TCP_RX_BUF_SIZE - 1;

    /* Retransmission */
    c->rto_ticks = TIKU_KITS_NET_TCP_RTO_INIT;
    c->rto_counter = 0;
    c->retries = 0;

    c->state = TIKU_KITS_NET_TCP_STATE_SYN_SENT;

    /* Store SYN in TX queue for retransmission */
    tcp_tx_enqueue(c, iss,
                             TIKU_KITS_NET_TCP_FLAG_SYN,
                             (void *)0, 0);

    /* Send SYN */
    if (tcp_send_segment(c,
                         TIKU_KITS_NET_TCP_FLAG_SYN,
                         iss,
                         (void *)0, 0) != TIKU_KITS_NET_OK) {
        tcp_free_conn(c);
        return (tiku_kits_net_tcp_conn_t *)0;
    }

    return c;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API: SEND                                                          */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_tcp_send(tiku_kits_net_tcp_conn_t *conn,
                        const uint8_t *data,
                        uint16_t data_len)
{
    tcp_txseg_t *seg;

    if (conn == (void *)0 || data == (void *)0 || data_len == 0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Must be in a data-transfer state */
    if (conn->state != TIKU_KITS_NET_TCP_STATE_ESTABLISHED &&
        conn->state != TIKU_KITS_NET_TCP_STATE_CLOSE_WAIT) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Check against negotiated MSS */
    if (data_len > conn->snd_mss) {
        return TIKU_KITS_NET_ERR_OVERFLOW;
    }

    /* Store in TX pool for retransmission */
    seg = tcp_tx_enqueue(conn, conn->snd_nxt,
                         0,  /* no special flags for data */
                         data, data_len);
    if (seg == (void *)0) {
        return TIKU_KITS_NET_ERR_OVERFLOW;  /* Pool exhausted */
    }

    /* Send the data segment */
    tcp_send_segment(conn,
                     TIKU_KITS_NET_TCP_FLAG_ACK |
                     TIKU_KITS_NET_TCP_FLAG_PSH,
                     conn->snd_nxt,
                     data, data_len);

    conn->snd_nxt += data_len;

    /* Reset retransmit timer */
    conn->rto_counter = 0;
    conn->retries = 0;

    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API: READ                                                          */
/*---------------------------------------------------------------------------*/

uint16_t
tiku_kits_net_tcp_read(tiku_kits_net_tcp_conn_t *conn,
                        uint8_t *buf,
                        uint16_t buf_len)
{
    uint16_t avail;
    uint16_t to_read;
    uint16_t first;
    uint16_t tail;
    uint16_t old_wnd;

    if (conn == (void *)0 || buf == (void *)0 || buf_len == 0) {
        return 0;
    }

    avail = rx_available(conn);
    if (avail == 0) {
        return 0;
    }

    to_read = (buf_len < avail) ? buf_len : avail;
    tail = conn->rx_tail;

    /* Copy from ring buffer (FRAM -> SRAM, no MPU needed for reads) */
    first = TIKU_KITS_NET_TCP_RX_BUF_SIZE - tail;
    if (first > to_read) {
        first = to_read;
    }
    memcpy(buf, conn->rx_buf + tail, first);
    if (to_read > first) {
        memcpy(buf + first, conn->rx_buf, to_read - first);
    }

    conn->rx_tail = (tail + to_read) % TIKU_KITS_NET_TCP_RX_BUF_SIZE;

    /* Update receive window */
    old_wnd = conn->rcv_wnd;
    conn->rcv_wnd = rx_free(conn);

    /* If window was zero and now opened, send a window update ACK
     * to unblock the peer */
    if (old_wnd == 0 && conn->rcv_wnd > 0 &&
        conn->state == TIKU_KITS_NET_TCP_STATE_ESTABLISHED) {
        tcp_send_segment(conn, TIKU_KITS_NET_TCP_FLAG_ACK,
                         conn->snd_nxt, (void *)0, 0);
    }

    return to_read;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API: CLOSE                                                         */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_tcp_close(tiku_kits_net_tcp_conn_t *conn)
{
    if (conn == (void *)0) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    switch (conn->state) {
    case TIKU_KITS_NET_TCP_STATE_ESTABLISHED:
        /* Send FIN+ACK, store for retransmission */
        tcp_tx_enqueue(conn, conn->snd_nxt,
                             TIKU_KITS_NET_TCP_FLAG_FIN,
                             (void *)0, 0);
        tcp_send_segment(conn,
                         TIKU_KITS_NET_TCP_FLAG_FIN |
                         TIKU_KITS_NET_TCP_FLAG_ACK,
                         conn->snd_nxt,
                         (void *)0, 0);
        conn->snd_nxt++;  /* FIN consumes 1 seq */
        conn->state = TIKU_KITS_NET_TCP_STATE_FIN_WAIT_1;
        conn->rto_counter = 0;
        conn->retries = 0;
        return TIKU_KITS_NET_OK;

    case TIKU_KITS_NET_TCP_STATE_CLOSE_WAIT:
        /* Peer already closed, we close our end */
        tcp_tx_enqueue(conn, conn->snd_nxt,
                             TIKU_KITS_NET_TCP_FLAG_FIN,
                             (void *)0, 0);
        tcp_send_segment(conn,
                         TIKU_KITS_NET_TCP_FLAG_FIN |
                         TIKU_KITS_NET_TCP_FLAG_ACK,
                         conn->snd_nxt,
                         (void *)0, 0);
        conn->snd_nxt++;
        conn->state = TIKU_KITS_NET_TCP_STATE_LAST_ACK;
        conn->rto_counter = 0;
        conn->retries = 0;
        return TIKU_KITS_NET_OK;

    case TIKU_KITS_NET_TCP_STATE_SYN_SENT:
    case TIKU_KITS_NET_TCP_STATE_SYN_RCVD:
        /* Abort during handshake */
        tcp_free_conn(conn);
        return TIKU_KITS_NET_OK;

    default:
        return TIKU_KITS_NET_ERR_PARAM;
    }
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API: ABORT                                                         */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_tcp_abort(tiku_kits_net_tcp_conn_t *conn)
{
    if (conn == (void *)0 || !conn->active) {
        return;
    }

    /* Send RST if connection is past SYN_SENT */
    if (conn->state >= TIKU_KITS_NET_TCP_STATE_SYN_RCVD) {
        tcp_send_segment(conn,
                         TIKU_KITS_NET_TCP_FLAG_RST,
                         conn->snd_nxt,
                         (void *)0, 0);
    }

    tcp_free_conn(conn);
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API: INPUT                                                         */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_tcp_input(uint8_t *buf, uint16_t len, uint16_t ihl_len)
{
    uint8_t *tcp;
    uint16_t tcp_total;
    uint16_t sport, dport;
    uint32_t seg_seq, seg_ack;
    uint8_t  flags;
    uint16_t seg_wnd;
    uint16_t hdr_len;
    uint16_t data_len;
    const uint8_t *data;
    tiku_kits_net_tcp_conn_t *c;
    tcp_listener_t *listener;
    uint16_t peer_mss;
    uint8_t idx;
    uint32_t iss;

    tcp = buf + ihl_len;
    tcp_total = len - ihl_len;

    /* --- Validate minimum TCP header --- */
    if (tcp_total < TIKU_KITS_NET_TCP_HDR_LEN) {
        return;
    }

    /* --- Parse header fields --- */
    sport   = TIKU_KITS_NET_TCP_SPORT(tcp);
    dport   = TIKU_KITS_NET_TCP_DPORT(tcp);
    seg_seq = TIKU_KITS_NET_TCP_SEQ(tcp);
    seg_ack = TIKU_KITS_NET_TCP_ACKNUM(tcp);
    flags   = TIKU_KITS_NET_TCP_FLAGS(tcp);
    seg_wnd = TIKU_KITS_NET_TCP_WINDOW(tcp);

    /* --- Validate data offset --- */
    hdr_len = (uint16_t)(TIKU_KITS_NET_TCP_DOFF(tcp) * 4);
    if (hdr_len < TIKU_KITS_NET_TCP_HDR_LEN || hdr_len > tcp_total) {
        return;
    }

    /* --- Verify TCP checksum --- */
    if (tcp_chksum(buf, tcp, tcp_total) != 0) {
        return;
    }

    /* --- Calculate payload --- */
    data_len = tcp_total - hdr_len;
    data = tcp + hdr_len;

    /* --- Find matching connection --- */
    c = tcp_find_conn(buf + 12, sport, dport);  /* buf+12 = src IP */

    if (c != (tiku_kits_net_tcp_conn_t *)0) {
        /* Parse MSS from SYN if applicable */
        if (flags & TIKU_KITS_NET_TCP_FLAG_SYN) {
            peer_mss = tcp_parse_mss(tcp, hdr_len);
            c->peer_mss = peer_mss;
            c->snd_mss = (peer_mss < TIKU_KITS_NET_TCP_MSS)
                         ? peer_mss : TIKU_KITS_NET_TCP_MSS;
        }

        /* Process through state machine */
        tcp_process(c, seg_seq, seg_ack, flags, seg_wnd,
                    data, data_len);
        return;
    }

    /* --- No matching connection --- */

    /* Don't respond to RST */
    if (flags & TIKU_KITS_NET_TCP_FLAG_RST) {
        return;
    }

    /* Check for SYN on a listened port */
    if (flags & TIKU_KITS_NET_TCP_FLAG_SYN) {
        listener = tcp_find_listener(dport);
        if (listener != (tcp_listener_t *)0) {
            /* Allocate a new connection */
            c = tcp_alloc_conn(&idx);
            if (c == (tiku_kits_net_tcp_conn_t *)0) {
                /* No free slots -- silently drop (don't RST
                 * to avoid being a SYN flood reflector) */
                return;
            }

            /* Set up connection from SYN */
            memcpy(c->remote_addr.b, buf + 12, 4);  /* src IP */
            c->local_port = dport;
            c->remote_port = sport;
            c->recv_cb = listener->recv_cb;
            c->event_cb = listener->event_cb;

            /* Parse peer MSS */
            peer_mss = tcp_parse_mss(tcp, hdr_len);
            c->peer_mss = peer_mss;
            c->snd_mss = (peer_mss < TIKU_KITS_NET_TCP_MSS)
                         ? peer_mss : TIKU_KITS_NET_TCP_MSS;

            /* Sequence numbers */
            iss = tcp_gen_iss();
            c->snd_una = iss;
            c->snd_nxt = iss + 1;  /* SYN+ACK consumes 1 seq */
            c->rcv_nxt = seg_seq + 1;
            c->rcv_wnd = TIKU_KITS_NET_TCP_RX_BUF_SIZE - 1;
            c->snd_wnd = seg_wnd;

            /* Retransmission */
            c->rto_ticks = TIKU_KITS_NET_TCP_RTO_INIT;
            c->rto_counter = 0;
            c->retries = 0;

            c->state = TIKU_KITS_NET_TCP_STATE_SYN_RCVD;

            /* Store SYN+ACK in TX queue for retransmission */
            tcp_tx_enqueue(c, iss,
                           TIKU_KITS_NET_TCP_FLAG_SYN |
                           TIKU_KITS_NET_TCP_FLAG_ACK,
                           (void *)0, 0);

            /* Send SYN+ACK */
            tcp_send_segment(c,
                             TIKU_KITS_NET_TCP_FLAG_SYN |
                             TIKU_KITS_NET_TCP_FLAG_ACK,
                             iss,
                             (void *)0, 0);
            return;
        }
    }

    /* No connection, no listener -- send RST */
    tcp_send_rst_reply(buf, len, ihl_len);
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API: PERIODIC                                                      */
/*---------------------------------------------------------------------------*/

void
tiku_kits_net_tcp_periodic(void)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_TCP_MAX_CONNS; i++) {
        tiku_kits_net_tcp_conn_t *c = &conn_table[i];

        if (!c->active ||
            c->state == TIKU_KITS_NET_TCP_STATE_CLOSED) {
            continue;
        }

        /* --- TIME_WAIT countdown --- */
        if (c->state == TIKU_KITS_NET_TCP_STATE_TIME_WAIT) {
            if (c->tw_counter > 0) {
                c->tw_counter--;
            }
            if (c->tw_counter == 0) {
                if (c->event_cb) {
                    c->event_cb(c,
                                TIKU_KITS_NET_TCP_EVT_CLOSED);
                }
                tcp_free_conn(c);
            }
            continue;
        }

        /* --- Retransmission timeout --- */
        if (c->tx_head != (void *)0) {
            c->rto_counter++;
            if (c->rto_counter >= c->rto_ticks) {
                c->retries++;
                if (c->retries > TIKU_KITS_NET_TCP_MAX_RETRIES) {
                    /* Connection failed -- abort */
                    if (c->event_cb) {
                        c->event_cb(c,
                            TIKU_KITS_NET_TCP_EVT_ABORTED);
                    }
                    tcp_free_conn(c);
                } else {
                    /* Retransmit and back off */
                    tcp_retransmit_head(c);
                    c->rto_counter = 0;
                    if (c->rto_ticks < 320) {
                        c->rto_ticks *= 2;
                    }
                }
            }
        }
    }
}

#endif /* TIKU_KITS_NET_TCP_ENABLE */
