/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_queue.h - FIFO queue (ring-buffer based)
 *
 * Platform-independent FIFO queue for embedded systems. All storage
 * is statically allocated using a compile-time maximum size. The
 * element type is inherited from the DS common header.
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

#ifndef TIKU_KITS_DS_QUEUE_H_
#define TIKU_KITS_DS_QUEUE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of elements the queue can hold.
 *
 * This compile-time constant defines the upper bound on queue capacity.
 * Each queue instance reserves this many element slots in its static
 * circular buffer, so choose a value that balances memory usage against
 * the largest queue your application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_QUEUE_MAX_SIZE 64
 *   #include "tiku_kits_ds_queue.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_QUEUE_MAX_SIZE
#define TIKU_KITS_DS_QUEUE_MAX_SIZE 16
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_queue
 * @brief Fixed-capacity FIFO queue with contiguous static storage
 *
 * A general-purpose, first-in-first-out container implemented as a
 * circular buffer.  Elements are written at the tail index and read
 * from the head index; both indices wrap around via modulo arithmetic
 * so that the buffer is reused without shifting elements.  Because
 * all storage lives inside the struct itself, no heap allocation is
 * needed -- just declare the queue as a static or local variable.
 *
 * Three indices / counters are tracked:
 *   - @c head -- the read position: index of the next element that
 *     dequeue() will return.
 *   - @c tail -- the write position: index of the next free slot
 *     where enqueue() will store a new element.
 *   - @c count -- the number of elements currently stored.  This
 *     disambiguates the full vs. empty state (head == tail in both
 *     cases without a separate counter).
 *   - @c capacity -- the runtime limit passed to init (must be
 *     <= TIKU_KITS_DS_QUEUE_MAX_SIZE).  This lets different queue
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
 *   struct tiku_kits_ds_queue q;
 *   tiku_kits_ds_queue_init(&q, 8);
 *   tiku_kits_ds_queue_enqueue(&q, 42);
 *   tiku_kits_ds_queue_enqueue(&q, 7);
 *
 *   tiku_kits_ds_elem_t val;
 *   tiku_kits_ds_queue_dequeue(&q, &val);  // val == 42 (FIFO)
 * @endcode
 */
struct tiku_kits_ds_queue {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_QUEUE_MAX_SIZE];
    uint16_t head;      /**< Read position: index of next element to dequeue */
    uint16_t tail;      /**< Write position: index of next free slot */
    uint16_t count;     /**< Number of valid elements currently stored */
    uint16_t capacity;  /**< Runtime capacity set by init (<= MAX_SIZE) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a queue with the given capacity
 *
 * Resets the queue to an empty state and sets the runtime capacity
 * limit.  The backing buffer is zeroed so that uninitialised reads
 * (before any enqueue) return a deterministic value.
 *
 * @param q        Queue to initialize (must not be NULL)
 * @param capacity Maximum number of elements
 *                 (1..TIKU_KITS_DS_QUEUE_MAX_SIZE)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if q is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds MAX_SIZE
 */
int tiku_kits_ds_queue_init(struct tiku_kits_ds_queue *q,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* ENQUEUE / DEQUEUE                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add an element to the tail of the queue
 *
 * Stores @p value at the current tail position and advances the tail
 * index via modulo arithmetic.  This is an O(1) operation since no
 * element shifting is required.  Fails if the queue has already
 * reached its capacity.
 *
 * @param q     Queue (must not be NULL)
 * @param value Value to enqueue
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if q is NULL,
 *         TIKU_KITS_DS_ERR_FULL if count == capacity
 */
int tiku_kits_ds_queue_enqueue(struct tiku_kits_ds_queue *q,
                               tiku_kits_ds_elem_t value);

/**
 * @brief Remove and return the element at the head of the queue
 *
 * Copies the oldest element into the caller-provided location pointed
 * to by @p value, then advances the head index via modulo arithmetic.
 * This is an O(1) operation.  Fails if the queue is empty.
 *
 * @param q     Queue (must not be NULL)
 * @param value Output pointer where the dequeued element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if q or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if count == 0
 */
int tiku_kits_ds_queue_dequeue(struct tiku_kits_ds_queue *q,
                               tiku_kits_ds_elem_t *value);

/**
 * @brief Read the head element without removing it
 *
 * Copies the oldest element into the caller-provided location pointed
 * to by @p value without advancing the head index.  The element
 * remains in the queue and will be returned again by the next
 * dequeue() call.  This is an O(1) operation.
 *
 * @param q     Queue (must not be NULL)
 * @param value Output pointer where the head element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if q or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if count == 0
 */
int tiku_kits_ds_queue_peek(const struct tiku_kits_ds_queue *q,
                            tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the queue by resetting head, tail, and count to zero
 *
 * Logically removes all elements by resetting the indices and count.
 * The backing buffer is not zeroed for efficiency -- old values remain
 * in memory but are inaccessible through the public API since all
 * access functions check count before reading.
 *
 * @param q Queue (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if q is NULL
 */
int tiku_kits_ds_queue_clear(struct tiku_kits_ds_queue *q);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements in the queue
 *
 * Returns the logical size (number of valid, enqueued elements), not
 * the capacity.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param q Queue, or NULL
 * @return Number of elements currently stored, or 0 if q is NULL
 */
uint16_t tiku_kits_ds_queue_size(const struct tiku_kits_ds_queue *q);

/**
 * @brief Check whether the queue is full
 *
 * Returns 1 when count equals capacity, meaning no more elements can
 * be enqueued without first dequeuing.  Safe to call with a NULL
 * pointer -- returns 0 (not full).
 *
 * @param q Queue, or NULL
 * @return 1 if count == capacity, 0 otherwise or if q is NULL
 */
int tiku_kits_ds_queue_full(const struct tiku_kits_ds_queue *q);

/**
 * @brief Check whether the queue is empty
 *
 * Returns 1 when count is zero, meaning no elements are available for
 * dequeue.  A NULL pointer is treated as empty and returns 1.
 *
 * @param q Queue, or NULL
 * @return 1 if count == 0 or q is NULL, 0 otherwise
 */
int tiku_kits_ds_queue_empty(const struct tiku_kits_ds_queue *q);

#endif /* TIKU_KITS_DS_QUEUE_H_ */
