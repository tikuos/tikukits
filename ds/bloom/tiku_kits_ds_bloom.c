/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_bloom.c - Fixed-size Bloom filter implementation
 *
 * Platform-independent implementation of a fixed-capacity Bloom
 * filter. All storage is statically allocated; no heap usage.
 * Multiple hash functions are derived from FNV-1a with per-hash
 * seed variation, mapping keys to bit positions via modulo.
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_ds_bloom.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief FNV-1a hash with a seed parameter
 *
 * Computes a 32-bit FNV-1a hash of the given key bytes, using
 * the seed to vary the initial hash state. Each hash function
 * index uses a different seed so the Bloom filter gets
 * independent bit positions from the same key.
 *
 * FNV-1a constants:
 *   offset basis: 2166136261 (0x811C9DC5)
 *   prime:        16777619   (0x01000193)
 *
 * @param key     Pointer to key data
 * @param key_len Length of key in bytes
 * @param seed    Seed value to mix into the initial hash state
 * @return 32-bit hash value
 */
static uint32_t fnv1a_seeded(const uint8_t *key,
                             uint16_t key_len,
                             uint32_t seed)
{
    uint32_t hash;
    uint16_t i;

    hash = 2166136261u ^ seed;

    for (i = 0; i < key_len; i++) {
        hash ^= (uint32_t)key[i];
        hash *= 16777619u;
    }
    return hash;
}

/**
 * @brief Compute the bit position for a given hash index
 *
 * Uses fnv1a_seeded with the hash index as seed, then reduces
 * the result modulo num_bits to obtain a valid bit position.
 *
 * @param key       Pointer to key data
 * @param key_len   Length of key in bytes
 * @param hash_idx  Hash function index (0..num_hashes-1)
 * @param num_bits  Number of bits in the filter
 * @return Bit position in [0, num_bits)
 */
static uint16_t bloom_bit_pos(const uint8_t *key,
                              uint16_t key_len,
                              uint8_t hash_idx,
                              uint16_t num_bits)
{
    uint32_t h;

    h = fnv1a_seeded(key, key_len, (uint32_t)hash_idx);
    return (uint16_t)(h % num_bits);
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bloom_init(struct tiku_kits_ds_bloom *bloom,
                            uint16_t num_bits,
                            uint8_t num_hashes)
{
    if (bloom == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (num_bits == 0
        || num_bits > TIKU_KITS_DS_BLOOM_MAX_BITS) {
        return TIKU_KITS_DS_ERR_PARAM;
    }
    if (num_hashes == 0
        || num_hashes > TIKU_KITS_DS_BLOOM_MAX_HASHES) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    bloom->num_bits   = num_bits;
    bloom->num_hashes = num_hashes;
    bloom->count      = 0;
    memset(bloom->bits, 0, sizeof(bloom->bits));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERT                                                                    */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bloom_add(struct tiku_kits_ds_bloom *bloom,
                           const void *key,
                           uint16_t key_len)
{
    const uint8_t *k;
    uint8_t i;
    uint16_t pos;
    uint16_t byte_idx;
    uint8_t  bit_idx;

    if (bloom == NULL || key == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (key_len == 0) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    k = (const uint8_t *)key;

    for (i = 0; i < bloom->num_hashes; i++) {
        pos      = bloom_bit_pos(k, key_len, i,
                                 bloom->num_bits);
        byte_idx = pos / 8;
        bit_idx  = pos % 8;
        bloom->bits[byte_idx] |= (uint8_t)(1u << bit_idx);
    }

    bloom->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bloom_check(
    const struct tiku_kits_ds_bloom *bloom,
    const void *key,
    uint16_t key_len)
{
    const uint8_t *k;
    uint8_t i;
    uint16_t pos;
    uint16_t byte_idx;
    uint8_t  bit_idx;

    if (bloom == NULL || key == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (key_len == 0) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    k = (const uint8_t *)key;

    for (i = 0; i < bloom->num_hashes; i++) {
        pos      = bloom_bit_pos(k, key_len, i,
                                 bloom->num_bits);
        byte_idx = pos / 8;
        bit_idx  = pos % 8;

        if ((bloom->bits[byte_idx] & (1u << bit_idx)) == 0) {
            return 0;
        }
    }

    return 1;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bloom_clear(struct tiku_kits_ds_bloom *bloom)
{
    uint16_t n_bytes;

    if (bloom == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    n_bytes = (bloom->num_bits + 7) / 8;
    memset(bloom->bits, 0, (size_t)n_bytes);
    bloom->count = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_bloom_count(
    const struct tiku_kits_ds_bloom *bloom)
{
    if (bloom == NULL) {
        return 0;
    }
    return bloom->count;
}
