/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_ringbuf.c - Fixed-size circular buffer implementation
 *
 * Platform-independent implementation of a fixed-capacity ring buffer.
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

#include "tiku_kits_ds_ringbuf.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a ring buffer with the given capacity
 *
 * Zeros the entire backing buffer so that any subsequent reads of
 * unpopulated slots return a deterministic value.  The runtime
 * capacity is clamped to TIKU_KITS_DS_RINGBUF_MAX_SIZE at compile
 * time so that the static buffer is never overrun.
 */
int tiku_kits_ds_ringbuf_init(struct tiku_kits_ds_ringbuf *rb,
                              uint16_t capacity)
{
    if (rb == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_RINGBUF_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->capacity = capacity;

    /* Zero the full static buffer, not just `capacity` slots, so that
     * the struct is in a clean state regardless of prior contents. */
    memset(rb->data, 0, sizeof(rb->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PUSH / POP / PEEK                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push an element into the ring buffer
 *
 * O(1) -- writes into the current tail slot, then advances the tail
 * index using modulo arithmetic to wrap around the circular buffer.
 */
int tiku_kits_ds_ringbuf_push(struct tiku_kits_ds_ringbuf *rb,
                              tiku_kits_ds_elem_t value)
{
    if (rb == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (rb->count >= rb->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    rb->data[rb->tail] = value;
    /* Modulo wrap keeps the index within [0, capacity) without
     * branching -- works because capacity is always > 0 after init. */
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Pop the oldest element from the ring buffer
 *
 * O(1) -- copies the head element out through @p value, then advances
 * the head index via modulo wrap.  The slot is not cleared; it becomes
 * inaccessible because all access functions check count.
 */
int tiku_kits_ds_ringbuf_pop(struct tiku_kits_ds_ringbuf *rb,
                             tiku_kits_ds_elem_t *value)
{
    if (rb == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (rb->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = rb->data[rb->head];
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read the oldest element without removing it
 *
 * Copies the value out through @p value so the caller owns its own
 * copy.  The head index and count remain untouched, so the element
 * is still available for a subsequent pop().
 */
int tiku_kits_ds_ringbuf_peek(const struct tiku_kits_ds_ringbuf *rb,
                              tiku_kits_ds_elem_t *value)
{
    if (rb == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (rb->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = rb->data[rb->head];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the ring buffer by resetting head, tail, and count
 *
 * Logically removes all elements without zeroing the backing buffer.
 * Old values remain in memory but are inaccessible through the public
 * API because all access functions check count before reading.
 */
int tiku_kits_ds_ringbuf_clear(struct tiku_kits_ds_ringbuf *rb)
{
    if (rb == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements in the ring buffer
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the logical size, not the capacity.
 */
uint16_t tiku_kits_ds_ringbuf_count(
    const struct tiku_kits_ds_ringbuf *rb)
{
    if (rb == NULL) {
        return 0;
    }
    return rb->count;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check if the ring buffer is full
 *
 * Returns 1 when count equals capacity.  A NULL pointer is treated
 * as not full (returns 0) to allow safe guard expressions.
 */
int tiku_kits_ds_ringbuf_full(const struct tiku_kits_ds_ringbuf *rb)
{
    if (rb == NULL) {
        return 0;
    }
    return (rb->count >= rb->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check if the ring buffer is empty
 *
 * Returns 1 when count is zero.  A NULL pointer is treated as not
 * empty (returns 0) to match the existing convention for this module.
 */
int tiku_kits_ds_ringbuf_empty(
    const struct tiku_kits_ds_ringbuf *rb)
{
    if (rb == NULL) {
        return 0;
    }
    return (rb->count == 0) ? 1 : 0;
}
