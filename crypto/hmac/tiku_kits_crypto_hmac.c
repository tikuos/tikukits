/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_hmac.c - HMAC-SHA256 for embedded systems
 *
 * Implements RFC 2104 HMAC-SHA256.  Zero heap allocation -- all
 * scratch buffers are static locals, keeping the stack footprint
 * predictable on MSP430.
 *
 * Algorithm (RFC 2104):
 *   1. If key > block size (64), replace key with SHA-256(key)
 *   2. Pad key to block size with zeros
 *   3. ipad = key_pad XOR 0x36 (repeated)
 *   4. opad = key_pad XOR 0x5C (repeated)
 *   5. inner = SHA-256(ipad || message)
 *   6. MAC   = SHA-256(opad || inner)
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

#include "tiku_kits_crypto_hmac.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* PRIVATE CONSTANTS                                                         */
/*---------------------------------------------------------------------------*/

/** SHA-256 block size in bytes (512 bits). */
#define HMAC_BLOCK_SIZE  64

/** Inner padding byte (RFC 2104). */
#define HMAC_IPAD        0x36

/** Outer padding byte (RFC 2104). */
#define HMAC_OPAD        0x5C

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_hmac_sha256(const uint8_t *key,
                                  uint16_t key_len,
                                  const uint8_t *data,
                                  uint16_t data_len,
                                  uint8_t *mac)
{
    static uint8_t key_pad[HMAC_BLOCK_SIZE];
    static uint8_t inner_digest[TIKU_KITS_CRYPTO_SHA256_DIGEST_SIZE];
    tiku_kits_crypto_sha256_ctx_t ctx;
    uint8_t i;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (key == NULL || data == NULL || mac == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* Step 1: Prepare the block-sized key                            */
    /*   - If key > block size, hash it down to 32 bytes              */
    /*   - Otherwise, copy as-is                                      */
    /*   - Zero-pad to HMAC_BLOCK_SIZE                                */
    /* ---------------------------------------------------------------*/

    memset(key_pad, 0, HMAC_BLOCK_SIZE);

    if (key_len > HMAC_BLOCK_SIZE) {
        /* Hash the long key: key_pad = SHA-256(key) */
        tiku_kits_crypto_sha256_init(&ctx);
        tiku_kits_crypto_sha256_update(&ctx, key, key_len);
        tiku_kits_crypto_sha256_final(&ctx, key_pad);
        /* Remaining bytes already zeroed by memset above */
    } else {
        memcpy(key_pad, key, key_len);
    }

    /* ---------------------------------------------------------------*/
    /* Step 2: Inner hash — H(ipad || message)                        */
    /*   XOR key_pad with ipad in-place, hash, then restore           */
    /* ---------------------------------------------------------------*/

    for (i = 0; i < HMAC_BLOCK_SIZE; i++) {
        key_pad[i] ^= HMAC_IPAD;
    }

    tiku_kits_crypto_sha256_init(&ctx);
    tiku_kits_crypto_sha256_update(&ctx, key_pad, HMAC_BLOCK_SIZE);
    tiku_kits_crypto_sha256_update(&ctx, data, data_len);
    tiku_kits_crypto_sha256_final(&ctx, inner_digest);

    /* ---------------------------------------------------------------*/
    /* Step 3: Outer hash — H(opad || inner_digest)                   */
    /*   Undo ipad XOR, apply opad XOR                                */
    /* ---------------------------------------------------------------*/

    for (i = 0; i < HMAC_BLOCK_SIZE; i++) {
        key_pad[i] ^= (HMAC_IPAD ^ HMAC_OPAD);
    }

    tiku_kits_crypto_sha256_init(&ctx);
    tiku_kits_crypto_sha256_update(&ctx, key_pad, HMAC_BLOCK_SIZE);
    tiku_kits_crypto_sha256_update(&ctx, inner_digest,
                                    TIKU_KITS_CRYPTO_SHA256_DIGEST_SIZE);
    tiku_kits_crypto_sha256_final(&ctx, mac);

    /* ---------------------------------------------------------------*/
    /* Scrub sensitive material from static buffers                    */
    /* ---------------------------------------------------------------*/

    memset(key_pad, 0, HMAC_BLOCK_SIZE);
    memset(inner_digest, 0, TIKU_KITS_CRYPTO_SHA256_DIGEST_SIZE);

    return TIKU_KITS_CRYPTO_OK;
}
