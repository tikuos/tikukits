/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Sorted Array Data Structure
 *
 * Demonstrates the sorted array library for embedded systems:
 *   - Calibration threshold lookup table with binary search
 *   - Alarm level table for sensor monitoring
 *   - Sensor priority ranking with dynamic insertion/removal
 *
 * All sorted arrays use static storage (no heap). Maximum capacity
 * is controlled by TIKU_KITS_DS_SORTARRAY_MAX_SIZE (default 16).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/sortarray/tiku_kits_ds_sortarray.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Calibration threshold lookup table                             */
/*---------------------------------------------------------------------------*/

/**
 * Store ADC calibration thresholds in a sorted array for fast binary
 * search lookups. In embedded systems, sensors are often calibrated
 * at specific voltage levels. A sorted table lets us quickly find
 * which calibration bracket a raw reading falls into.
 */
static void example_sortarray_calibration(void)
{
    struct tiku_kits_ds_sortarray thresholds;
    tiku_kits_ds_elem_t val;
    uint16_t idx;
    int rc;

    printf("--- Calibration Threshold Lookup ---\n");

    tiku_kits_ds_sortarray_init(&thresholds, 8);

    /* Insert ADC threshold values (millivolts) */
    tiku_kits_ds_sortarray_insert(&thresholds, 500);
    tiku_kits_ds_sortarray_insert(&thresholds, 1000);
    tiku_kits_ds_sortarray_insert(&thresholds, 1500);
    tiku_kits_ds_sortarray_insert(&thresholds, 2000);
    tiku_kits_ds_sortarray_insert(&thresholds, 2500);
    tiku_kits_ds_sortarray_insert(&thresholds, 3000);

    printf("  Thresholds loaded: %u values\n",
           tiku_kits_ds_sortarray_size(&thresholds));

    /* Look up a known threshold */
    rc = tiku_kits_ds_sortarray_find(&thresholds, 1500, &idx);
    if (rc == TIKU_KITS_DS_OK) {
        printf("  Found 1500 mV at index %u\n", idx);
    }

    /* Check if a raw reading matches any threshold */
    rc = tiku_kits_ds_sortarray_find(&thresholds, 1750, &idx);
    if (rc == TIKU_KITS_DS_ERR_NOTFOUND) {
        printf("  1750 mV not a calibration point"
               " (between brackets)\n");
    }

    /* Display min/max calibration range */
    tiku_kits_ds_sortarray_min(&thresholds, &val);
    printf("  Calibration range: %ld", (long)val);
    tiku_kits_ds_sortarray_max(&thresholds, &val);
    printf(" - %ld mV\n", (long)val);

    /* Add a new calibration point at runtime */
    tiku_kits_ds_sortarray_insert(&thresholds, 1750);
    printf("  Added 1750 mV calibration point\n");
    tiku_kits_ds_sortarray_find(&thresholds, 1750, &idx);
    printf("  1750 mV now at index %u (auto-sorted)\n", idx);
}

/*---------------------------------------------------------------------------*/
/* Example 2: Alarm level table                                              */
/*---------------------------------------------------------------------------*/

/**
 * Maintain sorted alarm thresholds for sensor monitoring. When a
 * new reading arrives, binary search determines which alarm zone
 * the value falls into. Sorted order ensures efficient lookup and
 * easy min/max retrieval for display.
 */
