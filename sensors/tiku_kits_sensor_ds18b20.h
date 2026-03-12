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

#include "tiku_kits_sensor.h"

/*---------------------------------------------------------------------------*/
/* FUNCTION PROTOTYPES                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize and verify a DS18B20 is present
 *
 * Issues a 1-Wire reset and checks for a presence pulse.
 * The 1-Wire bus must already be initialized.
 *
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_NO_DEVICE if no presence detected
 */
int tiku_kits_sensor_ds18b20_init(void);

/**
 * @brief Start a temperature conversion
 *
 * Issues Skip ROM + Convert T command. The caller must wait
 * at least 750 ms before calling tiku_kits_sensor_ds18b20_read().
 *
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS on 1-Wire error
 */
int tiku_kits_sensor_ds18b20_start_conversion(void);

/**
 * @brief Read the temperature result from the scratchpad
 *
 * Must be called after conversion is complete (>= 750 ms after
 * start_conversion). Reads the 9-byte scratchpad and extracts
 * the 12-bit temperature value.
 *
 * @param temp Output: temperature reading
 * @return TIKU_KITS_SENSOR_OK on success,
 *         TIKU_KITS_SENSOR_ERR_BUS on 1-Wire error,
 *         TIKU_KITS_SENSOR_ERR_PARAM if temp is NULL
 */
int tiku_kits_sensor_ds18b20_read(tiku_kits_sensor_temp_t *temp);

/**
 * @brief Get the sensor name string
 * @return "DS18B20"
 */
const char *tiku_kits_sensor_ds18b20_name(void);

#endif /* TIKU_KITS_SENSOR_DS18B20_H_ */
