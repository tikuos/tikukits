/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_coap_process.c - CoAP server demo protothread
 *
 * Registers test resources ("/test", "/led") and polls the CoAP
 * module periodically.  Designed for integration testing with
 * TikuBench.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_net_coap.h"
#include <kernel/process/tiku_process.h>
#include <kernel/timers/tiku_timer.h>
#include <kernel/timers/tiku_clock.h>
#include <interfaces/led/tiku_led.h>    /* board-independent LED API */
#include <string.h>

/*---------------------------------------------------------------------------*/
/* RESOURCE HANDLERS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * GET /test -- returns "TikuOS" as text/plain.
 * PUT /test -- echoes the request payload back as 2.04 Changed.
 */
static void
test_handler(const tiku_kits_net_coap_msg_t *req,
             tiku_kits_net_coap_resp_t      *resp)
{
    static const uint8_t hello[] = "TikuOS";

    switch (req->code) {
    case TIKU_KITS_NET_COAP_METHOD_GET:
        resp->code           = TIKU_KITS_NET_COAP_RESP_CONTENT;
        resp->payload        = hello;
        resp->payload_len    = 6;  /* no NUL */
        resp->content_format = TIKU_KITS_NET_COAP_FMT_TEXT;
        break;

    case TIKU_KITS_NET_COAP_METHOD_PUT:
    case TIKU_KITS_NET_COAP_METHOD_POST:
        resp->code           = TIKU_KITS_NET_COAP_RESP_CHANGED;
        resp->payload        = req->payload;
        resp->payload_len    = req->payload_len;
        resp->content_format = req->content_format;
        break;

    case TIKU_KITS_NET_COAP_METHOD_DELETE:
        resp->code = TIKU_KITS_NET_COAP_RESP_DELETED;
        break;

    default:
        resp->code = TIKU_KITS_NET_COAP_RESP_METHOD_NA;
        break;
    }
}

/**
 * GET /led -- returns LED1 state as "0" or "1".
 * PUT /led -- sets LED1: payload "1" = on, "0" = off.
 */
static void
led_handler(const tiku_kits_net_coap_msg_t *req,
            tiku_kits_net_coap_resp_t      *resp)
{
    static uint8_t led_state = '0';

    if (req->code == TIKU_KITS_NET_COAP_METHOD_GET) {
        resp->code           = TIKU_KITS_NET_COAP_RESP_CONTENT;
        resp->payload        = &led_state;
        resp->payload_len    = 1;
        resp->content_format = TIKU_KITS_NET_COAP_FMT_TEXT;
    } else if (req->code == TIKU_KITS_NET_COAP_METHOD_PUT) {
        if (req->payload_len >= 1) {
            if (req->payload[0] == '1') {
                tiku_led_on(0);
                led_state = '1';
            } else {
                tiku_led_off(0);
                led_state = '0';
            }
        }
        resp->code           = TIKU_KITS_NET_COAP_RESP_CHANGED;
        resp->payload        = &led_state;
        resp->payload_len    = 1;
        resp->content_format = TIKU_KITS_NET_COAP_FMT_TEXT;
    } else {
        resp->code = TIKU_KITS_NET_COAP_RESP_METHOD_NA;
    }
}

/*---------------------------------------------------------------------------*/
/* COAP PROCESS                                                              */
/*---------------------------------------------------------------------------*/

TIKU_PROCESS(tiku_kits_net_coap_process, "coap");

/** Boot delay before starting CoAP (wait for net process).
 *
 * The net process initialises SLIP and UDP immediately at boot
 * (no delay), so CoAP only needs to wait long enough for the
 * first net-process poll-loop iteration to run (~100 ms).  One
 * second provides a comfortable margin while keeping the CoAP
 * server available quickly after reset -- important for test
 * frameworks that open the serial port (triggering an eZ-FET
 * reset) and then send a request within a few seconds.
 */
#define COAP_BOOT_DELAY_SEC  1

/** Poll interval for CoAP (500 ms). */
/* Poll cadence.  recv_cb (in udp_input, under the net-buf re-entrancy
 * guard) only sets rx_pending; the reply is built here on the next poll,
 * so this bounds worst-case CoAP reply latency.  SECOND/8 = 16 ticks
 * (~125 ms at the 128 Hz kernel tick), down from SECOND/2 (~500 ms) --
 * a 4x tighter reply without measurable cost: the poll body is a cheap
 * rx_pending check plus the CON retransmit timer, and the process rides
 * the always-running kernel tick, so a shorter interval adds no wakes. */
#define COAP_POLL_TICKS      (TIKU_CLOCK_SECOND / 8)

TIKU_PROCESS_THREAD(tiku_kits_net_coap_process, ev, data)
{
    static struct tiku_timer coap_timer;

    (void)data;

    TIKU_PROCESS_BEGIN();

    /* Wait for the net process to initialise SLIP and UDP */
    tiku_timer_set_event(&coap_timer,
                          TIKU_CLOCK_SECOND * COAP_BOOT_DELAY_SEC);
    TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

    /* Initialise CoAP module (binds UDP port 5683) */
    tiku_kits_net_coap_init();

    /* Register test resources */
    tiku_kits_net_coap_resource_register("/test", test_handler);
    tiku_kits_net_coap_resource_register("/led",  led_handler);

    /* Poll loop */
    while (1) {
        tiku_timer_set_event(&coap_timer, COAP_POLL_TICKS);
        TIKU_PROCESS_WAIT_EVENT_UNTIL(ev == TIKU_EVENT_TIMER);

        tiku_kits_net_coap_poll();
    }

    TIKU_PROCESS_END();
}
