/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_time_ntp_process.c - NTP protothread process
 *
 * A self-contained protothread that sends an NTP request to the host
 * (172.16.7.1) after the network stack is initialised, polls for the
 * response, and prints the received UTC time over UART.
 *
 * Designed to run alongside tiku_kits_net_process in APP=net.
 * The NTP request is triggered by a 3-second boot delay to ensure
 * the network stack and SLIP link are fully operational.
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
#include "tiku_kits_time_ntp.h"

#ifdef PLATFORM_MSP430
#include "tiku.h"
#define NTP_PRINTF(...) TIKU_PRINTF(__VA_ARGS__)
#else
#include <stdio.h>
#define NTP_PRINTF(...) printf(__VA_ARGS__)
#endif

/*---------------------------------------------------------------------------*/
/* NTP SERVER ADDRESS                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Default NTP server: the SLIP host (172.16.7.1).
 *
 * In the TikuOS SLIP point-to-point configuration the host acts as
 * the gateway, so we query it directly.  The TikuBench ntp_test.py
 * script runs an NTP server on the host side.
 */
#ifndef TIKU_KITS_TIME_NTP_SERVER
#define TIKU_KITS_TIME_NTP_SERVER  {172, 16, 7, 1}
#endif

/** Delay before sending NTP request (seconds). */
#define NTP_BOOT_DELAY_SEC  3

/** Poll interval while waiting for NTP reply (ticks). */
#define NTP_POLL_TICKS      (TIKU_CLOCK_SECOND / 2)

/*---------------------------------------------------------------------------*/
/* PROCESS                                                                   */
/*---------------------------------------------------------------------------*/

TIKU_PROCESS(tiku_kits_time_ntp_process, "ntp");

static struct tiku_timer ntp_timer;

TIKU_PROCESS_THREAD(tiku_kits_time_ntp_process, ev, data)
{
    static const uint8_t server[] = TIKU_KITS_TIME_NTP_SERVER;
    static int rc;

    (void)data;

    TIKU_PROCESS_BEGIN();

    /* Wait for network stack to initialise */
    tiku_timer_set_event(&ntp_timer,
                          TIKU_CLOCK_SECOND * NTP_BOOT_DELAY_SEC);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /* Send NTP request */
    tiku_kits_time_ntp_init();
    rc = tiku_kits_time_ntp_request(server);

    if (rc != TIKU_KITS_TIME_OK) {
        NTP_PRINTF("[NTP] request failed: %d\n", rc);
        TIKU_PROCESS_EXIT();
    }

    NTP_PRINTF("[NTP] request sent to %d.%d.%d.%d\n",
               server[0], server[1], server[2], server[3]);

    /* Poll for response */
    while (tiku_kits_time_ntp_get_state() ==
           TIKU_KITS_TIME_NTP_STATE_SENT) {

        tiku_timer_set_event(&ntp_timer, NTP_POLL_TICKS);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

        tiku_kits_time_ntp_poll();
    }

    /* Report result */
    if (tiku_kits_time_ntp_get_state() ==
        TIKU_KITS_TIME_NTP_STATE_DONE) {

        tiku_kits_time_unix_t ts;
        tiku_kits_time_tm_t   tm;

        tiku_kits_time_ntp_get_time(&ts);
        tiku_kits_time_ntp_get_tm(&tm);

        NTP_PRINTF("[NTP] OK unix=%lu stratum=%u\n",
                   (unsigned long)ts,
                   tiku_kits_time_ntp_get_stratum());
        NTP_PRINTF("[NTP] %04u-%02u-%02u %02u:%02u:%02u UTC\n",
                   tm.year, tm.month, tm.day,
                   tm.hour, tm.minute, tm.second);
    } else {
        NTP_PRINTF("[NTP] FAILED (timeout or error)\n");
    }

    TIKU_PROCESS_END();
}
