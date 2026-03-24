/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_tftp.c - TFTP client implementation (RFC 1350)
 *
 * Poll-based TFTP client that runs on top of the UDP layer.  The
 * UDP receive callback stores incoming packets into static state
 * variables; the application calls tftp_poll() from its own context
 * to drive state transitions and send ACK/DATA responses.
 *
 * RAM budget:
 *   - Transfer state variables:  ~20 bytes
 *   - Block buffer (blk_buf):   100 bytes (4 hdr + 96 data)
 *   - Total:                    ~120 bytes
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

#include "tiku_kits_net_tftp.h"
#include "tiku_kits_net_udp.h"
#include "tiku_kits_net_ipv4.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* STATIC STATE                                                              */
/*---------------------------------------------------------------------------*/

/** Current transfer state */
static tiku_kits_net_tftp_state_t  state;

/** Server IPv4 address (copied from get/put call) */
static uint8_t  server_ip[4];

/** Server's transfer ID (ephemeral port from first response) */
static uint16_t server_tid;

/** Current block number */
static uint16_t block_num;

/** Pending event set by the receive callback, consumed by poll() */
static tiku_kits_net_tftp_evt_t pending_evt;

/** TFTP error code from the last ERROR packet (valid when state==ERROR) */
static uint16_t tftp_error_code;

/** Transfer direction: 0 = read (RRQ), 1 = write (WRQ) */
static uint8_t  direction;

/** 1 if the last DATA block was a full BLOCK_SIZE bytes */
static uint8_t  last_was_full;

/** Application callbacks (only one is active at a time) */
static tiku_kits_net_tftp_data_cb_t   rx_data_cb;
static tiku_kits_net_tftp_supply_cb_t tx_supply_cb;

/**
 * Combined buffer for TFTP packet assembly and received data.
 *
 * Layout: [opcode(2)][block(2)][data(BLOCK_SIZE)]
 *
 * For RX (RRQ path): the receive callback copies incoming DATA
 * payload here.  data_cb reads from blk_buf + 4.
 *
 * For TX (WRQ path): send_data() writes the opcode and block
 * number into [0..3], supply_cb fills [4..] with application data,
 * and the whole buffer is passed to udp_send().
 *
 * For ACK packets: only [0..3] are used (4 bytes).
 */
static uint8_t blk_buf[TIKU_KITS_NET_TFTP_HDR_LEN
                        + TIKU_KITS_NET_TFTP_BLOCK_SIZE];

/** Number of data bytes in the current block (0..BLOCK_SIZE) */
static uint16_t blk_data_len;

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send a TFTP ACK packet for the given block number.
 *
 * Constructs a 4-byte ACK (opcode=4 + block) in blk_buf and
 * sends it via udp_send to the server's TID.
 *
 * @param block  Block number to acknowledge
 * @return Result from udp_send()
 */
static int8_t
send_ack(uint16_t block)
{
    uint8_t ack[4];

    ack[0] = 0;
    ack[1] = TIKU_KITS_NET_TFTP_OP_ACK;
    ack[2] = (uint8_t)(block >> 8);
    ack[3] = (uint8_t)(block & 0xFF);

    return tiku_kits_net_udp_send(server_ip, server_tid,
                                  TIKU_KITS_NET_TFTP_LOCAL_PORT,
                                  ack, 4);
}

/**
 * @brief Send a TFTP DATA packet for the given block.
 *
 * Writes the DATA opcode and block number into the first 4 bytes
 * of blk_buf (data must already be at blk_buf + 4), then sends
 * the assembled packet via udp_send.
 *
 * @param block  Block number
 * @param len    Number of data bytes (at blk_buf + 4)
 * @return Result from udp_send()
 */
static int8_t
send_data(uint16_t block, uint16_t len)
{
    blk_buf[0] = 0;
    blk_buf[1] = TIKU_KITS_NET_TFTP_OP_DATA;
    blk_buf[2] = (uint8_t)(block >> 8);
    blk_buf[3] = (uint8_t)(block & 0xFF);

    return tiku_kits_net_udp_send(
        server_ip, server_tid,
        TIKU_KITS_NET_TFTP_LOCAL_PORT,
        blk_buf, TIKU_KITS_NET_TFTP_HDR_LEN + len);
}

