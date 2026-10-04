/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_adt7410.c - ADT7410 I2C temperature sensor driver
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Analog Devices ADT7410 high-accuracy digital temperature sensor
 * driver using the TikuOS I2C bus abstraction.  Reads the 16-bit
 * temperature register (13-bit mode by default) and converts the
 * raw two's-complement value to a tiku_kits_sensor_temp_t.
 * Typical accuracy is +/-0.5 C from -40 to +105 C.
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

/*
 * 16-bit read-only register containing the latest conversion
 * result.  In 13-bit mode (default), bits [15:3] hold the
 * temperature in two's complement with 0.0625 C/LSB and
 * bits [2:0] are status flags.
 */

/**
 * @brief Temperature register address.
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

/**
 * @brief Configuration register address.
 *
 * 8-bit register: [7] resolution (1 = 16-bit), [6:5] operation mode
 * (00 continuous, 11 shutdown), [4:0] alert and fault settings.
 */
#define ADT7410_REG_CONFIG      0x03

/** Configuration bit: 16-bit resolution; clear selects 13-bit. */
#define ADT7410_CONFIG_RES16    0x80

/** Configuration field: operation mode; both bits set is shutdown. */
#define ADT7410_CONFIG_MODE     0x60

/** Status bits [2:0] of the temperature register in 13-bit mode. */
#define ADT7410_TEMP_FLAGS      0x0007u

/** LSBs per degree Celsius of the 16-bit temperature value (1/128 C). */
#define ADT7410_LSB_PER_C       128L

/*---------------------------------------------------------------------------*/
/* INTERNAL STATE                                                            */
/*---------------------------------------------------------------------------*/

/** Stored I2C address latched by init, used by all subsequent reads */
static uint8_t sensor_addr;

/** 1 after a successful init: the device at sensor_addr identified itself */
static uint8_t sensor_ready;

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/*
 * Issues a combined I2C write-read transaction: writes the 1-byte
 * register address, then reads 2 bytes back.  The ADT7410 stores
 * multi-byte registers in big-endian (MSB-first) order, so the
 * first received byte is shifted up to form the high byte.
 */

/**
 * @brief Read a 16-bit big-endian register from the ADT7410
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

/*
 * Latches the I2C address for subsequent reads, then reads the
 * 8-bit ID register and checks the upper 5 bits against the
 * Analog Devices manufacturer code.  The lower 3 bits (silicon
 * revision) are masked off and ignored.
 */

/**
 * @brief Initialize and verify the ADT7410 sensor
 */
