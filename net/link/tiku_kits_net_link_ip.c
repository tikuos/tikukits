/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_link_ip.c - the window session over a dialled TCP connection.
 *
 * A message goes out as its length and its bytes.  What the transport will
 * not take now waits in the outbox for the flush timer rather than being
 * abandoned mid-message, which a length-framed reader has no way back from.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_net_link_ip.h"

#include <string.h>

/*---------------------------------------------------------------------------*/
/* PRIVATE STATE                                                             */
/*---------------------------------------------------------------------------*/

/*
 * The open links, found from the TCP callbacks by the connection they name.
 * A board runs one window session, so one entry is the common case.
 */
#ifndef TIKU_KITS_NET_LINK_IP_MAX
#define TIKU_KITS_NET_LINK_IP_MAX 2
#endif

static tiku_kits_net_link_ip_t *links[TIKU_KITS_NET_LINK_IP_MAX];

/** @brief The link that dialled @p conn, or NULL. */
static tiku_kits_net_link_ip_t *
link_of(const tiku_kits_net_tcp_conn_t *conn)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_LINK_IP_MAX; i++) {
        if (links[i] != NULL && links[i]->conn == conn) {
            return links[i];
        }
    }
    return NULL;
}

/** @brief Remember @p l, or say the table is full. */
static int
remember(tiku_kits_net_link_ip_t *l)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_LINK_IP_MAX; i++) {
        if (links[i] == l) {
            return 0;
        }
    }
    for (i = 0; i < TIKU_KITS_NET_LINK_IP_MAX; i++) {
        if (links[i] == NULL) {
            links[i] = l;
            return 0;
        }
    }
    return -1;
}