/**
 * @brief Build and send an RRQ or WRQ packet.
 *
 * Packet format: opcode(2) + filename + NUL + "octet" + NUL
 *                + "blksize" + NUL + "<size>" + NUL
 *
 * Assembled in blk_buf and sent to server port 69.
 *
 * @param opcode    TIKU_KITS_NET_TFTP_OP_RRQ or OP_WRQ
 * @param filename  Null-terminated filename
 * @return TIKU_KITS_NET_OK on success, negative on error
 */
static int8_t
send_request(uint8_t opcode, const char *filename)
{
    uint16_t pos = 0;
    uint16_t fname_len;
    /* "octet" + NUL = 6 bytes, "blksize" + NUL + "96" + NUL = 12 bytes */
    static const char mode[] = "octet";
    static const char opt_name[] = "blksize";
    static const char opt_val[] = "96";

    fname_len = (uint16_t)strlen(filename);

    /* Opcode (2 bytes, network order) */
    blk_buf[pos++] = 0;
    blk_buf[pos++] = opcode;

    /* Filename + NUL */
    memcpy(blk_buf + pos, filename, fname_len);
    pos += fname_len;
    blk_buf[pos++] = 0;

    /* Mode "octet" + NUL */
    memcpy(blk_buf + pos, mode, sizeof(mode));  /* includes NUL */
    pos += (uint16_t)sizeof(mode);

    /* blksize option: "blksize" + NUL + "96" + NUL */
    memcpy(blk_buf + pos, opt_name, sizeof(opt_name));
    pos += (uint16_t)sizeof(opt_name);
    memcpy(blk_buf + pos, opt_val, sizeof(opt_val));
    pos += (uint16_t)sizeof(opt_val);

    return tiku_kits_net_udp_send(
        server_ip, TIKU_KITS_NET_TFTP_PORT,
        TIKU_KITS_NET_TFTP_LOCAL_PORT,
        blk_buf, pos);
}

/*---------------------------------------------------------------------------*/
/* UDP RECEIVE CALLBACK                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief UDP receive callback for TFTP responses.
 *
 * Registered via udp_bind() when a transfer starts.  Stores
 * incoming packet data into static state variables and sets
 * pending_evt for poll() to process.  Does NOT call udp_send()
 * (re-entrancy guard would reject it).
 *
 * Validates:
 *   - State is not IDLE (transfer must be active)
 *   - Payload is at least 2 bytes (opcode)
 *   - Server TID matches (after first response establishes it)
 */
