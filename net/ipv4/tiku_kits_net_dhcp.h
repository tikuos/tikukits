/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_dhcp.h - DHCP client (RFC 2131)
 *
 * Lightweight DHCP client that obtains an IPv4 address, subnet mask,
 * default gateway, and lease time from a DHCP server.  Designed for
 * ultra-low-power embedded targets with static allocation only.
 *
 * Requires TIKU_KITS_NET_DHCP_ENABLE=1 at compile time, which
 * automatically raises the MTU to 300 bytes so that DHCP packets
 * (236-byte fixed header + options) fit in the shared net_buf.
 *
 * The client follows the DORA flow (Discover-Offer-Request-Ack)
 * using a poll-based state machine.  DHCP packets are built
 * directly in net_buf (bypassing udp_send) because DHCP uses
 * source IP 0.0.0.0 and destination 255.255.255.255, and the
 * 236-byte fixed header exceeds the normal UDP max payload.
 *
 * DHCP responses are received via a standard UDP port 68 binding.
 * The broadcast flag (0x8000) is set in all outgoing messages so
 * the server responds to 255.255.255.255 (accepted by the IPv4
 * input pipeline).
 *
 * On successful BOUND, the client calls ipv4_set_addr() to update
 * the system's IP address to the one assigned by the server.
 *
 * RAM cost: ~42 bytes static state + MTU increase from 128 to 300
 * (+172 bytes).  Total additional RAM: ~214 bytes.
 *
 * Typical usage:
 * @code
 *   static const uint8_t hw[] = {0x54, 0x49, 0x4B, 0x55, 0x01, 0x00};
 *   tiku_kits_net_dhcp_init();
 *   tiku_kits_net_dhcp_start(hw);
 *   while (tiku_kits_net_dhcp_get_state() < TIKU_KITS_NET_DHCP_STATE_BOUND) {
 *       tiku_kits_net_dhcp_poll();
 *   }
 *   const tiku_kits_net_dhcp_lease_t *l = tiku_kits_net_dhcp_get_lease();
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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_NET_DHCP_H_
#define TIKU_KITS_NET_DHCP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"

