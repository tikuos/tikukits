/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: State Machine Table
 *
 * Demonstrates table-driven state machine usage for embedded tasks:
 *   1. LED blink    -- simple ON/OFF toggle with button event
 *   2. Comm protocol -- IDLE -> SENDING -> WAITING -> DONE
 *   3. Power mgmt   -- ACTIVE -> SLEEP -> DEEP_SLEEP with events
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/sm/tiku_kits_ds_sm.h"

/*---------------------------------------------------------------------------*/
/* Example 1: LED blink state machine                                        */
/*---------------------------------------------------------------------------*/

/* States */
#define LED_STATE_OFF  0
#define LED_STATE_ON   1
#define LED_N_STATES   2

/* Events */
#define LED_EVT_BUTTON 0
#define LED_N_EVENTS   1

/* Action callbacks */
static void action_led_on(void)
{
    printf("    [action] LED ON  (P1OUT |= BIT0)\n");
}

static void action_led_off(void)
{
    printf("    [action] LED OFF (P1OUT &= ~BIT0)\n");
}

static void example_led_blink(void)
{
    struct tiku_kits_ds_sm sm;

    printf("--- LED Blink State Machine ---\n");

    tiku_kits_ds_sm_init(&sm, LED_N_STATES, LED_N_EVENTS);

    /* OFF --button--> ON (turn LED on) */
    tiku_kits_ds_sm_set_transition(&sm,
        LED_STATE_OFF, LED_EVT_BUTTON, LED_STATE_ON,
        action_led_on);

    /* ON  --button--> OFF (turn LED off) */
    tiku_kits_ds_sm_set_transition(&sm,
        LED_STATE_ON, LED_EVT_BUTTON, LED_STATE_OFF,
        action_led_off);

    printf("  State: %u (OFF)\n",
           tiku_kits_ds_sm_get_state(&sm));

    /* Simulate button presses */
    printf("  Button press #1:\n");
    tiku_kits_ds_sm_process(&sm, LED_EVT_BUTTON);
    printf("  State: %u (ON)\n",
           tiku_kits_ds_sm_get_state(&sm));

    printf("  Button press #2:\n");
    tiku_kits_ds_sm_process(&sm, LED_EVT_BUTTON);
    printf("  State: %u (OFF)\n",
           tiku_kits_ds_sm_get_state(&sm));

    printf("  Button press #3:\n");
    tiku_kits_ds_sm_process(&sm, LED_EVT_BUTTON);
    printf("  State: %u (ON)\n",
           tiku_kits_ds_sm_get_state(&sm));
}

/*---------------------------------------------------------------------------*/
/* Example 2: Communication protocol state machine                           */
/*---------------------------------------------------------------------------*/

/* States */
#define COMM_STATE_IDLE     0
#define COMM_STATE_SENDING  1
#define COMM_STATE_WAITING  2
#define COMM_STATE_DONE     3
#define COMM_N_STATES       4

/* Events */
#define COMM_EVT_SEND       0
#define COMM_EVT_TX_DONE    1
#define COMM_EVT_ACK        2
#define COMM_EVT_RESET      3
#define COMM_N_EVENTS       4

static void action_start_tx(void)
{
    printf("    [action] Starting UART transmission\n");
}

static void action_wait_ack(void)
{
    printf("    [action] TX complete, waiting for ACK\n");
}

static void action_ack_received(void)
{
    printf("    [action] ACK received, transfer done\n");
}

static void action_comm_reset(void)
{
    printf("    [action] Protocol reset to IDLE\n");
}

