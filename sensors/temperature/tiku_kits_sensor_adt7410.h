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

#include "../tiku_kits_sensor.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Default 7-bit I2C address for the ADT7410.
 *
 * The ADT7410 base address is 0x48.  Two address pins (A0, A1)
 * allow up to four sensors on the same bus at addresses 0x48-0x4B.
 * This constant assumes both pins are tied to GND, giving address
 * 0x48.
 *
 * Override at the call site to use a different pin configuration:
 * @code
 *   // A0 = VCC, A1 = GND  ->  address 0x49
 *   tiku_kits_sensor_adt7410_init(0x49);
 * @endcode
 */
#define TIKU_KITS_SENSOR_ADT7410_ADDR_DEFAULT    0x48

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize and verify the ADT7410 sensor
 *
 * Reads the 8-bit ID register (0x0B) and checks the upper 5 bits
 * against the Analog Devices manufacturer code (0xC8 masked with
 * 0xF8).  The lower 3 bits contain the silicon revision and are
 * ignored during verification.
 *
 * The validated address is stored internally so that subsequent
 * read calls do not require re-specifying it.  The I2C bus must
 * already be initialized before calling this function.
 *
 * @param addr 7-bit I2C address of the ADT7410.  Use
 *             TIKU_KITS_SENSOR_ADT7410_ADDR_DEFAULT (0x48) for
 *             default pin configuration, or 0x48-0x4B for custom
 *             A0/A1 settings.
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS if the I2C transaction fails
 *             (NACK, bus timeout, etc.),
 *         TIKU_KITS_SENSOR_ERR_ID if the upper 5 bits of the ID
 *             register do not match the Analog Devices code
 */
int tiku_kits_sensor_adt7410_init(uint8_t addr);

/**
 * @brief Read the ambient temperature from the ADT7410
 *
 * Reads the 16-bit temperature register (0x00) and extracts the
 * 13-bit value (bits [15:3]) in the default resolution mode.
 * The ADT7410 uses standard two's complement encoding, so
 * negative temperatures are handled by complementing the raw
 * value and splitting the magnitude into integer and fractional
 * parts.  Resolution is 0.0625 C per LSB (1/16 C), matching the
 * common tiku_kits_sensor_temp_t fractional encoding.
 *
 * The sensor must have been initialized via
 * tiku_kits_sensor_adt7410_init() before calling this function.
 *
 * @param temp Pointer to a caller-allocated temperature structure
 *             where the result is written.  Must not be NULL.
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS if the I2C read fails,
 *         TIKU_KITS_SENSOR_ERR_PARAM if @p temp is NULL
 */
int tiku_kits_sensor_adt7410_read(tiku_kits_sensor_temp_t *temp);

/**
 * @brief Get the human-readable sensor name string
 *
 * Returns a pointer to a static, null-terminated string identifying
 * this sensor model.  Useful for diagnostic output or building a
 * sensor registry table at runtime.
 *
 * @return Pointer to the constant string "ADT7410" (never NULL)
 */
const char *tiku_kits_sensor_adt7410_name(void);

#endif /* TIKU_KITS_SENSOR_ADT7410_H_ */
