/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_syslog_process.c - Syslog protothread process
 *
 * A self-contained protothread that sends test syslog messages to
 * the SLIP host (172.16.7.1) after the network stack is initialised.
 * Designed to run alongside tiku_kits_net_process in APP=net.
 *
 * The first message is sent after a 5-second boot delay.  Additional
 * boundary-test messages follow at 500 ms intervals to exercise the
 * C-side implementation paths (severity clamping, message truncation,
 * hostname/tag truncation).
 *
 * Messages sent:
 *   1. <134>tikuOS os: boot ok            (happy path)
 *   2. <135>tikuOS os: sev-clamp          (severity 255 -> clamped to 7)
 *   3. <134>tikuOS os: AAAAAA...          (message truncated to fit MTU)
 *   4. <134>ABCDEFGH os: host-trunc       (hostname 16 chars -> truncated to 8)
 *   5. <134>ABCDEFGH ZYXWVUTS: tag-trunc  (tag 10 chars -> truncated to 8)
 *
 * TikuBench test_syslog_send.py validates message 1.
 * TikuBench test_syslog_boundary.py validates messages 2-5.
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

#include <kernel/process/tiku_process.h>
#include <kernel/timers/tiku_timer.h>
#include <kernel/timers/tiku_clock.h>
#include "tiku_kits_net_syslog.h"

/*---------------------------------------------------------------------------*/
/* SYSLOG SERVER ADDRESS                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Default syslog server: the SLIP host (172.16.7.1).
 *
 * In the TikuOS SLIP point-to-point configuration the host acts as
 * the gateway and syslog collector.  The TikuBench syslog test
 * captures and validates the message on the host side.
 */
#ifndef TIKU_KITS_NET_SYSLOG_SERVER
#define TIKU_KITS_NET_SYSLOG_SERVER  {172, 16, 7, 1}
#endif

/** Delay before sending the first syslog message (seconds). */
#define SYSLOG_BOOT_DELAY_SEC  5

/** Delay between boundary test messages (ticks). */
#define SYSLOG_MSG_GAP_TICKS   (TIKU_CLOCK_SECOND / 2)

/*---------------------------------------------------------------------------*/
/* PROCESS                                                                   */
/*---------------------------------------------------------------------------*/

TIKU_PROCESS(tiku_kits_net_syslog_process, "syslog");

static struct tiku_timer syslog_timer;

TIKU_PROCESS_THREAD(tiku_kits_net_syslog_process, ev, data)
{
    static const uint8_t server[] = TIKU_KITS_NET_SYSLOG_SERVER;

    (void)data;

    TIKU_PROCESS_BEGIN();

    /* Wait for network stack to initialise and NTP to complete */
    tiku_timer_set_event(&syslog_timer,
                          TIKU_CLOCK_SECOND * SYSLOG_BOOT_DELAY_SEC);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /* Initialise syslog client and configure server */
    tiku_kits_net_syslog_init();
    tiku_kits_net_syslog_set_server(server);

    /* ----- Message 1: happy-path INFO message -----
     * PRI = LOCAL0(16) * 8 + INFO(6) = 134
     * Expected: "<134>tikuOS os: boot ok" */
    tiku_kits_net_syslog_send(TIKU_KITS_NET_SYSLOG_SEV_INFO,
                               "boot ok");

    /* ----- Message 2: severity clamping -----
     * Severity 255 exceeds 7; C code clamps to 7 (DEBUG).
     * PRI = LOCAL0(16) * 8 + DEBUG(7) = 135
     * Expected: "<135>tikuOS os: sev-clamp" */
    tiku_timer_set_event(&syslog_timer, SYSLOG_MSG_GAP_TICKS);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    tiku_kits_net_syslog_send(255, "sev-clamp");

    /* ----- Message 3: message truncation -----
     * A message longer than the available payload budget (~83 bytes
     * with default header) is truncated by syslog_send().
     * We use 120 'A' characters; the message will be cut to fit
     * within the 100-byte UDP max payload.
     * Expected: "<134>tikuOS os: AAAAAA..." (truncated) */
    tiku_timer_set_event(&syslog_timer, SYSLOG_MSG_GAP_TICKS);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    tiku_kits_net_syslog_send(TIKU_KITS_NET_SYSLOG_SEV_INFO,
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

    /* ----- Message 4: hostname truncation -----
     * Set a 16-char hostname; MAX_HOSTNAME=8 -> truncated to "ABCDEFGH".
     * Expected: "<134>ABCDEFGH os: host-trunc" */
    tiku_timer_set_event(&syslog_timer, SYSLOG_MSG_GAP_TICKS);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    tiku_kits_net_syslog_set_hostname("ABCDEFGHIJKLMNOP");
    tiku_kits_net_syslog_send(TIKU_KITS_NET_SYSLOG_SEV_INFO,
                               "host-trunc");

    /* ----- Message 5: tag truncation -----
     * Set a 10-char tag; MAX_TAG=8 -> truncated to "ZYXWVUTS".
     * Expected: "<134>ABCDEFGH ZYXWVUTS: tag-trunc" */
    tiku_timer_set_event(&syslog_timer, SYSLOG_MSG_GAP_TICKS);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    tiku_kits_net_syslog_set_tag("ZYXWVUTSRQ");
    tiku_kits_net_syslog_send(TIKU_KITS_NET_SYSLOG_SEV_INFO,
                               "tag-trunc");

    TIKU_PROCESS_END();
}
