/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_adt7410.c - ADT7410 I2C temperature sensor driver
 *
 * Analog Devices ADT7410 high-accuracy digital temperature sensor
 * driver using the TikuOS I2C bus abstraction.  Reads the 16-bit
 * temperature register (13-bit mode by default) and converts the
 * raw two's-complement value to a tiku_kits_sensor_temp_t.
 * Typical accuracy is +/-0.5 C from -40 to +105 C.
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

/**
 * @brief Temperature register address.
 *
 * 16-bit read-only register containing the latest conversion
 * result.  In 13-bit mode (default), bits [15:3] hold the
 * temperature in two's complement with 0.0625 C/LSB and
 * bits [2:0] are status flags.
 */
#define ADT7410_REG_TEMP        0x00

/**
 * @brief ID register address.
 *
 * 8-bit read-only register.  Upper 5 bits identify the
 * manufacturer (Analog Devices = 0xC8 when masked); lower 3
 * bits hold the silicon revision.
 */
#define ADT7410_REG_ID          0x0B

/** Mask for the manufacturer identification bits (upper 5 bits) */
#define ADT7410_ID_MASK         0xF8

/** Expected manufacturer code after masking (Analog Devices, 11001xxx) */
#define ADT7410_ID_EXPECTED     0xC8

/*---------------------------------------------------------------------------*/
/* INTERNAL STATE                                                            */
/*---------------------------------------------------------------------------*/

/** Stored I2C address latched by init, used by all subsequent reads */
static uint8_t sensor_addr;

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read a 16-bit big-endian register from the ADT7410
 *
 * Issues a combined I2C write-read transaction: writes the 1-byte
 * register address, then reads 2 bytes back.  The ADT7410 stores
 * multi-byte registers in big-endian (MSB-first) order, so the
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

/**
 * @brief Read a single 8-bit register from the ADT7410
 *
 * Issues a combined I2C write-read transaction: writes the 1-byte
 * register address, then reads 1 byte back.  Used for the ID
 * register during initialization.
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

/**
 * @brief Initialize and verify the ADT7410 sensor
 *
 * Latches the I2C address for subsequent reads, then reads the
 * 8-bit ID register and checks the upper 5 bits against the
 * Analog Devices manufacturer code.  The lower 3 bits (silicon
 * revision) are masked off and ignored.
 */
int tiku_kits_sensor_adt7410_init(uint8_t addr)
{
    uint8_t id;

    /* Store the address so read helpers can use it for all future
     * transactions without the caller needing to pass it again. */
    sensor_addr = addr;

    /* Verify manufacturer code in the upper 5 bits of the ID reg.
     * Lower 3 bits are silicon revision and are intentionally
     * ignored to support all ADT7410 steppings. */
    if (read_reg8(ADT7410_REG_ID, &id) != TIKU_KITS_SENSOR_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    if ((id & ADT7410_ID_MASK) != ADT7410_ID_EXPECTED) {
        return TIKU_KITS_SENSOR_ERR_ID;
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read the ambient temperature from the ADT7410
 *
 * Fetches the 16-bit temperature register and extracts the 13-bit
 * value (bits [15:3]) in default resolution mode.  The ADT7410
 * uses standard two's complement encoding.  For negative values
 * the raw register is complemented and the absolute magnitude is
 * right-shifted by 3 to discard the status flags before splitting
 * into integer and fractional parts.
 */
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
        /* Negative temperature: invert and add 1 to get absolute
         * value in two's complement, then shift right by 3 to
         * discard status flag bits. */
        uint16_t abs_val = ((~raw) + 1) >> 3;
        temp->negative = 1;
        temp->integer  = (int16_t)(abs_val >> 4);
        temp->frac     = (uint8_t)(abs_val & 0x0F);
    } else {
        /* Positive temperature: shift right by 3 to remove status
         * bits, leaving 13-bit magnitude directly. */
        uint16_t val = raw >> 3;
        temp->negative = 0;
        temp->integer  = (int16_t)(val >> 4);
        temp->frac     = (uint8_t)(val & 0x0F);
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
const char *tiku_kits_sensor_adt7410_name(void)
{
    return "ADT7410";
}
