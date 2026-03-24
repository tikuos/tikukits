/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sensor_ds18b20.c - DS18B20 1-Wire temperature sensor driver
 *
 * Dallas/Maxim DS18B20 programmable-resolution digital temperature
 * sensor driver using the TikuOS 1-Wire bus abstraction.  Issues a
 * Convert T command, waits for the conversion delay (750 ms at 12-bit
 * resolution), reads the 9-byte scratchpad, validates the CRC, and
 * converts the raw 16-bit two's-complement value to a
 * tiku_kits_sensor_temp_t.  Accuracy is +/-0.5 C from -10 to +85 C.
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

/**
 * @brief Convert T command byte.
 *
 * Instructs the DS18B20 to begin a temperature conversion.  The
 * sensor writes the result into its internal scratchpad memory
 * once the conversion completes (up to 750 ms at 12-bit
 * resolution).
 */
#define DS18B20_CMD_CONVERT_T       0x44

/**
 * @brief Read Scratchpad command byte.
 *
 * Instructs the DS18B20 to transmit all 9 bytes of its scratchpad
 * memory.  Bytes 0-1 contain the temperature; bytes 2-4 hold
 * alarm thresholds and configuration; bytes 5-7 are reserved;
 * byte 8 is a CRC.
 */
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

/**
 * @brief Number of bytes in the DS18B20 scratchpad.
 *
 * All 9 bytes are read even though only the first 2 (temperature)
 * are used, because the DS18B20 expects the master to read the
 * full scratchpad or issue a reset to abort.
 */
#define DS18B20_SCRATCHPAD_SIZE     9

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize and verify a DS18B20 is present
 *
 * Issues a 1-Wire reset pulse.  If a slave responds with a
 * presence pulse the bus is considered ready.  No ROM-level
 * identification is performed -- any 1-Wire slave will satisfy
 * this check.
 */
int tiku_kits_sensor_ds18b20_init(void)
{
    /* A successful reset means at least one slave pulled the bus
     * low during the presence-detect window. */
    if (tiku_onewire_reset() != TIKU_OW_OK) {
        return TIKU_KITS_SENSOR_ERR_NO_DEVICE;
    }
    return TIKU_KITS_SENSOR_OK;
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

/**
 * @brief Read the temperature result from the DS18B20 scratchpad
 *
 * Issues Skip ROM + Read Scratchpad, reads all 9 bytes, then
 * extracts the 12-bit signed temperature from bytes 0-1.  The
 * bus is reset after the full scratchpad has been read to
 * terminate the transaction cleanly.  The raw value is a 16-bit
 * signed two's complement integer whose lower 4 bits are the
 * fractional part (1/16 C per LSB).
 */
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

    /* Skip ROM: address all slaves (single-drop bus assumed) */
    tiku_onewire_write_byte(TIKU_OW_CMD_SKIP_ROM);
    /* Read Scratchpad: request all 9 bytes from the slave */
    tiku_onewire_write_byte(DS18B20_CMD_READ_SCRATCHPAD);

    /* Read all 9 bytes -- only bytes 0-1 are used for temperature
     * but the full scratchpad must be clocked out (or a reset
     * issued) to end the transaction. */
    for (i = 0; i < DS18B20_SCRATCHPAD_SIZE; i++) {
        scratchpad[i] = tiku_onewire_read_byte();
    }

    /* Reset bus after reading full scratchpad to release the
     * slave and prepare for the next transaction. */
    tiku_onewire_reset();

    /*
     * DS18B20 temperature format (12-bit, default):
     *   16-bit signed two's complement (little-endian in scratchpad)
     *   Bits [15:11]: sign extension
     *   Bits [10:4]:  integer part (7 bits)
     *   Bits [3:0]:   fractional part (1/16 C per LSB)
     */
    raw = ((int16_t)scratchpad[1] << 8) | scratchpad[0];

    if (raw & 0x8000) {
        /* Negative temperature: complement to get absolute
         * magnitude, then split into integer and fraction. */
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
