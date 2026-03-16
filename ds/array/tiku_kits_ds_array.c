/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_array.c - Fixed-size contiguous array implementation
 *
 * Platform-independent implementation of a fixed-capacity array.
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

#include "tiku_kits_ds_array.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an array with the given capacity
 *
 * Zeros the entire backing buffer so that any subsequent reads of
 * unpopulated slots return a deterministic value.  The runtime
 * capacity is clamped to TIKU_KITS_DS_ARRAY_MAX_SIZE at compile time
 * so that the static buffer is never overrun.
 */
int tiku_kits_ds_array_init(struct tiku_kits_ds_array *arr,
                            uint16_t capacity)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_ARRAY_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    arr->size = 0;
    arr->capacity = capacity;

    /* Zero the full static buffer, not just `capacity` slots, so that
     * the struct is in a clean state regardless of prior contents. */
    memset(arr->data, 0, sizeof(arr->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Overwrite an existing element at the given index
 *
 * Only indices within [0, size) are writable.  Writing beyond size
 * (even within capacity) is rejected because those slots have never
 * been logically populated.
 */
int tiku_kits_ds_array_set(struct tiku_kits_ds_array *arr,
                           uint16_t index,
                           tiku_kits_ds_elem_t value)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= arr->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    arr->data[index] = value;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read the element at the given index
 *
 * Copies the value out through @p value so the caller owns its own
 * copy and the array contents remain untouched.
 */
int tiku_kits_ds_array_get(const struct tiku_kits_ds_array *arr,
                           uint16_t index,
                           tiku_kits_ds_elem_t *value)
{
    if (arr == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= arr->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    *value = arr->data[index];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERTION / REMOVAL                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append an element to the end of the array
 *
 * O(1) -- simply writes into the next free slot and bumps the size
 * counter.  No element shifting required.
 */
int tiku_kits_ds_array_push_back(struct tiku_kits_ds_array *arr,
                                 tiku_kits_ds_elem_t value)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (arr->size >= arr->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    arr->data[arr->size] = value;
    arr->size++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove and optionally return the last element
 *
 * O(1) -- decrements size first, then copies the value out if the
 * caller provided a non-NULL output pointer.  The element's memory
 * slot is not cleared; it becomes inaccessible via the public API
 * because all access functions check against size.
 */
int tiku_kits_ds_array_pop_back(struct tiku_kits_ds_array *arr,
                                tiku_kits_ds_elem_t *value)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (arr->size == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    arr->size--;
    if (value != NULL) {
        *value = arr->data[arr->size];
    }
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Insert an element at the given index, shifting successors right
 *
 * O(n) worst case when inserting at position 0 (every element must
 * move).  The shift loop runs backwards from size down to index+1 to
 * avoid overwriting data before it has been copied.  Inserting at
 * index == size degenerates to a push_back with the extra shift
 * overhead of zero iterations.
 */
int tiku_kits_ds_array_insert(struct tiku_kits_ds_array *arr,
                              uint16_t index,
                              tiku_kits_ds_elem_t value)
{
    uint16_t i;

    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index > arr->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }
    if (arr->size >= arr->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    /* Shift elements right starting from the end to avoid clobbering
     * values that have not yet been moved. */
    for (i = arr->size; i > index; i--) {
        arr->data[i] = arr->data[i - 1];
    }

    arr->data[index] = value;
    arr->size++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove the element at the given index, shifting successors left
 *
 * O(n) worst case when removing at position 0.  The shift loop runs
 * forward from index to size-2, pulling each successor one slot to
 * the left to fill the gap left by the removed element.
 */
int tiku_kits_ds_array_remove(struct tiku_kits_ds_array *arr,
                              uint16_t index)
{
    uint16_t i;

    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= arr->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    /* Shift elements left starting from the removal point to
     * close the gap without a temporary buffer. */
    for (i = index; i < arr->size - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }

    arr->size--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Linear search for the first occurrence of a value
 *
 * Scans elements from index 0 upward and returns the position of the
 * first match.  O(n) worst case.  For data that is maintained in
 * sorted order, use tiku_kits_ds_sortarray_find() which provides
 * O(log n) binary search instead.
 */
int tiku_kits_ds_array_find(const struct tiku_kits_ds_array *arr,
                            tiku_kits_ds_elem_t value,
                            uint16_t *index)
{
    uint16_t i;

    if (arr == NULL || index == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Simple forward scan -- stop at the first match */
    for (i = 0; i < arr->size; i++) {
        if (arr->data[i] == value) {
            *index = i;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_NOTFOUND;
}

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Fill every capacity slot with the given value
 *
 * Iterates over all capacity slots (not MAX_SIZE) and writes @p value
 * into each one.  After filling, size is set equal to capacity so
 * that every slot is accessible through get/set.  Useful for
 * initialising an array with a sentinel or default value.
 */
int tiku_kits_ds_array_fill(struct tiku_kits_ds_array *arr,
                            tiku_kits_ds_elem_t value)
{
    uint16_t i;

    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Write exactly `capacity` slots -- the user-requested limit,
     * not the full MAX_SIZE backing buffer. */
    for (i = 0; i < arr->capacity; i++) {
        arr->data[i] = value;
    }

    arr->size = arr->capacity;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the array by resetting size to zero
 *
 * Logically removes all elements without zeroing the backing buffer.
 * Old values remain in memory but are inaccessible through the public
 * API because all access functions bounds-check against size.
 */
int tiku_kits_ds_array_clear(struct tiku_kits_ds_array *arr)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    arr->size = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements in the array
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the logical size, not the capacity.
 */
uint16_t tiku_kits_ds_array_size(const struct tiku_kits_ds_array *arr)
{
    if (arr == NULL) {
        return 0;
    }
    return arr->size;
}
