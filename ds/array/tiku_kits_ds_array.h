/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_array.h - Fixed-size contiguous array
 *
 * Platform-independent array library for embedded systems. All storage
 * is statically allocated using a compile-time maximum capacity. The
 * element type is configured via tiku_kits_ds_elem_t.
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

#ifndef TIKU_KITS_DS_ARRAY_H_
#define TIKU_KITS_DS_ARRAY_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of elements the array can hold.
 *
 * This compile-time constant defines the upper bound on array capacity.
 * Each array instance reserves this many element slots in its static
 * storage, so choose a value that balances memory usage against the
 * largest array your application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_ARRAY_MAX_SIZE 64
 *   #include "tiku_kits_ds_array.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_ARRAY_MAX_SIZE
#define TIKU_KITS_DS_ARRAY_MAX_SIZE 32
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_array
 * @brief Fixed-capacity array with contiguous static storage
 *
 * A general-purpose, index-addressable container that stores elements
 * in a contiguous block of statically allocated memory.  Because all
 * storage lives inside the struct itself, no heap allocation is needed
 * -- just declare the array as a static or local variable.
 *
 * Two sizes are tracked independently:
 *   - @c capacity -- the runtime limit passed to init (must be
 *     <= TIKU_KITS_DS_ARRAY_MAX_SIZE).  This lets different array
 *     instances use different logical sizes while sharing the same
 *     compile-time backing buffer.
 *   - @c size -- the number of elements currently stored.  All
 *     access functions bounds-check against this value.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_array arr;
 *   tiku_kits_ds_array_init(&arr, 16);   // use 16 of 32 slots
 *   tiku_kits_ds_array_push_back(&arr, 42);
 *   tiku_kits_ds_array_push_back(&arr, 7);
 *   // arr now contains: [42, 7], size == 2
 * @endcode
 */
struct tiku_kits_ds_array {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_ARRAY_MAX_SIZE];
    uint16_t size;      /**< Number of valid elements currently stored */
    uint16_t capacity;  /**< Runtime capacity set by init (<= MAX_SIZE) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an array with the given capacity
 *
 * Resets the array to an empty state and sets the runtime capacity
 * limit.  The backing buffer is zeroed so that uninitialised reads
 * (before any push/set) return a deterministic value.
 *
 * @param arr      Array to initialize (must not be NULL)
 * @param capacity Maximum number of elements (1..ARRAY_MAX_SIZE)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds MAX_SIZE
 */
