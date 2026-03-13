/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_sensor.h - TikuKits sensor test interface
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

#ifndef TEST_KITS_SENSOR_H_
#define TEST_KITS_SENSOR_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tikukits/sensors/tiku_kits_sensor.h"
#include "tikukits/sensors/temperature/tiku_kits_sensor_mcp9808.h"
#include "tikukits/sensors/temperature/tiku_kits_sensor_adt7410.h"
#include "tikukits/sensors/temperature/tiku_kits_sensor_ds18b20.h"

/*---------------------------------------------------------------------------*/
/* TEST HARNESS                                                              */
/*---------------------------------------------------------------------------*/

#ifdef PLATFORM_MSP430
#include "tiku.h"
#define TEST_PRINT(...) TIKU_PRINTF(__VA_ARGS__)
#else
#define TEST_PRINT(...) printf(__VA_ARGS__)
#endif

extern int tests_run;
extern int tests_passed;
extern int tests_failed;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        tests_run++;                                                        \
        if (cond) {                                                         \
            tests_passed++;                                                 \
            TEST_PRINT("  PASS: %s\n", msg);                                \
        } else {                                                            \
            tests_failed++;                                                 \
            TEST_PRINT("  FAIL: %s\n", msg);                                \
        }                                                                   \
    } while (0)

/*---------------------------------------------------------------------------*/
/* TEST DECLARATIONS                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_sensor_frac_conv(void);
void test_kits_sensor_name(void);

#endif /* TEST_KITS_SENSOR_H_ */
