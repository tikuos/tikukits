/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_adt7410.c - ADT7410 I2C temperature sensor driver
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

#include "tiku_kits_sensor_adt7410.h"
#include <interfaces/bus/tiku_i2c_bus.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* REGISTER DEFINITIONS                                                      */
/*---------------------------------------------------------------------------*/

#define ADT7410_REG_TEMP        0x00    /**< Temperature register (16-bit) */
#define ADT7410_REG_ID          0x0B    /**< ID register (8-bit) */

#define ADT7410_ID_MASK         0xF8    /**< Upper 5 bits = manufacturer */
#define ADT7410_ID_EXPECTED     0xC8    /**< 11001xxx = Analog Devices */

/*---------------------------------------------------------------------------*/
/* INTERNAL STATE                                                            */
/*---------------------------------------------------------------------------*/

/** Stored I2C address from init */
static uint8_t sensor_addr;

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read a 16-bit big-endian register
 */
static int read_reg16(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    int rc;

    rc = tiku_i2c_write_read(sensor_addr, &reg, 1, buf, 2);
    if (rc != TIKU_I2C_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return TIKU_KITS_SENSOR_OK;
}

/**
 * @brief Read an 8-bit register
 */
static int read_reg8(uint8_t reg, uint8_t *value)
{
    int rc;

    rc = tiku_i2c_write_read(sensor_addr, &reg, 1, value, 1);
    if (rc != TIKU_I2C_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_sensor_adt7410_init(uint8_t addr)
{
    uint8_t id;

    sensor_addr = addr;

    /* Verify device ID */
    if (read_reg8(ADT7410_REG_ID, &id) != TIKU_KITS_SENSOR_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    if ((id & ADT7410_ID_MASK) != ADT7410_ID_EXPECTED) {
        return TIKU_KITS_SENSOR_ERR_ID;
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sensor_adt7410_read(tiku_kits_sensor_temp_t *temp)
{
    uint16_t raw;

    if (temp == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }

    if (read_reg16(ADT7410_REG_TEMP, &raw) != TIKU_KITS_SENSOR_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    /*
     * ADT7410 temperature register (13-bit mode, default):
     *   Bits [15:3] = temperature (two's complement)
     *   Bits [2:0]  = status flags
     *   Resolution: 0.0625 C per LSB
     */
    if (raw & 0x8000) {
        /* Negative temperature: two's complement */
        uint16_t abs_val = ((~raw) + 1) >> 3;
        temp->negative = 1;
        temp->integer  = (int16_t)(abs_val >> 4);
        temp->frac     = (uint8_t)(abs_val & 0x0F);
    } else {
        uint16_t val = raw >> 3;
        temp->negative = 0;
        temp->integer  = (int16_t)(val >> 4);
        temp->frac     = (uint8_t)(val & 0x0F);
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

const char *tiku_kits_sensor_adt7410_name(void)
{
    return "ADT7410";
}
