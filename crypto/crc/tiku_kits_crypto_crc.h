/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_crc.h - CRC-32 and CRC-16/CCITT for embedded systems
 *
 * Provides two widely used CRC algorithms optimized for ultra-low-power
 * microcontrollers:
 *   - CRC-32 (IEEE 802.3) using a 16-entry nibble table (64 bytes RAM)
 *   - CRC-16/CCITT-FALSE using bit-by-bit computation (zero table RAM)
 *
 * Zero heap allocation.  All state is stack-local.
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

#ifndef TIKU_KITS_CRYPTO_CRC_H_
#define TIKU_KITS_CRYPTO_CRC_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_crypto.h"

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute IEEE 802.3 CRC-32 over a data buffer.
 *
 * Uses the standard reflected polynomial 0xEDB88320 with a 16-entry
 * nibble lookup table (64 bytes) to balance code size and speed on
 * resource-constrained MCUs.
 *
 * @param[in]  data     Input data buffer
 * @param[in]  len      Length of data in bytes
 * @param[out] crc_out  Pointer to uint32_t receiving the CRC-32 value
 *
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if data or crc_out is NULL
 */
int tiku_kits_crypto_crc32(const uint8_t *data,
                            uint16_t len,
                            uint32_t *crc_out);

/**
 * @brief Compute CRC-16/CCITT-FALSE over a data buffer.
 *
 * Parameters: polynomial 0x1021, initial value 0xFFFF, no final XOR,
 * no input/output reflection (CCITT-FALSE variant).  Computed
 * bit-by-bit to avoid any lookup table, saving RAM on MSP430.
 *
 * @param[in]  data     Input data buffer
 * @param[in]  len      Length of data in bytes
 * @param[out] crc_out  Pointer to uint16_t receiving the CRC-16 value
 *
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if data or crc_out is NULL
 */
int tiku_kits_crypto_crc16_ccitt(const uint8_t *data,
                                  uint16_t len,
                                  uint16_t *crc_out);

#endif /* TIKU_KITS_CRYPTO_CRC_H_ */
