/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_sortarray.c - Sorted array with binary search
 *
 * Platform-independent implementation of a sorted array. Elements
 * are maintained in ascending order with O(log n) binary search.
 * All storage is statically allocated; no heap usage.
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

#include "tiku_kits_ds_sortarray.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Binary search for a value in sorted data
 *
 * Returns the insertion point: the index where value should be
 * inserted to maintain sorted order. If the value is found,
 * *found is set to 1 and the returned index points to the first
 * occurrence. If not found, *found is set to 0 and the returned
 * index is the insertion point.
 *
 * @param data  Sorted array of elements
 * @param size  Number of elements in the array
 * @param value Value to search for
 * @param found Output: 1 if exact match found, 0 otherwise
 * @return Insertion point index
 */
static uint16_t binary_search(const tiku_kits_ds_elem_t *data,
                              uint16_t size,
                              tiku_kits_ds_elem_t value,
                              uint8_t *found)
{
    uint16_t lo = 0;
    uint16_t hi = size;
    uint16_t mid;

    *found = 0;

    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        if (data[mid] < value) {
            lo = mid + 1;
        } else if (data[mid] > value) {
            hi = mid;
        } else {
            *found = 1;
            /* Walk back to find the first occurrence */
            while (mid > 0 && data[mid - 1] == value) {
                mid--;
            }
            return mid;
        }
    }

    return lo;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_init(struct tiku_kits_ds_sortarray *sa,
                                uint16_t capacity)
{
    if (sa == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 ||
        capacity > TIKU_KITS_DS_SORTARRAY_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    sa->size = 0;
    sa->capacity = capacity;
    memset(sa->data, 0, sizeof(sa->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERTION / REMOVAL                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_insert(struct tiku_kits_ds_sortarray *sa,
                                  tiku_kits_ds_elem_t value)
{
    uint16_t pos;
    uint8_t found;
    uint16_t i;

    if (sa == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (sa->size >= sa->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    /* Find insertion point via binary search */
    pos = binary_search(sa->data, sa->size, value, &found);

    /*
     * If duplicates exist, insert after the last equal element
     * so that order of equal-valued insertions is stable.
     */
    if (found) {
        while (pos < sa->size && sa->data[pos] == value) {
            pos++;
        }
    }

    /* Shift elements right to make room */
    for (i = sa->size; i > pos; i--) {
        sa->data[i] = sa->data[i - 1];
    }

    sa->data[pos] = value;
    sa->size++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_remove(struct tiku_kits_ds_sortarray *sa,
                                  tiku_kits_ds_elem_t value)
{
    uint16_t pos;
    uint8_t found;
    uint16_t i;

    if (sa == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (sa->size == 0) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    pos = binary_search(sa->data, sa->size, value, &found);
    if (!found) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    /* Shift elements left to fill the gap */
    for (i = pos; i < sa->size - 1; i++) {
        sa->data[i] = sa->data[i + 1];
    }

    sa->size--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_find(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t value, uint16_t *index)
{
    uint8_t found;
    uint16_t pos;

    if (sa == NULL || index == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    pos = binary_search(sa->data, sa->size, value, &found);
    if (!found) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    *index = pos;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_contains(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t value)
{
    uint8_t found;

    if (sa == NULL) {
        return 0;
    }

    binary_search(sa->data, sa->size, value, &found);
    return (int)found;
}

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_get(
    const struct tiku_kits_ds_sortarray *sa,
    uint16_t index, tiku_kits_ds_elem_t *value)
{
    if (sa == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= sa->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    *value = sa->data[index];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_min(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t *value)
{
    if (sa == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (sa->size == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = sa->data[0];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_max(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t *value)
{
    if (sa == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (sa->size == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = sa->data[sa->size - 1];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_sortarray_clear(struct tiku_kits_ds_sortarray *sa)
{
    if (sa == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    sa->size = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_sortarray_size(
    const struct tiku_kits_ds_sortarray *sa)
{
    if (sa == NULL) {
        return 0;
    }
    return sa->size;
}