/*---------------------------------------------------------------------------*/
/* DHCP MESSAGE TYPES (RFC 2131 Section 9.6)                                 */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_DHCP_MSG DHCP Message Types
 * @brief Values carried in DHCP option 53 (Message Type).
 * @{
 */
#define TIKU_KITS_NET_DHCP_MSG_DISCOVER  1  /**< Client broadcast to find servers */
#define TIKU_KITS_NET_DHCP_MSG_OFFER     2  /**< Server response with IP offer */
#define TIKU_KITS_NET_DHCP_MSG_REQUEST   3  /**< Client requests offered IP */
#define TIKU_KITS_NET_DHCP_MSG_DECLINE   4  /**< Client rejects offered IP */
#define TIKU_KITS_NET_DHCP_MSG_ACK       5  /**< Server confirms the lease */
#define TIKU_KITS_NET_DHCP_MSG_NAK       6  /**< Server rejects the request */
#define TIKU_KITS_NET_DHCP_MSG_RELEASE   7  /**< Client releases the lease */
/** @} */

/*---------------------------------------------------------------------------*/
/* DHCP HEADER BYTE OFFSETS (RFC 2131 Section 2)                             */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_DHCP_OFFSETS DHCP Header Byte Offsets
 * @brief Offsets within the UDP payload for DHCP fixed-header fields.
 *
 * The DHCP fixed header is 236 bytes.  After it comes a 4-byte
 * magic cookie (99.130.83.99 = 0x63825363), followed by variable-
 * length options in tag-length-value format.
 * @{
 */
#define TIKU_KITS_NET_DHCP_OFF_OP        0    /**< Op: 1=request, 2=reply */
#define TIKU_KITS_NET_DHCP_OFF_HTYPE     1    /**< Hardware type (1=Ethernet) */
#define TIKU_KITS_NET_DHCP_OFF_HLEN      2    /**< Hardware addr length (6) */
#define TIKU_KITS_NET_DHCP_OFF_HOPS      3    /**< Hops (0 for client) */
#define TIKU_KITS_NET_DHCP_OFF_XID       4    /**< Transaction ID (32b) */
#define TIKU_KITS_NET_DHCP_OFF_SECS      8    /**< Seconds since start (16b) */
#define TIKU_KITS_NET_DHCP_OFF_FLAGS    10    /**< Flags (16b, 0x8000=broadcast) */
#define TIKU_KITS_NET_DHCP_OFF_CIADDR   12    /**< Client IP (if known) (32b) */
#define TIKU_KITS_NET_DHCP_OFF_YIADDR   16    /**< "Your" IP (server fills) (32b) */
#define TIKU_KITS_NET_DHCP_OFF_SIADDR   20    /**< Server IP (32b) */
#define TIKU_KITS_NET_DHCP_OFF_GIADDR   24    /**< Gateway IP (32b) */
#define TIKU_KITS_NET_DHCP_OFF_CHADDR   28    /**< Client hardware addr (16B) */
#define TIKU_KITS_NET_DHCP_OFF_SNAME    44    /**< Server hostname (64B, zeroed) */
#define TIKU_KITS_NET_DHCP_OFF_FILE    108    /**< Boot filename (128B, zeroed) */
#define TIKU_KITS_NET_DHCP_OFF_MAGIC   236    /**< Magic cookie (4B) */
#define TIKU_KITS_NET_DHCP_OFF_OPTIONS 240    /**< Options start */
#define TIKU_KITS_NET_DHCP_HDR_LEN     236    /**< Fixed header (before cookie) */

/**
 * @brief Compact DHCP header offsets.
 *
 * The eZ-FET backchannel UART has a small host-to-target buffer
 * (~150 bytes) that cannot carry a full 240+ byte DHCP reply.
 * In "compact" format the 192 unused sname(64) + file(128) bytes
 * are omitted: the magic cookie sits at offset 44 (right after
 * chaddr) and options at offset 48.  Outbound packets (DISCOVER,
 * REQUEST) still use the standard format.
 */
#define TIKU_KITS_NET_DHCP_COMPACT_MAGIC    44  /**< Cookie in compact format */
#define TIKU_KITS_NET_DHCP_COMPACT_OPTIONS  48  /**< Options in compact format */
/** @} */

/*---------------------------------------------------------------------------*/
/* DHCP OPTION CODES (RFC 2132)                                              */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_DHCP_OPT DHCP Option Codes
 * @brief Selected option tags used by this client.
 * @{
 */
#define TIKU_KITS_NET_DHCP_OPT_PAD           0   /**< Padding (no length) */
#define TIKU_KITS_NET_DHCP_OPT_SUBNET        1   /**< Subnet Mask (4B) */
#define TIKU_KITS_NET_DHCP_OPT_ROUTER        3   /**< Router/Gateway (4B+) */
#define TIKU_KITS_NET_DHCP_OPT_DNS           6   /**< DNS Server (4B+) */
#define TIKU_KITS_NET_DHCP_OPT_REQUESTED_IP 50   /**< Requested IP (4B) */
#define TIKU_KITS_NET_DHCP_OPT_LEASE_TIME   51   /**< IP Lease Time (4B) */
#define TIKU_KITS_NET_DHCP_OPT_MSG_TYPE     53   /**< DHCP Message Type (1B) */
#define TIKU_KITS_NET_DHCP_OPT_SERVER_ID    54   /**< Server Identifier (4B) */
#define TIKU_KITS_NET_DHCP_OPT_PARAM_LIST   55   /**< Parameter Request List */
#define TIKU_KITS_NET_DHCP_OPT_END         255   /**< End of options */
/** @} */

/*---------------------------------------------------------------------------*/
/* DHCP PROTOCOL CONSTANTS                                                   */
/*---------------------------------------------------------------------------*/

/** BOOTP request (client to server). */
#define TIKU_KITS_NET_DHCP_OP_REQUEST    1

/** BOOTP reply (server to client). */
#define TIKU_KITS_NET_DHCP_OP_REPLY      2

/** DHCP client UDP port (RFC 2131). */
#define TIKU_KITS_NET_DHCP_CLIENT_PORT  68

/** DHCP server UDP port (RFC 2131). */
#define TIKU_KITS_NET_DHCP_SERVER_PORT  67

/** Magic cookie: 99.130.83.99 (0x63825363). */
#define TIKU_KITS_NET_DHCP_MAGIC_0    0x63
#define TIKU_KITS_NET_DHCP_MAGIC_1    0x82
#define TIKU_KITS_NET_DHCP_MAGIC_2    0x53
#define TIKU_KITS_NET_DHCP_MAGIC_3    0x63

/** Broadcast flag (bit 15 of the flags field). */
#define TIKU_KITS_NET_DHCP_FLAG_BROADCAST  0x8000

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of retries before timeout.
 *
 * Each retry takes DHCP_POLL_TICKS (1 second).  On SLIP links with
 * eZ-FET backchannel, the host test needs multiple serial reopens
 * during the DORA handshake, so 8 retries provides enough time.
 */
#ifndef TIKU_KITS_NET_DHCP_MAX_RETRIES
#define TIKU_KITS_NET_DHCP_MAX_RETRIES   8
#endif

/**
 * @brief Default client hardware address for SLIP links.
 *
 * SLIP has no MAC address, so we use a fabricated 6-byte identifier.
 * "TIKU" + 0x01 + 0x00.  Override at compile time if needed.
 */
#ifndef TIKU_KITS_NET_DHCP_DEFAULT_HWADDR
#define TIKU_KITS_NET_DHCP_DEFAULT_HWADDR \
    {0x54, 0x49, 0x4B, 0x55, 0x01, 0x00}
#endif

/*---------------------------------------------------------------------------*/
/* STATE AND EVENT TYPES                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief DHCP client state.
 *
 * Progresses linearly: IDLE -> DISCOVER_SENT -> REQUESTING ->
 * BOUND (or ERROR).  BOUND and ERROR are terminal states;
 * call init() to return to IDLE.
 */
typedef enum {
    TIKU_KITS_NET_DHCP_STATE_IDLE,           /**< No DHCP in progress */
    TIKU_KITS_NET_DHCP_STATE_DISCOVER_SENT,  /**< DISCOVER sent, awaiting OFFER */
    TIKU_KITS_NET_DHCP_STATE_REQUESTING,     /**< REQUEST sent, awaiting ACK */
    TIKU_KITS_NET_DHCP_STATE_BOUND,          /**< IP assigned, lease active */
    TIKU_KITS_NET_DHCP_STATE_ERROR           /**< DHCP failed */
} tiku_kits_net_dhcp_state_t;

/**
 * @brief Events returned by dhcp_poll().
 */
typedef enum {
    TIKU_KITS_NET_DHCP_EVT_NONE,     /**< No event pending */
    TIKU_KITS_NET_DHCP_EVT_OFFER,    /**< OFFER received */
    TIKU_KITS_NET_DHCP_EVT_ACK,      /**< ACK received, lease active */
    TIKU_KITS_NET_DHCP_EVT_NAK,      /**< NAK received */
    TIKU_KITS_NET_DHCP_EVT_TIMEOUT   /**< Retries exhausted */
} tiku_kits_net_dhcp_evt_t;

/*---------------------------------------------------------------------------*/
/* LEASE INFORMATION                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief DHCP lease information.
 *
 * Populated after a successful DHCP exchange (state == BOUND).
 * All IP fields are 4 bytes in network order.
 */
typedef struct tiku_kits_net_dhcp_lease {
    uint8_t  ip[4];       /**< Assigned IP address */
    uint8_t  mask[4];     /**< Subnet mask */
    uint8_t  gateway[4];  /**< Default gateway */
    uint8_t  server[4];   /**< DHCP server IP */
    uint32_t lease_sec;   /**< Lease duration (seconds) */
} tiku_kits_net_dhcp_lease_t;

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the DHCP client.
 *
 * Resets all state to IDLE, clears the lease, and unbinds the
 * client port.  Call once at startup.  Safe to call again to
 * forcibly reset after a failed exchange.
 */
void tiku_kits_net_dhcp_init(void);

/*---------------------------------------------------------------------------*/
/* DHCP EXCHANGE                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Start a DHCP exchange (send DISCOVER).
 *
 * Binds UDP port 68, sets our IP to 0.0.0.0, and broadcasts a
 * DHCPDISCOVER message.  After calling start(), poll dhcp_poll()
 * until get_state() returns BOUND or ERROR.
 *
 * Only one exchange can be active at a time.
 *
 * @param client_hw  6-byte client identifier (e.g. MAC address
 *                   or fabricated ID for SLIP links).  If NULL,
 *                   the default "TIKU\x01\x00" is used.
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if already active,
 *         TIKU_KITS_NET_ERR_NOLINK if no link backend set.
 */
int8_t tiku_kits_net_dhcp_start(const uint8_t *client_hw);

/**
 * @brief Poll for DHCP events and drive the state machine.
 *
 * Checks for pending events set by the UDP receive callback,
 * handles state transitions (OFFER -> send REQUEST, ACK -> BOUND),
 * and updates the system IP address on success.
 *
 * Must be called from application context.
 *
 * @return The event that was processed, or EVT_NONE.
 */
tiku_kits_net_dhcp_evt_t tiku_kits_net_dhcp_poll(void);

/*---------------------------------------------------------------------------*/
/* STATE AND LEASE QUERY                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current DHCP client state.
 *
 * @return Current state.
 */
tiku_kits_net_dhcp_state_t tiku_kits_net_dhcp_get_state(void);

/**
 * @brief Get the lease information.
 *
 * Only valid when get_state() returns BOUND.
 *
 * @return Pointer to the static lease struct, or NULL if not bound.
 */
const tiku_kits_net_dhcp_lease_t *tiku_kits_net_dhcp_get_lease(void);

/*---------------------------------------------------------------------------*/
/* RELEASE                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Release the DHCP lease and return to IDLE.
 *
 * Sends a DHCPRELEASE to the server, unbinds the client port,
 * and resets state.  The system IP address is NOT cleared --
 * the caller should set a new address or call dhcp_start() again.
 *
 * @return TIKU_KITS_NET_OK always.
 */
int8_t tiku_kits_net_dhcp_release(void);

/*---------------------------------------------------------------------------*/
/* DHCP PROCESS (for APP=net auto-start)                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief DHCP protothread process.
 *
 * Runs a DHCP exchange after a boot delay, then sets the system
 * IP address.  Register via TIKU_AUTOSTART_PROCESSES.
 */
extern struct tiku_process tiku_kits_net_dhcp_process;

#endif /* TIKU_KITS_NET_DHCP_H_ */
