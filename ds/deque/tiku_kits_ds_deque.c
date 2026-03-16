/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_deque.c - Double-ended queue (circular-array based)
 *
 * Platform-independent implementation of a circular-buffer deque.
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

#include "tiku_kits_ds_deque.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a deque with the given capacity
 *
 * Zeros the entire backing buffer so that any subsequent reads of
 * unpopulated slots return a deterministic value.  The runtime
 * capacity is clamped to TIKU_KITS_DS_DEQUE_MAX_SIZE at compile time
 * so that the static buffer is never overrun.
 */
int tiku_kits_ds_deque_init(struct tiku_kits_ds_deque *dq,
                            uint16_t capacity)
{
    if (dq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_DEQUE_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    dq->head = 0;
    dq->count = 0;
    dq->capacity = capacity;
    memset(dq->data, 0, sizeof(dq->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PUSH / POP                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add an element to the front of the deque
 *
 * O(1) -- decrements head modulo capacity (wrapping backwards) and
 * writes the value at the new head slot.  No element shifting needed.
 */
int tiku_kits_ds_deque_push_front(struct tiku_kits_ds_deque *dq,
                                  tiku_kits_ds_elem_t value)
{
    if (dq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count >= dq->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    /* Wrap head backwards: add capacity before subtracting to avoid
     * underflow on unsigned arithmetic. */
    dq->head = (dq->head - 1 + dq->capacity) % dq->capacity;
    dq->data[dq->head] = value;
    dq->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Add an element to the back of the deque
 *
 * O(1) -- computes the tail index as (head + count) % capacity and
 * writes the value there.  No element shifting needed.
 */
int tiku_kits_ds_deque_push_back(struct tiku_kits_ds_deque *dq,
                                 tiku_kits_ds_elem_t value)
{
    uint16_t tail;

    if (dq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count >= dq->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    /* Tail is the first free slot past the last element */
    tail = (dq->head + dq->count) % dq->capacity;
    dq->data[tail] = value;
    dq->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove and return the front element of the deque
 *
 * O(1) -- copies the element at head, then advances head forward
 * modulo capacity.  The vacated slot is not cleared; it becomes
 * inaccessible because all access functions check against count.
 */
int tiku_kits_ds_deque_pop_front(struct tiku_kits_ds_deque *dq,
                                 tiku_kits_ds_elem_t *value)
{
    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = dq->data[dq->head];
    dq->head = (dq->head + 1) % dq->capacity;
    dq->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove and return the back element of the deque
 *
 * O(1) -- computes the back index as (head + count - 1) % capacity,
 * copies the element out, and decrements count.  The vacated slot
 * is not cleared; it becomes inaccessible via the public API.
 */
int tiku_kits_ds_deque_pop_back(struct tiku_kits_ds_deque *dq,
                                tiku_kits_ds_elem_t *value)
{
    uint16_t back;

    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    back = (dq->head + dq->count - 1) % dq->capacity;
    *value = dq->data[back];
    dq->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PEEK                                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read the front element without removing it
 *
 * O(1) -- copies the element at the head position through @p value.
 * The deque state is not modified.
 */
int tiku_kits_ds_deque_peek_front(
    const struct tiku_kits_ds_deque *dq,
    tiku_kits_ds_elem_t *value)
{
    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = dq->data[dq->head];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read the back element without removing it
 *
 * O(1) -- computes the back index as (head + count - 1) % capacity
 * and copies the element through @p value.  The deque state is not
 * modified.
 */
int tiku_kits_ds_deque_peek_back(
    const struct tiku_kits_ds_deque *dq,
    tiku_kits_ds_elem_t *value)
{
    uint16_t back;

    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    back = (dq->head + dq->count - 1) % dq->capacity;
    *value = dq->data[back];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* RANDOM ACCESS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read an element by logical index (0 = front)
 *
 * O(1) -- translates the logical index to a physical position in the
 * circular buffer via (head + index) % capacity, then copies the
 * element out.  The deque contents are not modified.
 */
int tiku_kits_ds_deque_get(
    const struct tiku_kits_ds_deque *dq,
    uint16_t index,
    tiku_kits_ds_elem_t *value)
{
    uint16_t phys;

    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= dq->count) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    /* Map logical index to physical slot in the circular buffer */
    phys = (dq->head + index) % dq->capacity;
    *value = dq->data[phys];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the deque to empty
 *
 * Logically removes all elements by resetting head and count to 0
 * without zeroing the backing buffer.  Old values remain in memory
 * but are inaccessible through the public API because all access
 * functions bounds-check against count.
 */
int tiku_kits_ds_deque_clear(struct tiku_kits_ds_deque *dq)
{
    if (dq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    dq->head = 0;
    dq->count = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of elements currently in the deque
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the logical count, not the capacity.
 */
uint16_t tiku_kits_ds_deque_count(
    const struct tiku_kits_ds_deque *dq)
{
    if (dq == NULL) {
        return 0;
    }
    return dq->count;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether the deque is full
 *
 * Returns 1 when count equals capacity.  Safe to call with a NULL
 * pointer -- returns 0.
 */
int tiku_kits_ds_deque_full(
    const struct tiku_kits_ds_deque *dq)
{
    if (dq == NULL) {
        return 0;
    }
    return (dq->count == dq->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether the deque is empty
 *
 * Returns 1 when the deque contains no elements.  Safe to call with
 * a NULL pointer -- returns 0.
 */
int tiku_kits_ds_deque_empty(
    const struct tiku_kits_ds_deque *dq)
{
    if (dq == NULL) {
        return 0;
    }
    return (dq->count == 0) ? 1 : 0;
}
