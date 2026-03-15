/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_bloom.h - Fixed-size Bloom filter for set membership
 *
 * Platform-independent Bloom filter library for embedded systems.
 * All storage is statically allocated using a compile-time maximum
 * bit count. Multiple hash functions are derived from FNV-1a with
 * seed variation. Suited for probabilistic set membership queries
 * on resource-constrained microcontrollers.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_DS_BLOOM_H_
#define TIKU_KITS_DS_BLOOM_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of bits the Bloom filter can hold.
 * Override before including this header to change the limit.
 * The backing storage is (MAX_BITS / 8) bytes.
 */
#ifndef TIKU_KITS_DS_BLOOM_MAX_BITS
#define TIKU_KITS_DS_BLOOM_MAX_BITS 256
#endif

/** Number of bytes needed to store MAX_BITS bits */
#define TIKU_KITS_DS_BLOOM_MAX_BYTES \
    ((TIKU_KITS_DS_BLOOM_MAX_BITS + 7) / 8)

/**
 * Maximum number of independent hash functions.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_BLOOM_MAX_HASHES
#define TIKU_KITS_DS_BLOOM_MAX_HASHES 3
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_bloom
 * @brief Fixed-capacity Bloom filter with static storage
 *
 * Bits are packed into a uint8_t array. Bit N is stored in
 * bits[N / 8] at position (N % 8). Multiple hash functions are
 * derived from FNV-1a with different seed values to map keys
 * to bit positions.
 *
 * Declare Bloom filters as static or local variables -- no heap
 * required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_bloom bf;
 *   tiku_kits_ds_bloom_init(&bf, 128, 3);
 *   tiku_kits_ds_bloom_add(&bf, "hello", 5);
 *   int present = tiku_kits_ds_bloom_check(&bf, "hello", 5);
 * @endcode
 */
struct tiku_kits_ds_bloom {
    uint8_t  bits[TIKU_KITS_DS_BLOOM_MAX_BYTES];
    uint16_t num_bits;      /**< Number of valid bits     */
    uint8_t  num_hashes;    /**< Number of hash functions */
    uint16_t count;         /**< Number of items added    */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Bloom filter
 *
 * Clears all bits and sets the filter dimensions. The actual
 * number of bits used is num_bits; the backing array is sized
 * at compile time to BLOOM_MAX_BITS.
 *
 * @param bloom      Bloom filter to initialize
 * @param num_bits   Number of bits (1..BLOOM_MAX_BITS)
 * @param num_hashes Number of hash functions (1..BLOOM_MAX_HASHES)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bloom_init(struct tiku_kits_ds_bloom *bloom,
                            uint16_t num_bits,
                            uint8_t num_hashes);

/*---------------------------------------------------------------------------*/
/* INSERT                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add a key to the Bloom filter
 *
 * Computes num_hashes bit positions from the key and sets each
 * corresponding bit. Duplicate inserts are harmless.
 *
 * @param bloom   Bloom filter
 * @param key     Pointer to key data
 * @param key_len Length of key in bytes
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bloom_add(struct tiku_kits_ds_bloom *bloom,
                           const void *key,
                           uint16_t key_len);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether a key may be in the Bloom filter
 *
 * Returns 1 if all hash-derived bits are set (key is possibly
 * present) or 0 if any bit is clear (key is definitely absent).
 * False positives are possible; false negatives are not.
 *
 * @param bloom   Bloom filter
 * @param key     Pointer to key data
 * @param key_len Length of key in bytes
 * @return 1 if possibly present, 0 if definitely absent,
 *         or a negative error code
 */
int tiku_kits_ds_bloom_check(const struct tiku_kits_ds_bloom *bloom,
                             const void *key,
                             uint16_t key_len);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all bits and reset the item count
 *
 * Resets the filter to its initial state without changing
 * num_bits or num_hashes.
 *
 * @param bloom Bloom filter
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bloom_clear(struct tiku_kits_ds_bloom *bloom);

/**
 * @brief Return the number of items added to the filter
 * @param bloom Bloom filter
 * @return Number of items added, or 0 if bloom is NULL
 */
uint16_t tiku_kits_ds_bloom_count(
    const struct tiku_kits_ds_bloom *bloom);

#endif /* TIKU_KITS_DS_BLOOM_H_ */
