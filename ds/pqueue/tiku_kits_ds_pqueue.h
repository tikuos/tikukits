/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_pqueue.h - Priority queue (array of per-level FIFO queues)
 *
 * Platform-independent priority queue for embedded systems. Internally
 * organized as an array of FIFO ring-buffer levels, where level 0 is
 * the highest priority. All storage is statically allocated.
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

#ifndef TIKU_KITS_DS_PQUEUE_H_
#define TIKU_KITS_DS_PQUEUE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of priority levels.
 *
 * This compile-time constant defines the upper bound on the number
 * of distinct priority tiers the queue can support.  Each priority
 * level carries its own FIFO ring buffer, so total static memory
 * usage scales as MAX_LEVELS * MAX_PER_LEVEL * sizeof(elem).
 * Choose a value that reflects the number of distinct urgency
 * classes your application requires.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_PQUEUE_MAX_LEVELS 8
 *   #include "tiku_kits_ds_pqueue.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_PQUEUE_MAX_LEVELS
#define TIKU_KITS_DS_PQUEUE_MAX_LEVELS 4
#endif

/**
 * @brief Maximum number of elements per priority level.
 *
 * This compile-time constant defines the capacity of each per-level
 * FIFO ring buffer.  All levels share the same maximum, but each
 * level's runtime capacity is set individually at init time and may
 * be smaller.  Increasing this value linearly increases the memory
 * footprint of every priority level.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL 16
 *   #include "tiku_kits_ds_pqueue.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL
#define TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_pqueue_level
 * @brief Single priority level -- a mini FIFO ring buffer
 *
 * Each level is an independent circular buffer that maintains FIFO
 * ordering within its priority tier.  The ring buffer uses separate
 * @c head and @c tail indices that wrap around modulo @c capacity,
 * avoiding the need to shift elements on enqueue/dequeue:
 *   - @c head -- position of the oldest (next-to-dequeue) element.
 *   - @c tail -- position of the next free slot for enqueue.
 *   - @c count -- distinguishes full from empty (both have
 *     head == tail without a count).
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 */
struct tiku_kits_ds_pqueue_level {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL];
    uint8_t head;       /**< Index of next element to dequeue */
    uint8_t tail;       /**< Index of next free slot for enqueue */
    uint8_t count;      /**< Number of elements at this level */
    uint8_t capacity;   /**< Runtime capacity set by init (<= MAX_PER_LEVEL) */
};

/**
 * @struct tiku_kits_ds_pqueue
 * @brief Priority queue with multiple FIFO levels
 *
 * A multi-tier priority queue built as an array of independent FIFO
 * ring buffers.  Level 0 is the highest priority; level n_levels-1
 * is the lowest.  Dequeue scans from level 0 upward and returns the
 * front element from the first non-empty level, giving strict
 * priority ordering with FIFO tie-breaking within each tier.
 *
 * Because all storage lives inside the struct itself, no heap
 * allocation is needed -- just declare the queue as a static or
 * local variable.  Total memory usage is dominated by the
 * MAX_LEVELS * MAX_PER_LEVEL element grid; unused levels are
 * zeroed at init time.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_pqueue pq;
 *   tiku_kits_ds_pqueue_init(&pq, 3, 4);       // 3 levels, 4 slots each
 *   tiku_kits_ds_pqueue_enqueue(&pq, 42, 0);   // highest priority
 *   tiku_kits_ds_pqueue_enqueue(&pq, 99, 2);   // lowest priority
 *   tiku_kits_ds_pqueue_enqueue(&pq, 7,  0);   // also highest priority
 *   // dequeue order: 42, 7 (FIFO within level 0), then 99
 * @endcode
 */
