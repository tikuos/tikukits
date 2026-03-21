/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_crc.c - CRC-32 and CRC-16/CCITT for embedded systems
 *
 * CRC-32 uses a 16-entry nibble table (64 bytes of const data) to
 * process 4 bits per iteration -- a good trade-off between the full
 * 256-entry table (1 KB) and pure bit-by-bit (8x slower).
 *
 * CRC-16/CCITT-FALSE is computed bit-by-bit (no table at all) since
 * the 16-bit arithmetic is fast enough on MSP430 and saves every
 * byte of RAM.
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

#include "tiku_kits_crypto_crc.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* CRC-32 NIBBLE TABLE                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Precomputed CRC-32 table for 4-bit (nibble) lookups.
 *
 * Polynomial: 0xEDB88320 (reflected representation of 0x04C11DB7).
 * Each entry covers one nibble value (0x0..0xF), yielding 16 entries
 * at 4 bytes each = 64 bytes total.
 */
static const uint32_t crc32_nibble_table[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26178UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL
};

/*---------------------------------------------------------------------------*/
/* CRC-32                                                                    */
/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_crc32(const uint8_t *data,
                            uint16_t len,
                            uint32_t *crc_out)
{
    uint32_t crc;
    uint16_t i;

    if (data == NULL || crc_out == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    crc = 0xFFFFFFFFUL;

    for (i = 0; i < len; i++) {
        /* Process low nibble */
        crc = crc32_nibble_table[(crc ^ data[i]) & 0x0F] ^
              (crc >> 4);
        /* Process high nibble */
        crc = crc32_nibble_table[(crc ^ (data[i] >> 4)) & 0x0F] ^
              (crc >> 4);
    }

    *crc_out = crc ^ 0xFFFFFFFFUL;

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* CRC-16/CCITT-FALSE                                                        */
/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_crc16_ccitt(const uint8_t *data,
                                  uint16_t len,
                                  uint16_t *crc_out)
{
    uint16_t crc;
    uint16_t i;
    uint8_t bit;

    if (data == NULL || crc_out == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    crc = 0xFFFF;

    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }

    *crc_out = crc;

    return TIKU_KITS_CRYPTO_OK;
}
