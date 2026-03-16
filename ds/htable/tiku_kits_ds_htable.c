/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_htable.c - Hash table with open addressing implementation
 *
 * Platform-independent implementation of a fixed-capacity hash table
 * using open addressing with linear probing. FNV-1a is used for
 * hashing uint32_t keys. Deleted entries are marked as tombstones
 * to maintain correct probe chains.
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

#include "tiku_kits_ds_htable.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief FNV-1a hash for a uint32_t key, reduced to table capacity
 *
 * Applies the FNV-1a algorithm byte-by-byte over the 4 bytes of the
 * key (little-endian extraction via shifting).  The 32-bit hash is
 * then reduced to [0, capacity) with a modulus.  FNV-1a provides
 * good distribution for integer keys with minimal code size -- well
 * suited to embedded targets.
 */
static uint16_t hash_fnv1a(tiku_kits_ds_htable_key_t key,
                            uint16_t capacity)
{
    uint32_t h = 2166136261u;  /* FNV offset basis */
    uint8_t i;

    /* XOR each byte of the key into the hash, then multiply by the
     * FNV prime.  Processing byte-by-byte ensures the hash captures
     * entropy from every part of the key. */
    for (i = 0; i < 4; i++) {
        h ^= (key >> (i * 8)) & 0xFF;
        h *= 16777619u;  /* FNV prime */
    }
    return (uint16_t)(h % capacity);
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a hash table with the given capacity
 *
 * Zeros the entire entries array so that every slot starts in the
 * EMPTY state (TIKU_KITS_DS_HTABLE_EMPTY == 0).  The runtime
 * capacity is clamped to TIKU_KITS_DS_HTABLE_MAX_SIZE so the static
 * buffer is never overrun.
 */
int tiku_kits_ds_htable_init(struct tiku_kits_ds_htable *ht,
                              uint16_t capacity)
{
    if (ht == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_HTABLE_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    memset(ht->entries, 0, sizeof(ht->entries));
    ht->capacity = capacity;
    ht->count = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PUT / GET / REMOVE                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert or update a key-value pair
 *
 * Linear probe from the FNV-1a hash index.  During the probe, the
 * first DELETED (tombstone) slot is remembered so that new insertions
 * can reclaim it, keeping the table compact.  If the key is found in
 * an OCCUPIED slot, the value is updated in place (upsert).  If an
 * EMPTY slot is reached before the key is found, the key is new and
 * is inserted at the first tombstone (if one was seen) or at the
 * EMPTY slot itself.
 */
int tiku_kits_ds_htable_put(struct tiku_kits_ds_htable *ht,
                             tiku_kits_ds_htable_key_t key,
                             tiku_kits_ds_elem_t value)
{
    uint16_t idx;
    uint16_t start;
    uint16_t first_deleted;
    int found_deleted;

    if (ht == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    start = hash_fnv1a(key, ht->capacity);
    idx = start;
    found_deleted = 0;
    first_deleted = 0;

    do {
        struct tiku_kits_ds_htable_entry *e = &ht->entries[idx];

        if (e->state == TIKU_KITS_DS_HTABLE_EMPTY) {
            /* EMPTY proves the key is not in the table.  Insert at
             * the first tombstone we passed (if any) to reclaim it,
             * otherwise insert right here. */
            if (found_deleted) {
                idx = first_deleted;
                e = &ht->entries[idx];
            }
            e->key = key;
            e->value = value;
            e->state = TIKU_KITS_DS_HTABLE_OCCUPIED;
            ht->count++;
            return TIKU_KITS_DS_OK;
        }

        if (e->state == TIKU_KITS_DS_HTABLE_DELETED) {
            /* Remember the first tombstone for potential reuse,
             * but keep probing -- the key may exist further along. */
            if (!found_deleted) {
                found_deleted = 1;
                first_deleted = idx;
            }
        } else if (e->key == key) {
            /* Key already present: update value in place (upsert) */
            e->value = value;
            return TIKU_KITS_DS_OK;
        }

        idx = (idx + 1) % ht->capacity;
    } while (idx != start);

    /* Full wrap-around with no EMPTY slot found.  The table is
     * entirely OCCUPIED + DELETED.  Use the first tombstone if one
     * was seen during the probe; otherwise the table is truly full. */
    if (found_deleted) {
        struct tiku_kits_ds_htable_entry *e =
            &ht->entries[first_deleted];
        e->key = key;
        e->value = value;
        e->state = TIKU_KITS_DS_HTABLE_OCCUPIED;
        ht->count++;
        return TIKU_KITS_DS_OK;
    }

    return TIKU_KITS_DS_ERR_FULL;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Look up a key and retrieve its value
 *
 * Linear probe from the FNV-1a hash index.  DELETED slots are
 * skipped (they must not terminate the search because the target
 * key may reside beyond them in the probe chain).  The search
 * stops at the first EMPTY slot or on a full wrap-around.
 */
int tiku_kits_ds_htable_get(const struct tiku_kits_ds_htable *ht,
                             tiku_kits_ds_htable_key_t key,
                             tiku_kits_ds_elem_t *value)
{
    uint16_t idx;
    uint16_t start;

    if (ht == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    start = hash_fnv1a(key, ht->capacity);
    idx = start;

    do {
        const struct tiku_kits_ds_htable_entry *e =
            &ht->entries[idx];

        if (e->state == TIKU_KITS_DS_HTABLE_EMPTY) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        if (e->state == TIKU_KITS_DS_HTABLE_OCCUPIED &&
            e->key == key) {
            *value = e->value;
            return TIKU_KITS_DS_OK;
        }
        /* Skip DELETED (tombstone) entries -- the target key may
         * have been inserted further along the probe chain. */
        idx = (idx + 1) % ht->capacity;
    } while (idx != start);

    return TIKU_KITS_DS_ERR_NOTFOUND;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove a key from the hash table (tombstone deletion)
 *
 * Locates the key via the same linear probe used by get(), then
 * marks the slot as DELETED rather than clearing it to EMPTY.  The
 * tombstone preserves probe-chain continuity for keys that were
 * inserted after this one.  The count is decremented by one.
 */
int tiku_kits_ds_htable_remove(struct tiku_kits_ds_htable *ht,
                                tiku_kits_ds_htable_key_t key)
{
    uint16_t idx;
    uint16_t start;

    if (ht == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    start = hash_fnv1a(key, ht->capacity);
    idx = start;

    do {
        struct tiku_kits_ds_htable_entry *e = &ht->entries[idx];

        if (e->state == TIKU_KITS_DS_HTABLE_EMPTY) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        if (e->state == TIKU_KITS_DS_HTABLE_OCCUPIED &&
            e->key == key) {
            e->state = TIKU_KITS_DS_HTABLE_DELETED;
            ht->count--;
            return TIKU_KITS_DS_OK;
        }
        idx = (idx + 1) % ht->capacity;
    } while (idx != start);

    return TIKU_KITS_DS_ERR_NOTFOUND;
}

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether a key exists in the hash table
 *
 * Convenience wrapper around get() that discards the retrieved value.
 * Safe to call with a NULL pointer -- returns 0.
 */
int tiku_kits_ds_htable_contains(const struct tiku_kits_ds_htable *ht,
                                  tiku_kits_ds_htable_key_t key)
{
    tiku_kits_ds_elem_t dummy;

    if (ht == NULL) {
        return 0;
    }
    return (tiku_kits_ds_htable_get(ht, key, &dummy) ==
            TIKU_KITS_DS_OK) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the hash table, marking all entries EMPTY
 *
 * Zeros the entire entries array via memset.  Because
 * TIKU_KITS_DS_HTABLE_EMPTY is 0, this sets every slot's state to
 * EMPTY and also clears any lingering key/value data.  Tombstones
 * are removed as well, restoring the table to a freshly-initialized
 * state.
 */
int tiku_kits_ds_htable_clear(struct tiku_kits_ds_htable *ht)
{
    if (ht == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    memset(ht->entries, 0, sizeof(ht->entries));
    ht->count = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of occupied entries
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the number of live entries, not including
 * tombstones.
 */
uint16_t tiku_kits_ds_htable_count(
    const struct tiku_kits_ds_htable *ht)
{
    if (ht == NULL) {
        return 0;
    }
    return ht->count;
}
