/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_mcp9808.c - MCP9808 I2C temperature sensor driver
 *
 * Microchip MCP9808 digital temperature sensor driver using the
 * TikuOS I2C bus abstraction.  Reads the 16-bit ambient temperature
 * register and converts the raw two's-complement value to a
 * tiku_kits_sensor_temp_t with integer and fractional (1/16 C)
 * parts.  Typical accuracy is +/-0.5 C from -20 to +100 C.
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
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* REGISTER DEFINITIONS                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Ambient temperature register address.
 *
 * 16-bit read-only register.  Upper byte: [7:5] alert flags,
 * [4] sign, [3:0] integer bits 7-4.  Lower byte: [7:4] integer
 * bits 3-0, [3:0] fractional (1/16 C per LSB).
 */
#define MCP9808_REG_TEMP        0x05

/**
 * @brief Manufacturer ID register address.
 *
 * 16-bit read-only register that returns 0x0054 for Microchip
 * Technology parts.  Used during init to verify the correct
 * sensor is present on the bus.
 */
#define MCP9808_REG_MANUF_ID    0x06

/**
 * @brief Device ID / revision register address.
 *
 * 16-bit read-only register.  Upper byte identifies the device
 * family (0x04 for MCP9808); lower byte holds the silicon
 * revision.
 */
#define MCP9808_REG_DEVICE_ID   0x07

/** Expected Microchip manufacturer ID value (0x0054) */
#define MCP9808_MANUF_ID        0x0054

/** Expected upper byte of the device ID register (MCP9808 family) */
#define MCP9808_DEVICE_ID_UPPER 0x04

/*---------------------------------------------------------------------------*/
/* INTERNAL STATE                                                            */
/*---------------------------------------------------------------------------*/

/** Stored I2C address latched by init, used by all subsequent reads */
static uint8_t sensor_addr;

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read a 16-bit big-endian register from the MCP9808
 *
 * Issues a combined I2C write-read transaction: writes the 1-byte
 * register address, then reads 2 bytes back.  The MCP9808 stores
 * all 16-bit registers in big-endian (MSB-first) order, so the
 * first received byte is shifted up to form the high byte.
 */
static int read_reg16(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    int rc;

    rc = tiku_i2c_write_read(sensor_addr, &reg, 1, buf, 2);
    if (rc != TIKU_I2C_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    /* Reassemble big-endian 16-bit value from the two received bytes */
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize and verify the MCP9808 sensor
 *
 * Latches the I2C address for subsequent reads, then performs a
 * two-register identification check (manufacturer ID followed by
 * device ID) to confirm the correct sensor is present.  If either
 * check fails the address is still stored, but the caller should
 * treat the sensor as unusable.
 */
int tiku_kits_sensor_mcp9808_init(uint8_t addr)
{
    uint16_t id;

    /* Store the address so read_reg16() can use it for all future
     * transactions without the caller needing to pass it again. */
    sensor_addr = addr;

    /* Step 1: verify manufacturer ID (expect Microchip 0x0054) */
    if (read_reg16(MCP9808_REG_MANUF_ID, &id) != TIKU_KITS_SENSOR_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    if (id != MCP9808_MANUF_ID) {
        return TIKU_KITS_SENSOR_ERR_ID;
    }

    /* Step 2: verify device ID (upper byte 0x04 = MCP9808 family) */
    if (read_reg16(MCP9808_REG_DEVICE_ID, &id) != TIKU_KITS_SENSOR_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    if ((id >> 8) != MCP9808_DEVICE_ID_UPPER) {
        return TIKU_KITS_SENSOR_ERR_ID;
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read the ambient temperature from the MCP9808
 *
 * Fetches the 16-bit temperature register and decodes the 13-bit
 * value into integer + fractional form.  The MCP9808 uses a
 * sign+magnitude encoding (bit 12 = sign) rather than two's
 * complement, so the negative path computes the complement of
 * the magnitude fields independently.
 */
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

    /* Mask off alert flag bits [7:5] -- only keep bits [4:0] */
    upper = (uint8_t)(raw >> 8) & 0x1F;
    lower = (uint8_t)(raw & 0xFF);

    if (upper & 0x10) {
        /* Negative temperature: sign bit (bit 4 of upper) is set.
         * MCP9808 uses sign+magnitude, so we subtract from the
         * full-scale range (256 for integer, 16 for fraction). */
        temp->negative = 1;
        upper &= 0x0F;
        temp->integer  = 256 - ((int16_t)(upper << 4) | (lower >> 4));
        temp->frac     = 16 - (lower & 0x0F);
    } else {
        /* Positive temperature: magnitude is directly usable */
        temp->negative = 0;
        upper &= 0x0F;
        temp->integer  = (int16_t)(upper << 4) | (lower >> 4);
        temp->frac     = lower & 0x0F;
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the human-readable sensor name
 *
 * Returns a pointer to a static string literal.  The pointer
 * remains valid for the lifetime of the program.
 */
const char *tiku_kits_sensor_mcp9808_name(void)
{
    return "MCP9808";
}
