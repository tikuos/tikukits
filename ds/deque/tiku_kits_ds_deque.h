/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_deque.h - Double-ended queue (circular-array based)
 *
 * Platform-independent double-ended queue for embedded systems. All
 * storage is statically allocated using a compile-time maximum size.
 * The element type is inherited from the DS common header.
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

#ifndef TIKU_KITS_DS_DEQUE_H_
#define TIKU_KITS_DS_DEQUE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of elements the deque can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_DEQUE_MAX_SIZE
#define TIKU_KITS_DS_DEQUE_MAX_SIZE 32
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_deque
 * @brief Fixed-capacity double-ended queue with static storage
 *
 * Implemented as a circular buffer. Elements can be added or removed
 * from either end. The head field points to the front element and
 * count tracks the number of stored elements. The capacity field
 * stores the runtime capacity (<= TIKU_KITS_DS_DEQUE_MAX_SIZE).
 *
 * Declare deques as static or local variables -- no heap required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_deque dq;
 *   tiku_kits_ds_deque_init(&dq, 16);
 *   tiku_kits_ds_deque_push_back(&dq, 42);
 *   tiku_kits_ds_deque_push_front(&dq, 7);
 * @endcode
 */
struct tiku_kits_ds_deque {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_DEQUE_MAX_SIZE];
    uint16_t head;      /**< Index of the front element        */
    uint16_t count;     /**< Number of elements in deque       */
    uint16_t capacity;  /**< Runtime capacity (<= MAX_SIZE)    */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a deque with the given capacity
 * @param dq       Deque to initialize
 * @param capacity Maximum number of elements
 *                 (1..TIKU_KITS_DS_DEQUE_MAX_SIZE)
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_PARAM
 */
int tiku_kits_ds_deque_init(struct tiku_kits_ds_deque *dq,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* PUSH / POP                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add an element to the front of the deque
 * @param dq    Deque
 * @param value Element to insert
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_deque_push_front(struct tiku_kits_ds_deque *dq,
                                  tiku_kits_ds_elem_t value);

/**
 * @brief Add an element to the back of the deque
 * @param dq    Deque
 * @param value Element to insert
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_deque_push_back(struct tiku_kits_ds_deque *dq,
                                 tiku_kits_ds_elem_t value);

/**
 * @brief Remove an element from the front of the deque
 * @param dq    Deque
 * @param value Output: the removed element
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_deque_pop_front(struct tiku_kits_ds_deque *dq,
                                 tiku_kits_ds_elem_t *value);

/**
 * @brief Remove an element from the back of the deque
 * @param dq    Deque
 * @param value Output: the removed element
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_deque_pop_back(struct tiku_kits_ds_deque *dq,
                                tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* PEEK                                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read the front element without removing it
 * @param dq    Deque
 * @param value Output: the front element
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_deque_peek_front(
    const struct tiku_kits_ds_deque *dq,
    tiku_kits_ds_elem_t *value);

/**
 * @brief Read the back element without removing it
 * @param dq    Deque
 * @param value Output: the back element
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_deque_peek_back(
    const struct tiku_kits_ds_deque *dq,
    tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* RANDOM ACCESS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read an element by logical index (0 = front)
 * @param dq    Deque
 * @param index Logical index (0..count-1)
 * @param value Output: the element at the given index
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_BOUNDS
 */
int tiku_kits_ds_deque_get(
    const struct tiku_kits_ds_deque *dq,
    uint16_t index,
    tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the deque to empty (head and count set to 0)
 * @param dq Deque to clear
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_deque_clear(struct tiku_kits_ds_deque *dq);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of elements currently in the deque
 * @param dq Deque
 * @return Element count, or 0 if dq is NULL
 */
uint16_t tiku_kits_ds_deque_count(
    const struct tiku_kits_ds_deque *dq);

/**
 * @brief Check whether the deque is full
 * @param dq Deque
 * @return 1 if count == capacity, 0 otherwise (0 if NULL)
 */
int tiku_kits_ds_deque_full(
    const struct tiku_kits_ds_deque *dq);

/**
 * @brief Check whether the deque is empty
 * @param dq Deque
 * @return 1 if count == 0, 0 otherwise (1 if NULL)
 */
int tiku_kits_ds_deque_empty(
    const struct tiku_kits_ds_deque *dq);

#endif /* TIKU_KITS_DS_DEQUE_H_ */