struct tiku_kits_ds_pqueue {
    struct tiku_kits_ds_pqueue_level levels[TIKU_KITS_DS_PQUEUE_MAX_LEVELS];
    uint8_t n_levels;   /**< Number of active priority levels (set by init) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a priority queue
 *
 * Resets all levels to empty state and zeroes the backing storage.
 * Each of the @p n_levels FIFO ring buffers is given a runtime
 * capacity of @p per_level.  Unused level slots (beyond n_levels)
 * are zeroed so the struct is in a clean state regardless of prior
 * contents.
 *
 * @param pq        Priority queue to initialize (must not be NULL)
 * @param n_levels  Number of priority levels
 *                  (1..TIKU_KITS_DS_PQUEUE_MAX_LEVELS)
 * @param per_level Capacity of each level
 *                  (1..TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if pq is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if n_levels or per_level is 0 or
 *         exceeds the corresponding compile-time maximum
 */
int tiku_kits_ds_pqueue_init(struct tiku_kits_ds_pqueue *pq,
                             uint8_t n_levels,
                             uint8_t per_level);

/*---------------------------------------------------------------------------*/
/* ENQUEUE / DEQUEUE                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Enqueue an element at a given priority level
 *
 * Appends @p value to the tail of the FIFO ring buffer at the
 * specified priority tier.  This is an O(1) operation -- the tail
 * index wraps around modulo the level's capacity without any
 * element shifting.  Fails if the target level's ring buffer is
 * already full.
 *
 * @param pq       Priority queue (must not be NULL)
 * @param value    Element to enqueue
 * @param priority Priority level (0 = highest, n_levels-1 = lowest)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if pq is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if priority >= n_levels,
 *         TIKU_KITS_DS_ERR_FULL if the target level is full
 */
int tiku_kits_ds_pqueue_enqueue(struct tiku_kits_ds_pqueue *pq,
                                tiku_kits_ds_elem_t value,
                                uint8_t priority);

/**
 * @brief Dequeue the highest-priority element
 *
 * Scans from level 0 (highest priority) to n_levels-1 (lowest) and
 * removes the front element from the first non-empty level.  The
 * scan is O(n_levels) in the worst case (all higher levels empty);
 * the actual removal from the chosen level is O(1) since it is a
 * ring-buffer head advance.  Within a single priority tier, elements
 * are returned in FIFO order.
 *
 * @param pq    Priority queue (must not be NULL)
 * @param value Output pointer where the dequeued element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if pq or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if all levels are empty
 */
int tiku_kits_ds_pqueue_dequeue(struct tiku_kits_ds_pqueue *pq,
                                tiku_kits_ds_elem_t *value);

/**
 * @brief Peek at the highest-priority element without removing it
 *
 * Performs the same level scan as dequeue but does not advance the
 * ring-buffer head, leaving the element in place.  Useful for
 * inspecting what would be dequeued next without committing to the
 * removal.  O(n_levels) worst case for the scan; no data is moved.
 *
 * @param pq    Priority queue (must not be NULL)
 * @param value Output pointer where the head element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if pq or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if all levels are empty
 */
int tiku_kits_ds_pqueue_peek(const struct tiku_kits_ds_pqueue *pq,
                             tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all levels of the priority queue
 *
 * Resets head, tail, and count of every active level to zero,
 * logically emptying all ring buffers.  The backing data arrays
 * are not zeroed for efficiency -- old values remain in memory but
 * are inaccessible through the public API since all access functions
 * check against count.
 *
 * @param pq Priority queue (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if pq is NULL
 */
int tiku_kits_ds_pqueue_clear(struct tiku_kits_ds_pqueue *pq);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the total number of elements across all levels
 *
 * Sums the count fields of all active levels.  O(n_levels).  Safe
 * to call with a NULL pointer -- returns 0.
 *
 * @param pq Priority queue, or NULL
 * @return Total number of elements across all levels, or 0 if pq
 *         is NULL
 */
uint16_t tiku_kits_ds_pqueue_size(const struct tiku_kits_ds_pqueue *pq);

/**
 * @brief Check whether all levels are empty
 *
 * Scans each active level's count.  Returns as soon as any non-empty
 * level is found (short-circuit), so the best case is O(1) when
 * level 0 has elements.  Worst case is O(n_levels).  A NULL pointer
 * is treated as empty (returns 1).
 *
 * @param pq Priority queue, or NULL
 * @return 1 if all levels are empty (or pq is NULL), 0 otherwise
 */
int tiku_kits_ds_pqueue_empty(const struct tiku_kits_ds_pqueue *pq);

#endif /* TIKU_KITS_DS_PQUEUE_H_ */
