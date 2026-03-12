/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_mcp9808.c - MCP9808 I2C temperature sensor driver
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

#include "tiku_kits_sensor_mcp9808.h"
#include <interfaces/bus/tiku_i2c_bus.h>

/*---------------------------------------------------------------------------*/
/* REGISTER DEFINITIONS                                                      */
/*---------------------------------------------------------------------------*/

#define MCP9808_REG_TEMP        0x05    /**< Ambient temperature register */
#define MCP9808_REG_MANUF_ID    0x06    /**< Manufacturer ID register */
#define MCP9808_REG_DEVICE_ID   0x07    /**< Device ID register */

#define MCP9808_MANUF_ID        0x0054  /**< Expected manufacturer ID */
#define MCP9808_DEVICE_ID_UPPER 0x04    /**< Expected device ID upper byte */

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

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_sensor_mcp9808_init(uint8_t addr)
{
    uint16_t id;

    sensor_addr = addr;

    /* Verify manufacturer ID */
    if (read_reg16(MCP9808_REG_MANUF_ID, &id) != TIKU_KITS_SENSOR_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    if (id != MCP9808_MANUF_ID) {
        return TIKU_KITS_SENSOR_ERR_ID;
    }

    /* Verify device ID */
    if (read_reg16(MCP9808_REG_DEVICE_ID, &id) != TIKU_KITS_SENSOR_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    if ((id >> 8) != MCP9808_DEVICE_ID_UPPER) {
        return TIKU_KITS_SENSOR_ERR_ID;
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sensor_mcp9808_read(tiku_kits_sensor_temp_t *temp)
{
    uint16_t raw;
    uint8_t upper;
    uint8_t lower;

    if (temp == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }

    if (read_reg16(MCP9808_REG_TEMP, &raw) != TIKU_KITS_SENSOR_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    /*
     * MCP9808 temperature register format (16-bit, big-endian):
     *   Upper byte: [7:5] alert flags, [4] sign, [3:0] integer bits 7:4
     *   Lower byte: [7:4] integer bits 3:0, [3:0] fractional (1/16 C)
     */
    upper = (uint8_t)(raw >> 8) & 0x1F;
    lower = (uint8_t)(raw & 0xFF);

    if (upper & 0x10) {
        /* Negative temperature */
        temp->negative = 1;
        upper &= 0x0F;
        temp->integer  = 256 - ((int16_t)(upper << 4) | (lower >> 4));
        temp->frac     = 16 - (lower & 0x0F);
    } else {
        temp->negative = 0;
        upper &= 0x0F;
        temp->integer  = (int16_t)(upper << 4) | (lower >> 4);
        temp->frac     = lower & 0x0F;
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

const char *tiku_kits_sensor_mcp9808_name(void)
{
    return "MCP9808";
}
