/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_time_ntp.h - SNTP client (RFC 4330) for TikuOS
 *
 * Lightweight SNTP (Simple Network Time Protocol) client that
 * queries an NTP server over UDP to obtain wall-clock time.
 * Designed for ultra-low-power MSP430 microcontrollers with
 * static allocation and no heap usage.
 *
 * Architecture follows the same poll-based pattern as the TFTP
 * client: the UDP receive callback stores incoming data into
 * static state, and the application calls ntp_poll() from its
 * own context to drive state transitions.
 *
 * NTP timestamps use a 64-bit format: 32-bit seconds since
 * 1900-01-01 + 32-bit fractional seconds.  This client extracts
 * only the seconds field and converts it to Unix epoch (seconds
 * since 1970-01-01) by subtracting the NTP-Unix epoch offset.
 *
 * Typical usage:
 * @code
 *   static const uint8_t ntp_server[] = {216, 239, 35, 0};
 *   tiku_kits_time_ntp_init();
 *   tiku_kits_time_ntp_request(ntp_server);
 *   while (tiku_kits_time_ntp_get_state() < TIKU_KITS_TIME_NTP_STATE_DONE) {
 *       tiku_kits_time_ntp_poll();
 *   }
 *   if (tiku_kits_time_ntp_get_state() == TIKU_KITS_TIME_NTP_STATE_DONE) {
 *       tiku_kits_time_unix_t t;
 *       tiku_kits_time_ntp_get_time(&t);
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

#ifndef TIKU_KITS_TIME_NTP_H_
#define TIKU_KITS_TIME_NTP_H_

#include "../tiku_kits_time.h"

/*---------------------------------------------------------------------------*/
/* NTP PROTOCOL CONSTANTS                                                    */
/*---------------------------------------------------------------------------*/

/** NTP server port (RFC 5905). */
#define TIKU_KITS_TIME_NTP_PORT             123

/** NTP packet size (48 bytes, RFC 4330 Section 4). */
#define TIKU_KITS_TIME_NTP_PACKET_SIZE       48

/**
 * @brief Seconds between NTP epoch (1900-01-01) and Unix epoch (1970-01-01).
 *
 * 70 years * 365.25 days * 86400 seconds = 2208988800.
 * Used to convert NTP timestamps to Unix timestamps.
 */
#define TIKU_KITS_TIME_NTP_UNIX_OFFSET   2208988800UL

/*---------------------------------------------------------------------------*/
/* NTP PACKET FIELD OFFSETS (RFC 4330 Section 4)                             */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_TIME_NTP_OFF_LI_VN_MODE    0  /**< LI(2b)+VN(3b)+Mode(3b) */
#define TIKU_KITS_TIME_NTP_OFF_STRATUM       1  /**< Stratum level */
#define TIKU_KITS_TIME_NTP_OFF_POLL          2  /**< Poll interval */
#define TIKU_KITS_TIME_NTP_OFF_PRECISION     3  /**< Clock precision */
#define TIKU_KITS_TIME_NTP_OFF_ROOT_DELAY    4  /**< Root delay (32b) */
#define TIKU_KITS_TIME_NTP_OFF_ROOT_DISP     8  /**< Root dispersion (32b) */
#define TIKU_KITS_TIME_NTP_OFF_REF_ID       12  /**< Reference ID (32b) */
#define TIKU_KITS_TIME_NTP_OFF_REF_TS       16  /**< Reference timestamp (64b) */
#define TIKU_KITS_TIME_NTP_OFF_ORIG_TS      24  /**< Originate timestamp (64b) */
#define TIKU_KITS_TIME_NTP_OFF_RX_TS        32  /**< Receive timestamp (64b) */
#define TIKU_KITS_TIME_NTP_OFF_TX_TS        40  /**< Transmit timestamp (64b) */

/*---------------------------------------------------------------------------*/
/* NTP HEADER FIELD VALUES                                                   */
/*---------------------------------------------------------------------------*/

/** Leap Indicator: no warning (bits 7-6 = 0). */
#define TIKU_KITS_TIME_NTP_LI_NONE       0x00

/** Version Number: NTPv4 (bits 5-3 = 4). */
#define TIKU_KITS_TIME_NTP_VN_4          0x20

/** Mode: client (bits 2-0 = 3). */
#define TIKU_KITS_TIME_NTP_MODE_CLIENT   0x03

/** Mode: server (bits 2-0 = 4). */
#define TIKU_KITS_TIME_NTP_MODE_SERVER   0x04

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Local UDP port for NTP client.
 *
 * Must not conflict with other bound ports (TFTP uses 49152).
 */
#ifndef TIKU_KITS_TIME_NTP_LOCAL_PORT
#define TIKU_KITS_TIME_NTP_LOCAL_PORT    49200
#endif

/**
 * @brief Maximum number of retry attempts before timeout.
 */
#ifndef TIKU_KITS_TIME_NTP_MAX_RETRIES
#define TIKU_KITS_TIME_NTP_MAX_RETRIES       3
#endif

/*---------------------------------------------------------------------------*/
/* STATE TYPES                                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief NTP client state.
 *
 * Progresses from IDLE -> SENT -> DONE or ERROR.
 */
typedef enum {
    TIKU_KITS_TIME_NTP_STATE_IDLE,   /**< No request active */
    TIKU_KITS_TIME_NTP_STATE_SENT,   /**< Request sent, awaiting reply */
    TIKU_KITS_TIME_NTP_STATE_DONE,   /**< Reply received, time available */
    TIKU_KITS_TIME_NTP_STATE_ERROR   /**< Request failed */
} tiku_kits_time_ntp_state_t;

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the NTP client.
 *
 * Resets state to IDLE and clears stored time.  Safe to call
 * again to forcibly reset after a query.
 */
void tiku_kits_time_ntp_init(void);

/**
 * @brief Send an NTP request to a server.
 *
 * Constructs a 48-byte SNTP client request and sends it to the
 * specified server via UDP port 123.  Binds the local NTP port
 * to receive the reply.
 *
 * After calling request(), poll ntp_poll() until get_state()
 * returns DONE or ERROR.
 *
 * @param server_addr  NTP server IPv4 address (4 bytes, network order)
 * @return TIKU_KITS_TIME_OK on success,
 *         TIKU_KITS_TIME_ERR_NULL if server_addr is NULL,
 *         TIKU_KITS_TIME_ERR_PARAM if a request is already active,
 *         TIKU_KITS_TIME_ERR_NET if UDP send fails.
 */
int tiku_kits_time_ntp_request(const uint8_t *server_addr);

/**
 * @brief Poll for NTP reply and update state.
 *
 * Checks if a reply has been received by the UDP callback.
 * If so, parses the NTP response, extracts the transmit
 * timestamp, converts to Unix epoch, and transitions to DONE.
 *
 * Must be called from application context (not from inside
 * a UDP receive callback).
 *
 * @return TIKU_KITS_TIME_OK if reply processed,
 *         TIKU_KITS_TIME_ERR_NODATA if no reply yet,
 *         TIKU_KITS_TIME_ERR_TIMEOUT if max retries exhausted.
 */
int tiku_kits_time_ntp_poll(void);

/**
 * @brief Get the current client state.
 *
 * @return Current NTP state (IDLE, SENT, DONE, or ERROR).
 */
tiku_kits_time_ntp_state_t tiku_kits_time_ntp_get_state(void);

/**
 * @brief Get the received Unix timestamp.
 *
 * Only valid when get_state() returns DONE.
 *
 * @param ts  Output: Unix timestamp (seconds since 1970-01-01 UTC)
 * @return TIKU_KITS_TIME_OK on success,
 *         TIKU_KITS_TIME_ERR_NULL if ts is NULL,
 *         TIKU_KITS_TIME_ERR_NODATA if no time available.
 */
int tiku_kits_time_ntp_get_time(tiku_kits_time_unix_t *ts);

/**
 * @brief Get the received time as broken-down UTC.
 *
 * Convenience wrapper: calls get_time() and converts via
 * tiku_kits_time_to_tm().
 *
 * @param tm  Output: broken-down time structure
 * @return TIKU_KITS_TIME_OK on success,
 *         TIKU_KITS_TIME_ERR_NULL if tm is NULL,
 *         TIKU_KITS_TIME_ERR_NODATA if no time available.
 */
int tiku_kits_time_ntp_get_tm(tiku_kits_time_tm_t *tm);

/**
 * @brief Get the NTP stratum of the server's response.
 *
 * @return Stratum level (1 = primary, 2-15 = secondary), or 0
 *         if no response received.
 */
uint8_t tiku_kits_time_ntp_get_stratum(void);

/**
 * @brief Abort any active NTP request.
 *
 * Unbinds the NTP port and resets state to IDLE.
 *
 * @return TIKU_KITS_TIME_OK always.
 */
int tiku_kits_time_ntp_abort(void);

/*---------------------------------------------------------------------------*/
/* NTP PROCESS (for APP=net auto-start)                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief NTP protothread process.
 *
 * Sends an NTP request to the SLIP host after a boot delay and
 * prints the received time over UART.  Register alongside
 * tiku_kits_net_process via TIKU_AUTOSTART_PROCESSES.
 */
extern struct tiku_process tiku_kits_time_ntp_process;

#endif /* TIKU_KITS_TIME_NTP_H_ */