/** @brief Forget @p l. */
static void
forget(const tiku_kits_net_link_ip_t *l)
{
    uint8_t i;

    for (i = 0; i < TIKU_KITS_NET_LINK_IP_MAX; i++) {
        if (links[i] == l) {
            links[i] = NULL;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* DIALLING                                                                  */
/*---------------------------------------------------------------------------*/

static void ip_recv_cb(tiku_kits_net_tcp_conn_t *conn, uint16_t available);
static void ip_event_cb(tiku_kits_net_tcp_conn_t *conn, uint8_t event);
static void dial(tiku_kits_net_link_ip_t *l);
static void flush_later(void *ptr);

/** @brief The redial timer: try again from the process that opened it. */
static void
redial(void *ptr)
{
    dial((tiku_kits_net_link_ip_t *)ptr);
}

/** @brief Wait a while, then dial again. */
static void
arm_redial(tiku_kits_net_link_ip_t *l)
{
    tiku_timer_set_callback(&l->redial,
                            (tiku_clock_time_t)TIKU_KITS_NET_LINK_IP_REDIAL_SEC
                                * TIKU_CLOCK_SECOND,
                            redial, l);
}

/**
 * @brief Forget what was half read and what waited to go: a new connection
 *        starts a new stream both ways.
 */
static void
rewind_stream(tiku_kits_net_link_ip_t *l)
{
    l->head_len = 0u;
    l->len = 0u;
    l->need = 0u;
    l->skip = 0u;
    l->out_len = 0u;
    tiku_timer_stop(&l->flush);
}

/** @brief Dial the desktop.  A refused dial is tried again later. */
static void
dial(tiku_kits_net_link_ip_t *l)
{
    if (l->state != TIKU_KITS_NET_LINK_IP_DOWN) {
        return;
    }
    rewind_stream(l);
    l->stats.dials++;
    l->local_port++;
    if (l->local_port < TIKU_KITS_NET_LINK_IP_LOCAL_PORT) {
        l->local_port = TIKU_KITS_NET_LINK_IP_LOCAL_PORT;
    }
    l->conn = tiku_kits_net_tcp_connect(l->addr, l->port, l->local_port,
                                        ip_recv_cb, ip_event_cb);
    if (l->conn == NULL) {
        l->stats.drops++;
        arm_redial(l);
        return;
    }
    l->state = TIKU_KITS_NET_LINK_IP_DIALLING;
}

/** @brief The connection is gone: forget it and arrange another. */
static void
went_down(tiku_kits_net_link_ip_t *l)
{
    l->conn = NULL;
    l->state = TIKU_KITS_NET_LINK_IP_DOWN;
    l->stats.drops++;
    rewind_stream(l);
    arm_redial(l);
}

/*---------------------------------------------------------------------------*/
/* IN                                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Take @p len bytes of the stream: a length, then that many bytes,
 *        then the next length.  A message larger than the buffer is passed
 *        over rather than delivered short, so the stream stays in step.
 */
static void
absorb(tiku_kits_net_link_ip_t *l, const uint8_t *p, size_t len)
{
    while (len > 0u) {
        size_t take;

        if (l->skip > 0u) {
            take = (l->skip < len) ? l->skip : len;
            l->skip -= take;
            p += take;
            len -= take;
            continue;
        }
        if (l->head_len < TIKU_KITS_NET_LINK_IP_HEADER) {
            take = (size_t)TIKU_KITS_NET_LINK_IP_HEADER - l->head_len;
            if (take > len) {
                take = len;
            }
            memcpy(l->head + l->head_len, p, take);
            l->head_len = (uint8_t)(l->head_len + take);
            p += take;
            len -= take;
            if (l->head_len < TIKU_KITS_NET_LINK_IP_HEADER) {
                return;
            }
            l->need = (uint32_t)l->head[0] | ((uint32_t)l->head[1] << 8) |
                      ((uint32_t)l->head[2] << 16) |
                      ((uint32_t)l->head[3] << 24);
            l->len = 0u;
            if (l->need > (uint32_t)l->cap) {
                l->stats.oversize++;
                l->skip = (size_t)l->need;
                l->head_len = 0u;
                l->need = 0u;
            } else if (l->need == 0u) {
                l->head_len = 0u;      /* an empty message is nothing to say */
            }
            continue;
        }
        take = (size_t)l->need - l->len;
        if (take > len) {
            take = len;
        }
        memcpy(l->buf + l->len, p, take);
        l->len += take;
        p += take;
        len -= take;
        if (l->len == (size_t)l->need) {
            l->stats.rx++;
            l->head_len = 0u;
            l->need = 0u;
            tiku_link_deliver(&l->link, l->buf, l->len);
            l->len = 0u;
        }
    }
}

/** @brief Read what the connection holds and take it into messages. */
static void
ip_recv_cb(tiku_kits_net_tcp_conn_t *conn, uint16_t available)
{
    tiku_kits_net_link_ip_t *l = link_of(conn);
    uint8_t chunk[64];
    uint16_t n;

    (void)available;
    if (l == NULL) {
        return;
    }
    while ((n = tiku_kits_net_tcp_read(conn, chunk, sizeof chunk)) > 0u) {
        absorb(l, chunk, (size_t)n);
    }
}

/** @brief The connection came up, or ended. */
static void
ip_event_cb(tiku_kits_net_tcp_conn_t *conn, uint8_t event)
{
    tiku_kits_net_link_ip_t *l = link_of(conn);

    if (l == NULL) {
        return;
    }
    if (event == TIKU_KITS_NET_TCP_EVT_CONNECTED) {
        rewind_stream(l);
        l->state = TIKU_KITS_NET_LINK_IP_UP;
    } else {
        went_down(l);
    }
}

/*---------------------------------------------------------------------------*/
/* OUT                                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Offer the outbox to the transport, a segment at a time.
 *
 * What it will not take stays where it is and is offered again on the
 * flush timer, so a message is never half written and then abandoned:
 * TCP keeps what went before it, and the rest follows in order.
 */
static void
flush_out(tiku_kits_net_link_ip_t *l)
{
    while (l->out_len > 0u && l->state == TIKU_KITS_NET_LINK_IP_UP) {
        uint16_t mss = l->conn->snd_mss;
        uint16_t take = (uint16_t)((l->out_len < (size_t)mss) ? l->out_len
                                                              : (size_t)mss);

        if (tiku_kits_net_tcp_send(l->conn, l->out, take) !=
            TIKU_KITS_NET_OK) {
            l->stats.stalled++;
            tiku_timer_set_callback(&l->flush,
                                    TIKU_KITS_NET_LINK_IP_FLUSH_TICKS,
                                    flush_later, l);
            return;
        }
        l->out_len -= take;
        memmove(l->out, l->out + take, l->out_len);
    }
    tiku_timer_stop(&l->flush);
}

/** @brief The flush timer: offer the outbox again. */
static void
flush_later(void *ptr)
{
    flush_out((tiku_kits_net_link_ip_t *)ptr);
}

/**
 * @brief One message into the outbox: its length, @p head, then @p body.
 *
 * Whole or not at all, since the peer reads lengths and half a message
 * would make every one after it nonsense.  What the transport takes now
 * goes now; the rest waits for the flush.
 */
static int
ip_send(tiku_link_t *link, const void *head, size_t hlen,
        const void *body, size_t blen)
{
    tiku_kits_net_link_ip_t *l = link->ctx;
    size_t total = hlen + blen;
    size_t whole = TIKU_KITS_NET_LINK_IP_HEADER + total;

    /* What bounds a send is the outbox, not the buffer messages arrive in:
     * a window's display list is larger than anything the desk sends it. */
    if (l->state != TIKU_KITS_NET_LINK_IP_UP || l->conn == NULL ||
        whole > sizeof l->out - l->out_len) {
        l->stats.refused++;
        return -1;
    }
    l->out[l->out_len] = (uint8_t)total;
    l->out[l->out_len + 1u] = (uint8_t)(total >> 8);
    l->out[l->out_len + 2u] = (uint8_t)(total >> 16);
    l->out[l->out_len + 3u] = (uint8_t)(total >> 24);
    l->out_len += TIKU_KITS_NET_LINK_IP_HEADER;
    if (hlen > 0u && head != NULL) {
        memcpy(l->out + l->out_len, head, hlen);
        l->out_len += hlen;
    }
    if (blen > 0u && body != NULL) {
        memcpy(l->out + l->out_len, body, blen);
        l->out_len += blen;
    }
    l->stats.tx++;
    flush_out(l);
    return 0;
}

/*---------------------------------------------------------------------------*/
/* THE LINK                                                                  */
/*---------------------------------------------------------------------------*/

/** @brief Hang up and forget the link; nothing is delivered after this. */
static void
ip_close(tiku_link_t *link)
{
    tiku_kits_net_link_ip_t *l = link->ctx;

    tiku_timer_stop(&l->redial);
    tiku_timer_stop(&l->flush);
    if (l->conn != NULL) {
        tiku_kits_net_tcp_abort(l->conn);
        l->conn = NULL;
    }
    l->state = TIKU_KITS_NET_LINK_IP_DOWN;
    forget(l);
}

static const tiku_link_ops_t ip_ops = {
    ip_send, NULL, ip_close
};

tiku_link_t *
tiku_kits_net_link_ip_open(tiku_kits_net_link_ip_t *l, const uint8_t addr[4],
                           uint16_t port, uint8_t *buf, size_t cap)
{
    if (l == NULL || addr == NULL || buf == NULL || cap == 0u || port == 0u) {
        return NULL;
    }
    /* The transport this link stands on, in case nothing else asked for
     * it: without its segment pool every send would be refused. */
    tiku_kits_net_tcp_init();
    memset(l, 0, sizeof *l);
    memcpy(l->addr, addr, sizeof l->addr);
    l->port = port;
    l->local_port = TIKU_KITS_NET_LINK_IP_LOCAL_PORT;
    l->buf = buf;
    l->cap = cap;
    l->link.ops = &ip_ops;
    l->link.ctx = l;
    /*
     * What an IP link may touch is the address it dialled, and nothing
     * here can tell a desktop on the desk from one across the internet.
     * It confers nothing until milestone 6 weighs the address.
     */
    l->link.cap = 0u;
    if (remember(l) != 0) {
        return NULL;
    }
    dial(l);
    return &l->link;
}

uint8_t
tiku_kits_net_link_ip_state(const tiku_kits_net_link_ip_t *l)
{
    return (l != NULL) ? l->state : (uint8_t)TIKU_KITS_NET_LINK_IP_DOWN;
}

const tiku_kits_net_link_ip_stats_t *
tiku_kits_net_link_ip_stats(const tiku_kits_net_link_ip_t *l)
{
    return (l != NULL) ? &l->stats : NULL;
}
