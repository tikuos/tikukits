/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: FIFO Queue
 *
 * Demonstrates the ring-buffer FIFO queue for embedded use cases:
 *   - Sensor event queue (ISR produces, main loop consumes)
 *   - Command buffer for serial protocol processing
 *   - Message passing between cooperative tasks
 *
 * All queues use static storage (no heap). Maximum capacity is
 * controlled by TIKU_KITS_DS_QUEUE_MAX_SIZE (default 16).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/queue/tiku_kits_ds_queue.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Sensor event queue                                             */
/*---------------------------------------------------------------------------*/

/**
 * Simulates an ISR-to-main-loop pattern. The ISR (simulated) enqueues
 * sensor readings, and the main loop consumes them in FIFO order.
 */
static void example_sensor_event_queue(void)
{
    struct tiku_kits_ds_queue event_q;
    tiku_kits_ds_elem_t reading;
    int rc;

    printf("--- Sensor Event Queue ---\n");

    tiku_kits_ds_queue_init(&event_q, 8);

    /* Simulate ISR posting sensor readings */
    printf("  ISR: posting readings 2501, 2498, 2510, 2495\n");
    tiku_kits_ds_queue_enqueue(&event_q, 2501);
    tiku_kits_ds_queue_enqueue(&event_q, 2498);
    tiku_kits_ds_queue_enqueue(&event_q, 2510);
    tiku_kits_ds_queue_enqueue(&event_q, 2495);

    printf("  Queue size: %u\n", tiku_kits_ds_queue_size(&event_q));

    /* Main loop drains the queue */
    printf("  Main loop processing:\n");
    while (!tiku_kits_ds_queue_empty(&event_q)) {
        rc = tiku_kits_ds_queue_dequeue(&event_q, &reading);
        if (rc == TIKU_KITS_DS_OK) {
            printf("    Processed reading: %ld\n", (long)reading);
        }
    }

    printf("  Queue empty: %s\n",
           tiku_kits_ds_queue_empty(&event_q) ? "yes" : "no");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Command buffer                                                 */
/*---------------------------------------------------------------------------*/

/**
 * Simulates a serial command buffer. Commands arrive asynchronously
 * and are queued for sequential processing. Demonstrates full-queue
 * handling.
 */
static void example_command_buffer(void)
{
    struct tiku_kits_ds_queue cmd_q;
    tiku_kits_ds_elem_t cmd;
    int rc;

    /* Command IDs */
    #define CMD_READ_TEMP    1
    #define CMD_SET_LED      2
    #define CMD_SEND_DATA    3
    #define CMD_SLEEP        4

    printf("--- Command Buffer ---\n");

    /* Small buffer to demonstrate overflow handling */
    tiku_kits_ds_queue_init(&cmd_q, 3);

    /* Queue commands */
    tiku_kits_ds_queue_enqueue(&cmd_q, CMD_READ_TEMP);
    tiku_kits_ds_queue_enqueue(&cmd_q, CMD_SET_LED);
    tiku_kits_ds_queue_enqueue(&cmd_q, CMD_SEND_DATA);

    printf("  Buffer full: %s\n",
           tiku_kits_ds_queue_full(&cmd_q) ? "yes" : "no");

    /* Attempt to queue when full */
    rc = tiku_kits_ds_queue_enqueue(&cmd_q, CMD_SLEEP);
    printf("  Overflow enqueue: %s\n",
           (rc == TIKU_KITS_DS_ERR_FULL) ? "rejected (full)" : "OK");

    /* Peek at next command without consuming */
    tiku_kits_ds_queue_peek(&cmd_q, &cmd);
    printf("  Next command (peek): %ld\n", (long)cmd);

    /* Process all commands */
    printf("  Processing commands:\n");
    while (tiku_kits_ds_queue_dequeue(&cmd_q, &cmd) == TIKU_KITS_DS_OK) {
        printf("    Executing command ID %ld\n", (long)cmd);
    }

    #undef CMD_READ_TEMP
    #undef CMD_SET_LED
    #undef CMD_SEND_DATA
    #undef CMD_SLEEP
}

/*---------------------------------------------------------------------------*/
/* Example 3: Message passing between tasks                                  */
/*---------------------------------------------------------------------------*/

/**
 * Simulates message passing between two cooperative tasks using a
 * shared queue. Producer enqueues messages, consumer dequeues them.
 * Demonstrates interleaved produce/consume with wraparound.
 */
static void example_message_passing(void)
{
    struct tiku_kits_ds_queue msg_q;
    tiku_kits_ds_elem_t msg;
    int i;

    printf("--- Message Passing ---\n");

    tiku_kits_ds_queue_init(&msg_q, 4);

    /* Round 1: producer sends 2 messages */
    printf("  Producer: sending 0xA1, 0xA2\n");
    tiku_kits_ds_queue_enqueue(&msg_q, 0xA1);
    tiku_kits_ds_queue_enqueue(&msg_q, 0xA2);

    /* Consumer reads 1 message */
    tiku_kits_ds_queue_dequeue(&msg_q, &msg);
    printf("  Consumer: received 0x%lX\n", (unsigned long)msg);

    /* Round 2: producer sends 2 more (tail wraps) */
    printf("  Producer: sending 0xB1, 0xB2\n");
    tiku_kits_ds_queue_enqueue(&msg_q, 0xB1);
    tiku_kits_ds_queue_enqueue(&msg_q, 0xB2);

    printf("  Queue size: %u\n", tiku_kits_ds_queue_size(&msg_q));

    /* Consumer drains remaining messages */
    printf("  Consumer: draining all:\n");
    for (i = 0; i < 4; i++) {
        if (tiku_kits_ds_queue_dequeue(&msg_q, &msg)
            == TIKU_KITS_DS_OK) {
            printf("    msg = 0x%lX\n", (unsigned long)msg);
        }
    }

    /* Clear and reuse */
    tiku_kits_ds_queue_clear(&msg_q);
    printf("  After clear, empty: %s\n",
           tiku_kits_ds_queue_empty(&msg_q) ? "yes" : "no");
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_queue_run(void)
{
    printf("=== TikuKits FIFO Queue Examples ===\n\n");

    example_sensor_event_queue();
    printf("\n");

    example_command_buffer();
    printf("\n");

    example_message_passing();
    printf("\n");
}
