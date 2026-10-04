/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_ds18b20.c - DS18B20 1-Wire temperature sensor driver
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Dallas/Maxim DS18B20 programmable-resolution digital temperature
 * sensor driver using the TikuOS 1-Wire bus abstraction.  Issues a
 * Convert T command, waits for the conversion delay (750 ms at 12-bit
 * resolution), reads the 9-byte scratchpad, validates the CRC, and
 * converts the raw 16-bit two's-complement value to a
 * tiku_kits_sensor_temp_t.  Accuracy is +/-0.5 C from -10 to +85 C.
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

/*
 * Instructs the DS18B20 to begin a temperature conversion.  The
 * sensor writes the result into its internal scratchpad memory
 * once the conversion completes (up to 750 ms at 12-bit
 * resolution).
 */

/**
 * @brief Convert T command byte.
 */
#define DS18B20_CMD_CONVERT_T       0x44

/*
 * Instructs the DS18B20 to transmit all 9 bytes of its scratchpad
 * memory.  Bytes 0-1 contain the temperature; bytes 2-4 hold
 * alarm thresholds and configuration; bytes 5-7 are reserved;
 * byte 8 is a CRC.
 */

/**
 * @brief Read Scratchpad command byte.
 */
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

/**
 * @brief Write Scratchpad command byte: TH, TL and configuration follow.
 */
#define DS18B20_CMD_WRITE_SCRATCHPAD 0x4E

/**
 * @brief Number of bytes in the DS18B20 scratchpad.
 *
 * All 9 bytes are read even though only the first 2 (temperature)
 * are used, because the DS18B20 expects the master to read the
 * full scratchpad or issue a reset to abort.
 */
#define DS18B20_SCRATCHPAD_SIZE     9

/**
 * @brief Scratchpad byte holding the configuration register.
 *
 * Its fixed bits (bit 7 clear, bits 4:0 set) tell a real scratchpad from a
 * data line held low, which reads as nine zero bytes with a valid CRC.
 */
#define DS18B20_CONFIG_BYTE         4
#define DS18B20_CONFIG_FIXED_MASK   0x9F
#define DS18B20_CONFIG_FIXED_BITS   0x1F

/** Scratchpad bytes of the TH and TL alarm registers. */
#define DS18B20_TH_BYTE             2
#define DS18B20_TL_BYTE             3

/** Resolution field of the configuration register: bits 6:5, 9..12 bits. */
#define DS18B20_CONFIG_RES_SHIFT    5
#define DS18B20_CONFIG_RES_MASK     0x03

/*---------------------------------------------------------------------------*/
/* INTERNAL STATE                                                            */
/*---------------------------------------------------------------------------*/

/** 1 after a successful init: a device answered the presence check. */
static uint8_t sensor_ready;

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read the whole scratchpad and validate it.
 *
 * Issues Skip ROM + Read Scratchpad, clocks out all nine bytes and resets
 * the bus.  Rejects a bad CRC and a configuration byte with wrong fixed bits,
 * as nine zero bytes from a data line held low carry a valid CRC.
 *
 * @param scratchpad  Receives the nine scratchpad bytes
 * @return TIKU_KITS_SENSOR_OK, TIKU_KITS_SENSOR_ERR_BUS or
 *         TIKU_KITS_SENSOR_ERR_CRC
 */
