/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_tftp.h - TFTP client (RFC 1350, RFC 2348 blksize)
 *
 * Provides a lightweight TFTP client for reading (RRQ) and writing
 * (WRQ) files over UDP.  Designed for ultra-low-power microcontrollers
 * with severe memory constraints: the entire transfer state fits in
 * approximately 120 bytes of static RAM with no heap allocation.
 *
 * The client negotiates a small block size (96 bytes by default) via
 * the RFC 2348 blksize option so that every packet fits within the
 * 128-byte MTU: IPv4(20) + UDP(8) + TFTP_HDR(4) + DATA(96) = 128.
 *
 * Because udp_send() cannot be called from inside a UDP receive
 * callback (the shared net_buf is in use), the TFTP client uses a
 * poll-based design: the receive callback stores incoming packet
 * data into static state variables, and the application calls
 * tftp_poll() from its own context to drive state transitions and
 * send ACK/DATA responses.
 *
 * Typical usage (read a file):
 * @code
 *   static uint16_t my_data_cb(uint16_t block, const uint8_t *data,
 *                               uint16_t len) {
 *       process_block(data, len);
 *       return len;
 *   }
 *   tiku_kits_net_tftp_init();
 *   tiku_kits_net_tftp_get(server_ip, "config.bin", my_data_cb);
 *   while (tiku_kits_net_tftp_get_state() < TFTP_STATE_COMPLETE) {
 *       tiku_kits_net_tftp_poll();
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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_NET_TFTP_H_
#define TIKU_KITS_NET_TFTP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"

/*---------------------------------------------------------------------------*/
/* TFTP OPCODES (RFC 1350 Section 5)                                         */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_TFTP_OPCODES TFTP Opcodes
 * @brief Packet type identifiers in the first two bytes of every
 *        TFTP packet.  Stored in network byte order in the buffer.
 * @{
 */
#define TIKU_KITS_NET_TFTP_OP_RRQ     1  /**< Read Request */
#define TIKU_KITS_NET_TFTP_OP_WRQ     2  /**< Write Request */
#define TIKU_KITS_NET_TFTP_OP_DATA    3  /**< Data */
#define TIKU_KITS_NET_TFTP_OP_ACK     4  /**< Acknowledgement */
#define TIKU_KITS_NET_TFTP_OP_ERROR   5  /**< Error */
#define TIKU_KITS_NET_TFTP_OP_OACK    6  /**< Option Acknowledgement (RFC 2348) */
/** @} */

/*---------------------------------------------------------------------------*/
/* TFTP HEADER BYTE OFFSETS                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_TFTP_OFFSETS TFTP Header Byte Offsets
 * @brief Offsets within the TFTP payload (inside the UDP payload).
 *
 * For DATA and ACK packets the layout is:
 *   opcode(2) + block_num(2) [+ data(variable)]
 *
 * For RRQ/WRQ the layout is:
 *   opcode(2) + filename(NUL) + mode(NUL) [+ options(NUL-pairs)]
 * @{
 */
#define TIKU_KITS_NET_TFTP_OFF_OPCODE  0  /**< Opcode (16b) */
#define TIKU_KITS_NET_TFTP_OFF_BLOCK   2  /**< Block number (16b) */
#define TIKU_KITS_NET_TFTP_OFF_DATA    4  /**< Start of data payload */
#define TIKU_KITS_NET_TFTP_HDR_LEN     4  /**< DATA/ACK header: opcode + block */
/** @} */

/*---------------------------------------------------------------------------*/
/* TFTP ERROR CODES (RFC 1350 Section 5)                                     */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_TFTP_ERRORS TFTP Error Codes
 * @brief Error codes carried in the TFTP ERROR packet (bytes 2-3).
 * @{
 */
#define TIKU_KITS_NET_TFTP_ERR_UNDEFINED     0  /**< Not defined */
#define TIKU_KITS_NET_TFTP_ERR_NOT_FOUND     1  /**< File not found */
#define TIKU_KITS_NET_TFTP_ERR_ACCESS        2  /**< Access violation */
#define TIKU_KITS_NET_TFTP_ERR_DISK_FULL     3  /**< Disk full */
#define TIKU_KITS_NET_TFTP_ERR_ILLEGAL_OP    4  /**< Illegal TFTP operation */
#define TIKU_KITS_NET_TFTP_ERR_UNKNOWN_TID   5  /**< Unknown transfer ID */
#define TIKU_KITS_NET_TFTP_ERR_EXISTS        6  /**< File already exists */
#define TIKU_KITS_NET_TFTP_ERR_NO_USER       7  /**< No such user */
/** @} */

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum data bytes per TFTP block.
 *
 * Derived from the MTU minus all protocol overhead:
 *   MTU(128) - IPv4(20) - UDP(8) - TFTP_HDR(4) = 96 bytes.
 *
 * The client sends a "blksize" option in every RRQ/WRQ to negotiate
 * this value with the server (RFC 2348).  If the server does not
 * support the option, it will respond with standard 512-byte blocks
 * which exceed our MTU and will be dropped -- the transfer will
 * time out.  Use a server that supports RFC 2348.
 */
#ifndef TIKU_KITS_NET_TFTP_BLOCK_SIZE
#define TIKU_KITS_NET_TFTP_BLOCK_SIZE  96
#endif

/**
 * @brief Local UDP port used by the TFTP client.
 *
 * The client binds this port via udp_bind() to receive server
 * responses.  One binding slot is consumed for the duration of
 * the transfer.  Must not conflict with other bound ports.
 */
#ifndef TIKU_KITS_NET_TFTP_LOCAL_PORT
#define TIKU_KITS_NET_TFTP_LOCAL_PORT  49152
#endif

/**
 * @brief Maximum filename length (excluding NUL terminator).
 *
 * Filenames longer than this are rejected by get() and put().
 * The filename, mode string, and blksize option must all fit
 * within the UDP max payload (100 bytes).
 */
#ifndef TIKU_KITS_NET_TFTP_MAX_FILENAME
#define TIKU_KITS_NET_TFTP_MAX_FILENAME  31
#endif

/** Well-known TFTP server port (RFC 1350). */
#define TIKU_KITS_NET_TFTP_PORT  69

/*---------------------------------------------------------------------------*/
/* STATE AND EVENT TYPES                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief TFTP client transfer state.
 *
 * The state progresses linearly from IDLE through the transfer
 * phases to either COMPLETE or ERROR.  COMPLETE and ERROR are
 * terminal states; call init() or abort() to return to IDLE.
 */
typedef enum {
    TIKU_KITS_NET_TFTP_STATE_IDLE,          /**< No transfer active */
    TIKU_KITS_NET_TFTP_STATE_RRQ_SENT,      /**< RRQ sent, awaiting response */
    TIKU_KITS_NET_TFTP_STATE_WRQ_SENT,      /**< WRQ sent, awaiting response */
    TIKU_KITS_NET_TFTP_STATE_TRANSFERRING,   /**< Receiving DATA blocks (RRQ) */
    TIKU_KITS_NET_TFTP_STATE_SENDING,        /**< Sending DATA blocks (WRQ) */
    TIKU_KITS_NET_TFTP_STATE_COMPLETE,       /**< Transfer finished OK */
    TIKU_KITS_NET_TFTP_STATE_ERROR           /**< Transfer failed */
} tiku_kits_net_tftp_state_t;

/**
 * @brief Events returned by tftp_poll().
 *
 * Each call to poll() returns at most one event.  The application
 * should call poll() in a loop until the state reaches COMPLETE
 * or ERROR.
 */
typedef enum {
    TIKU_KITS_NET_TFTP_EVT_NONE,        /**< No event pending */
    TIKU_KITS_NET_TFTP_EVT_DATA_READY,  /**< DATA block received + ACKed */
    TIKU_KITS_NET_TFTP_EVT_ACK_RECV,    /**< ACK received (WRQ path) */
    TIKU_KITS_NET_TFTP_EVT_OACK_RECV,   /**< OACK received (blksize OK) */
    TIKU_KITS_NET_TFTP_EVT_ERROR_RECV,  /**< ERROR packet received */
    TIKU_KITS_NET_TFTP_EVT_COMPLETE     /**< Transfer complete */
} tiku_kits_net_tftp_evt_t;

/*---------------------------------------------------------------------------*/
/* CALLBACK TYPES                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Data block receive callback (used during RRQ / read).
 *
 * Called from tftp_poll() (application context, NOT from the UDP
 * receive callback) each time a DATA block arrives.  The application
 * should process or store the data before returning.
 *
 * @param block_num  Block number (1-based, incrementing)
 * @param data       Pointer to block data (static buffer, valid
 *                   until the next poll() call)
 * @param data_len   Number of data bytes (0..TFTP_BLOCK_SIZE)
 * @return Number of bytes consumed (should equal data_len)
 */
typedef uint16_t (*tiku_kits_net_tftp_data_cb_t)(
    uint16_t       block_num,
    const uint8_t *data,
    uint16_t       data_len);

/**
 * @brief Data block supply callback (used during WRQ / write).
 *
 * Called from tftp_poll() (application context) to obtain the
 * next block of data to send.  The application writes into @p buf
 * and returns the number of bytes written.  Returning fewer than
 * @p max_len signals the last block (end of file).
 *
 * @param block_num  Block number being requested (1-based)
 * @param buf        Buffer to fill with data
 * @param max_len    Maximum bytes to write (TFTP_BLOCK_SIZE)
 * @return Actual bytes written to buf (< max_len = last block)
 */
typedef uint16_t (*tiku_kits_net_tftp_supply_cb_t)(
    uint16_t  block_num,
    uint8_t  *buf,
    uint16_t  max_len);

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the TFTP client.
 *
 * Resets all state to IDLE, clears pending events, and unbinds any
 * previously bound TFTP port.  Call once at startup.  Safe to call
 * again to forcibly reset after a transfer.
 */
void tiku_kits_net_tftp_init(void);

/*---------------------------------------------------------------------------*/
/* TRANSFER INITIATION                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Start a read transfer (download file from server).
 *
 * Constructs and sends a TFTP RRQ packet with mode "octet" and
 * the blksize option set to TIKU_KITS_NET_TFTP_BLOCK_SIZE.  Binds
 * the local TFTP port to receive the server's response.
 *
 * After calling get(), the application must call poll() repeatedly
 * to drive the transfer.  Each DATA block is delivered to @p data_cb
 * from within poll().  The transfer is complete when get_state()
 * returns COMPLETE or ERROR.
 *
 * Only one transfer can be active at a time.  Calling get() while
 * a transfer is in progress returns TIKU_KITS_NET_ERR_PARAM.
 *
 * @param server_addr  Server IPv4 address (4 bytes, network order)
 * @param filename     Null-terminated filename (max MAX_FILENAME chars)
 * @param data_cb      Callback to receive data blocks (must not be NULL)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_NET_ERR_PARAM if transfer already active or
 *         filename too long,
 *         TIKU_KITS_NET_ERR_OVERFLOW if RRQ packet exceeds max payload.
 */
int8_t tiku_kits_net_tftp_get(
    const uint8_t *server_addr,
    const char    *filename,
    tiku_kits_net_tftp_data_cb_t data_cb);

/**
 * @brief Start a write transfer (upload file to server).
 *
 * Constructs and sends a TFTP WRQ packet.  The server will respond
 * with an ACK(0) or OACK.  After that, poll() calls @p supply_cb
 * to get data for each block and sends DATA packets.
 *
 * @param server_addr  Server IPv4 address (4 bytes, network order)
 * @param filename     Null-terminated filename (max MAX_FILENAME chars)
 * @param supply_cb    Callback to supply data blocks (must not be NULL)
 * @return Same error codes as get().
 */
int8_t tiku_kits_net_tftp_put(
    const uint8_t *server_addr,
    const char    *filename,
    tiku_kits_net_tftp_supply_cb_t supply_cb);

/*---------------------------------------------------------------------------*/
/* POLLING AND STATE                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Poll for TFTP events and drive the state machine.
 *
 * Checks for pending events set by the UDP receive callback,
 * handles state transitions, delivers data to the application
 * callback, and sends ACK/DATA responses.  Must be called from
 * application context (not from inside a UDP callback).
 *
 * @return The event that was processed, or EVT_NONE if nothing
 *         happened since the last poll.
 */
tiku_kits_net_tftp_evt_t tiku_kits_net_tftp_poll(void);

/**
 * @brief Get the current transfer state.
 *
 * @return Current state (IDLE, *_SENT, TRANSFERRING, SENDING,
 *         COMPLETE, or ERROR).
 */
tiku_kits_net_tftp_state_t tiku_kits_net_tftp_get_state(void);

/**
 * @brief Get the error code from the last ERROR packet received.
 *
 * Only valid when get_state() returns TFTP_STATE_ERROR.
 *
 * @return TFTP error code (0-7 per RFC 1350), or 0 if no error.
 */
uint16_t tiku_kits_net_tftp_get_error(void);

/*---------------------------------------------------------------------------*/
/* TRANSFER CONTROL                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Abort the current transfer.
 *
 * Unbinds the TFTP port and resets state to IDLE.  Does not send
 * an ERROR packet to the server (the server will time out).
 *
 * @return TIKU_KITS_NET_OK always.
 */
int8_t tiku_kits_net_tftp_abort(void);

#endif /* TIKU_KITS_NET_TFTP_H_ */
