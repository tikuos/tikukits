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

/** @defgroup TIKU_KITS_SENSOR_STATUS Sensor Status Codes
 * @{ */
#define TIKU_KITS_SENSOR_OK              0   /**< Operation succeeded */
#define TIKU_KITS_SENSOR_ERR_BUS       (-1)  /**< Bus communication error */
#define TIKU_KITS_SENSOR_ERR_ID        (-2)  /**< Sensor ID mismatch */
#define TIKU_KITS_SENSOR_ERR_PARAM     (-3)  /**< Invalid parameter */
#define TIKU_KITS_SENSOR_ERR_NO_DEVICE (-4)  /**< No device detected */
/** @} */

/*---------------------------------------------------------------------------*/
/* COMMON DATA TYPES                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sensor_temp_t
 * @brief Temperature reading shared by all temperature sensors
 *
 * The temperature is split into integer and fractional parts.
 * Fractional part is in 1/16 degree C units (0-15), matching
 * the native resolution of most digital temperature sensors.
 *
 * To convert to a decimal display string:
 * @code
 *   tiku_kits_sensor_temp_t t;
 *   // ... read from sensor ...
 *   int frac_decimal = (int)((uint16_t)t.frac * 625 / 100);
 *   printf("%s%d.%02d C",
 *          t.negative ? "-" : "",
 *          (int)t.integer,
 *          frac_decimal);
 * @endcode
 */
typedef struct tiku_kits_sensor_temp {
    int16_t integer;  /**< Integer part of temperature (degrees C) */
    uint8_t frac;     /**< Fractional part (1/16 C units, 0-15) */
    uint8_t negative; /**< 1 if temperature is below 0 C, 0 otherwise */
} tiku_kits_sensor_temp_t;

/*---------------------------------------------------------------------------*/
/* HELPER MACROS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Convert 1/16 C fractional units to two-digit decimal
 * @param frac16 Fractional value in 1/16 C units (0-15)
 * @return Two-digit decimal fraction (e.g. 8 -> 50, 3 -> 18)
 *
 * Usage: printf("%d.%02d", temp.integer, TIKU_KITS_SENSOR_FRAC_TO_DEC(temp.frac));
 */
#define TIKU_KITS_SENSOR_FRAC_TO_DEC(frac16) \
    ((int)((uint16_t)(frac16) * 625 / 100))

#endif /* TIKU_KITS_SENSOR_H_ */
