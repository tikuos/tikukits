/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_time_ntp.c - SNTP client implementation
 *
 * Implements a lightweight SNTP client (RFC 4330) over UDP.
 * Uses the same poll-based architecture as the TFTP client:
 * the UDP receive callback stores response data into static
 * state variables, and ntp_poll() processes them from
 * application context.
 *
 * Memory footprint: ~64 bytes static RAM (no heap).
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

#include <string.h>
#include "tiku_kits_time_ntp.h"
#include "tikukits/net/tiku_kits_net.h"
#include "tikukits/net/ipv4/tiku_kits_net_udp.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL STATE                                                            */
/*---------------------------------------------------------------------------*/

/** Current client state. */
static tiku_kits_time_ntp_state_t ntp_state;

/** Received Unix timestamp (valid only in DONE state). */
static tiku_kits_time_unix_t ntp_unix_time;

/** Stratum from server response. */
static uint8_t ntp_stratum;

/** Flag set by UDP callback when a response arrives. */
static volatile uint8_t ntp_reply_ready;

/** Retry counter for timeout detection. */
static uint8_t ntp_retries;

/** Raw NTP transmit timestamp (seconds part, big-endian). */
static uint32_t ntp_rx_seconds;

/*---------------------------------------------------------------------------*/
/* BYTE-ORDER HELPER (32-bit)                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read a 32-bit big-endian value from a byte buffer.
 *
 * NTP timestamps are in network (big-endian) byte order.
 * MSP430 is little-endian, so we manually reassemble.
 */
static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           ((uint32_t)p[3]);
}

/*---------------------------------------------------------------------------*/
/* UDP RECEIVE CALLBACK                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief UDP callback for NTP responses.
 *
 * Called by the UDP layer when a packet arrives on our bound port.
 * Validates the response and extracts the transmit timestamp.
 * Does NOT call udp_send() -- just sets flags for poll().
 */
