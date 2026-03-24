/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_tcp.h - TCP transport protocol (RFC 793)
 *
 * Provides connection-oriented reliable byte-stream transport for
 * TikuOS embedded networking.  Uses FRAM-backed buffers for TX
 * retransmission (via the kernel pool allocator) and RX data
 * buffering, keeping volatile SRAM usage low while providing
 * larger buffer capacity from non-volatile memory.
 *
 * Design constraints for ultra-low-power embedded targets:
 *   - Static allocation only (no heap)
 *   - Fixed maximum connections (compile-time configurable)
 *   - No out-of-order segment reassembly (OOO segments dropped)
 *   - No congestion control (assume controlled/point-to-point link)
 *   - No urgent data, no Nagle, no window scaling
 *   - MSS option only (negotiated during SYN exchange)
 *   - Simple fixed retransmission timeout (no Karn/Jacobson)
 *
 * FRAM buffer strategy:
 *   TX retransmission pool -- fixed-size segment blocks managed by
 *   the kernel pool allocator (tiku_pool_t).  Each block stores a
 *   copy of the payload data and metadata needed to reconstruct the
 *   segment for retransmission.  Blocks are freed when the peer
 *   ACKs the corresponding sequence range.
 *
 *   RX ring buffer -- per-connection circular buffer for incoming
 *   data that has been validated and ACK'd but not yet read by the
 *   application.  The receive window (rcv_wnd) tracks free space,
 *   providing end-to-end flow control.
 *
 * Both buffer types are placed in FRAM (.persistent section) so
 * they survive power loss and provide larger capacity than SRAM.
 * Connection control blocks remain in SRAM for fast access to
 * frequently updated fields (sequence numbers, state, timers).
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

#ifndef TIKU_KITS_NET_TCP_H_
#define TIKU_KITS_NET_TCP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"

/*---------------------------------------------------------------------------*/
/* TCP HEADER BYTE OFFSETS                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_TCP_OFFSETS TCP Header Byte Offsets
 * @brief Offsets into the raw packet buffer for each TCP header field.
 *
 * These offsets assume the minimum 20-byte TCP header (data offset=5,
 * no options).  Multi-byte fields are in network byte order.  Use
 * the accessor macros below for convenient extraction.
 * @{
 */
#define TIKU_KITS_NET_TCP_OFF_SPORT     0   /**< Source Port (16b) */
#define TIKU_KITS_NET_TCP_OFF_DPORT     2   /**< Destination Port (16b) */
#define TIKU_KITS_NET_TCP_OFF_SEQ       4   /**< Sequence Number (32b) */
#define TIKU_KITS_NET_TCP_OFF_ACK       8   /**< Acknowledgment Number (32b) */
#define TIKU_KITS_NET_TCP_OFF_DOFF     12   /**< Data Offset (4b) + Reserved */
#define TIKU_KITS_NET_TCP_OFF_FLAGS    13   /**< Flags (8b) */
#define TIKU_KITS_NET_TCP_OFF_WINDOW   14   /**< Window Size (16b) */
#define TIKU_KITS_NET_TCP_OFF_CHKSUM   16   /**< Checksum (16b) */
#define TIKU_KITS_NET_TCP_OFF_URGPTR   18   /**< Urgent Pointer (16b) */
#define TIKU_KITS_NET_TCP_HDR_LEN      20   /**< Minimum header (no options) */
/** @} */

/*---------------------------------------------------------------------------*/
/* TCP ACCESSOR MACROS                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Extract source port in host byte order.
 * @param tcp  Pointer to the start of the TCP header
 */
#define TIKU_KITS_NET_TCP_SPORT(tcp) \
    ((uint16_t)((uint16_t)(tcp)[0] << 8 | (tcp)[1]))

/**
 * @brief Extract destination port in host byte order.
 * @param tcp  Pointer to the start of the TCP header
 */
#define TIKU_KITS_NET_TCP_DPORT(tcp) \
    ((uint16_t)((uint16_t)(tcp)[2] << 8 | (tcp)[3]))

/**
 * @brief Extract 32-bit sequence number in host byte order.
 * @param tcp  Pointer to the start of the TCP header
 */
