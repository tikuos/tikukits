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
 * Maximum number of elements the queue can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_QUEUE_MAX_SIZE
#define TIKU_KITS_DS_QUEUE_MAX_SIZE 16
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_queue
 * @brief Fixed-capacity FIFO queue with static storage
 *
 * Implemented as a circular buffer. Elements are added at the tail
 * and removed from the head. The capacity field stores the runtime
 * capacity (which must be <= TIKU_KITS_DS_QUEUE_MAX_SIZE).
 *
 * Declare queues as static or local variables -- no heap required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_queue q;
 *   tiku_kits_ds_queue_init(&q, 8);
 *   tiku_kits_ds_queue_enqueue(&q, 42);
 * @endcode
 */
struct tiku_kits_ds_queue {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_QUEUE_MAX_SIZE];
    uint16_t head;      /**< Index of next element to dequeue */
    uint16_t tail;      /**< Index of next free slot           */
    uint16_t count;     /**< Number of elements in queue       */
    uint16_t capacity;  /**< Runtime capacity (<= MAX_SIZE)    */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a queue with the given capacity
 * @param q        Queue to initialize
 * @param capacity Maximum number of elements (1..TIKU_KITS_DS_QUEUE_MAX_SIZE)
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_PARAM
 */
int tiku_kits_ds_queue_init(struct tiku_kits_ds_queue *q,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* ENQUEUE / DEQUEUE                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add an element to the tail of the queue
 * @param q     Queue
 * @param value Element to enqueue
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_queue_enqueue(struct tiku_kits_ds_queue *q,
                               tiku_kits_ds_elem_t value);

/**
 * @brief Remove an element from the head of the queue
 * @param q     Queue
 * @param value Output: the dequeued element
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_queue_dequeue(struct tiku_kits_ds_queue *q,
                               tiku_kits_ds_elem_t *value);

/**
 * @brief Read the head element without removing it
 * @param q     Queue
 * @param value Output: the head element
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_queue_peek(const struct tiku_kits_ds_queue *q,
                            tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the queue to empty (head, tail, count all set to 0)
 * @param q Queue to clear
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_queue_clear(struct tiku_kits_ds_queue *q);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of elements currently in the queue
 * @param q Queue
 * @return Element count, or 0 if q is NULL
 */
uint16_t tiku_kits_ds_queue_size(const struct tiku_kits_ds_queue *q);

/**
 * @brief Check whether the queue is full
 * @param q Queue
 * @return 1 if count == capacity, 0 otherwise (0 if NULL)
 */
int tiku_kits_ds_queue_full(const struct tiku_kits_ds_queue *q);

/**
 * @brief Check whether the queue is empty
 * @param q Queue
 * @return 1 if count == 0, 0 otherwise (1 if NULL)
 */
int tiku_kits_ds_queue_empty(const struct tiku_kits_ds_queue *q);

#endif /* TIKU_KITS_DS_QUEUE_H_ */
