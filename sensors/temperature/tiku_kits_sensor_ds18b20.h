/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_ds18b20.h - Dallas/Maxim DS18B20 1-Wire temperature sensor driver
 *
 * The DS18B20 is a digital temperature sensor with 12-bit resolution
 * (0.0625 C/LSB, default) and a range of -55 to +125 C. It uses the
 * 1-Wire bus protocol with a conversion time of up to 750 ms.
 *
 * Prerequisites:
 *   - 1-Wire bus must be initialized before calling any driver function
 *   - Include <interfaces/onewire/tiku_onewire.h> in your build
 *
 * Typical usage (two-phase read):
 * @code
 *   tiku_onewire_init();
 *
 *   if (tiku_kits_sensor_ds18b20_init() == TIKU_KITS_SENSOR_OK) {
 *       tiku_kits_sensor_ds18b20_start_conversion();
 *       // ... wait at least 750 ms ...
 *       tiku_kits_sensor_temp_t temp;
 *       tiku_kits_sensor_ds18b20_read(&temp);
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

#ifndef TIKU_KITS_SENSOR_DS18B20_H_
#define TIKU_KITS_SENSOR_DS18B20_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sensor.h"

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize and verify a DS18B20 is present on the bus
 *
 * Issues a 1-Wire reset pulse and listens for the presence pulse
 * that every 1-Wire slave must transmit in response.  This
 * confirms at least one device is connected but does not verify
 * the specific device family (any 1-Wire slave will respond).
 *
 * The 1-Wire bus must already be initialized via
 * tiku_onewire_init() before calling this function.
 *
 * @return TIKU_KITS_SENSOR_OK if a presence pulse was detected,
 *         TIKU_KITS_SENSOR_ERR_NO_DEVICE if the bus remained
 *             idle (no slave pulled the line low)
 */
int tiku_kits_sensor_ds18b20_init(void);

/**
 * @brief Start a temperature conversion on the DS18B20
 *
 * Issues a 1-Wire reset followed by the Skip ROM command (0xCC)
 * and the Convert T command (0x44).  Skip ROM addresses all
 * slaves on the bus simultaneously, so this function assumes a
 * single DS18B20 is connected (multi-drop requires Match ROM).
 *
 * The conversion takes up to 750 ms in the default 12-bit
 * resolution mode.  The caller must wait at least this long
 * before calling tiku_kits_sensor_ds18b20_read() to retrieve
 * the result.  Reading before conversion completes will return
 * stale data from the previous conversion (power-on default is
 * +85 C).
 *
 * @return TIKU_KITS_SENSOR_OK if the commands were sent
 *             successfully,
 *         TIKU_KITS_SENSOR_ERR_BUS if the 1-Wire reset failed
 *             (no presence pulse detected)
 */
int tiku_kits_sensor_ds18b20_start_conversion(void);

/**
 * @brief Read the temperature result from the DS18B20 scratchpad
 *
 * Must be called after a conversion has completed (at least 750 ms
 * after tiku_kits_sensor_ds18b20_start_conversion() for 12-bit
 * resolution).  Issues Skip ROM + Read Scratchpad (0xBE), reads
 * all 9 scratchpad bytes, then extracts the 12-bit signed
 * temperature from bytes 0-1.
 *
 * The DS18B20 stores temperature as a 16-bit signed two's
 * complement value with 4 fractional bits (1/16 C per LSB),
 * which maps directly to the common tiku_kits_sensor_temp_t
 * representation.
 *
 * @param temp Pointer to a caller-allocated temperature structure
 *             where the result is written.  Must not be NULL.
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS if the 1-Wire reset or read
 *             fails,
 *         TIKU_KITS_SENSOR_ERR_PARAM if @p temp is NULL
 */
int tiku_kits_sensor_ds18b20_read(tiku_kits_sensor_temp_t *temp);

/**
 * @brief Get the human-readable sensor name string
 *
 * Returns a pointer to a static, null-terminated string identifying
 * this sensor model.  Useful for diagnostic output or building a
 * sensor registry table at runtime.
 *
 * @return Pointer to the constant string "DS18B20" (never NULL)
 */
const char *tiku_kits_sensor_ds18b20_name(void);

#endif /* TIKU_KITS_SENSOR_DS18B20_H_ */
