/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_ds18b20.c - DS18B20 1-Wire temperature sensor driver
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_sensor_ds18b20.h"
#include <interfaces/onewire/tiku_onewire.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* DS18B20 COMMAND DEFINITIONS                                               */
/*---------------------------------------------------------------------------*/

#define DS18B20_CMD_CONVERT_T       0x44  /**< Start temperature conversion */
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE  /**< Read scratchpad memory */

/** Number of scratchpad bytes */
#define DS18B20_SCRATCHPAD_SIZE     9

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_sensor_ds18b20_init(void)
{
    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sensor_ds18b20_start_conversion(void)
{
    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    tiku_onewire_write_byte(TIKU_OW_CMD_SKIP_ROM);
    tiku_onewire_write_byte(DS18B20_CMD_CONVERT_T);

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sensor_ds18b20_read(tiku_kits_sensor_temp_t *temp)
{
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];
    uint8_t i;
    int16_t raw;

    if (temp == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }

    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    tiku_onewire_write_byte(TIKU_OW_CMD_SKIP_ROM);
    tiku_onewire_write_byte(DS18B20_CMD_READ_SCRATCHPAD);

    /* Read all 9 bytes of scratchpad */
    for (i = 0; i < DS18B20_SCRATCHPAD_SIZE; i++) {
        scratchpad[i] = tiku_onewire_read_byte();
    }

    /* Reset bus after reading full scratchpad */
    tiku_onewire_reset();

    /*
     * DS18B20 temperature format (12-bit, default):
     *   16-bit signed two's complement
     *   Bits [15:11]: sign extension
     *   Bits [10:4]:  integer part
     *   Bits [3:0]:   fractional part (1/16 C per LSB)
     */
    raw = ((int16_t)scratchpad[1] << 8) | scratchpad[0];

    if (raw & 0x8000) {
        temp->negative = 1;
        raw = (~raw) + 1;
    } else {
        temp->negative = 0;
    }

    temp->integer = (int16_t)(raw >> 4);
    temp->frac    = (uint8_t)(raw & 0x0F);

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

const char *tiku_kits_sensor_ds18b20_name(void)
{
    return "DS18B20";
}
