/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_mcp9808.h - Microchip MCP9808 I2C temperature sensor driver
 *
 * The MCP9808 is a high-accuracy (+/- 0.25 C) digital temperature
 * sensor with a 13-bit resolution (0.0625 C/LSB). It communicates
 * over I2C at address 0x18 (default, configurable via A0-A2).
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
 *   if (tiku_kits_sensor_mcp9808_init(TIKU_KITS_SENSOR_MCP9808_ADDR_DEFAULT)
 *       == TIKU_KITS_SENSOR_OK) {
 *       tiku_kits_sensor_temp_t temp;
 *       tiku_kits_sensor_mcp9808_read(&temp);
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

#ifndef TIKU_KITS_SENSOR_MCP9808_H_
#define TIKU_KITS_SENSOR_MCP9808_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sensor.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Default 7-bit I2C address for the MCP9808.
 *
 * The MCP9808 base address is 0x18.  Three address pins (A0, A1,
 * A2) allow up to eight sensors on the same bus at addresses
 * 0x18-0x1F.  This constant assumes all three pins are tied to
 * GND, giving address 0x18.
 *
 * Override at the call site to use a different pin configuration:
 * @code
 *   // A0 = VCC, A1 = A2 = GND  ->  address 0x19
 *   tiku_kits_sensor_mcp9808_init(0x19);
 * @endcode
 */
#define TIKU_KITS_SENSOR_MCP9808_ADDR_DEFAULT    0x18

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize and verify the MCP9808 sensor
 *
 * Performs a two-stage identification sequence to confirm the
 * sensor is present and responding correctly on the I2C bus:
 *   1. Reads the manufacturer ID register (0x06) and checks for
 *      the Microchip ID value 0x0054.
 *   2. Reads the device ID register (0x07) and verifies that the
 *      upper byte equals 0x04 (MCP9808 family).
 *
 * The validated address is stored internally so that subsequent
 * read calls do not require re-specifying it.  The I2C bus must
 * already be initialized before calling this function.
 *
 * @param addr 7-bit I2C address of the MCP9808.  Use
 *             TIKU_KITS_SENSOR_MCP9808_ADDR_DEFAULT (0x18) for
 *             default pin configuration, or 0x18-0x1F for custom
 *             A0/A1/A2 settings.
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS if the I2C transaction fails
 *             (NACK, bus timeout, etc.),
 *         TIKU_KITS_SENSOR_ERR_ID if the manufacturer ID or device
 *             ID does not match expected values
 */
int tiku_kits_sensor_mcp9808_init(uint8_t addr);

/**
 * @brief Read the ambient temperature from the MCP9808
 *
 * Reads the 16-bit ambient temperature register (0x05) and
 * converts the 13-bit value into the common
 * tiku_kits_sensor_temp_t representation.  The upper three bits
 * of the register carry alert flags and are masked off before
 * conversion.
 *
 * Resolution is 0.0625 C per LSB (1/16 C), which maps directly
 * to the @c frac field of the output structure.  Negative
 * temperatures are encoded with a dedicated sign bit (bit 12),
 * not two's complement, so the conversion handles sign
 * separately from magnitude.
 *
 * The sensor must have been initialized via
 * tiku_kits_sensor_mcp9808_init() before calling this function.
 *
 * @param temp Pointer to a caller-allocated temperature structure
 *             where the result is written.  Must not be NULL.
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS if the I2C read fails,
 *         TIKU_KITS_SENSOR_ERR_PARAM if @p temp is NULL
 */
int tiku_kits_sensor_mcp9808_read(tiku_kits_sensor_temp_t *temp);

/**
 * @brief Get the human-readable sensor name string
 *
 * Returns a pointer to a static, null-terminated string identifying
 * this sensor model.  Useful for diagnostic output or building a
 * sensor registry table at runtime.
 *
 * @return Pointer to the constant string "MCP9808" (never NULL)
 */
const char *tiku_kits_sensor_mcp9808_name(void);

#endif /* TIKU_KITS_SENSOR_MCP9808_H_ */