static void example_sortarray_alarm_levels(void)
{
    struct tiku_kits_ds_sortarray alarms;
    tiku_kits_ds_elem_t reading;
    tiku_kits_ds_elem_t min_alarm;
    tiku_kits_ds_elem_t max_alarm;
    uint16_t i;

    printf("--- Alarm Level Table ---\n");

    tiku_kits_ds_sortarray_init(&alarms, 8);

    /* Define temperature alarm thresholds (Celsius * 100) */
    tiku_kits_ds_sortarray_insert(&alarms, -4000);  /* -40.00 C */
    tiku_kits_ds_sortarray_insert(&alarms, 0);      /*   0.00 C */
    tiku_kits_ds_sortarray_insert(&alarms, 2500);   /*  25.00 C */
    tiku_kits_ds_sortarray_insert(&alarms, 6000);   /*  60.00 C */
    tiku_kits_ds_sortarray_insert(&alarms, 8500);   /*  85.00 C */

    printf("  Alarm thresholds defined:\n");
    for (i = 0; i < tiku_kits_ds_sortarray_size(&alarms); i++) {
        tiku_kits_ds_sortarray_get(&alarms, i, &reading);
        printf("    Level %u: %ld.%02ld C\n", i,
               (long)(reading / 100),
               (long)(reading >= 0 ? reading % 100
                                   : (-reading) % 100));
    }

    /* Check bounds */
    tiku_kits_ds_sortarray_min(&alarms, &min_alarm);
    tiku_kits_ds_sortarray_max(&alarms, &max_alarm);
    printf("  Operating range: %ld.%02ld to %ld.%02ld C\n",
           (long)(min_alarm / 100),
           (long)(min_alarm >= 0 ? min_alarm % 100
                                 : (-min_alarm) % 100),
           (long)(max_alarm / 100),
           (long)(max_alarm % 100));

    /* Simulated reading: check if it's a known threshold */
    reading = 2500;
    if (tiku_kits_ds_sortarray_contains(&alarms, reading)) {
        printf("  Reading %ld.%02ld C matches alarm threshold\n",
               (long)(reading / 100), (long)(reading % 100));
    }

    /* Remove a threshold no longer needed */
    tiku_kits_ds_sortarray_remove(&alarms, -4000);
    printf("  Removed -40.00 C threshold (not needed indoors)\n");
    printf("  Remaining thresholds: %u\n",
           tiku_kits_ds_sortarray_size(&alarms));
}

/*---------------------------------------------------------------------------*/
/* Example 3: Sensor priority ranking                                        */
/*---------------------------------------------------------------------------*/

/**
 * Dynamically rank sensors by priority score. Higher-priority sensors
 * get more frequent sampling. The sorted array keeps priorities in
 * order, making it trivial to find the highest/lowest priority sensor
 * and to insert/remove sensors as they come online or go offline.
 */
static void example_sortarray_priority_ranking(void)
{
    struct tiku_kits_ds_sortarray priorities;
    tiku_kits_ds_elem_t val;

    printf("--- Sensor Priority Ranking ---\n");

    tiku_kits_ds_sortarray_init(&priorities, 8);

    /* Assign priorities (higher = more important) */
    printf("  Adding sensors with priorities:\n");
    tiku_kits_ds_sortarray_insert(&priorities, 50);
    printf("    Humidity sensor:    priority 50\n");
    tiku_kits_ds_sortarray_insert(&priorities, 90);
    printf("    Temperature sensor: priority 90\n");
    tiku_kits_ds_sortarray_insert(&priorities, 30);
    printf("    Light sensor:       priority 30\n");
    tiku_kits_ds_sortarray_insert(&priorities, 70);
    printf("    Pressure sensor:    priority 70\n");
    tiku_kits_ds_sortarray_insert(&priorities, 90);
    printf("    Vibration sensor:   priority 90 (duplicate)\n");

    /* Highest priority sensor */
    tiku_kits_ds_sortarray_max(&priorities, &val);
    printf("  Highest priority: %ld (sample first)\n", (long)val);

    /* Lowest priority sensor */
    tiku_kits_ds_sortarray_min(&priorities, &val);
    printf("  Lowest priority:  %ld (sample last)\n", (long)val);

    /* Light sensor goes offline, remove it */
    tiku_kits_ds_sortarray_remove(&priorities, 30);
    printf("  Light sensor offline (priority 30 removed)\n");

    /* Check remaining count */
    printf("  Active sensors: %u\n",
           tiku_kits_ds_sortarray_size(&priorities));

    /* New sensor comes online with high priority */
    tiku_kits_ds_sortarray_insert(&priorities, 95);
    printf("  Gas sensor online:    priority 95\n");

    tiku_kits_ds_sortarray_max(&priorities, &val);
    printf("  New highest priority: %ld\n", (long)val);

    /* Display final ranking */
    printf("  Final priority ranking (ascending):\n");
    {
        uint16_t i;
        for (i = 0; i < tiku_kits_ds_sortarray_size(&priorities);
             i++) {
            tiku_kits_ds_sortarray_get(&priorities, i, &val);
            printf("    Rank %u: priority %ld\n", i, (long)val);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_sortarray_run(void)
{
    printf("=== TikuKits Sorted Array Examples ===\n\n");

    example_sortarray_calibration();
    printf("\n");

    example_sortarray_alarm_levels();
    printf("\n");

    example_sortarray_priority_ranking();
    printf("\n");
}
