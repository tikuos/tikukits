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
 * Implements a classic iterative binary search over an ascending-sorted
 * array.  The search space is halved on each iteration by comparing the
 * midpoint element against the target value:
 *   - If data[mid] < value, the target must lie in the upper half.
 *   - If data[mid] > value, the target must lie in the lower half.
 *   - If data[mid] == value, the target is found.
 *
 * When the target is found, the function walks backwards from the
 * match to locate the *first* occurrence among duplicates.  This
 * guarantees that find() always returns the lowest index and that
 * remove() always deletes the earliest duplicate.
 *
 * When the target is not found, the returned index is the correct
 * insertion point -- the position where the value would need to be
 * placed to maintain ascending order.
 *
 * @note The midpoint is computed as @c lo+(hi-lo)/2 rather than
 *       @c (lo+hi)/2 to avoid potential uint16_t overflow when
 *       lo and hi are both large.
 *
 * @param data  Sorted array of elements
 * @param size  Number of valid elements in the array
 * @param value Value to search for
 * @param found Output: set to 1 if an exact match is found, 0 otherwise
 * @return Index of the first occurrence if found, or the insertion
 *         point if not found
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
            /* Walk back to find the first occurrence so that
             * find() returns the lowest index among duplicates. */
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

/**
 * @brief Initialize a sorted array with the given capacity
 *
 * Zeros the entire backing buffer so that uninitialised reads return
 * a deterministic value.  The runtime capacity is clamped to
 * TIKU_KITS_DS_SORTARRAY_MAX_SIZE at compile time so that the static
 * buffer is never overrun.
 */
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

    /* Zero the full static buffer for a clean initial state */
    memset(sa->data, 0, sizeof(sa->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERTION / REMOVAL                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a value in sorted (ascending) order
 *
 * The insertion proceeds in three steps:
 *   1. Binary search to find the correct position -- O(log n).
 *   2. If duplicates exist, advance past them so the new element
 *      is placed after all existing equal values (stable insertion).
 *   3. Shift elements right and write the new value -- O(n).
 */
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

    /* Step 1: locate the insertion point via binary search */
    pos = binary_search(sa->data, sa->size, value, &found);

    /*
     * Step 2: if duplicates exist, advance past them.  binary_search()
     * returns the index of the *first* equal element; we walk forward
     * so the new duplicate lands after existing ones, preserving the
     * relative order of equal-valued insertions (stability).
     */
    if (found) {
        while (pos < sa->size && sa->data[pos] == value) {
            pos++;
        }
    }

    /* Step 3: shift elements right starting from the end to avoid
     * overwriting values that have not yet been moved. */
    for (i = sa->size; i > pos; i--) {
        sa->data[i] = sa->data[i - 1];
    }

    sa->data[pos] = value;
    sa->size++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove the first occurrence of a value
 *
 * Uses binary search to locate the value in O(log n), then shifts
 * all subsequent elements one slot left in O(n) to close the gap.
 * Only the first (lowest-index) occurrence is removed.
 */
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

    /* Shift elements left starting from the removal point to
     * close the gap without a temporary buffer. */
    for (i = pos; i < sa->size - 1; i++) {
        sa->data[i] = sa->data[i + 1];
    }

    sa->size--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Find a value using O(log n) binary search
 *
 * Returns the index of the first occurrence among duplicates.
 */
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

/**
 * @brief Check whether the sorted array contains a value
 *
 * Thin wrapper around binary_search() that discards the positional
 * information and returns a simple boolean.  Safe to call with a
 * NULL pointer -- returns 0.
 */
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

/**
 * @brief Read the element at the given index
 *
 * Index-based random access into the sorted data.  Since elements
 * are in ascending order, index 0 is the minimum and index (size-1)
 * is the maximum.
 */
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

/**
 * @brief Get the minimum element (first in sorted order)
 *
 * O(1) -- the minimum is always at index 0 because elements are
 * maintained in ascending order.
 */
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

    /* Ascending order guarantees the minimum is at position 0 */
    *value = sa->data[0];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the maximum element (last in sorted order)
 *
 * O(1) -- the maximum is always at index (size-1) because elements
 * are maintained in ascending order.
 */
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

    /* Ascending order guarantees the maximum is at the last slot */
    *value = sa->data[sa->size - 1];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the sorted array by resetting size to zero
 *
 * Logically removes all elements without zeroing the backing buffer.
 * Old values remain in memory but are inaccessible through the public
 * API because all access functions bounds-check against size.
 */
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

/**
 * @brief Return the current number of elements in the sorted array
 *
 * Returns the logical size, not the capacity.  Safe to call with a
 * NULL pointer -- returns 0 rather than dereferencing.
 */
uint16_t tiku_kits_ds_sortarray_size(
    const struct tiku_kits_ds_sortarray *sa)
{
    if (sa == NULL) {
        return 0;
    }
    return sa->size;
}
