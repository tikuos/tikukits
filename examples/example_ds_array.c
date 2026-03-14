/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Array Data Structure
 *
 * Demonstrates the fixed-size array for embedded applications:
 *   - Sensor data buffer with push/pop
 *   - Lookup table with set/get
 *   - Rolling window with insert/remove
 *
 * All arrays use static storage (no heap). Maximum capacity is
 * controlled by TIKU_KITS_DS_ARRAY_MAX_SIZE (default 32).
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

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/array/tiku_kits_ds_array.h"

/*---------------------------------------------------------------------------*/
/* Helper: print array contents                                              */
/*---------------------------------------------------------------------------*/

static void print_array(const char *label,
                        const struct tiku_kits_ds_array *arr)
{
    uint16_t i;
    tiku_kits_ds_elem_t val;

    printf("  %s (size=%u, cap=%u): [",
           label,
           tiku_kits_ds_array_size(arr),
           arr->capacity);
    for (i = 0; i < tiku_kits_ds_array_size(arr); i++) {
        tiku_kits_ds_array_get(arr, i, &val);
        printf("%ld", (long)val);
        if (i < tiku_kits_ds_array_size(arr) - 1) {
            printf(", ");
        }
    }
    printf("]\n");
}

/*---------------------------------------------------------------------------*/
/* Example 1: Sensor data buffer                                             */
/*---------------------------------------------------------------------------*/

static void example_sensor_buffer(void)
{
    struct tiku_kits_ds_array readings;
    tiku_kits_ds_elem_t val;
    uint16_t i;

    /* Simulated temperature readings (in 0.1 deg C) */
    static const tiku_kits_ds_elem_t samples[] = {
        225, 228, 231, 227, 230, 233, 229, 226
    };

    printf("--- Sensor Data Buffer ---\n");

    tiku_kits_ds_array_init(&readings, 8);

    /* Collect sensor readings */
    for (i = 0; i < 8; i++) {
        tiku_kits_ds_array_push_back(&readings, samples[i]);
    }
    print_array("Temperature readings", &readings);

    /* Compute simple average */
    {
        int32_t sum = 0;
        for (i = 0; i < tiku_kits_ds_array_size(&readings); i++) {
            tiku_kits_ds_array_get(&readings, i, &val);
            sum += val;
        }
        printf("  Average: %ld.%ld deg C\n",
               (long)(sum / 8 / 10),
               (long)((sum / 8) % 10));
    }

    /* Remove oldest reading, add new one */
    tiku_kits_ds_array_remove(&readings, 0);
    tiku_kits_ds_array_push_back(&readings, 235);
    print_array("After sliding window", &readings);
}

/*---------------------------------------------------------------------------*/
/* Example 2: Lookup table (GPIO pin mapping)                                */
/*---------------------------------------------------------------------------*/

static void example_lookup_table(void)
{
    struct tiku_kits_ds_array pin_map;
    tiku_kits_ds_elem_t val;
    uint16_t idx;

    printf("--- Lookup Table (Pin Map) ---\n");

    tiku_kits_ds_array_init(&pin_map, 8);

    /*
     * Map logical channel 0..3 to physical GPIO pins.
     * Index = channel, value = pin number.
     */
    tiku_kits_ds_array_push_back(&pin_map, 14);  /* Ch0 -> P1.4 */
    tiku_kits_ds_array_push_back(&pin_map, 15);  /* Ch1 -> P1.5 */
    tiku_kits_ds_array_push_back(&pin_map, 20);  /* Ch2 -> P2.0 */
    tiku_kits_ds_array_push_back(&pin_map, 21);  /* Ch3 -> P2.1 */

    print_array("Pin map", &pin_map);

    /* Lookup: which pin is channel 2? */
    tiku_kits_ds_array_get(&pin_map, 2, &val);
    printf("  Channel 2 -> GPIO pin %ld\n", (long)val);

    /* Reverse lookup: which channel uses pin 15? */
    if (tiku_kits_ds_array_find(&pin_map, 15, &idx)
        == TIKU_KITS_DS_OK) {
        printf("  GPIO pin 15 -> Channel %u\n", idx);
    }

    /* Update mapping: remap channel 1 to pin 16 */
    tiku_kits_ds_array_set(&pin_map, 1, 16);
    print_array("After remap", &pin_map);
}

/*---------------------------------------------------------------------------*/
/* Example 3: Rolling window (last N samples)                                */
/*---------------------------------------------------------------------------*/

static void example_rolling_window(void)
{
    struct tiku_kits_ds_array window;
    tiku_kits_ds_elem_t val;
    uint16_t i;
    int32_t sum;

    /* Simulated ADC values arriving one at a time */
    static const tiku_kits_ds_elem_t stream[] = {
        100, 105, 98, 110, 102, 107, 95, 112, 108, 103
    };

    printf("--- Rolling Window (last 4) ---\n");

    tiku_kits_ds_array_init(&window, 4);

    for (i = 0; i < 10; i++) {
        /* If window is full, drop oldest (index 0) */
        if (tiku_kits_ds_array_size(&window) >= 4) {
            tiku_kits_ds_array_remove(&window, 0);
        }

        /* Append new sample */
        tiku_kits_ds_array_push_back(&window, stream[i]);

        /* Compute running average of window */
        sum = 0;
        {
            uint16_t j;
            for (j = 0;
                 j < tiku_kits_ds_array_size(&window);
                 j++) {
                tiku_kits_ds_array_get(&window, j, &val);
                sum += val;
            }
        }
        printf("  Sample %2u = %3ld  |  window avg = %ld\n",
               i, (long)stream[i],
               (long)(sum / tiku_kits_ds_array_size(&window)));
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_array_run(void)
{
    printf("=== TikuKits Array Examples ===\n\n");

    example_sensor_buffer();
    printf("\n");

    example_lookup_table();
    printf("\n");

    example_rolling_window();
    printf("\n");
}