#define TIKU_KITS_NET_TCP_SEQ(tcp) \
    ((uint32_t)((uint32_t)(tcp)[4] << 24 | (uint32_t)(tcp)[5] << 16 | \
                (uint32_t)(tcp)[6] << 8  | (uint32_t)(tcp)[7]))

/**
 * @brief Extract 32-bit acknowledgment number in host byte order.
 * @param tcp  Pointer to the start of the TCP header
 */
#define TIKU_KITS_NET_TCP_ACKNUM(tcp) \
    ((uint32_t)((uint32_t)(tcp)[8] << 24 | (uint32_t)(tcp)[9] << 16 | \
                (uint32_t)(tcp)[10] << 8 | (uint32_t)(tcp)[11]))

/**
 * @brief Extract data offset in 32-bit words (upper 4 bits of byte 12).
 * @param tcp  Pointer to the start of the TCP header
 * @return Data offset (5-15); multiply by 4 for header length in bytes
 */
#define TIKU_KITS_NET_TCP_DOFF(tcp) \
    ((uint8_t)((tcp)[12] >> 4))

/**
 * @brief Extract the flags byte (byte 13).
 * @param tcp  Pointer to the start of the TCP header
 */
#define TIKU_KITS_NET_TCP_FLAGS(tcp) \
    ((uint8_t)(tcp)[13])

/**
 * @brief Extract window size in host byte order.
 * @param tcp  Pointer to the start of the TCP header
 */
#define TIKU_KITS_NET_TCP_WINDOW(tcp) \
    ((uint16_t)((uint16_t)(tcp)[14] << 8 | (tcp)[15]))

/*---------------------------------------------------------------------------*/
/* TCP FLAG CONSTANTS                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_TCP_FLAGS TCP Flag Bits
 * @brief Individual flag bits in the TCP flags byte (byte 13).
 * @{
 */
#define TIKU_KITS_NET_TCP_FLAG_FIN   0x01   /**< No more data from sender */
#define TIKU_KITS_NET_TCP_FLAG_SYN   0x02   /**< Synchronize sequence numbers */
#define TIKU_KITS_NET_TCP_FLAG_RST   0x04   /**< Reset the connection */
#define TIKU_KITS_NET_TCP_FLAG_PSH   0x08   /**< Push function */
#define TIKU_KITS_NET_TCP_FLAG_ACK   0x10   /**< Acknowledgment field valid */
/** @} */

/*---------------------------------------------------------------------------*/
/* TCP OPTION CONSTANTS                                                      */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_NET_TCP_OPT_END       0   /**< End of options list */
#define TIKU_KITS_NET_TCP_OPT_NOP       1   /**< No-operation (padding) */
#define TIKU_KITS_NET_TCP_OPT_MSS_KIND  2   /**< MSS option kind */
#define TIKU_KITS_NET_TCP_OPT_MSS_LEN   4   /**< MSS option total length */

/** TCP header length with MSS option (20 base + 4 MSS option) */
#define TIKU_KITS_NET_TCP_SYN_HDR_LEN  24

/*---------------------------------------------------------------------------*/
/* TCP STATES                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_TCP_STATES TCP Connection States
 * @brief RFC 793 Section 3.2 connection state machine.
 * @{
 */
#define TIKU_KITS_NET_TCP_STATE_CLOSED      0
#define TIKU_KITS_NET_TCP_STATE_LISTEN      1
#define TIKU_KITS_NET_TCP_STATE_SYN_SENT    2
#define TIKU_KITS_NET_TCP_STATE_SYN_RCVD    3
#define TIKU_KITS_NET_TCP_STATE_ESTABLISHED 4
#define TIKU_KITS_NET_TCP_STATE_FIN_WAIT_1  5
#define TIKU_KITS_NET_TCP_STATE_FIN_WAIT_2  6
#define TIKU_KITS_NET_TCP_STATE_CLOSE_WAIT  7
#define TIKU_KITS_NET_TCP_STATE_CLOSING     8
#define TIKU_KITS_NET_TCP_STATE_LAST_ACK    9
#define TIKU_KITS_NET_TCP_STATE_TIME_WAIT  10
/** @} */