static void ntp_udp_recv(const uint8_t *src_addr,
                          uint16_t       src_port,
                          const uint8_t *payload,
                          uint16_t       payload_len)
{
    uint8_t mode;

    (void)src_addr;
    (void)src_port;

    /* Validate minimum NTP packet size */
    if (payload == (void *)0 ||
        payload_len < TIKU_KITS_TIME_NTP_PACKET_SIZE) {
        return;
    }

    /* Check mode field (bits 2-0) is server (4) */
    mode = payload[TIKU_KITS_TIME_NTP_OFF_LI_VN_MODE] & 0x07;
    if (mode != TIKU_KITS_TIME_NTP_MODE_SERVER) {
        return;
    }

    /* Extract stratum */
    ntp_stratum = payload[TIKU_KITS_TIME_NTP_OFF_STRATUM];

    /* Extract transmit timestamp seconds (bytes 40-43, big-endian) */
    ntp_rx_seconds = read_be32(
        &payload[TIKU_KITS_TIME_NTP_OFF_TX_TS]);

    /* Signal poll() that a valid reply arrived */
    ntp_reply_ready = 1;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

void tiku_kits_time_ntp_init(void)
{
    ntp_state = TIKU_KITS_TIME_NTP_STATE_IDLE;
    ntp_unix_time = 0;
    ntp_stratum = 0;
    ntp_reply_ready = 0;
    ntp_retries = 0;
    ntp_rx_seconds = 0;

    /* Ensure port is unbound from any previous session */
    tiku_kits_net_udp_unbind(TIKU_KITS_TIME_NTP_LOCAL_PORT);
}

/*---------------------------------------------------------------------------*/

int tiku_kits_time_ntp_request(const uint8_t *server_addr)
{
    uint8_t pkt[TIKU_KITS_TIME_NTP_PACKET_SIZE];
    int8_t rc;

    if (server_addr == (void *)0) {
        return TIKU_KITS_TIME_ERR_NULL;
    }

    if (ntp_state == TIKU_KITS_TIME_NTP_STATE_SENT) {
        return TIKU_KITS_TIME_ERR_PARAM;
    }

    /* Reset state for new request */
    ntp_reply_ready = 0;
    ntp_retries = 0;
    ntp_unix_time = 0;
    ntp_stratum = 0;

    /* Bind local port to receive response */
    rc = tiku_kits_net_udp_bind(TIKU_KITS_TIME_NTP_LOCAL_PORT,
                                 ntp_udp_recv);
    if (rc != TIKU_KITS_NET_OK) {
        ntp_state = TIKU_KITS_TIME_NTP_STATE_ERROR;
        return TIKU_KITS_TIME_ERR_NET;
    }

    /*
     * Build SNTP client request packet (48 bytes).
     *
     * Byte 0: LI=0 (no warning), VN=4 (NTPv4), Mode=3 (client)
     *         = 0x00 | 0x20 | 0x03 = 0x23
     * Bytes 1-47: all zeros (SNTP client sends minimal request).
     */
    memset(pkt, 0, TIKU_KITS_TIME_NTP_PACKET_SIZE);
    pkt[TIKU_KITS_TIME_NTP_OFF_LI_VN_MODE] =
        TIKU_KITS_TIME_NTP_LI_NONE |
        TIKU_KITS_TIME_NTP_VN_4 |
        TIKU_KITS_TIME_NTP_MODE_CLIENT;

    /* Send via UDP to NTP server port 123 */
    rc = tiku_kits_net_udp_send(
             server_addr,
             TIKU_KITS_TIME_NTP_PORT,
             TIKU_KITS_TIME_NTP_LOCAL_PORT,
             pkt,
             TIKU_KITS_TIME_NTP_PACKET_SIZE);

    if (rc != TIKU_KITS_NET_OK) {
        tiku_kits_net_udp_unbind(TIKU_KITS_TIME_NTP_LOCAL_PORT);
        ntp_state = TIKU_KITS_TIME_NTP_STATE_ERROR;
        return TIKU_KITS_TIME_ERR_NET;
    }

    ntp_state = TIKU_KITS_TIME_NTP_STATE_SENT;
    return TIKU_KITS_TIME_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_time_ntp_poll(void)
{
    if (ntp_state != TIKU_KITS_TIME_NTP_STATE_SENT) {
        return TIKU_KITS_TIME_ERR_NODATA;
    }

    if (ntp_reply_ready) {
        /*
         * Convert NTP timestamp to Unix timestamp.
         *
         * NTP epoch = 1900-01-01 00:00:00 UTC
         * Unix epoch = 1970-01-01 00:00:00 UTC
         * Offset = 2208988800 seconds (70 years)
         *
         * Guard against pre-1970 timestamps (shouldn't happen with
         * modern NTP servers, but defensive coding for embedded).
         */
        if (ntp_rx_seconds >= TIKU_KITS_TIME_NTP_UNIX_OFFSET) {
            ntp_unix_time = ntp_rx_seconds -
                            TIKU_KITS_TIME_NTP_UNIX_OFFSET;
        } else {
            ntp_unix_time = 0;
        }

        /* Clean up */
        tiku_kits_net_udp_unbind(TIKU_KITS_TIME_NTP_LOCAL_PORT);
        ntp_state = TIKU_KITS_TIME_NTP_STATE_DONE;
        ntp_reply_ready = 0;
        return TIKU_KITS_TIME_OK;
    }

    /* No reply yet -- check retry count for timeout.
     * Each poll() call without a reply counts as one retry.
     * The caller is expected to poll periodically (e.g. every
     * 1 second), so MAX_RETRIES ~ seconds to wait. */
    ntp_retries++;
    if (ntp_retries >= TIKU_KITS_TIME_NTP_MAX_RETRIES) {
        tiku_kits_net_udp_unbind(TIKU_KITS_TIME_NTP_LOCAL_PORT);
        ntp_state = TIKU_KITS_TIME_NTP_STATE_ERROR;
        return TIKU_KITS_TIME_ERR_TIMEOUT;
    }

    return TIKU_KITS_TIME_ERR_NODATA;
}

/*---------------------------------------------------------------------------*/

tiku_kits_time_ntp_state_t tiku_kits_time_ntp_get_state(void)
{
    return ntp_state;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_time_ntp_get_time(tiku_kits_time_unix_t *ts)
{
    if (ts == (void *)0) {
        return TIKU_KITS_TIME_ERR_NULL;
    }

    if (ntp_state != TIKU_KITS_TIME_NTP_STATE_DONE) {
        return TIKU_KITS_TIME_ERR_NODATA;
    }

    *ts = ntp_unix_time;
    return TIKU_KITS_TIME_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_time_ntp_get_tm(tiku_kits_time_tm_t *tm)
{
    tiku_kits_time_unix_t ts;
    int rc;

    if (tm == (void *)0) {
        return TIKU_KITS_TIME_ERR_NULL;
    }

    rc = tiku_kits_time_ntp_get_time(&ts);
    if (rc != TIKU_KITS_TIME_OK) {
        return rc;
    }

    return tiku_kits_time_to_tm(ts, tm);
}

/*---------------------------------------------------------------------------*/

uint8_t tiku_kits_time_ntp_get_stratum(void)
{
    return ntp_stratum;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_time_ntp_abort(void)
{
    tiku_kits_net_udp_unbind(TIKU_KITS_TIME_NTP_LOCAL_PORT);
    ntp_state = TIKU_KITS_TIME_NTP_STATE_IDLE;
    ntp_reply_ready = 0;
    ntp_retries = 0;
    return TIKU_KITS_TIME_OK;
}
