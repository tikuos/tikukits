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
 * Maximum number of priority levels.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_PQUEUE_MAX_LEVELS
#define TIKU_KITS_DS_PQUEUE_MAX_LEVELS 4
#endif

/**
 * Maximum number of elements per priority level.
 * Override before including this header to change the limit.
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
 * Each level holds up to `capacity` elements in FIFO order.
 * The head/tail/count fields track the ring buffer state.
 */
struct tiku_kits_ds_pqueue_level {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL];
    uint8_t head;       /**< Index of next element to dequeue */
    uint8_t tail;       /**< Index of next free slot           */
    uint8_t count;      /**< Number of elements at this level  */
    uint8_t capacity;   /**< Runtime capacity (<= MAX_PER_LEVEL) */
};

/**
 * @struct tiku_kits_ds_pqueue
 * @brief Priority queue with multiple FIFO levels
 *
 * Level 0 is the highest priority. Dequeue always returns from the
 * highest-priority (lowest-index) non-empty level.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_pqueue pq;
 *   tiku_kits_ds_pqueue_init(&pq, 3, 4);
 *   tiku_kits_ds_pqueue_enqueue(&pq, 42, 0);  // highest priority
 *   tiku_kits_ds_pqueue_enqueue(&pq, 99, 2);  // lowest priority
 * @endcode
 */
struct tiku_kits_ds_pqueue {
    struct tiku_kits_ds_pqueue_level levels[TIKU_KITS_DS_PQUEUE_MAX_LEVELS];
    uint8_t n_levels;   /**< Number of active priority levels */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a priority queue
 * @param pq        Priority queue to initialize
 * @param n_levels  Number of priority levels
 *                  (1..TIKU_KITS_DS_PQUEUE_MAX_LEVELS)
 * @param per_level Capacity of each level
 *                  (1..TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL)
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_PARAM
 */
int tiku_kits_ds_pqueue_init(struct tiku_kits_ds_pqueue *pq,
                             uint8_t n_levels,
                             uint8_t per_level);

/*---------------------------------------------------------------------------*/
/* ENQUEUE / DEQUEUE                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Enqueue an element at a given priority level
 * @param pq       Priority queue
 * @param value    Element to enqueue
 * @param priority Priority level (0 = highest, n_levels-1 = lowest)
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL,
 *         TIKU_KITS_DS_ERR_PARAM (invalid priority), or
 *         TIKU_KITS_DS_ERR_FULL (that level is full)
 */
int tiku_kits_ds_pqueue_enqueue(struct tiku_kits_ds_pqueue *pq,
                                tiku_kits_ds_elem_t value,
                                uint8_t priority);

/**
 * @brief Dequeue the highest-priority element
 *
 * Scans from level 0 (highest) to n_levels-1 (lowest), returning
 * the first available element.
 *
 * @param pq    Priority queue
 * @param value Output: the dequeued element
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_EMPTY (all levels empty)
 */
int tiku_kits_ds_pqueue_dequeue(struct tiku_kits_ds_pqueue *pq,
                                tiku_kits_ds_elem_t *value);

/**
 * @brief Peek at the highest-priority element without removing it
 * @param pq    Priority queue
 * @param value Output: the head element
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_pqueue_peek(const struct tiku_kits_ds_pqueue *pq,
                             tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all levels of the priority queue
 * @param pq Priority queue to clear
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_pqueue_clear(struct tiku_kits_ds_pqueue *pq);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the total number of elements across all levels
 * @param pq Priority queue
 * @return Total count, or 0 if pq is NULL
 */
uint16_t tiku_kits_ds_pqueue_size(const struct tiku_kits_ds_pqueue *pq);

/**
 * @brief Check whether all levels are empty
 * @param pq Priority queue
 * @return 1 if all levels are empty, 0 otherwise (1 if NULL)
 */
int tiku_kits_ds_pqueue_empty(const struct tiku_kits_ds_pqueue *pq);

#endif /* TIKU_KITS_DS_PQUEUE_H_ */