/*---------------------------------------------------------------------------*/
/* TCP EVENTS                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_TCP_EVENTS TCP Connection Events
 * @brief Events delivered to the application via event_cb.
 * @{
 */
#define TIKU_KITS_NET_TCP_EVT_CONNECTED  1  /**< 3WH complete */
#define TIKU_KITS_NET_TCP_EVT_CLOSED     2  /**< Graceful close complete */
#define TIKU_KITS_NET_TCP_EVT_ABORTED    3  /**< RST or timeout */
/** @} */

/*---------------------------------------------------------------------------*/
/* CONFIGURATION (compile-time overrideable)                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of simultaneous TCP connections.
 *
 * Each connection uses ~56 bytes of SRAM for the control block
 * plus TIKU_KITS_NET_TCP_RX_BUF_SIZE bytes of FRAM for the RX
 * ring buffer.
 */
#ifndef TIKU_KITS_NET_TCP_MAX_CONNS
#define TIKU_KITS_NET_TCP_MAX_CONNS         2
#endif

/**
 * @brief Maximum number of listening ports.
 *
 * Each listener costs 6 bytes of SRAM (port + two callback pointers
 * on MSP430).
 */
#ifndef TIKU_KITS_NET_TCP_MAX_LISTENERS
#define TIKU_KITS_NET_TCP_MAX_LISTENERS     2
#endif

/**
 * @brief Enable the built-in TCP echo service on port 7.
 *
 * When enabled, tcp_init() registers an echo listener on port 7
 * that reads incoming data and sends it back.  This provides a
 * simple integration test alongside ICMP and UDP echo.  Consumes
 * one listener slot.  Set to 0 to disable and save code space.
 */
#ifndef TIKU_KITS_NET_TCP_ECHO_ENABLE
#define TIKU_KITS_NET_TCP_ECHO_ENABLE       1
#endif

/** TCP echo port number. */
#define TIKU_KITS_NET_TCP_ECHO_PORT         7

/**
 * @brief Number of TX retransmission segment blocks in the FRAM pool.
 *
 * Shared across all connections.  Each block stores one segment's
 * payload data plus 10 bytes of metadata.  Block size is computed
 * as TIKU_KITS_NET_TCP_MSS + 10.
 */
#ifndef TIKU_KITS_NET_TCP_TX_POOL_COUNT
#define TIKU_KITS_NET_TCP_TX_POOL_COUNT     8
#endif

/**
 * @brief Per-connection RX ring buffer size in bytes (FRAM).
 *
 * Determines the maximum receive window advertised to the peer.
 * Must be at least MSS + 1 to accept one full segment.
 */
#ifndef TIKU_KITS_NET_TCP_RX_BUF_SIZE
#define TIKU_KITS_NET_TCP_RX_BUF_SIZE       256
#endif

/**
 * @brief Initial retransmission timeout in periodic intervals.
 *
 * Each periodic interval is approximately 50 ms (POLL_TICKS).
 * Default 200 intervals = ~10 seconds.  Longer than typical
 * TCP stacks to accommodate the eZ-FET backchannel reopen
 * delay (~7 seconds).  Doubled on each retry.
 */
#ifndef TIKU_KITS_NET_TCP_RTO_INIT
#define TIKU_KITS_NET_TCP_RTO_INIT          200
#endif

/**
 * @brief Maximum retransmission attempts before aborting.
 */
#ifndef TIKU_KITS_NET_TCP_MAX_RETRIES
#define TIKU_KITS_NET_TCP_MAX_RETRIES       4
#endif

/**
 * @brief TIME_WAIT duration in periodic intervals.
 *
 * Default 80 intervals = ~4 seconds.  Shorter than the RFC-
 * specified 2*MSL (4 minutes), appropriate for embedded use.
 */
#ifndef TIKU_KITS_NET_TCP_TIME_WAIT_TICKS
#define TIKU_KITS_NET_TCP_TIME_WAIT_TICKS   80
#endif

/**
 * @brief Default MSS when peer does not advertise (RFC 879).
 */
