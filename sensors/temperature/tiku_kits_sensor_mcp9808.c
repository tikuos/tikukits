/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_mcp9808.c - MCP9808 I2C temperature sensor driver
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Microchip MCP9808 digital temperature sensor driver using the
 * TikuOS I2C bus abstraction.  Reads the 16-bit ambient temperature
 * register and converts the raw two's-complement value to a
 * tiku_kits_sensor_temp_t with integer and fractional (1/16 C)
 * parts.  Typical accuracy is +/-0.5 C from -20 to +100 C.
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

/**
 * @brief Configuration register address.
 *
 * 16-bit register: [8] shutdown, [7] critical lock, [6] window lock,
 * [5] interrupt clear, [4:0] alert output control.
 */
#define MCP9808_REG_CONFIG      0x01

/** Configuration bit: shutdown (low-power, no conversions). */
#define MCP9808_CONFIG_SHDN     0x0100u

/** Configuration bits: critical and window locks; set, they forbid shutdown */
#define MCP9808_CONFIG_LOCKS    0x00C0u

/** Configuration bit: interrupt clear; writing 1 clears the alert. */
#define MCP9808_CONFIG_INT_CLR  0x0020u

/** Resolution register address: 8-bit, [1:0] select 9..12 bits. */
#define MCP9808_REG_RESOLUTION  0x08

/** Resolution field mask of the resolution register. */
#define MCP9808_RES_MASK        0x03

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
 * register address, then reads 2 bytes back.  The MCP9808 stores
 * all 16-bit registers in big-endian (MSB-first) order, so the
 * first received byte is shifted up to form the high byte.
 */

/**
 * @brief Read a 16-bit big-endian register from the MCP9808
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

/*
 * Latches the I2C address for subsequent reads, then performs a
 * two-register identification check (manufacturer ID followed by
 * device ID) to confirm the correct sensor is present.  If either
 * check fails the address is still stored, but the caller should
 * treat the sensor as unusable.
 */

/**
 * @brief Initialize and verify the MCP9808 sensor
 */
int tiku_kits_sensor_mcp9808_init(uint8_t addr)
{
    uint16_t id;

    if (addr < TIKU_KITS_SENSOR_MCP9808_ADDR_MIN ||
        addr > TIKU_KITS_SENSOR_MCP9808_ADDR_MAX) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    sensor_ready = 0;

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

    sensor_ready = 1;
    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

/*
 * Fetches the 16-bit temperature register and decodes the 13-bit
 * value into integer + fractional form. Bit 12 subtracts 256 C
 * from the unsigned value in bits 11:0. Convert the complete
 * fixed-point value before splitting it; the fraction can borrow
 * from the integer part.
 */

/**
 * @brief Read the ambient temperature from the MCP9808
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
        uint16_t magnitude = (uint16_t)(0x2000u - (raw & 0x1FFFu));
        temp->negative = 1;
        temp->integer  = (int16_t)(magnitude >> 4);
        temp->frac     = (uint8_t)(magnitude & 0x0F);
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

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the address of the initialized sensor, 0 if none
 */
uint8_t tiku_kits_sensor_mcp9808_address(void)
{
    return sensor_ready ? sensor_addr : 0;
}

/**
 * @brief Read the conversion resolution, 9..12 bits
 */
int tiku_kits_sensor_mcp9808_get_resolution(uint8_t *bits)
{
    uint8_t reg = MCP9808_REG_RESOLUTION;
    uint8_t value;

    if (bits == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    if (tiku_i2c_write_read(sensor_addr, &reg, 1, &value, 1) !=
        TIKU_I2C_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    *bits = (uint8_t)(TIKU_KITS_SENSOR_MCP9808_RES_MIN +
                      (value & MCP9808_RES_MASK));
    return TIKU_KITS_SENSOR_OK;
}

/**
 * @brief Set the conversion resolution, verified by reading it back
 */
int tiku_kits_sensor_mcp9808_set_resolution(uint8_t bits)
{
    uint8_t tx[2] = { MCP9808_REG_RESOLUTION, 0 };
    uint8_t actual;
    int rc;

    if (bits < TIKU_KITS_SENSOR_MCP9808_RES_MIN ||
        bits > TIKU_KITS_SENSOR_MCP9808_RES_MAX) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    tx[1] = (uint8_t)(bits - TIKU_KITS_SENSOR_MCP9808_RES_MIN);
    if (tiku_i2c_write(sensor_addr, tx, 2) != TIKU_I2C_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    rc = tiku_kits_sensor_mcp9808_get_resolution(&actual);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    return (actual == bits) ? TIKU_KITS_SENSOR_OK : TIKU_KITS_SENSOR_ERR_BUS;
}

/**
 * @brief Read whether the sensor is in shutdown
 */
int tiku_kits_sensor_mcp9808_get_shutdown(uint8_t *enabled)
{
    uint16_t value;
    int rc;

    if (enabled == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    rc = read_reg16(MCP9808_REG_CONFIG, &value);
    if (rc == TIKU_KITS_SENSOR_OK) {
        *enabled = (uint8_t)((value & MCP9808_CONFIG_SHDN) != 0u);
    }
    return rc;
}

/**
 * @brief Enter or leave shutdown, verified by reading it back
 *
 * Alerts and lock bits are left as they are; a set lock refuses shutdown.
 */
int tiku_kits_sensor_mcp9808_set_shutdown(uint8_t enabled)
{
    uint16_t value;
    uint8_t tx[3] = { MCP9808_REG_CONFIG, 0, 0 };
    uint8_t actual;
    int rc;

    if (enabled > 1u) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    rc = read_reg16(MCP9808_REG_CONFIG, &value);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    if (enabled && (value & MCP9808_CONFIG_LOCKS) != 0u) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    /* Interrupt clear is written as 0, so a pending alert stays pending. */
    value = (uint16_t)((value & ~(MCP9808_CONFIG_SHDN |
                                  MCP9808_CONFIG_INT_CLR)) |
                       (enabled ? MCP9808_CONFIG_SHDN : 0u));
    tx[1] = (uint8_t)(value >> 8);
    tx[2] = (uint8_t)value;
    if (tiku_i2c_write(sensor_addr, tx, 3) != TIKU_I2C_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    rc = tiku_kits_sensor_mcp9808_get_shutdown(&actual);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    return (actual == enabled) ? TIKU_KITS_SENSOR_OK
                               : TIKU_KITS_SENSOR_ERR_BUS;
}
