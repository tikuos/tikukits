/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: B-Tree
 *
 * Demonstrates B-Tree usage for common embedded use cases:
 *   1. FRAM key store    -- insert sensor IDs, search for specific ID
 *   2. Sorted index      -- build a sorted index of sensor readings
 *   3. Range check       -- use min/max to validate reading boundaries
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/btree/tiku_kits_ds_btree.h"

/*---------------------------------------------------------------------------*/
/* Example 1: FRAM key store (sensor ID registry)                            */
/*---------------------------------------------------------------------------*/

static void example_key_store(void)
{
    struct tiku_kits_ds_btree bt;
    int rc;

    /*
     * Sensor IDs assigned during provisioning. The B-Tree
     * serves as a fast-lookup registry stored in FRAM.
     */
    static const tiku_kits_ds_elem_t sensor_ids[] = {
        1001, 1042, 1007, 1023, 1099, 1055, 1033
    };
    static const uint8_t n =
        sizeof(sensor_ids) / sizeof(sensor_ids[0]);
    uint8_t i;

    printf("--- FRAM Key Store ---\n");

    tiku_kits_ds_btree_init(&bt);

    /* Register all sensor IDs */
    for (i = 0; i < n; i++) {
        rc = tiku_kits_ds_btree_insert(&bt, sensor_ids[i]);
        if (rc != TIKU_KITS_DS_OK) {
            printf("  ERROR: insert ID %ld failed (%d)\n",
                   (long)sensor_ids[i], rc);
            return;
        }
    }

    printf("  Registered %u sensor IDs\n",
           tiku_kits_ds_btree_count(&bt));
    printf("  Tree height: %u\n",
           tiku_kits_ds_btree_height(&bt));

    /* Look up a known sensor */
    rc = tiku_kits_ds_btree_search(&bt, 1042);
    printf("  Search 1042: %s\n",
           (rc == TIKU_KITS_DS_OK) ? "FOUND" : "NOT FOUND");

    /* Look up an unknown sensor */
    rc = tiku_kits_ds_btree_search(&bt, 9999);
    printf("  Search 9999: %s\n",
           (rc == TIKU_KITS_DS_OK) ? "FOUND" : "NOT FOUND");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Sorted index for sensor readings                               */
/*---------------------------------------------------------------------------*/

static void example_sorted_index(void)
{
    struct tiku_kits_ds_btree bt;
    uint8_t i;

    /*
     * Temperature readings (in deci-degrees C) collected over
     * a monitoring period. Insert into B-Tree to maintain a
     * sorted index for range queries.
     */
    static const tiku_kits_ds_elem_t readings[] = {
        234, 221, 245, 210, 238, 229, 251, 217
    };
    static const uint8_t n =
        sizeof(readings) / sizeof(readings[0]);

    printf("--- Sorted Index ---\n");

    tiku_kits_ds_btree_init(&bt);

    for (i = 0; i < n; i++) {
        tiku_kits_ds_btree_insert(&bt, readings[i]);
    }

    printf("  Indexed %u readings\n",
           tiku_kits_ds_btree_count(&bt));
    printf("  Tree height: %u\n",
           tiku_kits_ds_btree_height(&bt));

    /* Check if a specific reading exists */
    printf("  Reading 245: %s\n",
           (tiku_kits_ds_btree_search(&bt, 245)
                == TIKU_KITS_DS_OK)
           ? "present" : "absent");

    printf("  Reading 240: %s\n",
           (tiku_kits_ds_btree_search(&bt, 240)
                == TIKU_KITS_DS_OK)
           ? "present" : "absent");
}

/*---------------------------------------------------------------------------*/
/* Example 3: Range check with min/max                                       */
/*---------------------------------------------------------------------------*/

static void example_range_check(void)
{
    struct tiku_kits_ds_btree bt;
    tiku_kits_ds_elem_t min_val, max_val;
    uint8_t i;

    /*
     * ADC readings from a voltage sensor. Use min/max to
     * detect whether any reading falls outside acceptable
     * bounds (e.g. 2800..4200 mV for a LiPo battery).
     */
    static const tiku_kits_ds_elem_t adc_mv[] = {
        3700, 3650, 3800, 3500, 3900, 3400, 3850, 3600
    };
    static const uint8_t n =
        sizeof(adc_mv) / sizeof(adc_mv[0]);

    const tiku_kits_ds_elem_t lower_bound = 2800;
    const tiku_kits_ds_elem_t upper_bound = 4200;

    printf("--- Range Check (Battery Voltage) ---\n");

    tiku_kits_ds_btree_init(&bt);

    for (i = 0; i < n; i++) {
        tiku_kits_ds_btree_insert(&bt, adc_mv[i]);
    }

    tiku_kits_ds_btree_min(&bt, &min_val);
    tiku_kits_ds_btree_max(&bt, &max_val);

    printf("  Samples: %u\n", tiku_kits_ds_btree_count(&bt));
    printf("  Min: %ld mV, Max: %ld mV\n",
           (long)min_val, (long)max_val);

    if (min_val >= lower_bound && max_val <= upper_bound) {
        printf("  -> All readings within safe range "
               "[%ld, %ld]\n",
               (long)lower_bound, (long)upper_bound);
    } else {
        if (min_val < lower_bound) {
            printf("  -> WARNING: under-voltage detected "
                   "(%ld mV < %ld mV)\n",
                   (long)min_val, (long)lower_bound);
        }
        if (max_val > upper_bound) {
            printf("  -> WARNING: over-voltage detected "
                   "(%ld mV > %ld mV)\n",
                   (long)max_val, (long)upper_bound);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_btree_run(void)
{
    printf("=== TikuKits B-Tree Examples ===\n\n");

    example_key_store();
    printf("\n");

    example_sorted_index();
    printf("\n");

    example_range_check();
    printf("\n");
}