int tiku_kits_sensor_adt7410_init(uint8_t addr)
{
    uint8_t id;

    if (addr < TIKU_KITS_SENSOR_ADT7410_ADDR_MIN ||
        addr > TIKU_KITS_SENSOR_ADT7410_ADDR_MAX) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    sensor_ready = 0;

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

    sensor_ready = 1;
    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

/*
 * Fetches the 16-bit temperature register and extracts the 13-bit
 * value (bits [15:3]) in default resolution mode.  The ADT7410
 * uses standard two's complement encoding.  For negative values
 * the raw register is complemented and the absolute magnitude is
 * right-shifted by 3 to discard the status flags before splitting
 * into integer and fractional parts.
 */

/**
 * @brief Read the ambient temperature from the ADT7410
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
        /* Discard non-temperature bits before negating the 13-bit
         * value. Unsigned subtraction also avoids host/16-bit int
         * promotion differences in ~raw and right shifts. */
        uint16_t abs_val = (uint16_t)(0x2000u - (raw >> 3));
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

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the address of the initialized sensor, 0 if none
 */
uint8_t tiku_kits_sensor_adt7410_address(void)
{
    return sensor_ready ? sensor_addr : 0;
}

/**
 * @brief Read the conversion resolution, 13 or 16 bits
 */
int tiku_kits_sensor_adt7410_get_resolution(uint8_t *bits)
{
    uint8_t value;
    int rc;

    if (bits == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    rc = read_reg8(ADT7410_REG_CONFIG, &value);
    if (rc == TIKU_KITS_SENSOR_OK) {
        *bits = (value & ADT7410_CONFIG_RES16) ?
                TIKU_KITS_SENSOR_ADT7410_RES_HIGH :
                TIKU_KITS_SENSOR_ADT7410_RES_LOW;
    }
    return rc;
}

/**
 * @brief Replace the configuration bits in @p mask, verified by read-back
 *
 * @param mask  Configuration bits to change
 * @param bits  Their new value, within @p mask
 * @return TIKU_KITS_SENSOR_OK, TIKU_KITS_SENSOR_ERR_NO_DEVICE before a
 *         successful init, or TIKU_KITS_SENSOR_ERR_BUS
 */
static int config_update(uint8_t mask, uint8_t bits)
{
    uint8_t old;
    uint8_t actual;
    uint8_t tx[2] = { ADT7410_REG_CONFIG, 0 };
    int rc;

    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    rc = read_reg8(ADT7410_REG_CONFIG, &old);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    tx[1] = (uint8_t)((old & ~mask) | bits);
    if (tiku_i2c_write(sensor_addr, tx, 2) != TIKU_I2C_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    rc = read_reg8(ADT7410_REG_CONFIG, &actual);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    return (actual == tx[1]) ? TIKU_KITS_SENSOR_OK : TIKU_KITS_SENSOR_ERR_BUS;
}

/**
 * @brief Set the conversion resolution, 13 or 16 bits
 */
int tiku_kits_sensor_adt7410_set_resolution(uint8_t bits)
{
    if (bits != TIKU_KITS_SENSOR_ADT7410_RES_LOW &&
        bits != TIKU_KITS_SENSOR_ADT7410_RES_HIGH) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    return config_update(ADT7410_CONFIG_RES16,
                         (bits == TIKU_KITS_SENSOR_ADT7410_RES_HIGH) ?
                         ADT7410_CONFIG_RES16 : 0);
}

/**
 * @brief Read whether the sensor is in shutdown
 */
int tiku_kits_sensor_adt7410_get_shutdown(uint8_t *enabled)
{
    uint8_t value;
    int rc;

    if (enabled == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    rc = read_reg8(ADT7410_REG_CONFIG, &value);
    if (rc == TIKU_KITS_SENSOR_OK) {
        *enabled = (uint8_t)((value & ADT7410_CONFIG_MODE) ==
                             ADT7410_CONFIG_MODE);
    }
    return rc;
}

/**
 * @brief Enter shutdown (1) or continuous conversion (0)
 */
int tiku_kits_sensor_adt7410_set_shutdown(uint8_t enabled)
{
    if (enabled > 1u) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    return config_update(ADT7410_CONFIG_MODE,
                         enabled ? ADT7410_CONFIG_MODE : 0);
}

/*---------------------------------------------------------------------------*/

/*
 * Reads the configuration first to know the resolution: in 13-bit mode the
 * three status bits are cleared, which leaves the value in the same 1/128 C
 * units as 16-bit mode.  The conversion rounds half away from zero.
 */

/**
 * @brief Read the temperature in millidegrees Celsius
 */
int tiku_kits_sensor_adt7410_read_mc(int32_t *millidegrees)
{
    uint8_t config;
    uint16_t raw;
    int32_t units;
    int rc;

    if (millidegrees == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    rc = read_reg8(ADT7410_REG_CONFIG, &config);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    rc = read_reg16(ADT7410_REG_TEMP, &raw);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    if (!(config & ADT7410_CONFIG_RES16)) {
        raw &= (uint16_t)~ADT7410_TEMP_FLAGS;
    }
    units = (raw & 0x8000u) ? (int32_t)raw - 65536L : (int32_t)raw;
    if (units < 0) {
        *millidegrees = -((-units * 1000L + ADT7410_LSB_PER_C / 2) /
                          ADT7410_LSB_PER_C);
    } else {
        *millidegrees = (units * 1000L + ADT7410_LSB_PER_C / 2) /
                        ADT7410_LSB_PER_C;
    }
    return TIKU_KITS_SENSOR_OK;
}
