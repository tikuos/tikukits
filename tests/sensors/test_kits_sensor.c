/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_sensor.c - Sensor common types and macro tests
 *
 * Tests the shared sensor types and helper macros. Individual sensor
 * driver tests (init, read) require hardware and are run on-target
 * with the appropriate bus (I2C/1-Wire) initialized.
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

#include "test_kits_sensor.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: FRAC_TO_DEC CONVERSION MACRO                                      */
/*---------------------------------------------------------------------------*/

void test_kits_sensor_frac_conv(void)
{
    TEST_PRINT("\n--- Test: Sensor FRAC_TO_DEC ---\n");

    /*
     * The macro converts 1/16 C fractional units to two-digit decimal.
     * frac16=0  -> 0/16 = 0.00   -> 00
     * frac16=1  -> 1/16 = 0.0625 -> 06
     * frac16=4  -> 4/16 = 0.25   -> 25
     * frac16=8  -> 8/16 = 0.50   -> 50
     * frac16=12 -> 12/16 = 0.75  -> 75
     * frac16=15 -> 15/16 = 0.9375-> 93
     */
    TEST_ASSERT(TIKU_KITS_SENSOR_FRAC_TO_DEC(0) == 0,
                "frac 0/16 -> 00");
    TEST_ASSERT(TIKU_KITS_SENSOR_FRAC_TO_DEC(1) == 6,
                "frac 1/16 -> 06");
    TEST_ASSERT(TIKU_KITS_SENSOR_FRAC_TO_DEC(2) == 12,
                "frac 2/16 -> 12");
    TEST_ASSERT(TIKU_KITS_SENSOR_FRAC_TO_DEC(4) == 25,
                "frac 4/16 -> 25");
    TEST_ASSERT(TIKU_KITS_SENSOR_FRAC_TO_DEC(8) == 50,
                "frac 8/16 -> 50");
    TEST_ASSERT(TIKU_KITS_SENSOR_FRAC_TO_DEC(12) == 75,
                "frac 12/16 -> 75");
    TEST_ASSERT(TIKU_KITS_SENSOR_FRAC_TO_DEC(15) == 93,
                "frac 15/16 -> 93");

    /* Verify monotonically increasing */
    {
        uint8_t i;
        int prev = -1;
        int all_increasing = 1;

        for (i = 0; i <= 15; i++) {
            int val = TIKU_KITS_SENSOR_FRAC_TO_DEC(i);

            if (val <= prev && i > 0) {
                all_increasing = 0;
            }
            prev = val;
        }
        TEST_ASSERT(all_increasing,
                    "FRAC_TO_DEC monotonically increasing 0-15");
    }
}

/*---------------------------------------------------------------------------*/
/* TEST 2: SENSOR NAME FUNCTIONS                                             */
/*---------------------------------------------------------------------------*/

void test_kits_sensor_name(void)
{
    const char *name;

    TEST_PRINT("\n--- Test: Sensor Name ---\n");

    name = tiku_kits_sensor_mcp9808_name();
    TEST_ASSERT(name != NULL, "MCP9808 name not NULL");
    TEST_ASSERT(strcmp(name, "MCP9808") == 0, "MCP9808 name is 'MCP9808'");

    name = tiku_kits_sensor_adt7410_name();
    TEST_ASSERT(name != NULL, "ADT7410 name not NULL");
    TEST_ASSERT(strcmp(name, "ADT7410") == 0, "ADT7410 name is 'ADT7410'");

    name = tiku_kits_sensor_ds18b20_name();
    TEST_ASSERT(name != NULL, "DS18B20 name not NULL");
    TEST_ASSERT(strcmp(name, "DS18B20") == 0, "DS18B20 name is 'DS18B20'");
}
