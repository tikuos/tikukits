/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_ringbuf.h - Fixed-size circular (ring) buffer
 *
 * Platform-independent ring buffer library for embedded systems. All
 * storage is statically allocated using a compile-time maximum capacity.
 * The element type is configured via tiku_kits_ds_elem_t.
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

#ifndef TIKU_KITS_DS_RINGBUF_H_
#define TIKU_KITS_DS_RINGBUF_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of elements the ring buffer can hold.
 *
 * This compile-time constant defines the upper bound on ring buffer
 * capacity.  Each ring buffer instance reserves this many element
 * slots in its static storage, so choose a value that balances memory
 * usage against the largest buffer your application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_RINGBUF_MAX_SIZE 64
 *   #include "tiku_kits_ds_ringbuf.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_RINGBUF_MAX_SIZE
#define TIKU_KITS_DS_RINGBUF_MAX_SIZE 32
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_ringbuf
 * @brief Fixed-capacity circular buffer with contiguous static storage
 *
 * A general-purpose ring buffer (circular buffer) that stores elements
 * in a contiguous block of statically allocated memory.  Because all
 * storage lives inside the struct itself, no heap allocation is needed
 * -- just declare the ring buffer as a static or local variable.
 *
 * Two indices and two counters are tracked:
 *   - @c head -- the read position: index of the next element that
 *     pop() will return.  Advances forward via modulo arithmetic on
 *     each pop.
 *   - @c tail -- the write position: index of the next free slot
 *     where push() will store a new element.  Advances forward via
 *     modulo arithmetic on each push.
 *   - @c count -- the number of elements currently stored.  This
 *     disambiguates the full vs. empty state (head == tail in both
 *     cases without a separate counter).
 *   - @c capacity -- the runtime limit passed to init (must be
 *     <= TIKU_KITS_DS_RINGBUF_MAX_SIZE).  This lets different ring
 *     buffer instances use different logical sizes while sharing the
 *     same compile-time backing buffer.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_ringbuf rb;
 *   tiku_kits_ds_ringbuf_init(&rb, 16);   // use 16 of 32 slots
 *   tiku_kits_ds_ringbuf_push(&rb, 42);
 *   tiku_kits_ds_ringbuf_push(&rb, 7);
 *
 *   tiku_kits_ds_elem_t val;
 *   tiku_kits_ds_ringbuf_pop(&rb, &val);  // val == 42 (FIFO order)
 * @endcode
 */
struct tiku_kits_ds_ringbuf {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_RINGBUF_MAX_SIZE];
    uint16_t head;      /**< Read position: index of next element to pop */
    uint16_t tail;      /**< Write position: index of next free slot */
    uint16_t count;     /**< Number of valid elements currently stored */
    uint16_t capacity;  /**< Runtime capacity set by init (<= MAX_SIZE) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a ring buffer with the given capacity
 *
 * Resets the ring buffer to an empty state and sets the runtime
 * capacity limit.  The backing buffer is zeroed so that uninitialised
 * reads (before any push) return a deterministic value.
 *
 * @param rb       Ring buffer to initialize (must not be NULL)
 * @param capacity Maximum number of elements
 *                 (1..TIKU_KITS_DS_RINGBUF_MAX_SIZE)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if rb is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds MAX_SIZE
 */
int tiku_kits_ds_ringbuf_init(struct tiku_kits_ds_ringbuf *rb,
                              uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* PUSH / POP / PEEK                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push an element into the ring buffer
 *
 * Stores @p value at the current tail position and advances the tail
 * index via modulo arithmetic.  This is an O(1) operation since no
 * element shifting is required.  Fails if the ring buffer has already
 * reached its capacity.
 *
 * @param rb    Ring buffer (must not be NULL)
 * @param value Value to push
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if rb is NULL,
 *         TIKU_KITS_DS_ERR_FULL if count == capacity
 */
int tiku_kits_ds_ringbuf_push(struct tiku_kits_ds_ringbuf *rb,
                              tiku_kits_ds_elem_t value);

/**
 * @brief Pop the oldest element from the ring buffer
 *
 * Copies the element at the head position into the caller-provided
 * location pointed to by @p value, then advances the head index via
 * modulo arithmetic.  This is an O(1) operation.  Fails if the ring
 * buffer is empty.
 *
 * @param rb    Ring buffer (must not be NULL)
 * @param value Output pointer where the popped element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if rb or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if count == 0
 */
int tiku_kits_ds_ringbuf_pop(struct tiku_kits_ds_ringbuf *rb,
                             tiku_kits_ds_elem_t *value);

/**
 * @brief Read the oldest element without removing it
 *
 * Copies the element at the head position into the caller-provided
 * location pointed to by @p value without advancing the head index.
 * The element remains in the ring buffer and will be returned again
 * by the next pop() call.  This is an O(1) operation.
 *
 * @param rb    Ring buffer (must not be NULL)
 * @param value Output pointer where the head element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if rb or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if count == 0
 */
int tiku_kits_ds_ringbuf_peek(const struct tiku_kits_ds_ringbuf *rb,
                              tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the ring buffer by resetting head, tail, and count
 *
 * Logically removes all elements by resetting the indices and count
 * to zero.  The backing buffer is not zeroed for efficiency -- old
 * values remain in memory but are inaccessible through the public API
 * since all access functions check count before reading.
 *
 * @param rb Ring buffer (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if rb is NULL
 */
int tiku_kits_ds_ringbuf_clear(struct tiku_kits_ds_ringbuf *rb);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements in the ring buffer
 *
 * Returns the logical size (number of valid, populated elements),
 * not the capacity.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param rb Ring buffer, or NULL
 * @return Number of elements currently stored, or 0 if rb is NULL
 */
uint16_t tiku_kits_ds_ringbuf_count(
    const struct tiku_kits_ds_ringbuf *rb);

/**
 * @brief Check if the ring buffer is full
 *
 * Returns 1 when count equals capacity, meaning no more elements can
 * be pushed without first popping.  Safe to call with a NULL pointer
 * -- returns 0 (not full).
 *
 * @param rb Ring buffer, or NULL
 * @return 1 if count == capacity, 0 otherwise or if rb is NULL
 */
int tiku_kits_ds_ringbuf_full(const struct tiku_kits_ds_ringbuf *rb);

/**
 * @brief Check if the ring buffer is empty
 *
 * Returns 1 when count is zero, meaning no elements are available for
 * pop.  A NULL pointer is treated as not empty and returns 0.
 *
 * @param rb Ring buffer, or NULL
 * @return 1 if count == 0, 0 otherwise or if rb is NULL
 */
int tiku_kits_ds_ringbuf_empty(
    const struct tiku_kits_ds_ringbuf *rb);

#endif /* TIKU_KITS_DS_RINGBUF_H_ */
