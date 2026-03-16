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
 * @brief Maximum number of entries the hash table can hold.
 *
 * This compile-time constant defines the upper bound on hash table
 * capacity.  Each hash table instance reserves this many entry slots
 * in its static storage, so choose a value that balances memory usage
 * against the largest table your application needs.  Because the
 * table uses open addressing, performance degrades as the load factor
 * approaches 1.0 -- sizing the table 25-50% larger than the expected
 * number of entries helps keep probe chains short.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_HTABLE_MAX_SIZE 64
 *   #include "tiku_kits_ds_htable.h"
 * @endcode
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
 *
 * Each entry carries a tri-state flag (@c state) that distinguishes
 * between slots that have never been used (EMPTY), slots that hold
 * a live key-value pair (OCCUPIED), and slots whose entry was removed
 * (DELETED / tombstone).  The tombstone state is essential for open
 * addressing: without it, a probe chain would terminate prematurely
 * at a gap left by a deletion, causing subsequent lookups to miss
 * keys that were inserted after the deleted one.
 */
struct tiku_kits_ds_htable_entry {
    tiku_kits_ds_htable_key_t key;  /**< Lookup key */
    tiku_kits_ds_elem_t value;      /**< Stored value */
    uint8_t state;                  /**< EMPTY, OCCUPIED, or DELETED */
};

/**
 * @struct tiku_kits_ds_htable
 * @brief Fixed-capacity hash table with open addressing and static storage
 *
 * A general-purpose key-value container that maps uint32_t keys to
 * tiku_kits_ds_elem_t values.  Because all storage lives inside the
 * struct itself, no heap allocation is needed -- just declare the
 * hash table as a static or local variable.
 *
 * Collision resolution uses linear probing: on a hash collision the
 * table walks forward one slot at a time (wrapping modulo capacity)
 * until an empty or matching slot is found.  Deleted entries are
 * marked as tombstones (DELETED state) rather than cleared to EMPTY,
 * so that probe chains past the deleted slot remain intact.
 *
 * Two sizes are tracked independently:
 *   - @c capacity -- the runtime limit passed to init (must be
 *     <= TIKU_KITS_DS_HTABLE_MAX_SIZE).  This lets different table
 *     instances use different logical sizes while sharing the same
 *     compile-time backing array.
 *   - @c count -- the number of currently OCCUPIED entries.  This
 *     does not include tombstones.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_htable ht;
 *   tiku_kits_ds_htable_init(&ht, 8);   // use 8 of 16 slots
 *   tiku_kits_ds_htable_put(&ht, 0x1001, 42);
 *   tiku_kits_ds_htable_put(&ht, 0x2002, 7);
 *
 *   tiku_kits_ds_elem_t val;
 *   tiku_kits_ds_htable_get(&ht, 0x1001, &val);
 *   // val == 42, count == 2
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
 *
 * Resets the hash table to an empty state by marking every entry
 * slot as EMPTY and zeroing the count.  The backing array is zeroed
 * via memset so that all fields start in a deterministic state.
 *
 * @param ht       Hash table to initialize (must not be NULL)
 * @param capacity Maximum number of entries (1..HTABLE_MAX_SIZE)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if ht is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds MAX_SIZE
 */
int tiku_kits_ds_htable_init(struct tiku_kits_ds_htable *ht,
                              uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* PUT / GET / REMOVE                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert or update a key-value pair
 *
 * Hashes @p key with FNV-1a and probes linearly until an EMPTY slot,
 * a matching OCCUPIED slot, or a full wrap-around is reached.  If the
 * key already exists its value is updated in place (upsert semantics).
 * During the probe, the first DELETED (tombstone) slot is remembered
 * so that a new insertion can reclaim it instead of leaving a gap --
 * this keeps the table compact and probe chains short.
 *
 * Average-case O(1) when the load factor is low; degrades toward
 * O(n) as the table fills.
 *
 * @param ht    Hash table (must not be NULL)
 * @param key   Lookup key
 * @param value Value to store
 * @return TIKU_KITS_DS_OK on success (insert or update),
 *         TIKU_KITS_DS_ERR_NULL if ht is NULL,
 *         TIKU_KITS_DS_ERR_FULL if all slots are OCCUPIED (no EMPTY
 *         or DELETED slots available)
 */
int tiku_kits_ds_htable_put(struct tiku_kits_ds_htable *ht,
                             tiku_kits_ds_htable_key_t key,
                             tiku_kits_ds_elem_t value);

/**
 * @brief Look up a key and retrieve its value
 *
 * Hashes @p key and probes linearly.  The probe skips DELETED
 * (tombstone) slots -- they must not terminate the search because
 * the target key may reside beyond them in the probe chain.  The
 * search terminates at the first EMPTY slot (proving the key was
 * never inserted past this point) or on a full wrap-around.
 *
 * @param ht    Hash table (must not be NULL)
 * @param key   Lookup key
 * @param value Output pointer where the stored value is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK if found,
 *         TIKU_KITS_DS_ERR_NULL if ht or value is NULL,
 *         TIKU_KITS_DS_ERR_NOTFOUND if the key is not present
 */
int tiku_kits_ds_htable_get(const struct tiku_kits_ds_htable *ht,
                             tiku_kits_ds_htable_key_t key,
                             tiku_kits_ds_elem_t *value);

/**
 * @brief Remove a key from the hash table (tombstone deletion)
 *
 * Locates @p key via the same linear probe used by get(), then marks
 * the slot as DELETED rather than EMPTY.  The tombstone state keeps
 * probe chains intact so that keys inserted after the deleted entry
 * remain reachable.  The count is decremented by one.
 *
 * @param ht  Hash table (must not be NULL)
 * @param key Key to remove
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if ht is NULL,
 *         TIKU_KITS_DS_ERR_NOTFOUND if the key is not present
 */
int tiku_kits_ds_htable_remove(struct tiku_kits_ds_htable *ht,
                                tiku_kits_ds_htable_key_t key);

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether a key exists in the hash table
 *
 * Convenience wrapper around get() that discards the value.
 * Performs the same linear probe but only reports presence/absence.
 * Safe to call with a NULL pointer -- returns 0.
 *
 * @param ht  Hash table, or NULL
 * @param key Key to search for
 * @return 1 if the key is present, 0 if not found (0 if ht is NULL)
 */
int tiku_kits_ds_htable_contains(const struct tiku_kits_ds_htable *ht,
                                  tiku_kits_ds_htable_key_t key);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the hash table, marking all entries EMPTY
 *
 * Zeros the entire entries array via memset (setting every slot's
 * state to EMPTY since TIKU_KITS_DS_HTABLE_EMPTY == 0) and resets
 * the count to 0.  All tombstones are also cleared, effectively
 * restoring the table to its freshly-initialized state.
 *
 * @param ht Hash table (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if ht is NULL
 */
int tiku_kits_ds_htable_clear(struct tiku_kits_ds_htable *ht);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of occupied entries
 *
 * Returns the number of live (OCCUPIED) entries, not including
 * tombstones or empty slots.  Safe to call with a NULL pointer --
 * returns 0.
 *
 * @param ht Hash table, or NULL
 * @return Number of entries currently stored, or 0 if ht is NULL
 */
uint16_t tiku_kits_ds_htable_count(
    const struct tiku_kits_ds_htable *ht);

#endif /* TIKU_KITS_DS_HTABLE_H_ */
