/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_syslog.h - Syslog client (RFC 3164) over UDP
 *
 * Lightweight BSD Syslog client that sends log messages to a remote
 * syslog collector via UDP port 514.  Designed for ultra-low-power
 * embedded targets with severe memory constraints: all state is
 * statically allocated, no heap is used, and the message is
 * assembled directly in the shared net_buf via udp_send().
 *
 * Syslog is a fire-and-forget protocol -- messages are sent as
 * single UDP datagrams with no acknowledgement or retransmission.
 * This makes it ideal for embedded systems that need remote logging
 * without the complexity of a connection-oriented protocol.
 *
 * Message format (RFC 3164 Section 4):
 *   <PRI>HOSTNAME TAG: MSG
 *
 * Where PRI = facility * 8 + severity.  The timestamp is omitted
 * (the collector will add a receive timestamp) to save payload
 * space on the constrained MTU.
 *
 * With the default 128-byte MTU, the maximum syslog payload is
 * 100 bytes (MTU - IPv4(20) - UDP(8)).  After the PRI header,
 * hostname, and tag prefix, approximately 70-80 bytes remain for
 * the actual log message.
 *
 * Typical usage:
 * @code
 *   static const uint8_t log_server[] = {172, 16, 7, 1};
 *   tiku_kits_net_syslog_init();
 *   tiku_kits_net_syslog_set_server(log_server);
 *   tiku_kits_net_syslog_send(TIKU_KITS_NET_SYSLOG_SEV_INFO,
 *                              "system booted");
 *   tiku_kits_net_syslog_send(TIKU_KITS_NET_SYSLOG_SEV_ERR,
 *                              "sensor read failed");
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

#ifndef TIKU_KITS_NET_SYSLOG_H_
#define TIKU_KITS_NET_SYSLOG_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"

