/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_queue.c - FIFO queue (ring-buffer based)
 *
 * Platform-independent implementation of a circular-buffer FIFO queue.
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

#include "tiku_kits_ds_queue.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a queue with the given capacity
 *
 * Zeros the entire backing buffer so that any subsequent reads of
 * unpopulated slots return a deterministic value.  The runtime
 * capacity is clamped to TIKU_KITS_DS_QUEUE_MAX_SIZE at compile time
 * so that the static buffer is never overrun.
 */
int tiku_kits_ds_queue_init(struct tiku_kits_ds_queue *q,
                            uint16_t capacity)
{
    if (q == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_QUEUE_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->capacity = capacity;

    /* Zero the full static buffer, not just `capacity` slots, so that
     * the struct is in a clean state regardless of prior contents. */
    memset(q->data, 0, sizeof(q->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* ENQUEUE / DEQUEUE                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add an element to the tail of the queue
 *
 * O(1) -- writes into the current tail slot, then advances the tail
 * index using modulo arithmetic to wrap around the circular buffer.
 */
int tiku_kits_ds_queue_enqueue(struct tiku_kits_ds_queue *q,
                               tiku_kits_ds_elem_t value)
{
    if (q == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (q->count >= q->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    q->data[q->tail] = value;
    /* Modulo wrap keeps the index within [0, capacity) without
     * branching -- works because capacity is always > 0 after init. */
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove and return the element at the head of the queue
 *
 * O(1) -- copies the head element out through @p value, then advances
 * the head index via modulo wrap.  The slot is not cleared; it becomes
 * inaccessible because all access functions check count.
 */
int tiku_kits_ds_queue_dequeue(struct tiku_kits_ds_queue *q,
                               tiku_kits_ds_elem_t *value)
{
    if (q == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (q->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = q->data[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read the head element without removing it
 *
 * Copies the value out through @p value so the caller owns its own
 * copy.  The head index and count remain untouched, so the element
 * is still available for a subsequent dequeue().
 */
int tiku_kits_ds_queue_peek(const struct tiku_kits_ds_queue *q,
                            tiku_kits_ds_elem_t *value)
{
    if (q == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (q->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = q->data[q->head];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the queue by resetting head, tail, and count to zero
 *
 * Logically removes all elements without zeroing the backing buffer.
 * Old values remain in memory but are inaccessible through the public
 * API because all access functions check count before reading.
 */
int tiku_kits_ds_queue_clear(struct tiku_kits_ds_queue *q)
{
    if (q == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    q->head = 0;
    q->tail = 0;
    q->count = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements in the queue
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the logical size, not the capacity.
 */
uint16_t tiku_kits_ds_queue_size(const struct tiku_kits_ds_queue *q)
{
    if (q == NULL) {
        return 0;
    }
    return q->count;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether the queue is full
 *
 * Returns 1 when count equals capacity.  A NULL pointer is treated
 * as not full (returns 0) to allow safe guard expressions.
 */
int tiku_kits_ds_queue_full(const struct tiku_kits_ds_queue *q)
{
    if (q == NULL) {
        return 0;
    }
    return (q->count == q->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether the queue is empty
 *
 * Returns 1 when count is zero.  A NULL pointer is treated as empty
 * (returns 1) to allow safe guard expressions.
 */
int tiku_kits_ds_queue_empty(const struct tiku_kits_ds_queue *q)
{
    if (q == NULL) {
        return 1;
    }
    return (q->count == 0) ? 1 : 0;
}
