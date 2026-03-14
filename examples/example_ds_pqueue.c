/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Priority Queue
 *
 * Demonstrates the multi-level priority queue for embedded use cases:
 *   - Event priority dispatch (critical / normal / low)
 *   - Task scheduling with priority levels
 *   - Alarm levels (emergency, warning, info)
 *
 * All queues use static storage (no heap). Level 0 is the highest
 * priority. Each level is a FIFO ring buffer.
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/pqueue/tiku_kits_ds_pqueue.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Event priority dispatch                                        */
/*---------------------------------------------------------------------------*/

/**
 * Simulates an event system with three priority levels: CRITICAL (0),
 * NORMAL (1), and LOW (2). Events arrive in arbitrary order but are
 * dispatched highest-priority first.
 */
static void example_event_dispatch(void)
{
    struct tiku_kits_ds_pqueue eq;
    tiku_kits_ds_elem_t event_id;

    /* Priority levels */
    #define PRI_CRITICAL  0
    #define PRI_NORMAL    1
    #define PRI_LOW       2

    /* Event IDs */
    #define EVT_SENSOR_FAULT   101
    #define EVT_DATA_READY     201
    #define EVT_LOG_ROTATE     301
    #define EVT_WATCHDOG_TRIP  102
    #define EVT_UART_RX        202

    printf("--- Event Priority Dispatch ---\n");

    tiku_kits_ds_pqueue_init(&eq, 3, 4);

    /* Events arrive in mixed priority order */
    printf("  Posting events:\n");
    printf("    DATA_READY      (normal)\n");
    tiku_kits_ds_pqueue_enqueue(&eq, EVT_DATA_READY, PRI_NORMAL);

    printf("    LOG_ROTATE      (low)\n");
    tiku_kits_ds_pqueue_enqueue(&eq, EVT_LOG_ROTATE, PRI_LOW);

    printf("    SENSOR_FAULT    (critical)\n");
    tiku_kits_ds_pqueue_enqueue(&eq, EVT_SENSOR_FAULT, PRI_CRITICAL);

    printf("    UART_RX         (normal)\n");
    tiku_kits_ds_pqueue_enqueue(&eq, EVT_UART_RX, PRI_NORMAL);

    printf("    WATCHDOG_TRIP   (critical)\n");
    tiku_kits_ds_pqueue_enqueue(&eq, EVT_WATCHDOG_TRIP, PRI_CRITICAL);

    /* Dispatch in priority order */
    printf("  Dispatching (highest priority first):\n");
    while (!tiku_kits_ds_pqueue_empty(&eq)) {
        tiku_kits_ds_pqueue_dequeue(&eq, &event_id);
        printf("    Event ID: %ld\n", (long)event_id);
    }
    /* Expected order: 101, 102, 201, 202, 301 */

    #undef PRI_CRITICAL
    #undef PRI_NORMAL
    #undef PRI_LOW
    #undef EVT_SENSOR_FAULT
    #undef EVT_DATA_READY
    #undef EVT_LOG_ROTATE
    #undef EVT_WATCHDOG_TRIP
    #undef EVT_UART_RX
}

/*---------------------------------------------------------------------------*/
/* Example 2: Task scheduling                                                */
/*---------------------------------------------------------------------------*/

/**
 * Simulates a simple cooperative task scheduler. Tasks are enqueued
 * with priority levels and executed in priority order. Within the
 * same priority, tasks run in FIFO order.
 */
static void example_task_scheduling(void)
{
    struct tiku_kits_ds_pqueue sched;
    tiku_kits_ds_elem_t task_id;

    #define TASK_PRI_HIGH   0
    #define TASK_PRI_MED    1
    #define TASK_PRI_LOW    2

    printf("--- Task Scheduling ---\n");

    tiku_kits_ds_pqueue_init(&sched, 3, 4);

    /* Schedule tasks */
    printf("  Scheduling tasks:\n");
    printf("    Task 10 (medium)\n");
    tiku_kits_ds_pqueue_enqueue(&sched, 10, TASK_PRI_MED);

    printf("    Task 11 (medium)\n");
    tiku_kits_ds_pqueue_enqueue(&sched, 11, TASK_PRI_MED);

    printf("    Task 20 (low)\n");
    tiku_kits_ds_pqueue_enqueue(&sched, 20, TASK_PRI_LOW);

    printf("    Task  1 (high)\n");
    tiku_kits_ds_pqueue_enqueue(&sched, 1, TASK_PRI_HIGH);

    printf("  Total tasks queued: %u\n",
           tiku_kits_ds_pqueue_size(&sched));

    /* Peek at next task without running it */
    tiku_kits_ds_pqueue_peek(&sched, &task_id);
    printf("  Next task (peek): %ld\n", (long)task_id);

    /* Run all tasks in priority order */
    printf("  Running tasks:\n");
    while (tiku_kits_ds_pqueue_dequeue(&sched, &task_id)
           == TIKU_KITS_DS_OK) {
        printf("    Running task %ld\n", (long)task_id);
    }
    /* Expected: 1, 10, 11, 20 */

    #undef TASK_PRI_HIGH
    #undef TASK_PRI_MED
    #undef TASK_PRI_LOW
}

/*---------------------------------------------------------------------------*/
/* Example 3: Alarm levels                                                   */
/*---------------------------------------------------------------------------*/

/**
 * Simulates an alarm management system with four severity levels:
 * EMERGENCY (0), WARNING (1), CAUTION (2), INFO (3). Alarms are
 * serviced from most severe to least severe.
 */
static void example_alarm_levels(void)
{
    struct tiku_kits_ds_pqueue alarms;
    tiku_kits_ds_elem_t alarm_code;
    int rc;

    #define ALARM_EMERGENCY  0
    #define ALARM_WARNING    1
    #define ALARM_CAUTION    2
    #define ALARM_INFO       3

    printf("--- Alarm Levels ---\n");

    tiku_kits_ds_pqueue_init(&alarms, 4, 4);

    /* Post alarms */
    tiku_kits_ds_pqueue_enqueue(&alarms, 3001, ALARM_INFO);
    tiku_kits_ds_pqueue_enqueue(&alarms, 2001, ALARM_CAUTION);
    tiku_kits_ds_pqueue_enqueue(&alarms, 1001, ALARM_WARNING);
    tiku_kits_ds_pqueue_enqueue(&alarms, 2002, ALARM_CAUTION);
    tiku_kits_ds_pqueue_enqueue(&alarms, 0001, ALARM_EMERGENCY);

    printf("  Active alarms: %u\n",
           tiku_kits_ds_pqueue_size(&alarms));

    /* Service alarms highest severity first */
    printf("  Servicing alarms:\n");
    while ((rc = tiku_kits_ds_pqueue_dequeue(&alarms, &alarm_code))
           == TIKU_KITS_DS_OK) {
        printf("    Alarm code: %ld\n", (long)alarm_code);
    }
    /* Expected: 1 (emergency), 1001 (warning), 2001, 2002 (caution),
     * 3001 (info) */

    /* Clear and verify */
    tiku_kits_ds_pqueue_clear(&alarms);
    printf("  After clear, empty: %s\n",
           tiku_kits_ds_pqueue_empty(&alarms) ? "yes" : "no");

    #undef ALARM_EMERGENCY
    #undef ALARM_WARNING
    #undef ALARM_CAUTION
    #undef ALARM_INFO
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_pqueue_run(void)
{
    printf("=== TikuKits Priority Queue Examples ===\n\n");

    example_event_dispatch();
    printf("\n");

    example_task_scheduling();
    printf("\n");

    example_alarm_levels();
    printf("\n");
}
