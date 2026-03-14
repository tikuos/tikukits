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
 * @param key      Key to hash
 * @param capacity Table capacity (modulus)
 * @return Hash index in [0, capacity)
 */
static uint16_t hash_fnv1a(tiku_kits_ds_htable_key_t key,
                            uint16_t capacity)
{
    uint32_t h = 2166136261u;
    uint8_t i;

    for (i = 0; i < 4; i++) {
        h ^= (key >> (i * 8)) & 0xFF;
        h *= 16777619u;
    }
    return (uint16_t)(h % capacity);
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

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
            /* Key not present; insert at first deleted or here */
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
            if (!found_deleted) {
                found_deleted = 1;
                first_deleted = idx;
            }
        } else if (e->key == key) {
            /* Key exists: update value in place */
            e->value = value;
            return TIKU_KITS_DS_OK;
        }

        idx = (idx + 1) % ht->capacity;
    } while (idx != start);

    /* Full probe with no EMPTY slot found; use tombstone if any */
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
        /* Skip DELETED entries and continue probing */
        idx = (idx + 1) % ht->capacity;
    } while (idx != start);

    return TIKU_KITS_DS_ERR_NOTFOUND;
}

/*---------------------------------------------------------------------------*/

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

uint16_t tiku_kits_ds_htable_count(
    const struct tiku_kits_ds_htable *ht)
{
    if (ht == NULL) {
        return 0;
    }
    return ht->count;
}
