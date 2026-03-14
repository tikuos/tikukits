/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_sortarray.h - Sorted array with binary search
 *
 * Platform-independent sorted array for embedded systems. Elements
 * are maintained in ascending order, enabling O(log n) lookups via
 * binary search. All storage is statically allocated using a
 * compile-time maximum capacity. Duplicates are allowed.
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

#ifndef TIKU_KITS_DS_SORTARRAY_H_
#define TIKU_KITS_DS_SORTARRAY_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of elements the sorted array can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_SORTARRAY_MAX_SIZE
#define TIKU_KITS_DS_SORTARRAY_MAX_SIZE 16
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_sortarray
 * @brief Fixed-capacity sorted array with static storage
 *
 * Elements are kept in ascending order. Insertions use binary search
 * to locate the correct position, then shift elements right. The
 * capacity field stores the runtime limit (<= MAX_SIZE).
 *
 * Declare sorted arrays as static or local variables -- no heap
 * required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_sortarray sa;
 *   tiku_kits_ds_sortarray_init(&sa, 8);
 *   tiku_kits_ds_sortarray_insert(&sa, 42);
 *   tiku_kits_ds_sortarray_insert(&sa, 10);
 *   // sa now contains: [10, 42]
 * @endcode
 */
struct tiku_kits_ds_sortarray {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_SORTARRAY_MAX_SIZE];
    uint16_t size;      /**< Number of valid elements */
    uint16_t capacity;  /**< Runtime capacity (<= MAX_SIZE) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a sorted array with the given capacity
 * @param sa       Sorted array to initialize
 * @param capacity Maximum number of elements (1..SORTARRAY_MAX_SIZE)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_sortarray_init(struct tiku_kits_ds_sortarray *sa,
                                uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* INSERTION / REMOVAL                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a value in sorted order
 *
 * Uses binary search to find the insertion point, then shifts
 * elements right to make room. Duplicates are allowed; a duplicate
 * is inserted after existing equal values.
 *
 * @param sa    Sorted array
 * @param value Value to insert
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_sortarray_insert(struct tiku_kits_ds_sortarray *sa,
                                  tiku_kits_ds_elem_t value);

/**
 * @brief Remove the first occurrence of a value
 *
 * Uses binary search to find the value, then shifts elements left
 * to fill the gap.
 *
 * @param sa    Sorted array
 * @param value Value to remove
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND
 */
int tiku_kits_ds_sortarray_remove(struct tiku_kits_ds_sortarray *sa,
                                  tiku_kits_ds_elem_t value);

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Find a value using binary search
 * @param sa    Sorted array
 * @param value Value to find
 * @param index Output: index of the value
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND
 */
int tiku_kits_ds_sortarray_find(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t value, uint16_t *index);

/**
 * @brief Check whether the sorted array contains a value
 * @param sa    Sorted array
 * @param value Value to search for
 * @return 1 if found, 0 if not (0 if sa is NULL)
 */
int tiku_kits_ds_sortarray_contains(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t value);

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the element at the given index
 * @param sa    Sorted array
 * @param index Index (must be < size)
 * @param value Output: element value
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_sortarray_get(
    const struct tiku_kits_ds_sortarray *sa,
    uint16_t index, tiku_kits_ds_elem_t *value);

/**
 * @brief Get the minimum element (first in sorted order)
 * @param sa    Sorted array
 * @param value Output: minimum value
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_sortarray_min(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t *value);

/**
 * @brief Get the maximum element (last in sorted order)
 * @param sa    Sorted array
 * @param value Output: maximum value
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_sortarray_max(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the sorted array (set size to 0)
 * @param sa Sorted array
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_sortarray_clear(struct tiku_kits_ds_sortarray *sa);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements
 * @param sa Sorted array
 * @return Current size, or 0 if sa is NULL
 */
uint16_t tiku_kits_ds_sortarray_size(
    const struct tiku_kits_ds_sortarray *sa);

#endif /* TIKU_KITS_DS_SORTARRAY_H_ */
