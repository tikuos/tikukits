/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor.h - Common sensor types and definitions
 *
 * Provides shared data types, return codes, and helper macros used
 * across all TikuKits sensor drivers. Each sensor driver includes
 * this header for its common types.
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

#ifndef TIKU_KITS_SENSOR_H_
#define TIKU_KITS_SENSOR_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_SENSOR_STATUS Sensor Status Codes
 * @brief Return codes shared by all TikuKits sensor drivers.
 *
 * Every public sensor function returns one of these codes.  Zero
 * indicates success; negative values indicate distinct error classes.
 * Drivers must never define their own return codes -- all codes live
 * here so that application code can handle errors uniformly across
 * different sensor types.
 *
 * @code
 *   int rc = tiku_kits_sensor_mcp9808_read(&temp);
 *   if (rc != TIKU_KITS_SENSOR_OK) {
 *       // handle error: rc is one of the ERR_* codes below
 *   }
 * @endcode
 * @{
 */
#define TIKU_KITS_SENSOR_OK              0   /**< Operation succeeded */
#define TIKU_KITS_SENSOR_ERR_BUS       (-1)  /**< Bus communication error (I2C NACK, 1-Wire timeout, etc.) */
#define TIKU_KITS_SENSOR_ERR_ID        (-2)  /**< Sensor ID/manufacturer code mismatch during init */
#define TIKU_KITS_SENSOR_ERR_PARAM     (-3)  /**< Invalid parameter (NULL pointer, out-of-range value) */
#define TIKU_KITS_SENSOR_ERR_NO_DEVICE (-4)  /**< No device detected on the bus (no presence pulse) */
/** @} */

/*---------------------------------------------------------------------------*/
/* COMMON DATA TYPES                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sensor_temp
 * @brief Temperature reading shared by all temperature sensors
 *
 * A platform-independent representation of a temperature value
 * produced by any TikuKits temperature sensor driver.  The reading
 * is split into three fields so that integer-only firmware can
 * display and compare temperatures without floating-point support.
 *
 * The fractional part uses 1/16 degree C units (values 0-15),
 * which matches the native resolution of most digital temperature
 * sensors (MCP9808, ADT7410, DS18B20).  One LSB = 0.0625 C.
 *
 * Sign is stored in a separate @c negative flag rather than making
 * @c integer a signed value because some sensors encode sign
 * independently of magnitude (e.g. MCP9808 uses a dedicated sign
 * bit, not two's complement).  This flag is 1 when the temperature
 * is below 0 C and 0 otherwise.
 *
 * @note Use the helper macro TIKU_KITS_SENSOR_FRAC_TO_DEC() to
 *       convert the 1/16 C fractional field to a two-digit decimal
 *       suitable for printf-style output.
 *
 * Example -- reading and displaying a temperature:
 * @code
 *   tiku_kits_sensor_temp_t t;
 *   tiku_kits_sensor_mcp9808_read(&t);
 *   printf("%s%d.%02d C",
 *          t.negative ? "-" : "",
 *          (int)t.integer,
 *          TIKU_KITS_SENSOR_FRAC_TO_DEC(t.frac));
 * @endcode
 *
 * Example -- comparing two readings:
 * @code
 *   // Both readings are positive
 *   int whole_diff = t1.integer - t2.integer;
 *   int frac_diff  = t1.frac - t2.frac;  // in 1/16 C units
 * @endcode
 */
typedef struct tiku_kits_sensor_temp {
    int16_t integer;  /**< Whole-degree part of temperature (degrees C) */
    uint8_t frac;     /**< Fractional part in 1/16 C units (0-15); one
                       *   LSB = 0.0625 C, so a value of 8 means 0.50 C */
    uint8_t negative; /**< Sign flag: 1 if temperature is below 0 C,
                       *   0 otherwise.  Kept separate from @c integer
                       *   because some sensors use sign+magnitude encoding */
} tiku_kits_sensor_temp_t;

/*---------------------------------------------------------------------------*/
/* HELPER MACROS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Convert 1/16 C fractional units to a two-digit decimal value.
 *
 * Temperature sensors store the fractional part in 1/16 C units
 * (0-15), but human-readable output needs a decimal fraction.
 * This macro multiplies by 625 (= 10000/16) and divides by 100 to
 * produce a two-digit decimal in the range 0-93.  The result is
 * intended for use with a @c %02d printf format specifier.
 *
 * Conversion examples:
 *   - 0  -> 00  (0.00 C)
 *   - 1  -> 06  (0.06 C)
 *   - 3  -> 18  (0.18 C)
 *   - 8  -> 50  (0.50 C)
 *   - 15 -> 93  (0.93 C)
 *
 * @param frac16 Fractional value in 1/16 C units (0-15).  Values
 *               outside this range produce undefined but safe results.
 * @return Two-digit decimal fraction suitable for @c %02d output
 *
 * @code
 *   printf("%d.%02d C", temp.integer,
 *          TIKU_KITS_SENSOR_FRAC_TO_DEC(temp.frac));
 * @endcode
 */
#define TIKU_KITS_SENSOR_FRAC_TO_DEC(frac16) \
    ((int)((uint16_t)(frac16) * 625 / 100))

#endif /* TIKU_KITS_SENSOR_H_ */
