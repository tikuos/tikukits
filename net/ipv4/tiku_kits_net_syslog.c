/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_syslog.c - Syslog client implementation (RFC 3164)
 *
 * Lightweight BSD Syslog client that sends log messages as UDP
 * datagrams to a remote syslog collector on port 514.  Messages
 * are fire-and-forget -- no acknowledgement or retransmission.
 *
 * The syslog payload is assembled in a small static buffer and
 * passed to udp_send(), which copies it into the shared net_buf.
 * This avoids direct manipulation of the shared buffer and keeps
 * the implementation simple.
 *
 * RAM budget:
 *   - Server IP:    4 bytes
 *   - Hostname:     9 bytes (8 chars + NUL)
 *   - Tag:          9 bytes (8 chars + NUL)
 *   - Facility:     1 byte
 *   - Server set:   1 byte
 *   - Total:       24 bytes (no additional buffers at rest)
 *
 * The message assembly buffer lives on the stack during send()
 * and is sized to TIKU_KITS_NET_UDP_MAX_PAYLOAD (100 bytes).
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

#include "tiku_kits_net_syslog.h"
#include "tiku_kits_net_udp.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* STATIC STATE                                                              */
/*---------------------------------------------------------------------------*/

/** Syslog server IPv4 address (network order). */
static uint8_t syslog_server[4];

/** Hostname string for the syslog header. */
static char syslog_hostname[TIKU_KITS_NET_SYSLOG_MAX_HOSTNAME + 1];

/** Tag (application name) for the syslog header. */
static char syslog_tag[TIKU_KITS_NET_SYSLOG_MAX_TAG + 1];

/** Current facility code (0-23). */
static uint8_t syslog_facility;

/** 1 if a server address has been configured, 0 otherwise. */
static uint8_t server_set;

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Write a decimal number into a buffer.
 *
 * Converts a uint8_t value (0-191) to its ASCII decimal
 * representation.  Returns the number of characters written
 * (1-3 digits, no NUL terminator).
 *
 * @param buf  Output buffer (must have room for at least 3 bytes)
 * @param val  Value to convert (0-255)
 * @return Number of characters written
 */
static uint16_t
u8_to_dec(char *buf, uint8_t val)
{
    uint16_t pos = 0;

    if (val >= 100) {
        buf[pos++] = (char)('0' + val / 100);
        val %= 100;
        buf[pos++] = (char)('0' + val / 10);
        val %= 10;
    } else if (val >= 10) {
        buf[pos++] = (char)('0' + val / 10);
        val %= 10;
    }
    buf[pos++] = (char)('0' + val);

    return pos;
}

/**
 * @brief Copy a string into a buffer, returning bytes written.
 *
 * Copies up to the NUL terminator (not including NUL).
 *
 * @param dst  Destination buffer
 * @param src  Source null-terminated string
 * @param max  Maximum bytes to copy
 * @return Number of bytes written
 */