#ifndef TIKU_KITS_NET_TCP_DEFAULT_MSS
#define TIKU_KITS_NET_TCP_DEFAULT_MSS       536
#endif

/**
 * @brief Our MSS: maximum TCP payload we can receive in one segment.
 *
 * Derived from MTU minus IPv4 header (20) minus TCP header (20).
 * Advertised to the peer in the MSS option during SYN exchange.
 */
#ifndef TIKU_KITS_NET_TCP_MSS
#define TIKU_KITS_NET_TCP_MSS \
    (TIKU_KITS_NET_MTU - 20 - TIKU_KITS_NET_TCP_HDR_LEN)
#endif

/*---------------------------------------------------------------------------*/
/* TX SEGMENT POOL SIZING                                                    */
/*---------------------------------------------------------------------------*/

/** Metadata overhead per TX segment block (next + seq + len + flags + pad) */
#define TIKU_KITS_NET_TCP_TX_SEG_HDR   10

/** Total block size for the TX segment pool */
#define TIKU_KITS_NET_TCP_TX_SEG_BLOCK \
    (TIKU_KITS_NET_TCP_TX_SEG_HDR + TIKU_KITS_NET_TCP_MSS)

/*---------------------------------------------------------------------------*/
/* SEQUENCE NUMBER HELPERS                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_TCP_SEQ TCP Sequence Number Comparisons
 * @brief Modular arithmetic comparisons for 32-bit sequence numbers.
 *
 * TCP sequence numbers wrap at 2^32.  These macros use signed
 * subtraction to correctly compare across the wrap boundary.
 * @{
 */
#define TIKU_KITS_NET_TCP_SEQ_LT(a, b) \
    ((int32_t)((uint32_t)(a) - (uint32_t)(b)) < 0)
#define TIKU_KITS_NET_TCP_SEQ_LEQ(a, b) \
    ((int32_t)((uint32_t)(a) - (uint32_t)(b)) <= 0)
#define TIKU_KITS_NET_TCP_SEQ_GT(a, b) \
    ((int32_t)((uint32_t)(a) - (uint32_t)(b)) > 0)
#define TIKU_KITS_NET_TCP_SEQ_GEQ(a, b) \
    ((int32_t)((uint32_t)(a) - (uint32_t)(b)) >= 0)
/** @} */

/*---------------------------------------------------------------------------*/
/* FORWARD DECLARATION                                                       */
/*---------------------------------------------------------------------------*/

struct tiku_kits_net_tcp_conn;

/*---------------------------------------------------------------------------*/
/* CALLBACK TYPES                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Data arrival notification callback.
 *
 * Invoked when new data has been written to the connection's RX
 * ring buffer.  The application should call tiku_kits_net_tcp_read()
 * to retrieve the data.  Runs in the net process context.
 *
 * @param conn       Connection that received data
 * @param available  Total bytes available in the RX ring buffer
 */
typedef void (*tiku_kits_net_tcp_recv_cb_t)(
    struct tiku_kits_net_tcp_conn *conn,
    uint16_t available);

/**
 * @brief Connection event notification callback.
 *
 * Invoked on state transitions that the application should know
 * about: connection established, graceful close complete, or
 * connection aborted (RST received or retransmission exhausted).
 *
 * @param conn   Connection (may be invalid after CLOSED/ABORTED)
 * @param event  One of TIKU_KITS_NET_TCP_EVT_*
 */
typedef void (*tiku_kits_net_tcp_event_cb_t)(
    struct tiku_kits_net_tcp_conn *conn,
    uint8_t event);

/*---------------------------------------------------------------------------*/
/* CONNECTION CONTROL BLOCK                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_net_tcp_conn
 * @brief TCP connection control block (TCB).
 *
 * Lives in SRAM for fast access to frequently updated fields.
 * Each connection has a dedicated FRAM-backed RX ring buffer
 * and shares a FRAM-backed TX segment pool with other connections.
 */