static void
tftp_recv_cb(const uint8_t *src_addr, uint16_t src_port,
             const uint8_t *payload, uint16_t payload_len)
{
    uint16_t opcode;
    uint16_t blk;
    uint16_t data_len;

    (void)src_addr;

    /* Ignore packets when no transfer is active */
    if (state == TIKU_KITS_NET_TFTP_STATE_IDLE ||
        state == TIKU_KITS_NET_TFTP_STATE_COMPLETE ||
        state == TIKU_KITS_NET_TFTP_STATE_ERROR) {
        return;
    }

    /* Minimum TFTP packet: 2-byte opcode */
    if (payload_len < 2) {
        return;
    }

    opcode = (uint16_t)((uint16_t)payload[0] << 8 | payload[1]);

    switch (opcode) {

    case TIKU_KITS_NET_TFTP_OP_DATA:
        /* DATA packet: opcode(2) + block(2) + data(0..BLOCK_SIZE) */
        if (payload_len < TIKU_KITS_NET_TFTP_HDR_LEN) {
            return;
        }

        /* Track server TID from the first response */
        if (state == TIKU_KITS_NET_TFTP_STATE_RRQ_SENT) {
            server_tid = src_port;
        }

        /* Reject packets from wrong TID */
        if (src_port != server_tid) {
            return;
        }

        blk = (uint16_t)((uint16_t)payload[2] << 8 | payload[3]);

        /* Copy data to block buffer.  Clamp to BLOCK_SIZE to
         * guard against oversized responses from servers that
         * ignored our blksize option. */
        data_len = payload_len - TIKU_KITS_NET_TFTP_HDR_LEN;
        if (data_len > TIKU_KITS_NET_TFTP_BLOCK_SIZE) {
            data_len = TIKU_KITS_NET_TFTP_BLOCK_SIZE;
        }
        if (data_len > 0) {
            memcpy(blk_buf + TIKU_KITS_NET_TFTP_HDR_LEN,
                   payload + TIKU_KITS_NET_TFTP_HDR_LEN,
                   data_len);
        }

        block_num = blk;
        blk_data_len = data_len;
        last_was_full = (data_len == TIKU_KITS_NET_TFTP_BLOCK_SIZE)
                        ? 1 : 0;
        state = TIKU_KITS_NET_TFTP_STATE_TRANSFERRING;
        pending_evt = TIKU_KITS_NET_TFTP_EVT_DATA_READY;
        break;

    case TIKU_KITS_NET_TFTP_OP_ACK:
        /* ACK packet: opcode(2) + block(2) */
        if (payload_len < TIKU_KITS_NET_TFTP_HDR_LEN) {
            return;
        }

        /* Track server TID from the first WRQ response */
        if (state == TIKU_KITS_NET_TFTP_STATE_WRQ_SENT) {
            server_tid = src_port;
        }

        if (src_port != server_tid) {
            return;
        }

        blk = (uint16_t)((uint16_t)payload[2] << 8 | payload[3]);
        block_num = blk;
        pending_evt = TIKU_KITS_NET_TFTP_EVT_ACK_RECV;
        break;

    case TIKU_KITS_NET_TFTP_OP_OACK:
        /* Option Acknowledgement: server accepted our blksize.
         * Track the server TID and signal the event. */
        server_tid = src_port;
        pending_evt = TIKU_KITS_NET_TFTP_EVT_OACK_RECV;
        break;

    case TIKU_KITS_NET_TFTP_OP_ERROR:
        /* Error packet: opcode(2) + error_code(2) + msg + NUL */
        if (payload_len >= TIKU_KITS_NET_TFTP_HDR_LEN) {
            tftp_error_code = (uint16_t)(
                (uint16_t)payload[2] << 8 | payload[3]);
        } else {
            tftp_error_code = TIKU_KITS_NET_TFTP_ERR_UNDEFINED;
        }
        state = TIKU_KITS_NET_TFTP_STATE_ERROR;
        pending_evt = TIKU_KITS_NET_TFTP_EVT_ERROR_RECV;
        break;

    default:
        /* Unknown opcode -- ignore */
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset all TFTP state to idle.
 *
 * Unbinds the local port if it was bound, clears all state
 * variables, and zeroes the block buffer.
 */
void
tiku_kits_net_tftp_init(void)
{
    /* Unbind in case a previous transfer left the port bound */
    tiku_kits_net_udp_unbind(TIKU_KITS_NET_TFTP_LOCAL_PORT);

    state          = TIKU_KITS_NET_TFTP_STATE_IDLE;
    pending_evt    = TIKU_KITS_NET_TFTP_EVT_NONE;
    server_tid     = 0;
    block_num      = 0;
    blk_data_len   = 0;
    tftp_error_code = 0;
    direction      = 0;
    last_was_full  = 0;
    rx_data_cb     = (tiku_kits_net_tftp_data_cb_t)0;
    tx_supply_cb   = (tiku_kits_net_tftp_supply_cb_t)0;

    memset(server_ip, 0, sizeof(server_ip));
    memset(blk_buf, 0, sizeof(blk_buf));
}

/*---------------------------------------------------------------------------*/
/* TRANSFER INITIATION                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Common setup for get() and put().
 *
 * Validates parameters, copies server address, binds the local
 * port, and sends the RRQ or WRQ packet.
 */
static int8_t
start_transfer(const uint8_t *server_addr, const char *filename,
               uint8_t opcode)
{
    uint16_t fname_len;
    int8_t rc;

    /* Validate parameters */
    if (server_addr == NULL || filename == NULL) {
        return TIKU_KITS_NET_ERR_NULL;
    }
    if (state != TIKU_KITS_NET_TFTP_STATE_IDLE) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    fname_len = (uint16_t)strlen(filename);
    if (fname_len == 0 || fname_len > TIKU_KITS_NET_TFTP_MAX_FILENAME) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Copy server address */
    memcpy(server_ip, server_addr, 4);

    /* Bind the local port for receiving responses */
    rc = tiku_kits_net_udp_bind(TIKU_KITS_NET_TFTP_LOCAL_PORT,
                                 tftp_recv_cb);
    if (rc != TIKU_KITS_NET_OK) {
        return rc;
    }

    /* Reset transfer state */
    pending_evt     = TIKU_KITS_NET_TFTP_EVT_NONE;
    server_tid      = 0;
    block_num       = 0;
    blk_data_len    = 0;
    tftp_error_code = 0;
    last_was_full   = 1;

    /* Send the request packet */
    rc = send_request(opcode, filename);
    if (rc != TIKU_KITS_NET_OK) {
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_TFTP_LOCAL_PORT);
        state = TIKU_KITS_NET_TFTP_STATE_IDLE;
        return rc;
    }

    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Start a read transfer (download file from server).
 *
 * Stores the data callback, calls start_transfer() to send an
 * RRQ with blksize option, and enters the RRQ_SENT state.
 * After this, the application must call tftp_poll() repeatedly
 * to receive DATA blocks via the callback and drive the ACK
 * exchange to completion.
 */
int8_t
tiku_kits_net_tftp_get(const uint8_t *server_addr,
                        const char *filename,
                        tiku_kits_net_tftp_data_cb_t data_cb)
{
    int8_t rc;

    if (data_cb == (tiku_kits_net_tftp_data_cb_t)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    rx_data_cb = data_cb;
    tx_supply_cb = (tiku_kits_net_tftp_supply_cb_t)0;
    direction = 0;

    rc = start_transfer(server_addr, filename,
                         TIKU_KITS_NET_TFTP_OP_RRQ);
    if (rc == TIKU_KITS_NET_OK) {
        state = TIKU_KITS_NET_TFTP_STATE_RRQ_SENT;
    }
    return rc;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Start a write transfer (upload file to server).
 *
 * Stores the supply callback, calls start_transfer() to send a
 * WRQ with blksize option, and enters the WRQ_SENT state.
 * After this, the application must call tftp_poll() repeatedly.
 * Each ACK from the server triggers the supply_cb to provide
 * the next data block for transmission.
 */
int8_t
tiku_kits_net_tftp_put(const uint8_t *server_addr,
                        const char *filename,
                        tiku_kits_net_tftp_supply_cb_t supply_cb)
{
    int8_t rc;

    if (supply_cb == (tiku_kits_net_tftp_supply_cb_t)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    tx_supply_cb = supply_cb;
    rx_data_cb = (tiku_kits_net_tftp_data_cb_t)0;
    direction = 1;

    rc = start_transfer(server_addr, filename,
                         TIKU_KITS_NET_TFTP_OP_WRQ);
    if (rc == TIKU_KITS_NET_OK) {
        state = TIKU_KITS_NET_TFTP_STATE_WRQ_SENT;
    }
    return rc;
}

/*---------------------------------------------------------------------------*/
/* POLLING                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process pending events and drive the TFTP state machine.
 *
 * RRQ path (direction==0):
 *   DATA_READY -> deliver to data_cb, send ACK, check last block
 *   OACK_RECV  -> send ACK(0) to confirm option acceptance
 *
 * WRQ path (direction==1):
 *   ACK_RECV   -> supply next block from tx_supply_cb, send DATA
 *   OACK_RECV  -> supply first block, send DATA(1)
 *
 * Both paths:
 *   ERROR_RECV -> unbind port, state already set to ERROR
 */
tiku_kits_net_tftp_evt_t
tiku_kits_net_tftp_poll(void)
{
    tiku_kits_net_tftp_evt_t evt;
    uint16_t next_block;
    uint16_t supply_len;

    evt = pending_evt;
    pending_evt = TIKU_KITS_NET_TFTP_EVT_NONE;

    if (evt == TIKU_KITS_NET_TFTP_EVT_NONE) {
        return evt;
    }

    switch (evt) {

    case TIKU_KITS_NET_TFTP_EVT_DATA_READY:
        /* RRQ path: a DATA block has been received.
         * Deliver the data to the application callback, then
         * send an ACK for this block. */
        if (rx_data_cb != (tiku_kits_net_tftp_data_cb_t)0) {
            rx_data_cb(block_num,
                       blk_buf + TIKU_KITS_NET_TFTP_HDR_LEN,
                       blk_data_len);
        }

        /* ACK the received block */
        send_ack(block_num);

        /* If the block was shorter than BLOCK_SIZE, this is the
         * last block -- the transfer is complete (RFC 1350 rule:
         * a DATA packet with fewer than blksize bytes signals EOF) */
        if (!last_was_full) {
            state = TIKU_KITS_NET_TFTP_STATE_COMPLETE;
            tiku_kits_net_udp_unbind(TIKU_KITS_NET_TFTP_LOCAL_PORT);
            evt = TIKU_KITS_NET_TFTP_EVT_COMPLETE;
        }
        break;

    case TIKU_KITS_NET_TFTP_EVT_ACK_RECV:
        /* WRQ path: server acknowledged a DATA block. */
        if (direction == 1) {
            /* If we already sent the final short block (last_was_full==0)
             * and now received the ACK for it, the transfer is complete.
             * Check BEFORE supplying the next block. */
            if (!last_was_full &&
                state == TIKU_KITS_NET_TFTP_STATE_SENDING) {
                state = TIKU_KITS_NET_TFTP_STATE_COMPLETE;
                tiku_kits_net_udp_unbind(
                    TIKU_KITS_NET_TFTP_LOCAL_PORT);
                return TIKU_KITS_NET_TFTP_EVT_COMPLETE;
            }

            /* Supply the next block from the application and send it */
            next_block = block_num + 1;
            supply_len = 0;

            if (tx_supply_cb != (tiku_kits_net_tftp_supply_cb_t)0) {
                supply_len = tx_supply_cb(
                    next_block,
                    blk_buf + TIKU_KITS_NET_TFTP_HDR_LEN,
                    TIKU_KITS_NET_TFTP_BLOCK_SIZE);
            }

            send_data(next_block, supply_len);
            block_num = next_block;
            state = TIKU_KITS_NET_TFTP_STATE_SENDING;

            /* If the application supplied fewer than BLOCK_SIZE
             * bytes, this is the last DATA block.  We must still
             * wait for the server's final ACK before completing,
             * but we mark that the transfer is finishing. */
            if (supply_len < TIKU_KITS_NET_TFTP_BLOCK_SIZE) {
                last_was_full = 0;
            }
        }
        break;

    case TIKU_KITS_NET_TFTP_EVT_OACK_RECV:
        /* Server accepted our blksize option.
         * RRQ: send ACK(0) to confirm, server will send DATA(1)
         * WRQ: supply first data block and send DATA(1) */
        if (direction == 0) {
            /* RRQ: acknowledge the OACK */
            send_ack(0);
            state = TIKU_KITS_NET_TFTP_STATE_TRANSFERRING;
        } else {
            /* WRQ: send the first data block */
            supply_len = 0;
            if (tx_supply_cb != (tiku_kits_net_tftp_supply_cb_t)0) {
                supply_len = tx_supply_cb(
                    1,
                    blk_buf + TIKU_KITS_NET_TFTP_HDR_LEN,
                    TIKU_KITS_NET_TFTP_BLOCK_SIZE);
            }
            send_data(1, supply_len);
            block_num = 1;
            state = TIKU_KITS_NET_TFTP_STATE_SENDING;
            if (supply_len < TIKU_KITS_NET_TFTP_BLOCK_SIZE) {
                last_was_full = 0;
            }
        }
        break;

    case TIKU_KITS_NET_TFTP_EVT_ERROR_RECV:
        /* Error already set by the receive callback.
         * Unbind the port and leave state as ERROR. */
        tiku_kits_net_udp_unbind(TIKU_KITS_NET_TFTP_LOCAL_PORT);
        break;

    default:
        break;
    }

    return evt;
}

/*---------------------------------------------------------------------------*/
/* STATE QUERY                                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current transfer state.
 *
 * IDLE = no transfer, *_SENT = request in flight, TRANSFERRING =
 * receiving DATA blocks, SENDING = transmitting DATA blocks,
 * COMPLETE = transfer finished OK, ERROR = transfer failed.
 */
tiku_kits_net_tftp_state_t
tiku_kits_net_tftp_get_state(void)
{
    return state;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the TFTP error code from the last ERROR packet.
 *
 * Only meaningful when get_state() returns ERROR and the cause
 * was an ERROR packet from the server.  Returns one of the
 * TIKU_KITS_NET_TFTP_ERR_* codes (0-7 per RFC 1350).
 */
uint16_t
tiku_kits_net_tftp_get_error(void)
{
    return tftp_error_code;
}

/*---------------------------------------------------------------------------*/
/* TRANSFER CONTROL                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Abort the current transfer and return to IDLE.
 *
 * Unbinds the local port and resets state.  No ERROR packet is
 * sent to the server -- the server will time out on its own.
 */
int8_t
tiku_kits_net_tftp_abort(void)
{
    tiku_kits_net_udp_unbind(TIKU_KITS_NET_TFTP_LOCAL_PORT);
    state       = TIKU_KITS_NET_TFTP_STATE_IDLE;
    pending_evt = TIKU_KITS_NET_TFTP_EVT_NONE;
    return TIKU_KITS_NET_OK;
}