static uint16_t
str_copy(char *dst, const char *src, uint16_t max)
{
    uint16_t i = 0;

    while (i < max && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    return i;
}

/*---------------------------------------------------------------------------*/
/* INITIALISATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset syslog client to default state.
 *
 * Clears the server address, sets facility to LOCAL0, hostname
 * to "tikuOS", and tag to "os".
 */
void
tiku_kits_net_syslog_init(void)
{
    memset(syslog_server, 0, sizeof(syslog_server));
    server_set = 0;
    syslog_facility = TIKU_KITS_NET_SYSLOG_DEFAULT_FACILITY;

    /* Default hostname and tag */
    memset(syslog_hostname, 0, sizeof(syslog_hostname));
    memset(syslog_tag, 0, sizeof(syslog_tag));

    syslog_hostname[0] = 't';
    syslog_hostname[1] = 'i';
    syslog_hostname[2] = 'k';
    syslog_hostname[3] = 'u';
    syslog_hostname[4] = 'O';
    syslog_hostname[5] = 'S';
    syslog_hostname[6] = '\0';

    syslog_tag[0] = 'o';
    syslog_tag[1] = 's';
    syslog_tag[2] = '\0';
}

/*---------------------------------------------------------------------------*/
/* CONFIGURATION API                                                         */
/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_syslog_set_server(const uint8_t *server_addr)
{
    if (server_addr == (void *)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    memcpy(syslog_server, server_addr, 4);
    server_set = 1;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_syslog_set_facility(uint8_t facility)
{
    if (facility > 23) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    syslog_facility = facility;
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_syslog_set_hostname(const char *hostname)
{
    uint16_t len;

    if (hostname == (void *)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    len = (uint16_t)strlen(hostname);
    if (len > TIKU_KITS_NET_SYSLOG_MAX_HOSTNAME) {
        len = TIKU_KITS_NET_SYSLOG_MAX_HOSTNAME;
    }

    memcpy(syslog_hostname, hostname, len);
    syslog_hostname[len] = '\0';
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/

int8_t
tiku_kits_net_syslog_set_tag(const char *tag)
{
    uint16_t len;

    if (tag == (void *)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    len = (uint16_t)strlen(tag);
    if (len > TIKU_KITS_NET_SYSLOG_MAX_TAG) {
        len = TIKU_KITS_NET_SYSLOG_MAX_TAG;
    }

    memcpy(syslog_tag, tag, len);
    syslog_tag[len] = '\0';
    return TIKU_KITS_NET_OK;
}

/*---------------------------------------------------------------------------*/
/* SEND                                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Assemble and send a BSD syslog message.
 *
 * Message format (RFC 3164):
 *   <PRI>HOSTNAME TAG: MSG
 *
 * The PRI field is the decimal encoding of (facility*8 + severity)
 * enclosed in angle brackets.  No timestamp is included -- the
 * receiving collector will add its own receive timestamp.
 *
 * Assembly is done in a stack-local buffer sized to the maximum
 * UDP payload.  If the formatted message exceeds this limit, the
 * MSG portion is truncated (the PRI, hostname, and tag are always
 * preserved).
 */
int8_t
tiku_kits_net_syslog_send(uint8_t severity, const char *msg)
{
    char payload[TIKU_KITS_NET_UDP_MAX_PAYLOAD];
    uint16_t pos = 0;
    uint16_t max = TIKU_KITS_NET_UDP_MAX_PAYLOAD;
    uint8_t pri;
    uint16_t msg_len;
    uint16_t avail;

    /* Validate: server must be configured */
    if (!server_set) {
        return TIKU_KITS_NET_ERR_PARAM;
    }

    /* Validate: message must not be NULL */
    if (msg == (void *)0) {
        return TIKU_KITS_NET_ERR_NULL;
    }

    /* Clamp severity to valid range (0-7) */
    if (severity > 7) {
        severity = 7;
    }

    /* Compute PRI = facility * 8 + severity */
    pri = TIKU_KITS_NET_SYSLOG_PRI(syslog_facility, severity);

    /* --- Assemble the syslog message ---
     *
     * Format: <PRI>HOSTNAME TAG: MSG
     *
     * Example: <134>tikuOS os: sensor read failed
     *          ^^^^^ ^^^^^^ ^^^ ^^^^^^^^^^^^^^^^^^^
     *          PRI   host   tag message
     */

    /* Opening angle bracket */
    payload[pos++] = '<';

    /* PRI value as decimal ASCII (1-3 digits) */
    pos += u8_to_dec(payload + pos, pri);

    /* Closing angle bracket */
    payload[pos++] = '>';

    /* Hostname + space separator */
    pos += str_copy(payload + pos, syslog_hostname,
                    (uint16_t)(max - pos));
    if (pos < max) {
        payload[pos++] = ' ';
    }

    /* Tag + ": " separator */
    pos += str_copy(payload + pos, syslog_tag,
                    (uint16_t)(max - pos));
    if (pos + 1 < max) {
        payload[pos++] = ':';
        payload[pos++] = ' ';
    }

    /* Message body (truncated if necessary to fit) */
    msg_len = (uint16_t)strlen(msg);
    avail = (pos < max) ? (uint16_t)(max - pos) : 0;
    if (msg_len > avail) {
        msg_len = avail;
    }
    if (msg_len > 0) {
        memcpy(payload + pos, msg, msg_len);
        pos += msg_len;
    }

    /* Send via UDP to the syslog server on port 514 */
    return tiku_kits_net_udp_send(
        syslog_server,
        TIKU_KITS_NET_SYSLOG_PORT,
        TIKU_KITS_NET_SYSLOG_LOCAL_PORT,
        (const uint8_t *)payload,
        pos);
}
