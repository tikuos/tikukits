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
 * @brief Maximum number of elements the sorted array can hold.
 *
 * This compile-time constant defines the upper bound on sorted array
 * capacity.  Each instance reserves this many element slots in its
 * static storage.  The default is smaller than the unsorted array
 * (16 vs 32) because sorted insertions involve O(n) shifting, which
 * becomes expensive for large arrays on constrained MCUs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_SORTARRAY_MAX_SIZE 32
 *   #include "tiku_kits_ds_sortarray.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_SORTARRAY_MAX_SIZE
#define TIKU_KITS_DS_SORTARRAY_MAX_SIZE 16
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_sortarray
 * @brief Fixed-capacity sorted array with static storage and O(log n) search
 *
 * A specialised array that maintains elements in ascending order at
 * all times.  Because the data is always sorted:
 *   - Lookups use binary search -- O(log n) instead of O(n).
 *   - Min/max retrieval is O(1) (first/last element).
 *   - Insertions and removals are O(n) due to element shifting,
 *     but the binary search step to locate the correct position is
 *     only O(log n).
 *
 * Duplicates are allowed.  When duplicates are inserted, they are
 * placed after existing equal values so that insertion order among
 * equal elements is preserved (stable insertion).
 *
 * Like the unsorted array, all storage lives inside the struct
 * itself -- no heap allocation is needed.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  The comparison operators @c < and @c > must be
 *       valid for the chosen type.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_sortarray sa;
 *   tiku_kits_ds_sortarray_init(&sa, 8);
 *   tiku_kits_ds_sortarray_insert(&sa, 42);
 *   tiku_kits_ds_sortarray_insert(&sa, 10);
 *   tiku_kits_ds_sortarray_insert(&sa, 25);
 *   // sa now contains: [10, 25, 42]
 *   //
 *   // Binary search finds 25 in O(log 3) steps:
 *   uint16_t idx;
 *   tiku_kits_ds_sortarray_find(&sa, 25, &idx);  // idx == 1
 * @endcode
 */
struct tiku_kits_ds_sortarray {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_SORTARRAY_MAX_SIZE];
    uint16_t size;      /**< Number of valid elements currently stored */
    uint16_t capacity;  /**< Runtime capacity set by init (<= MAX_SIZE) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a sorted array with the given capacity
 *
 * Resets the sorted array to an empty state and sets the runtime
 * capacity limit.  The backing buffer is zeroed for deterministic
 * initial contents.
 *
 * @param sa       Sorted array to initialize (must not be NULL)
 * @param capacity Maximum number of elements (1..SORTARRAY_MAX_SIZE)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sa is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds MAX_SIZE
 */
int tiku_kits_ds_sortarray_init(struct tiku_kits_ds_sortarray *sa,
                                uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* INSERTION / REMOVAL                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a value in sorted (ascending) order
 *
 * Uses O(log n) binary search to locate the correct insertion point,
 * then shifts all subsequent elements one slot to the right (O(n))
 * to make room.  The overall cost is O(n) per insertion.
 *
 * Duplicates are allowed.  When a duplicate is inserted, it is placed
 * after all existing equal values so that insertion order among equal
 * elements is preserved (stable insertion).
 *
 * @param sa    Sorted array (must not be NULL)
 * @param value Value to insert
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sa is NULL,
 *         TIKU_KITS_DS_ERR_FULL if size == capacity
 */
int tiku_kits_ds_sortarray_insert(struct tiku_kits_ds_sortarray *sa,
                                  tiku_kits_ds_elem_t value);

/**
 * @brief Remove the first occurrence of a value
 *
 * Uses O(log n) binary search to locate the value, then shifts all
 * subsequent elements one slot to the left (O(n)) to fill the gap.
 * If the value appears multiple times, only the first occurrence
 * (lowest index) is removed.
 *
 * @param sa    Sorted array (must not be NULL)
 * @param value Value to remove
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sa is NULL,
 *         TIKU_KITS_DS_ERR_NOTFOUND if value is not present
 */
int tiku_kits_ds_sortarray_remove(struct tiku_kits_ds_sortarray *sa,
                                  tiku_kits_ds_elem_t value);

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Find a value using O(log n) binary search
 *
 * Locates the first occurrence of @p value and writes its index into
 * @p index.  If duplicates exist, the lowest index is returned.
 *
 * @param sa    Sorted array (must not be NULL)
 * @param value Value to search for
 * @param index Output pointer where the found index is written (must
 *              not be NULL)
 * @return TIKU_KITS_DS_OK if found,
 *         TIKU_KITS_DS_ERR_NULL if sa or index is NULL,
 *         TIKU_KITS_DS_ERR_NOTFOUND if value is not present
 */
int tiku_kits_ds_sortarray_find(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t value, uint16_t *index);

/**
 * @brief Check whether the sorted array contains a value
 *
 * Convenience wrapper around binary search that returns a simple
 * boolean result.  Unlike find(), this does not require an output
 * pointer for the index.  Safe to call with a NULL pointer -- returns
 * 0.
 *
 * @param sa    Sorted array, or NULL
 * @param value Value to search for
 * @return 1 if value is present, 0 if not found or sa is NULL
 */
int tiku_kits_ds_sortarray_contains(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t value);

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read the element at the given index
 *
 * Provides direct index-based access to the sorted data.  Since
 * elements are maintained in ascending order, index 0 always holds
 * the minimum and index (size-1) holds the maximum.
 *
 * @param sa    Sorted array (must not be NULL)
 * @param index Position to read (must be < size)
 * @param value Output pointer where the element is written (must not
 *              be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sa or value is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if index >= size
 */
int tiku_kits_ds_sortarray_get(
    const struct tiku_kits_ds_sortarray *sa,
    uint16_t index, tiku_kits_ds_elem_t *value);

/**
 * @brief Get the minimum element (first in sorted order)
 *
 * O(1) -- the minimum is always at index 0 because the array is
 * maintained in ascending order.
 *
 * @param sa    Sorted array (must not be NULL)
 * @param value Output pointer where the minimum is written (must not
 *              be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sa or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if size == 0
 */
int tiku_kits_ds_sortarray_min(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t *value);

/**
 * @brief Get the maximum element (last in sorted order)
 *
 * O(1) -- the maximum is always at index (size-1) because the array
 * is maintained in ascending order.
 *
 * @param sa    Sorted array (must not be NULL)
 * @param value Output pointer where the maximum is written (must not
 *              be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sa or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if size == 0
 */
int tiku_kits_ds_sortarray_max(
    const struct tiku_kits_ds_sortarray *sa,
    tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the sorted array by resetting size to zero
 *
 * Logically removes all elements without zeroing the backing buffer.
 * Old values remain in memory but are inaccessible through the public
 * API because all access functions bounds-check against size.
 *
 * @param sa Sorted array (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sa is NULL
 */
int tiku_kits_ds_sortarray_clear(struct tiku_kits_ds_sortarray *sa);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements in the sorted array
 *
 * Returns the logical size (number of valid, populated elements),
 * not the capacity.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param sa Sorted array, or NULL
 * @return Number of elements currently stored, or 0 if sa is NULL
 */
uint16_t tiku_kits_ds_sortarray_size(
    const struct tiku_kits_ds_sortarray *sa);

#endif /* TIKU_KITS_DS_SORTARRAY_H_ */
