/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_adt7410.h - Analog Devices ADT7410 I2C temperature sensor driver
 *
 * The ADT7410 is a high-accuracy (+/- 0.5 C) digital temperature
 * sensor with 13-bit resolution (0.0625 C/LSB) in default mode.
 * It communicates over I2C at address 0x48 (default, configurable
 * via A0-A1).
 *
 * Prerequisites:
 *   - I2C bus must be initialized before calling any driver function
 *   - Include <interfaces/bus/tiku_i2c_bus.h> in your build
 *
 * Typical usage:
 * @code
 *   tiku_i2c_config_t cfg = { .speed = TIKU_I2C_SPEED_STANDARD };
 *   tiku_i2c_init(&cfg);
 *
 *   if (tiku_kits_sensor_adt7410_init(TIKU_KITS_SENSOR_ADT7410_ADDR_DEFAULT)
 *       == TIKU_KITS_SENSOR_OK) {
 *       tiku_kits_sensor_temp_t temp;
 *       tiku_kits_sensor_adt7410_read(&temp);
 *       printf("Temp: %d.%02d C\n", temp.integer,
 *              TIKU_KITS_SENSOR_FRAC_TO_DEC(temp.frac));
 *   }
 * @endcode
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

#ifndef TIKU_KITS_SENSOR_ADT7410_H_
#define TIKU_KITS_SENSOR_ADT7410_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_sensor.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** Default I2C address (A0=A1=GND) */
#define TIKU_KITS_SENSOR_ADT7410_ADDR_DEFAULT    0x48

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize and verify the ADT7410 sensor
 *
 * Reads the ID register (0x0B) and checks the upper 5 bits
 * for the Analog Devices manufacturer code. The I2C bus must
 * already be initialized.
 *
 * @param addr 7-bit I2C address (use TIKU_KITS_SENSOR_ADT7410_ADDR_DEFAULT)
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS on I2C failure,
 *         TIKU_KITS_SENSOR_ERR_ID if ID mismatch
 */
int tiku_kits_sensor_adt7410_init(uint8_t addr);

/**
 * @brief Read the ambient temperature
 *
 * Reads the 13-bit temperature register (default mode) and
 * converts it to integer + fractional form. Handles negative
 * temperatures via two's complement.
 *
 * @param temp Output: temperature reading
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS on I2C failure,
 *         TIKU_KITS_SENSOR_ERR_PARAM if temp is NULL
 */
int tiku_kits_sensor_adt7410_read(tiku_kits_sensor_temp_t *temp);

/**
 * @brief Get the sensor name string
 * @return "ADT7410"
 */
const char *tiku_kits_sensor_adt7410_name(void);

#endif /* TIKU_KITS_SENSOR_ADT7410_H_ */
