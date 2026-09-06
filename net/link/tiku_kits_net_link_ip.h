/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_link_ip.h - a link over TCP: whole messages, length-framed.
 *
 * The board dials a desktop and the window session rides the connection.
 * TCP delivers bytes whole and in order, so a message needs only its length
 * in front: no checksum and no acknowledgement of the link's own.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_NET_LINK_IP_H_
#define TIKU_KITS_NET_LINK_IP_H_

#include <kernel/link/tiku_link.h>
#include <kernel/timers/tiku_timer.h>

#include "../ipv4/tiku_kits_net_tcp.h"

/** @brief The port a desktop listens on for a board's window session. */
#ifndef TIKU_KITS_NET_LINK_IP_PORT
#define TIKU_KITS_NET_LINK_IP_PORT 7749u
#endif

/** @brief Seconds between one dial and the next after a connection ends. */
#ifndef TIKU_KITS_NET_LINK_IP_REDIAL_SEC
#define TIKU_KITS_NET_LINK_IP_REDIAL_SEC 5u
#endif

/**
 * @brief The first port a link dials from; each dial takes the next.
 *
 * A port is not reused while the peer may still hold the last connection
 * on it, which a board that resets and dials again would otherwise do.
 */
#ifndef TIKU_KITS_NET_LINK_IP_LOCAL_PORT
#define TIKU_KITS_NET_LINK_IP_LOCAL_PORT 49300u
#endif

/**
 * @brief The message's length in front of it, 32-bit little-endian.
 *
 * The same order the session's own message heads use, so both ends read
 * every length the same way.
 */
#define TIKU_KITS_NET_LINK_IP_HEADER 4u

/**
 * @brief What waits for the transport when it will not take more now.
 *
 * The transport holds each segment until the peer acknowledges it, so a
 * window announcing itself fills it; the rest waits here.  This bounds one
 * message too, since one that will not fit is refused whole.
 */
#ifndef TIKU_KITS_NET_LINK_IP_OUTBOX
#define TIKU_KITS_NET_LINK_IP_OUTBOX 2048u
#endif

/** @brief How often the outbox is offered to the transport again. */
#ifndef TIKU_KITS_NET_LINK_IP_FLUSH_TICKS
#define TIKU_KITS_NET_LINK_IP_FLUSH_TICKS 2u
#endif

/** @brief Where a link is in its dialling. */
typedef enum {
    TIKU_KITS_NET_LINK_IP_DOWN = 0,  /**< no connection, none being made */
    TIKU_KITS_NET_LINK_IP_DIALLING,  /**< a SYN is out */
    TIKU_KITS_NET_LINK_IP_UP         /**< established; messages may cross */
} tiku_kits_net_link_ip_state_t;

/** @brief A link's counters since it opened. */
typedef struct {
    uint32_t rx;        /**< messages delivered */
    uint32_t tx;        /**< messages sent */
    uint32_t dials;     /**< connections attempted */
    uint32_t drops;     /**< connections lost or refused */
    uint32_t oversize;  /**< messages too large for the buffer, skipped */
    uint32_t refused;   /**< sends refused whole: down, too large, no room */
    uint32_t stalled;   /**< times the transport would take no more for now */
} tiku_kits_net_link_ip_stats_t;

/** @brief The link's state: the caller keeps it, statically. */
typedef struct {
    tiku_link_t       link;
    tiku_kits_net_tcp_conn_t *conn;
    struct tiku_timer redial;
    struct tiku_timer flush;      /**< armed while the outbox has bytes */
    uint8_t           addr[4];
    uint16_t          port;
    uint16_t          local_port;  /**< the port this dial went out from */
    uint8_t           state;      /**< tiku_kits_net_link_ip_state_t */
    uint8_t          *buf;        /**< the message being gathered */
    size_t            cap;
    size_t            len;        /**< bytes of it gathered */
    uint32_t          need;       /**< its length, once the head is in */
    size_t            skip;       /**< bytes of one too large still to pass */
    uint8_t           head[TIKU_KITS_NET_LINK_IP_HEADER];
    uint8_t           head_len;
    uint8_t           out[TIKU_KITS_NET_LINK_IP_OUTBOX];
    size_t            out_len;    /**< bytes the transport has not taken */
    tiku_kits_net_link_ip_stats_t stats;
} tiku_kits_net_link_ip_t;

/**
 * @brief Open @p l as a link to the desktop at @p addr and @p port, and
 *        dial it; messages arrive in @p buf of @p cap bytes.
 *
 * The link redials on its own after a connection ends, so a desktop that
 * comes back is met without the board being told again.
 *
 * @note @p cap is the largest message the board will TAKE whole.  What it
 *       may send is the outbox, which is larger: a board draws more than
 *       it is ever told.
 * @return the link, or NULL when the arguments are unusable
 */
tiku_link_t *tiku_kits_net_link_ip_open(tiku_kits_net_link_ip_t *l,
                                        const uint8_t addr[4], uint16_t port,
                                        uint8_t *buf, size_t cap);

/** @brief Where @p l is in its dialling. */
uint8_t tiku_kits_net_link_ip_state(const tiku_kits_net_link_ip_t *l);

/** @brief The counters of @p l. */
const tiku_kits_net_link_ip_stats_t *
tiku_kits_net_link_ip_stats(const tiku_kits_net_link_ip_t *l);

#endif /* TIKU_KITS_NET_LINK_IP_H_ */