int tiku_kits_ds_array_init(struct tiku_kits_ds_array *arr,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Overwrite an existing element at the given index
 *
 * Replaces the element at @p index with @p value.  The index must
 * refer to a slot that has already been populated (i.e. index < size);
 * use push_back() or insert() to add new elements.
 *
 * @param arr   Array (must not be NULL)
 * @param index Position to overwrite (must be < size)
 * @param value New value to store
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if index >= size
 */
int tiku_kits_ds_array_set(struct tiku_kits_ds_array *arr,
                           uint16_t index,
                           tiku_kits_ds_elem_t value);

/**
 * @brief Read the element at the given index
 *
 * Copies the element at @p index into the caller-provided location
 * pointed to by @p value.  The original element remains in the array.
 *
 * @param arr   Array (must not be NULL)
 * @param index Position to read (must be < size)
 * @param value Output pointer where the element is written (must not
 *              be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr or value is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if index >= size
 */
int tiku_kits_ds_array_get(const struct tiku_kits_ds_array *arr,
                           uint16_t index,
                           tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* INSERTION / REMOVAL                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append an element to the end of the array
 *
 * Stores @p value at position @c size and increments the size by one.
 * This is an O(1) operation since no shifting is required.  Fails if
 * the array has already reached its capacity.
 *
 * @param arr   Array (must not be NULL)
 * @param value Value to append
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr is NULL,
 *         TIKU_KITS_DS_ERR_FULL if size == capacity
 */
int tiku_kits_ds_array_push_back(struct tiku_kits_ds_array *arr,
                                 tiku_kits_ds_elem_t value);

/**
 * @brief Remove and optionally return the last element
 *
 * Decrements the size by one.  If @p value is non-NULL the removed
 * element is copied out before the slot is logically freed.  This is
 * an O(1) operation.  Fails if the array is empty.
 *
 * @param arr   Array (must not be NULL)
 * @param value Output pointer for the removed element, or NULL if the
 *              caller does not need the value
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if size == 0
 */
int tiku_kits_ds_array_pop_back(struct tiku_kits_ds_array *arr,
                                tiku_kits_ds_elem_t *value);

/**
 * @brief Insert an element at the given index, shifting successors right
 *
 * All elements at positions [index .. size-1] are shifted one slot to
 * the right to make room for the new value.  This is an O(n) operation
 * in the worst case (inserting at index 0).  Inserting at index == size
 * is equivalent to push_back().
 *
 * @param arr   Array (must not be NULL)
 * @param index Insertion position (0 .. size, inclusive)
 * @param value Value to insert
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if index > size,
 *         TIKU_KITS_DS_ERR_FULL if size == capacity
 */
int tiku_kits_ds_array_insert(struct tiku_kits_ds_array *arr,
                              uint16_t index,
                              tiku_kits_ds_elem_t value);

/**
 * @brief Remove the element at the given index, shifting successors left
 *
 * All elements at positions [index+1 .. size-1] are shifted one slot
 * to the left to fill the gap.  This is an O(n) operation in the worst
 * case (removing at index 0).  The array size is decremented by one.
 *
 * @param arr   Array (must not be NULL)
 * @param index Position to remove (must be < size)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if index >= size
 */
int tiku_kits_ds_array_remove(struct tiku_kits_ds_array *arr,
                              uint16_t index);

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Linear search for the first occurrence of a value
 *
 * Scans the array from index 0 to size-1 and returns the position of
 * the first element that matches @p value.  Because elements are
 * unsorted, a linear scan is the only option -- O(n) worst case.
 * For sorted data, prefer tiku_kits_ds_sortarray which provides
 * O(log n) binary search.
 *
 * @param arr   Array (must not be NULL)
 * @param value Value to search for
 * @param index Output pointer where the found index is written (must
 *              not be NULL)
 * @return TIKU_KITS_DS_OK if found,
 *         TIKU_KITS_DS_ERR_NULL if arr or index is NULL,
 *         TIKU_KITS_DS_ERR_NOTFOUND if value is not present
 */
int tiku_kits_ds_array_find(const struct tiku_kits_ds_array *arr,
                            tiku_kits_ds_elem_t value,
                            uint16_t *index);

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Fill every capacity slot with the given value
 *
 * Writes @p value into all positions [0 .. capacity-1] and sets
 * size equal to capacity.  Useful for pre-populating an array with a
 * default or sentinel value immediately after initialization.
 *
 * @param arr   Array (must not be NULL)
 * @param value Fill value written to every slot
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr is NULL
 */
int tiku_kits_ds_array_fill(struct tiku_kits_ds_array *arr,
                            tiku_kits_ds_elem_t value);

/**
 * @brief Clear the array by resetting size to zero
 *
 * Logically removes all elements by setting size to 0.  The backing
 * buffer is not zeroed for efficiency -- old values remain in memory
 * but are inaccessible through the public API since all access
 * functions bounds-check against size.
 *
 * @param arr Array (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if arr is NULL
 */
int tiku_kits_ds_array_clear(struct tiku_kits_ds_array *arr);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements in the array
 *
 * Returns the logical size (number of valid, populated elements),
 * not the capacity.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param arr Array, or NULL
 * @return Number of elements currently stored, or 0 if arr is NULL
 */
uint16_t tiku_kits_ds_array_size(const struct tiku_kits_ds_array *arr);

#endif /* TIKU_KITS_DS_ARRAY_H_ */
