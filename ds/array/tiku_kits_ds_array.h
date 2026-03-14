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
 * Maximum number of elements the array can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_ARRAY_MAX_SIZE
#define TIKU_KITS_DS_ARRAY_MAX_SIZE 32
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_array
 * @brief Fixed-capacity array with static storage
 *
 * Elements are stored contiguously. The capacity field tracks the
 * user-requested limit (<= MAX_SIZE), and size tracks the current
 * number of valid elements.
 *
 * Declare arrays as static or local variables -- no heap required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_array arr;
 *   tiku_kits_ds_array_init(&arr, 16);
 *   tiku_kits_ds_array_push_back(&arr, 42);
 * @endcode
 */
struct tiku_kits_ds_array {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_ARRAY_MAX_SIZE];
    uint16_t size;      /**< Number of valid elements */
    uint16_t capacity;  /**< User-requested capacity */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an array with the given capacity
 * @param arr      Array to initialize
 * @param capacity Maximum number of elements (1..ARRAY_MAX_SIZE)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_array_init(struct tiku_kits_ds_array *arr,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set an element at the given index
 * @param arr   Array
 * @param index Index (must be < size)
 * @param value Value to set
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_array_set(struct tiku_kits_ds_array *arr,
                           uint16_t index,
                           tiku_kits_ds_elem_t value);

/**
 * @brief Get an element at the given index
 * @param arr   Array
 * @param index Index (must be < size)
 * @param value Output: element value
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_array_get(const struct tiku_kits_ds_array *arr,
                           uint16_t index,
                           tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* INSERTION / REMOVAL                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append an element to the end of the array
 * @param arr   Array
 * @param value Value to append
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_array_push_back(struct tiku_kits_ds_array *arr,
                                 tiku_kits_ds_elem_t value);

/**
 * @brief Remove the last element from the array
 * @param arr   Array
 * @param value Output: removed element (may be NULL)
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_array_pop_back(struct tiku_kits_ds_array *arr,
                                tiku_kits_ds_elem_t *value);

/**
 * @brief Insert an element at the given index, shifting right
 * @param arr   Array
 * @param index Insertion position (0..size)
 * @param value Value to insert
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_array_insert(struct tiku_kits_ds_array *arr,
                              uint16_t index,
                              tiku_kits_ds_elem_t value);

/**
 * @brief Remove an element at the given index, shifting left
 * @param arr   Array
 * @param index Position to remove (must be < size)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_array_remove(struct tiku_kits_ds_array *arr,
                              uint16_t index);

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Linear search for a value
 * @param arr   Array
 * @param value Value to find
 * @param index Output: index of first occurrence
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND
 */
int tiku_kits_ds_array_find(const struct tiku_kits_ds_array *arr,
                            tiku_kits_ds_elem_t value,
                            uint16_t *index);

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Fill all capacity slots with the given value
 * @param arr   Array
 * @param value Fill value
 * @return TIKU_KITS_DS_OK or error code
 *
 * Sets size equal to capacity after filling.
 */
int tiku_kits_ds_array_fill(struct tiku_kits_ds_array *arr,
                            tiku_kits_ds_elem_t value);

/**
 * @brief Clear the array (set size to 0)
 * @param arr Array
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_array_clear(struct tiku_kits_ds_array *arr);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of elements
 * @param arr Array
 * @return Current size, or 0 if arr is NULL
 */
uint16_t tiku_kits_ds_array_size(const struct tiku_kits_ds_array *arr);

#endif /* TIKU_KITS_DS_ARRAY_H_ */