static int read_scratchpad(uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE])
{
    uint8_t i;
    uint8_t crc = 0;

    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    /* Skip ROM: address all slaves (single-drop bus assumed) */
    tiku_onewire_write_byte(TIKU_OW_CMD_SKIP_ROM);
    /* Read Scratchpad: request all 9 bytes from the slave */
    tiku_onewire_write_byte(DS18B20_CMD_READ_SCRATCHPAD);

    /* Read all 9 bytes -- the full scratchpad must be clocked out (or a
     * reset issued) to end the transaction. */
    for (i = 0; i < DS18B20_SCRATCHPAD_SIZE; i++) {
        scratchpad[i] = tiku_onewire_read_byte();
    }

    /* Reset bus after reading full scratchpad to release the
     * slave and prepare for the next transaction. */
    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    /* Dallas CRC-8, LSB first: x^8 + x^5 + x^4 + 1. Validate
     * before changing the caller's last known-good measurement. */
    for (i = 0; i < DS18B20_SCRATCHPAD_SIZE - 1; i++) {
        uint8_t bit;
        crc ^= scratchpad[i];
        for (bit = 0; bit < 8; bit++) {
            crc = (uint8_t)((crc >> 1) ^ ((crc & 1) ? 0x8C : 0));
        }
    }
    if (crc != scratchpad[DS18B20_SCRATCHPAD_SIZE - 1]) {
        return TIKU_KITS_SENSOR_ERR_CRC;
    }
    if ((scratchpad[DS18B20_CONFIG_BYTE] & DS18B20_CONFIG_FIXED_MASK) !=
        DS18B20_CONFIG_FIXED_BITS) {
        return TIKU_KITS_SENSOR_ERR_CRC;
    }

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/*
 * Issues a 1-Wire reset pulse.  If a slave responds with a
 * presence pulse the bus is considered ready.  No ROM-level
 * identification is performed -- any 1-Wire slave will satisfy
 * this check.
 */

/**
 * @brief Initialize and verify a DS18B20 is present
 */
int tiku_kits_sensor_ds18b20_init(void)
{
    sensor_ready = 0;
    /* A successful reset means at least one slave pulled the bus
     * low during the presence-detect window. */
    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    sensor_ready = 1;
    return TIKU_KITS_SENSOR_OK;
}

/**
 * @brief Report whether the last init found a device
 */
uint8_t tiku_kits_sensor_ds18b20_ready(void)
{
    return sensor_ready;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Start a temperature conversion on the DS18B20
 *
 * Resets the bus, issues Skip ROM (0xCC) to address all slaves,
 * then sends Convert T (0x44) to begin a conversion.  The caller
 * must wait at least 750 ms before reading the result.
 */
int tiku_kits_sensor_ds18b20_start_conversion(void)
{
    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }

    /* Skip ROM: address all slaves (single-drop bus assumed) */
    tiku_onewire_write_byte(TIKU_OW_CMD_SKIP_ROM);
    /* Convert T: begin temperature conversion */
    tiku_onewire_write_byte(DS18B20_CMD_CONVERT_T);

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

/*
 * Reads and validates the scratchpad, then extracts the signed
 * temperature from bytes 0-1.  The raw value is a 16-bit signed
 * two's complement integer whose lower 4 bits are the fractional
 * part (1/16 C per LSB).
 */

/**
 * @brief Read the temperature result from the DS18B20 scratchpad
 */
int tiku_kits_sensor_ds18b20_read(tiku_kits_sensor_temp_t *temp)
{
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];
    uint8_t res;
    uint16_t raw;
    int rc;

    if (temp == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    rc = read_scratchpad(scratchpad);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    /*
     * DS18B20 temperature format (12-bit, default):
     *   16-bit signed two's complement (little-endian in scratchpad)
     *   Bits [15:11]: sign extension
     *   Bits [10:4]:  integer part (7 bits)
     *   Bits [3:0]:   fractional part (1/16 C per LSB)
     */
    raw = ((uint16_t)scratchpad[1] << 8) | scratchpad[0];
    /* Each bit of resolution below 12 leaves one more low bit of the
     * result undefined: clear them. */
    res = (uint8_t)(TIKU_KITS_SENSOR_DS18B20_RES_MIN +
                    ((scratchpad[DS18B20_CONFIG_BYTE] >>
                      DS18B20_CONFIG_RES_SHIFT) & DS18B20_CONFIG_RES_MASK));
    raw &= (uint16_t)~((1u << (TIKU_KITS_SENSOR_DS18B20_RES_MAX - res)) - 1u);

    if (raw & 0x8000) {
        /* Negative temperature: complement to get absolute
         * magnitude, then split into integer and fraction. */
        temp->negative = 1;
        raw = (uint16_t)(0u - raw);
    } else {
        temp->negative = 0;
    }

    temp->integer = (int16_t)(raw >> 4);
    temp->frac    = (uint8_t)(raw & 0x0F);

    return TIKU_KITS_SENSOR_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the human-readable sensor name
 *
 * Returns a pointer to a static string literal.  The pointer
 * remains valid for the lifetime of the program.
 */
const char *tiku_kits_sensor_ds18b20_name(void)
{
    return "DS18B20";
}

/*---------------------------------------------------------------------------*/

/*
 * The configuration stays in scratchpad RAM: Copy Scratchpad, which would
 * write the EEPROM, is never issued.  Both calls assume a single, externally
 * powered DS18B20 on an initialized 1-Wire bus.
 */

/**
 * @brief Read the conversion resolution from the scratchpad
 */
int tiku_kits_sensor_ds18b20_get_resolution(uint8_t *bits)
{
    uint8_t pad[DS18B20_SCRATCHPAD_SIZE];
    int rc;

    if (bits == NULL) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    rc = read_scratchpad(pad);
    if (rc == TIKU_KITS_SENSOR_OK) {
        *bits = (uint8_t)(TIKU_KITS_SENSOR_DS18B20_RES_MIN +
                          ((pad[DS18B20_CONFIG_BYTE] >>
                            DS18B20_CONFIG_RES_SHIFT) &
                           DS18B20_CONFIG_RES_MASK));
    }
    return rc;
}

/**
 * @brief Set the conversion resolution, keeping the alarm bytes
 */
int tiku_kits_sensor_ds18b20_set_resolution(uint8_t bits)
{
    uint8_t pad[DS18B20_SCRATCHPAD_SIZE];
    uint8_t actual[DS18B20_SCRATCHPAD_SIZE];
    uint8_t expected;
    int rc;

    if (bits < TIKU_KITS_SENSOR_DS18B20_RES_MIN ||
        bits > TIKU_KITS_SENSOR_DS18B20_RES_MAX) {
        return TIKU_KITS_SENSOR_ERR_PARAM;
    }
    if (!sensor_ready) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    rc = read_scratchpad(pad);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    expected = (uint8_t)(DS18B20_CONFIG_FIXED_BITS |
                         ((bits - TIKU_KITS_SENSOR_DS18B20_RES_MIN) <<
                          DS18B20_CONFIG_RES_SHIFT));
    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    tiku_onewire_write_byte(TIKU_OW_CMD_SKIP_ROM);
    tiku_onewire_write_byte(DS18B20_CMD_WRITE_SCRATCHPAD);
    tiku_onewire_write_byte(pad[DS18B20_TH_BYTE]);
    tiku_onewire_write_byte(pad[DS18B20_TL_BYTE]);
    tiku_onewire_write_byte(expected);

    /* Read back: the write has no acknowledgement of its own. */
    rc = read_scratchpad(actual);
    if (rc != TIKU_KITS_SENSOR_OK) {
        return rc;
    }
    if (actual[DS18B20_TH_BYTE] != pad[DS18B20_TH_BYTE] ||
        actual[DS18B20_TL_BYTE] != pad[DS18B20_TL_BYTE] ||
        actual[DS18B20_CONFIG_BYTE] != expected) {
        return TIKU_KITS_SENSOR_ERR_BUS;
    }
    return TIKU_KITS_SENSOR_OK;
}