/*---------------------------------------------------------------------------*/
/* SYSLOG SEVERITY LEVELS (RFC 3164 Section 4.1.1)                           */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_SYSLOG_SEV Syslog Severity Levels
 * @brief Numerical severity codes (0 = most severe, 7 = least).
 *
 * These values occupy the low 3 bits of the PRI field.
 * @{
 */
#define TIKU_KITS_NET_SYSLOG_SEV_EMERG      0  /**< System is unusable */
#define TIKU_KITS_NET_SYSLOG_SEV_ALERT      1  /**< Action must be taken immediately */
#define TIKU_KITS_NET_SYSLOG_SEV_CRIT       2  /**< Critical conditions */
#define TIKU_KITS_NET_SYSLOG_SEV_ERR        3  /**< Error conditions */
#define TIKU_KITS_NET_SYSLOG_SEV_WARNING    4  /**< Warning conditions */
#define TIKU_KITS_NET_SYSLOG_SEV_NOTICE     5  /**< Normal but significant */
#define TIKU_KITS_NET_SYSLOG_SEV_INFO       6  /**< Informational messages */
#define TIKU_KITS_NET_SYSLOG_SEV_DEBUG      7  /**< Debug-level messages */
/** @} */

/*---------------------------------------------------------------------------*/
/* SYSLOG FACILITY CODES (RFC 3164 Section 4.1.1)                            */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_NET_SYSLOG_FAC Syslog Facility Codes
 * @brief Numerical facility codes identifying the source subsystem.
 *
 * These values are multiplied by 8 and combined with the severity
 * to form the PRI field.  LOCAL0-LOCAL7 are reserved for custom
 * use and are the most appropriate for embedded devices.
 * @{
 */
#define TIKU_KITS_NET_SYSLOG_FAC_KERN       0  /**< Kernel messages */
#define TIKU_KITS_NET_SYSLOG_FAC_USER       1  /**< User-level messages */
#define TIKU_KITS_NET_SYSLOG_FAC_DAEMON     3  /**< System daemons */
#define TIKU_KITS_NET_SYSLOG_FAC_LOCAL0    16  /**< Local use 0 */
#define TIKU_KITS_NET_SYSLOG_FAC_LOCAL1    17  /**< Local use 1 */
#define TIKU_KITS_NET_SYSLOG_FAC_LOCAL2    18  /**< Local use 2 */
#define TIKU_KITS_NET_SYSLOG_FAC_LOCAL3    19  /**< Local use 3 */
#define TIKU_KITS_NET_SYSLOG_FAC_LOCAL4    20  /**< Local use 4 */
#define TIKU_KITS_NET_SYSLOG_FAC_LOCAL5    21  /**< Local use 5 */
#define TIKU_KITS_NET_SYSLOG_FAC_LOCAL6    22  /**< Local use 6 */
#define TIKU_KITS_NET_SYSLOG_FAC_LOCAL7    23  /**< Local use 7 */
/** @} */

/*---------------------------------------------------------------------------*/
/* CONFIGURATION (compile-time overrideable)                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Syslog server port (RFC 3164).
 *
 * Standard syslog collectors listen on UDP port 514.
 */
#define TIKU_KITS_NET_SYSLOG_PORT           514

/**
 * @brief Local UDP source port for syslog messages.
 *
 * Ephemeral port used as the source in outgoing syslog datagrams.
 * Must not conflict with other bound ports (TFTP=49152, NTP=49200).
 */
#ifndef TIKU_KITS_NET_SYSLOG_LOCAL_PORT
#define TIKU_KITS_NET_SYSLOG_LOCAL_PORT     49300
#endif

/**
 * @brief Default syslog facility.
 *
 * LOCAL0 (16) is the conventional choice for custom embedded
 * devices.  Override at compile time if needed.
 */
#ifndef TIKU_KITS_NET_SYSLOG_DEFAULT_FACILITY
#define TIKU_KITS_NET_SYSLOG_DEFAULT_FACILITY \
    TIKU_KITS_NET_SYSLOG_FAC_LOCAL0
#endif

/**
 * @brief Maximum hostname length (excluding NUL terminator).
 *
 * RFC 3164 recommends hostnames be short.  8 characters is
 * sufficient for embedded device identifiers (e.g. "tikuOS").
 */
#ifndef TIKU_KITS_NET_SYSLOG_MAX_HOSTNAME
#define TIKU_KITS_NET_SYSLOG_MAX_HOSTNAME   8
#endif

/**
 * @brief Maximum tag length (excluding NUL terminator).
 *
 * The tag identifies the application or subsystem generating the
 * log message (e.g. "kern", "app", "sensor").  RFC 3164
 * recommends tags be 32 characters or fewer; we use a tighter
 * limit to conserve payload space.
 */
#ifndef TIKU_KITS_NET_SYSLOG_MAX_TAG
#define TIKU_KITS_NET_SYSLOG_MAX_TAG        8
#endif

/**
 * @brief Compute the PRI value from facility and severity.
 *
 * PRI = facility * 8 + severity (RFC 3164 Section 4.1.1).
 * The result is encoded as a decimal number inside angle brackets
 * at the start of the syslog message (e.g. "<134>" for
 * LOCAL0.INFO = 16*8+6 = 134).
 *
 * @param fac  Facility code (0-23)
 * @param sev  Severity code (0-7)
 * @return PRI value (0-191)
 */
#define TIKU_KITS_NET_SYSLOG_PRI(fac, sev) \
    ((uint8_t)((fac) * 8 + (sev)))

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialise the syslog client.
 *
 * Resets the server address, sets the facility to the default
 * (LOCAL0), and sets the hostname to "tikuOS" and tag to "os".
 * Call once during system startup.
 */
void tiku_kits_net_syslog_init(void);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION API                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the syslog server address.
 *
 * All subsequent syslog_send() calls will direct messages to
 * this server.  Must be called before send() -- sending without
 * a configured server returns TIKU_KITS_NET_ERR_PARAM.
 *
 * @param server_addr  Server IPv4 address (4 bytes, network order)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NULL if server_addr is NULL.
 */
int8_t tiku_kits_net_syslog_set_server(const uint8_t *server_addr);

/**
 * @brief Set the syslog facility code.
 *
 * Changes the facility used in the PRI field of subsequent
 * messages.  Valid range: 0-23.
 *
 * @param facility  Facility code (use TIKU_KITS_NET_SYSLOG_FAC_* constants)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if facility > 23.
 */
int8_t tiku_kits_net_syslog_set_facility(uint8_t facility);

/**
 * @brief Set the hostname field in syslog messages.
 *
 * The hostname identifies the sending device in the syslog
 * message header.  Truncated to TIKU_KITS_NET_SYSLOG_MAX_HOSTNAME
 * characters if longer.
 *
 * @param hostname  Null-terminated hostname string
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NULL if hostname is NULL.
 */
int8_t tiku_kits_net_syslog_set_hostname(const char *hostname);

/**
 * @brief Set the tag (application name) in syslog messages.
 *
 * The tag identifies the subsystem or process generating the
 * log message.  Appears before the colon in the MSG part
 * (e.g. "kern: watchdog reset").  Truncated to
 * TIKU_KITS_NET_SYSLOG_MAX_TAG characters if longer.
 *
 * @param tag  Null-terminated tag string
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NULL if tag is NULL.
 */
int8_t tiku_kits_net_syslog_set_tag(const char *tag);

/*---------------------------------------------------------------------------*/
/* SEND API                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Send a syslog message.
 *
 * Assembles a BSD syslog message (RFC 3164) in the format:
 *   <PRI>HOSTNAME TAG: MSG
 *
 * and sends it as a single UDP datagram to the configured server
 * on port 514.  The message is fire-and-forget -- no ACK is
 * expected and no retransmission is attempted.
 *
 * The PRI field is computed from the configured facility and the
 * provided severity.  The timestamp is omitted to save payload
 * space; the syslog collector will add its own receive timestamp.
 *
 * The total formatted message must fit within the UDP max payload
 * (100 bytes with default MTU).  If the message is too long, it
 * is truncated to fit.
 *
 * @warning Must NOT be called from inside a UDP receive callback
 * (same constraint as udp_send).
 *
 * @param severity  Severity level (use TIKU_KITS_NET_SYSLOG_SEV_* constants,
 *                  0-7; values > 7 are clamped to 7)
 * @param msg       Null-terminated message string (may be NULL for
 *                  a header-only message)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_PARAM if no server is configured,
 *         TIKU_KITS_NET_ERR_NULL if msg is NULL,
 *         TIKU_KITS_NET_ERR_NOLINK if no link backend is set,
 *         or any error returned by udp_send().
 */
int8_t tiku_kits_net_syslog_send(uint8_t severity, const char *msg);

/*---------------------------------------------------------------------------*/
/* SYSLOG PROCESS (for APP=net auto-start)                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Syslog protothread process.
 *
 * Sends a test syslog message to the SLIP host after a boot delay.
 * Register alongside tiku_kits_net_process via
 * TIKU_AUTOSTART_PROCESSES.
 */
extern struct tiku_process tiku_kits_net_syslog_process;

#endif /* TIKU_KITS_NET_SYSLOG_H_ */