typedef struct tiku_kits_net_tcp_conn {
    /* --- Connection identity --- */
    tiku_kits_net_ip4_addr_t remote_addr;  /**< Remote IP address */
    uint16_t local_port;                   /**< Local port (host order) */
    uint16_t remote_port;                  /**< Remote port (host order) */
    uint8_t  state;                        /**< TCP state */
    uint8_t  active;                       /**< Non-zero if slot in use */

    /* --- Send sequence variables (RFC 793 §3.2) --- */
    uint32_t snd_una;    /**< Oldest unacknowledged sequence number */
    uint32_t snd_nxt;    /**< Next sequence number to send */
    uint16_t snd_wnd;    /**< Peer's advertised receive window */
    uint16_t snd_mss;    /**< Negotiated send MSS for this connection */

    /* --- Receive sequence variables --- */
    uint32_t rcv_nxt;    /**< Next expected sequence number from peer */
    uint16_t rcv_wnd;    /**< Our advertised receive window */

    /* --- RX ring buffer (FRAM-backed) --- */
    uint8_t  *rx_buf;    /**< Pointer to FRAM ring buffer storage */
    uint16_t  rx_head;   /**< Write position (incoming data) */
    uint16_t  rx_tail;   /**< Read position (application reads) */

    /* --- TX retransmission queue --- */
    void     *tx_head;   /**< Head of TX segment linked list (FRAM) */
    void     *tx_tail;   /**< Tail for O(1) append */
    uint8_t   tx_count;  /**< Number of segments in TX queue */

    /* --- Retransmission timer --- */
    uint16_t rto_ticks;     /**< Current retransmission timeout */
    uint16_t rto_counter;   /**< Ticks since last unACK'd send */
    uint8_t  retries;       /**< Retransmission attempt count */

    /* --- TIME_WAIT timer --- */
    uint16_t tw_counter;    /**< Ticks remaining in TIME_WAIT */

    /* --- Peer MSS --- */
    uint16_t peer_mss;      /**< Peer's advertised MSS (from SYN) */

    /* --- Application callbacks --- */
    tiku_kits_net_tcp_recv_cb_t  recv_cb;   /**< Data received */
    tiku_kits_net_tcp_event_cb_t event_cb;  /**< Connection events */
} tiku_kits_net_tcp_conn_t;

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the TCP subsystem.
 *
 * Clears the connection table, listener table, and creates the
 * FRAM-backed TX segment pool via the kernel pool allocator.
 * Called once during net process startup.
 */
void tiku_kits_net_tcp_init(void);

/*---------------------------------------------------------------------------*/
/* PASSIVE OPEN (SERVER)                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Listen for incoming TCP connections on a port.
 *
 * Registers callbacks that will be inherited by connections
 * accepted on this port.  When a SYN arrives for the port,
 * a connection is automatically allocated and the 3-way
 * handshake proceeds.  On completion, event_cb is called
 * with TIKU_KITS_NET_TCP_EVT_CONNECTED.
 *
 * @param port      Port number in host byte order (must be > 0)
 * @param recv_cb   Data callback for accepted connections
 * @param event_cb  Event callback for accepted connections
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if callbacks are NULL or port 0,
 *         TIKU_KITS_NET_ERR_OVERFLOW if listener table is full
 */
int8_t tiku_kits_net_tcp_listen(
    uint16_t port,
    tiku_kits_net_tcp_recv_cb_t recv_cb,
    tiku_kits_net_tcp_event_cb_t event_cb);

/**
 * @brief Stop listening on a port.
 *
 * Removes the listener entry.  Existing connections accepted
 * on this port are not affected.
 *
 * @param port  Port number in host byte order
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if port was not listened
 */
int8_t tiku_kits_net_tcp_unlisten(uint16_t port);

/*---------------------------------------------------------------------------*/
/* ACTIVE OPEN (CLIENT)                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initiate a TCP connection to a remote host.
 *
 * Allocates a connection slot, sends a SYN with MSS option,
 * and enters SYN_SENT state.  The event_cb will be called
 * with TIKU_KITS_NET_TCP_EVT_CONNECTED when the handshake
 * completes, or ABORTED if it fails.
 *
 * @param dst_addr   Destination IP address (4 bytes, network order)
 * @param dst_port   Destination port in host byte order
 * @param src_port   Source port in host byte order
 * @param recv_cb    Data callback
 * @param event_cb   Event callback
 * @return Pointer to the new connection, or NULL on failure
 *         (no free slot, NULL arguments, or send failure)
 */
