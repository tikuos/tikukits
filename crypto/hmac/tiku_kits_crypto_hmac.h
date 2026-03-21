/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_hmac.h - HMAC-SHA256 for embedded systems
 *
 * Implements HMAC (RFC 2104) using SHA-256 as the underlying hash
 * function.  Designed for ultra-low-power microcontrollers with
 * zero heap allocation -- all intermediate state lives on the stack
 * or in static local arrays.
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

#ifndef TIKU_KITS_CRYPTO_HMAC_H_
#define TIKU_KITS_CRYPTO_HMAC_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_crypto.h"
#include "../sha256/tiku_kits_crypto_sha256.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** @brief HMAC-SHA256 output size in bytes. */
#define TIKU_KITS_CRYPTO_HMAC_SHA256_SIZE  32

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute HMAC-SHA256 over a message in one shot.
 *
 * Implements RFC 2104 HMAC using SHA-256:
 *   MAC = H(K' XOR opad || H(K' XOR ipad || message))
 *
 * If the key is longer than the SHA-256 block size (64 bytes), it is
 * first hashed to 32 bytes.  Keys shorter than 64 bytes are zero-padded.
 *
 * @param[in]  key       HMAC key
 * @param[in]  key_len   Key length in bytes
 * @param[in]  data      Message data to authenticate
 * @param[in]  data_len  Message length in bytes
 * @param[out] mac       Output buffer for the 32-byte MAC
 *
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if any pointer is NULL
 */
int tiku_kits_crypto_hmac_sha256(const uint8_t *key,
                                  uint16_t key_len,
                                  const uint8_t *data,
                                  uint16_t data_len,
                                  uint8_t *mac);

#endif /* TIKU_KITS_CRYPTO_HMAC_H_ */