static void example_comm_protocol(void)
{
    struct tiku_kits_ds_sm sm;
    int rc;

    printf("--- Communication Protocol ---\n");

    tiku_kits_ds_sm_init(&sm, COMM_N_STATES, COMM_N_EVENTS);

    /* IDLE   --send-->    SENDING  */
    tiku_kits_ds_sm_set_transition(&sm,
        COMM_STATE_IDLE, COMM_EVT_SEND, COMM_STATE_SENDING,
        action_start_tx);

    /* SENDING --tx_done--> WAITING  */
    tiku_kits_ds_sm_set_transition(&sm,
        COMM_STATE_SENDING, COMM_EVT_TX_DONE,
        COMM_STATE_WAITING, action_wait_ack);

    /* WAITING --ack-->     DONE     */
    tiku_kits_ds_sm_set_transition(&sm,
        COMM_STATE_WAITING, COMM_EVT_ACK, COMM_STATE_DONE,
        action_ack_received);

    /* DONE   --reset-->   IDLE     */
    tiku_kits_ds_sm_set_transition(&sm,
        COMM_STATE_DONE, COMM_EVT_RESET, COMM_STATE_IDLE,
        action_comm_reset);

    /* Walk through a complete transfer cycle */
    printf("  State: IDLE (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    tiku_kits_ds_sm_process(&sm, COMM_EVT_SEND);
    printf("  State: SENDING (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    tiku_kits_ds_sm_process(&sm, COMM_EVT_TX_DONE);
    printf("  State: WAITING (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    tiku_kits_ds_sm_process(&sm, COMM_EVT_ACK);
    printf("  State: DONE (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    /* Test undefined transition while in DONE */
    rc = tiku_kits_ds_sm_process(&sm, COMM_EVT_SEND);
    printf("  Send in DONE state: %s\n",
           (rc == TIKU_KITS_DS_ERR_PARAM) ? "rejected" : "OK");

    tiku_kits_ds_sm_process(&sm, COMM_EVT_RESET);
    printf("  State: IDLE (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));
}

/*---------------------------------------------------------------------------*/
/* Example 3: Power management state machine                                 */
/*---------------------------------------------------------------------------*/

/* States */
#define PWR_STATE_ACTIVE     0
#define PWR_STATE_SLEEP      1
#define PWR_STATE_DEEP_SLEEP 2
#define PWR_N_STATES         3

/* Events */
#define PWR_EVT_TIMEOUT      0
#define PWR_EVT_WAKEUP       1
#define PWR_N_EVENTS         2

static void action_enter_sleep(void)
{
    printf("    [action] Entering LPM3 "
           "(disable CPU, keep ACLK)\n");
}

static void action_enter_deep_sleep(void)
{
    printf("    [action] Entering LPM4 "
           "(all clocks off)\n");
}

static void action_wake_active(void)
{
    printf("    [action] Waking up to ACTIVE "
           "(restore MCLK)\n");
}

static void example_power_mgmt(void)
{
    struct tiku_kits_ds_sm sm;

    printf("--- Power Management ---\n");

    tiku_kits_ds_sm_init(&sm, PWR_N_STATES, PWR_N_EVENTS);

    /* ACTIVE      --timeout--> SLEEP       */
    tiku_kits_ds_sm_set_transition(&sm,
        PWR_STATE_ACTIVE, PWR_EVT_TIMEOUT, PWR_STATE_SLEEP,
        action_enter_sleep);

    /* SLEEP       --timeout--> DEEP_SLEEP  */
    tiku_kits_ds_sm_set_transition(&sm,
        PWR_STATE_SLEEP, PWR_EVT_TIMEOUT,
        PWR_STATE_DEEP_SLEEP, action_enter_deep_sleep);

    /* SLEEP       --wakeup-->  ACTIVE      */
    tiku_kits_ds_sm_set_transition(&sm,
        PWR_STATE_SLEEP, PWR_EVT_WAKEUP, PWR_STATE_ACTIVE,
        action_wake_active);

    /* DEEP_SLEEP  --wakeup-->  ACTIVE      */
    tiku_kits_ds_sm_set_transition(&sm,
        PWR_STATE_DEEP_SLEEP, PWR_EVT_WAKEUP,
        PWR_STATE_ACTIVE, action_wake_active);

    printf("  State: ACTIVE (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    /* Idle timeout -> SLEEP */
    printf("  Idle timeout:\n");
    tiku_kits_ds_sm_process(&sm, PWR_EVT_TIMEOUT);
    printf("  State: SLEEP (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    /* Another timeout -> DEEP_SLEEP */
    printf("  Extended idle:\n");
    tiku_kits_ds_sm_process(&sm, PWR_EVT_TIMEOUT);
    printf("  State: DEEP_SLEEP (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    /* External interrupt wakes up */
    printf("  GPIO interrupt:\n");
    tiku_kits_ds_sm_process(&sm, PWR_EVT_WAKEUP);
    printf("  State: ACTIVE (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    /* Timeout -> SLEEP, then immediate wakeup */
    printf("  Brief sleep:\n");
    tiku_kits_ds_sm_process(&sm, PWR_EVT_TIMEOUT);
    printf("  State: SLEEP (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));

    printf("  Quick wakeup:\n");
    tiku_kits_ds_sm_process(&sm, PWR_EVT_WAKEUP);
    printf("  State: ACTIVE (%u)\n",
           tiku_kits_ds_sm_get_state(&sm));
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_sm_run(void)
{
    printf("=== TikuKits State Machine Examples ===\n\n");

    example_led_blink();
    printf("\n");

    example_comm_protocol();
    printf("\n");

    example_power_mgmt();
    printf("\n");
}
