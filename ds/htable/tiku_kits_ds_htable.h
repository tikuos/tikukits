/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_htable.h - Hash table with open addressing
 *
 * Platform-independent hash table library for embedded systems.
 * Uses open addressing with linear probing and FNV-1a hashing.
 * All storage is statically allocated using a compile-time maximum
 * capacity -- no heap required.
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

#ifndef TIKU_KITS_DS_HTABLE_H_
#define TIKU_KITS_DS_HTABLE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of entries the hash table can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_HTABLE_MAX_SIZE
#define TIKU_KITS_DS_HTABLE_MAX_SIZE 16
#endif

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** Entry slot is unused and has never been occupied. */
#define TIKU_KITS_DS_HTABLE_EMPTY     0

/** Entry slot holds a live key-value pair. */
#define TIKU_KITS_DS_HTABLE_OCCUPIED  1

/** Entry was previously occupied but has been removed (tombstone). */
#define TIKU_KITS_DS_HTABLE_DELETED   2

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** Key type for hash table lookups. */
typedef uint32_t tiku_kits_ds_htable_key_t;

/**
 * @struct tiku_kits_ds_htable_entry
 * @brief A single slot in the open-addressed hash table
 */
struct tiku_kits_ds_htable_entry {
    tiku_kits_ds_htable_key_t key;  /**< Lookup key */
    tiku_kits_ds_elem_t value;      /**< Stored value */
    uint8_t state;                  /**< EMPTY, OCCUPIED, or DELETED */
};

/**
 * @struct tiku_kits_ds_htable
 * @brief Fixed-capacity hash table with open addressing
 *
 * Uses linear probing for collision resolution and FNV-1a hashing
 * of uint32_t keys. Deleted entries are marked as tombstones to
 * maintain correct probe sequences.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_htable ht;
 *   tiku_kits_ds_htable_init(&ht, 8);
 *   tiku_kits_ds_htable_put(&ht, 0x1001, 42);
 * @endcode
 */
struct tiku_kits_ds_htable {
    struct tiku_kits_ds_htable_entry
        entries[TIKU_KITS_DS_HTABLE_MAX_SIZE];
    uint16_t capacity;  /**< User-requested capacity */
    uint16_t count;     /**< Number of OCCUPIED entries */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a hash table with the given capacity
 * @param ht       Hash table to initialize
 * @param capacity Maximum number of entries (1..HTABLE_MAX_SIZE)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_htable_init(struct tiku_kits_ds_htable *ht,
                              uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* PUT / GET / REMOVE                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert or update a key-value pair
 * @param ht    Hash table
 * @param key   Lookup key
 * @param value Value to store
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 *
 * If the key already exists, its value is updated in place.
 */
int tiku_kits_ds_htable_put(struct tiku_kits_ds_htable *ht,
                             tiku_kits_ds_htable_key_t key,
                             tiku_kits_ds_elem_t value);

/**
 * @brief Look up a key and retrieve its value
 * @param ht    Hash table
 * @param key   Lookup key
 * @param value Output: stored value
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND
 */
int tiku_kits_ds_htable_get(const struct tiku_kits_ds_htable *ht,
                             tiku_kits_ds_htable_key_t key,
                             tiku_kits_ds_elem_t *value);

/**
 * @brief Remove a key from the hash table (tombstone deletion)
 * @param ht  Hash table
 * @param key Key to remove
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND
 */
int tiku_kits_ds_htable_remove(struct tiku_kits_ds_htable *ht,
                                tiku_kits_ds_htable_key_t key);

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether a key exists in the hash table
 * @param ht  Hash table
 * @param key Key to search for
 * @return 1 if found, 0 if not found (0 if ht is NULL)
 */
int tiku_kits_ds_htable_contains(const struct tiku_kits_ds_htable *ht,
                                  tiku_kits_ds_htable_key_t key);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the hash table, marking all entries EMPTY
 * @param ht Hash table
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_htable_clear(struct tiku_kits_ds_htable *ht);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of occupied entries
 * @param ht Hash table
 * @return Current count, or 0 if ht is NULL
 */
uint16_t tiku_kits_ds_htable_count(
    const struct tiku_kits_ds_htable *ht);

#endif /* TIKU_KITS_DS_HTABLE_H_ */