tiku_kits_net_tcp_conn_t *tiku_kits_net_tcp_connect(
    const uint8_t *dst_addr,
    uint16_t dst_port,
    uint16_t src_port,
    tiku_kits_net_tcp_recv_cb_t recv_cb,
    tiku_kits_net_tcp_event_cb_t event_cb);

/*---------------------------------------------------------------------------*/
/* DATA TRANSFER                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send data on an established TCP connection.
 *
 * Copies the payload into a FRAM-backed TX segment pool block
 * for retransmission, constructs a TCP segment in the shared
 * net_buf, and transmits it.  The caller's buffer is free to
 * reuse immediately after this function returns.
 *
 * Data length must not exceed snd_mss (the negotiated MSS).
 * The caller is responsible for chunking larger payloads.
 *
 * @param conn      Connection (must be ESTABLISHED or CLOSE_WAIT)
 * @param data      Payload bytes to send
 * @param data_len  Payload length (max conn->snd_mss)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if conn/data is NULL or wrong state,
 *         TIKU_KITS_NET_ERR_OVERFLOW if data exceeds MSS or
 *         TX pool is exhausted
 */
int8_t tiku_kits_net_tcp_send(
    tiku_kits_net_tcp_conn_t *conn,
    const uint8_t *data,
    uint16_t data_len);

/**
 * @brief Read data from a connection's RX ring buffer.
 *
 * Copies up to @p buf_len bytes from the RX ring buffer into
 * the caller's buffer.  Advances the read position and updates
 * the receive window.  If the window was zero and opens up,
 * an ACK is sent to unblock the peer.
 *
 * @param conn     Connection to read from
 * @param buf      Destination buffer
 * @param buf_len  Maximum bytes to read
 * @return Number of bytes actually read (0 if no data available),
 *         or 0 if conn/buf is NULL
 */
uint16_t tiku_kits_net_tcp_read(
    tiku_kits_net_tcp_conn_t *conn,
    uint8_t *buf,
    uint16_t buf_len);

/*---------------------------------------------------------------------------*/
/* CONNECTION CLOSE                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initiate a graceful close (send FIN).
 *
 * In ESTABLISHED state, sends FIN+ACK and moves to FIN_WAIT_1.
 * In CLOSE_WAIT state (peer already closed), sends FIN+ACK and
 * moves to LAST_ACK.
 *
 * @param conn  Connection to close
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if conn is NULL or wrong state
 */
int8_t tiku_kits_net_tcp_close(tiku_kits_net_tcp_conn_t *conn);

/**
 * @brief Abort a connection immediately (send RST).
 *
 * Sends a RST segment and frees all resources.  The connection
 * slot becomes available for reuse.  No event callback is
 * invoked (the caller already knows).
 *
 * @param conn  Connection to abort (may be in any state)
 */
void tiku_kits_net_tcp_abort(tiku_kits_net_tcp_conn_t *conn);

/*---------------------------------------------------------------------------*/
/* INPUT / PERIODIC                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an incoming TCP segment.
 *
 * Called by the IPv4 input pipeline when protocol == 6 (TCP).
 * Validates the TCP header, verifies the checksum, looks up the
 * connection or listener, and drives the state machine.
 *
 * @param buf      Full IP packet buffer (may be modified for replies)
 * @param len      Total packet length (IPv4 header + TCP)
 * @param ihl_len  IPv4 header length in bytes (IHL * 4)
 */
void tiku_kits_net_tcp_input(uint8_t *buf, uint16_t len,
                              uint16_t ihl_len);

/**
 * @brief Periodic timer handler for TCP connections.
 *
 * Called from the net process timer loop (~20 Hz).  Handles
 * retransmission timeouts, TIME_WAIT expiration, and exponential
 * backoff.
 */
void tiku_kits_net_tcp_periodic(void);

#endif /* TIKU_KITS_NET_TCP_H_ */
