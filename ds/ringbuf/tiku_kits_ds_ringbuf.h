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
 * Maximum number of elements the ring buffer can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_RINGBUF_MAX_SIZE
#define TIKU_KITS_DS_RINGBUF_MAX_SIZE 32
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_ringbuf
 * @brief Fixed-capacity circular buffer with static storage
 *
 * Head is the read position, tail is the write position. Push writes
 * at data[tail] and advances tail via modulo arithmetic. Pop reads
 * from data[head] and advances head. A separate count field tracks
 * the number of valid elements to distinguish full from empty.
 *
 * Declare ring buffers as static or local variables -- no heap required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_ringbuf rb;
 *   tiku_kits_ds_ringbuf_init(&rb, 16);
 *   tiku_kits_ds_ringbuf_push(&rb, 42);
 * @endcode
 */
struct tiku_kits_ds_ringbuf {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_RINGBUF_MAX_SIZE];
    uint16_t head;      /**< Read position */
    uint16_t tail;      /**< Write position */
    uint16_t count;     /**< Number of valid elements */
    uint16_t capacity;  /**< User-requested capacity */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a ring buffer with the given capacity
 * @param rb       Ring buffer to initialize
 * @param capacity Maximum number of elements (1..RINGBUF_MAX_SIZE)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_ringbuf_init(struct tiku_kits_ds_ringbuf *rb,
                              uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* PUSH / POP / PEEK                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push an element into the ring buffer
 * @param rb    Ring buffer
 * @param value Value to push
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_ringbuf_push(struct tiku_kits_ds_ringbuf *rb,
                              tiku_kits_ds_elem_t value);

/**
 * @brief Pop the oldest element from the ring buffer
 * @param rb    Ring buffer
 * @param value Output: popped element
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_ringbuf_pop(struct tiku_kits_ds_ringbuf *rb,
                             tiku_kits_ds_elem_t *value);

/**
 * @brief Read the oldest element without removing it
 * @param rb    Ring buffer
 * @param value Output: head element
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_ringbuf_peek(const struct tiku_kits_ds_ringbuf *rb,
                              tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the ring buffer (reset head, tail, count)
 * @param rb Ring buffer
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_ringbuf_clear(struct tiku_kits_ds_ringbuf *rb);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of elements
 * @param rb Ring buffer
 * @return Current count, or 0 if rb is NULL
 */
uint16_t tiku_kits_ds_ringbuf_count(
    const struct tiku_kits_ds_ringbuf *rb);

/**
 * @brief Check if the ring buffer is full
 * @param rb Ring buffer
 * @return 1 if full, 0 otherwise (or if NULL)
 */
int tiku_kits_ds_ringbuf_full(const struct tiku_kits_ds_ringbuf *rb);

/**
 * @brief Check if the ring buffer is empty
 * @param rb Ring buffer
 * @return 1 if empty, 0 otherwise (or if NULL)
 */
int tiku_kits_ds_ringbuf_empty(
    const struct tiku_kits_ds_ringbuf *rb);

#endif /* TIKU_KITS_DS_RINGBUF_H_ */
