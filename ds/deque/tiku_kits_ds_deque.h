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
 * @brief Maximum number of elements the deque can hold.
 *
 * This compile-time constant defines the upper bound on deque capacity.
 * Each deque instance reserves this many element slots in its static
 * circular buffer, so choose a value that balances memory usage against
 * the largest deque your application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_DEQUE_MAX_SIZE 64
 *   #include "tiku_kits_ds_deque.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_DEQUE_MAX_SIZE
#define TIKU_KITS_DS_DEQUE_MAX_SIZE 32
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_deque
 * @brief Fixed-capacity double-ended queue with static circular-buffer storage
 *
 * A general-purpose double-ended queue that stores elements in a
 * statically allocated circular buffer.  Because all storage lives
 * inside the struct itself, no heap allocation is needed -- just
 * declare the deque as a static or local variable.
 *
 * The circular-buffer design allows O(1) push/pop at both ends by
 * maintaining a @c head index and a @c count:
 *   - @c head -- index of the front element in the backing array.
 *     When an element is pushed to the front, head wraps backwards
 *     modulo capacity; when popped from the front, head advances
 *     forward modulo capacity.
 *   - @c count -- the number of elements currently stored.  All
 *     access functions bounds-check against this value.
 *   - @c capacity -- the runtime limit passed to init (must be
 *     <= TIKU_KITS_DS_DEQUE_MAX_SIZE).  This lets different deque
 *     instances use different logical sizes while sharing the same
 *     compile-time backing buffer.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_deque dq;
 *   tiku_kits_ds_deque_init(&dq, 16);   // use 16 of 32 slots
 *   tiku_kits_ds_deque_push_back(&dq, 42);
 *   tiku_kits_ds_deque_push_front(&dq, 7);
 *   // dq now contains: [7, 42], count == 2
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
 *
 * Resets the deque to an empty state and sets the runtime capacity
 * limit.  The backing circular buffer is zeroed so that uninitialised
 * reads return a deterministic value.
 *
 * @param dq       Deque to initialize (must not be NULL)
 * @param capacity Maximum number of elements (1..DEQUE_MAX_SIZE)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds MAX_SIZE
 */
int tiku_kits_ds_deque_init(struct tiku_kits_ds_deque *dq,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* PUSH / POP                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add an element to the front of the deque
 *
 * Decrements the head index (wrapping around via modular arithmetic)
 * and stores @p value at the new head position.  This is an O(1)
 * operation since no element shifting is required -- only the head
 * pointer moves.  Fails if the deque has reached its capacity.
 *
 * @param dq    Deque (must not be NULL)
 * @param value Element to insert
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq is NULL,
 *         TIKU_KITS_DS_ERR_FULL if count == capacity
 */
int tiku_kits_ds_deque_push_front(struct tiku_kits_ds_deque *dq,
                                  tiku_kits_ds_elem_t value);

/**
 * @brief Add an element to the back of the deque
 *
 * Computes the tail index as (head + count) % capacity and stores
 * @p value there.  This is an O(1) operation since no element
 * shifting is required.  Fails if the deque has reached its capacity.
 *
 * @param dq    Deque (must not be NULL)
 * @param value Element to insert
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq is NULL,
 *         TIKU_KITS_DS_ERR_FULL if count == capacity
 */
int tiku_kits_ds_deque_push_back(struct tiku_kits_ds_deque *dq,
                                 tiku_kits_ds_elem_t value);

/**
 * @brief Remove and return the front element of the deque
 *
 * Copies the element at the head position into @p value, then
 * advances the head index forward (wrapping modulo capacity) and
 * decrements the count.  This is an O(1) operation.  Fails if the
 * deque is empty.
 *
 * @param dq    Deque (must not be NULL)
 * @param value Output pointer where the removed element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if count == 0
 */
int tiku_kits_ds_deque_pop_front(struct tiku_kits_ds_deque *dq,
                                 tiku_kits_ds_elem_t *value);

/**
 * @brief Remove and return the back element of the deque
 *
 * Computes the back index as (head + count - 1) % capacity, copies
 * the element at that position into @p value, then decrements the
 * count.  This is an O(1) operation.  Fails if the deque is empty.
 *
 * @param dq    Deque (must not be NULL)
 * @param value Output pointer where the removed element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if count == 0
 */
int tiku_kits_ds_deque_pop_back(struct tiku_kits_ds_deque *dq,
                                tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* PEEK                                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read the front element without removing it
 *
 * Copies the element at the head position into @p value.  The
 * element remains in the deque and the head/count are unchanged.
 * This is an O(1) operation.
 *
 * @param dq    Deque (must not be NULL)
 * @param value Output pointer where the front element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if count == 0
 */
int tiku_kits_ds_deque_peek_front(
    const struct tiku_kits_ds_deque *dq,
    tiku_kits_ds_elem_t *value);

/**
 * @brief Read the back element without removing it
 *
 * Computes the back index as (head + count - 1) % capacity and
 * copies the element at that position into @p value.  The element
 * remains in the deque and the head/count are unchanged.  This is
 * an O(1) operation.
 *
 * @param dq    Deque (must not be NULL)
 * @param value Output pointer where the back element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if count == 0
 */
int tiku_kits_ds_deque_peek_back(
    const struct tiku_kits_ds_deque *dq,
    tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* RANDOM ACCESS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read an element by logical index (0 = front)
 *
 * Maps the logical index to a physical position in the circular
 * buffer via (head + index) % capacity, then copies the element
 * into @p value.  The deque contents are not modified.  This is
 * an O(1) operation.
 *
 * @param dq    Deque (must not be NULL)
 * @param index Logical index from the front (0 .. count-1)
 * @param value Output pointer where the element is written (must
 *              not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq or value is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if index >= count
 */
int tiku_kits_ds_deque_get(
    const struct tiku_kits_ds_deque *dq,
    uint16_t index,
    tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the deque to empty
 *
 * Logically removes all elements by resetting head and count to 0.
 * The backing buffer is not zeroed for efficiency -- old values
 * remain in memory but are inaccessible through the public API
 * since all access functions bounds-check against count.
 *
 * @param dq Deque to clear (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if dq is NULL
 */
int tiku_kits_ds_deque_clear(struct tiku_kits_ds_deque *dq);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of elements currently in the deque
 *
 * Returns the logical count (number of valid, populated elements),
 * not the capacity.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param dq Deque, or NULL
 * @return Number of elements currently stored, or 0 if dq is NULL
 */
uint16_t tiku_kits_ds_deque_count(
    const struct tiku_kits_ds_deque *dq);

/**
 * @brief Check whether the deque is full
 *
 * Returns 1 when the deque has reached its runtime capacity and no
 * further pushes can succeed.  Safe to call with a NULL pointer --
 * returns 0 (a NULL deque is not considered full).
 *
 * @param dq Deque, or NULL
 * @return 1 if count == capacity, 0 otherwise (0 if dq is NULL)
 */
int tiku_kits_ds_deque_full(
    const struct tiku_kits_ds_deque *dq);

/**
 * @brief Check whether the deque is empty
 *
 * Returns 1 when the deque contains no elements.  Safe to call with
 * a NULL pointer -- returns 0 (consistent with other query functions
 * that return a safe default for NULL).
 *
 * @param dq Deque, or NULL
 * @return 1 if count == 0, 0 otherwise (0 if dq is NULL)
 */
int tiku_kits_ds_deque_empty(
    const struct tiku_kits_ds_deque *dq);

#endif /* TIKU_KITS_DS_DEQUE_H_ */
