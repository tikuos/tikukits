/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Linked List
 *
 * Demonstrates the pool-allocated singly linked list for common
 * embedded use cases:
 *   1. Timer list   -- ordered insertion and removal of deadlines
 *   2. Task queue   -- FIFO work queue with priority removal
 *   3. Sensor nodes -- registry of active sensor node IDs
 *
 * All storage is static (no heap). Maximum node count is controlled
 * by TIKU_KITS_DS_LIST_MAX_NODES (default 16).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/list/tiku_kits_ds_list.h"

/*---------------------------------------------------------------------------*/
/* Helper: print list contents in traversal order                            */
/*---------------------------------------------------------------------------*/

static void print_list(const char *label,
                       const struct tiku_kits_ds_list *lst)
{
    uint8_t cur;
    tiku_kits_ds_elem_t val;

    printf("  %s [%u nodes]: ", label,
           tiku_kits_ds_list_size(lst));

    cur = tiku_kits_ds_list_head(lst);
    while (cur != TIKU_KITS_DS_LIST_NONE) {
        tiku_kits_ds_list_get(lst, cur, &val);
        printf("%ld", (long)val);
        cur = tiku_kits_ds_list_next(lst, cur);
        if (cur != TIKU_KITS_DS_LIST_NONE) {
            printf(" -> ");
        }
    }
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 1: Timer list -- ordered deadlines                                */
/*---------------------------------------------------------------------------*/

/**
 * Maintains an ordered list of timer deadlines (in ticks). New
 * deadlines are inserted in sorted position; expired timers are
 * removed from the front.
 */
static void example_timer_list(void)
{
    struct tiku_kits_ds_list timers;
    tiku_kits_ds_elem_t val;
    uint8_t cur;
    uint8_t prev;

    printf("--- Timer List (Ordered Insert/Remove) ---\n");

    tiku_kits_ds_list_init(&timers, 8);

    /*
     * Insert deadlines in sorted order: 100, 300, 200, 150
     * Expected result: 100 -> 150 -> 200 -> 300
     */

    /* First deadline */
    tiku_kits_ds_list_push_back(&timers, 100);
    print_list("After 100", &timers);

    /* 300 goes at the end */
    tiku_kits_ds_list_push_back(&timers, 300);
    print_list("After 300", &timers);

    /* 200: insert after 100 (before 300) */
    cur = tiku_kits_ds_list_head(&timers);
    tiku_kits_ds_list_insert_after(&timers, cur, 200);
    print_list("After 200", &timers);

    /* 150: insert after 100 (before 200) */
    cur = tiku_kits_ds_list_head(&timers);
    tiku_kits_ds_list_insert_after(&timers, cur, 150);
    print_list("After 150", &timers);

    /* Expire the earliest timer (pop front) */
    tiku_kits_ds_list_pop_front(&timers, &val);
    printf("  Expired timer: %ld\n", (long)val);
    print_list("Remaining", &timers);
}

/*---------------------------------------------------------------------------*/
/* Example 2: Task queue -- FIFO with priority removal                       */
/*---------------------------------------------------------------------------*/

/**
 * Simple task queue where tasks are enqueued at the back and
 * dequeued from the front. High-priority tasks can be removed
 * by value from anywhere in the queue.
 */
static void example_task_queue(void)
{
    struct tiku_kits_ds_list tasks;
    tiku_kits_ds_elem_t val;
    uint8_t idx;

    printf("--- Task Queue (FIFO + Priority Remove) ---\n");

    tiku_kits_ds_list_init(&tasks, 8);

    /* Enqueue tasks: IDs 1, 2, 3, 4 */
    tiku_kits_ds_list_push_back(&tasks, 1);
    tiku_kits_ds_list_push_back(&tasks, 2);
    tiku_kits_ds_list_push_back(&tasks, 3);
    tiku_kits_ds_list_push_back(&tasks, 4);
    print_list("Task queue", &tasks);

    /* Cancel task 3 (high-priority removal) */
    if (tiku_kits_ds_list_find(&tasks, 3, &idx) ==
        TIKU_KITS_DS_OK) {
        tiku_kits_ds_list_remove(&tasks, idx);
        printf("  Cancelled task 3\n");
    }
    print_list("After cancel", &tasks);

    /* Process tasks in FIFO order */
    printf("  Processing: ");
    while (tiku_kits_ds_list_size(&tasks) > 0) {
        tiku_kits_ds_list_pop_front(&tasks, &val);
        printf("%ld ", (long)val);
    }
    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Sensor node registry                                           */
/*---------------------------------------------------------------------------*/

/**
 * Registry of active sensor node IDs. Nodes register on join and
 * unregister on leave. The registry supports lookup to check if a
 * particular node is active.
 */
static void example_sensor_registry(void)
{
    struct tiku_kits_ds_list registry;
    uint8_t idx;

    printf("--- Sensor Node Registry ---\n");

    tiku_kits_ds_list_init(&registry, 8);

    /* Sensors join the network */
    tiku_kits_ds_list_push_back(&registry, 0x0A);
    tiku_kits_ds_list_push_back(&registry, 0x0B);
    tiku_kits_ds_list_push_back(&registry, 0x0C);
    tiku_kits_ds_list_push_back(&registry, 0x0D);
    print_list("Active sensors", &registry);

    /* Check if sensor 0x0B is registered */
    if (tiku_kits_ds_list_find(&registry, 0x0B, &idx) ==
        TIKU_KITS_DS_OK) {
        printf("  Sensor 0x0B is ACTIVE\n");
    }

    /* Sensor 0x0C leaves the network */
    if (tiku_kits_ds_list_find(&registry, 0x0C, &idx) ==
        TIKU_KITS_DS_OK) {
        tiku_kits_ds_list_remove(&registry, idx);
        printf("  Sensor 0x0C unregistered\n");
    }
    print_list("After departure", &registry);

    /* Check for absent sensor */
    if (tiku_kits_ds_list_find(&registry, 0x0C, &idx) !=
        TIKU_KITS_DS_OK) {
        printf("  Sensor 0x0C is NOT FOUND\n");
    }

    printf("  Total active: %u\n",
           tiku_kits_ds_list_size(&registry));
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_list_run(void)
{
    printf("=== TikuKits Linked List Examples ===\n\n");

    example_timer_list();
    printf("\n");

    example_task_queue();
    printf("\n");

    example_sensor_registry();
    printf("\n");
}
