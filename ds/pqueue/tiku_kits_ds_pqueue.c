/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_pqueue.c - Priority queue (array of per-level FIFO queues)
 *
 * Platform-independent implementation of a multi-level priority queue.
 * Each level is a mini ring buffer. Dequeue scans from level 0
 * (highest priority) to n_levels-1 (lowest). No heap usage.
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

#include "tiku_kits_ds_pqueue.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a priority queue
 *
 * Resets all ring-buffer state (head, tail, count) for each active
 * level and zeroes the backing data arrays.  Unused level slots
 * beyond @p n_levels are also zeroed so the struct is in a clean
 * state regardless of prior contents.
 */
int tiku_kits_ds_pqueue_init(struct tiku_kits_ds_pqueue *pq,
                             uint8_t n_levels,
                             uint8_t per_level)
{
    uint8_t i;

    if (pq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (n_levels == 0 || n_levels > TIKU_KITS_DS_PQUEUE_MAX_LEVELS) {
        return TIKU_KITS_DS_ERR_PARAM;
    }
    if (per_level == 0 || per_level > TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    pq->n_levels = n_levels;

    /* Initialize each active level's ring-buffer metadata and zero
     * the data array for deterministic uninitialised reads. */
    for (i = 0; i < n_levels; i++) {
        pq->levels[i].head = 0;
        pq->levels[i].tail = 0;
        pq->levels[i].count = 0;
        pq->levels[i].capacity = per_level;
        memset(pq->levels[i].data, 0, sizeof(pq->levels[i].data));
    }

    /* Zero out unused levels so the struct is clean regardless of
     * prior contents -- prevents stale data from leaking. */
    for (i = n_levels; i < TIKU_KITS_DS_PQUEUE_MAX_LEVELS; i++) {
        memset(&pq->levels[i], 0, sizeof(pq->levels[i]));
    }

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* ENQUEUE / DEQUEUE                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Enqueue an element at a given priority level
 *
 * O(1) -- writes the value at the tail position of the target
 * level's ring buffer and advances the tail index with wrap-around.
 * No element shifting is required because the ring buffer uses
 * modular arithmetic.
 */
int tiku_kits_ds_pqueue_enqueue(struct tiku_kits_ds_pqueue *pq,
                                tiku_kits_ds_elem_t value,
                                uint8_t priority)
{
    struct tiku_kits_ds_pqueue_level *lvl;

    if (pq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (priority >= pq->n_levels) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    lvl = &pq->levels[priority];

    if (lvl->count >= lvl->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    lvl->data[lvl->tail] = value;
    /* Advance tail with wrap-around so the ring buffer cycles
     * through the static array without shifting elements. */
    lvl->tail = (lvl->tail + 1) % lvl->capacity;
    lvl->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Dequeue the highest-priority element
 *
 * Scans from level 0 (highest priority) to n_levels-1 (lowest) and
 * removes the front element from the first non-empty level.  The
 * scan ensures strict priority ordering; within a single tier,
 * elements come out in FIFO order.  The removal itself is O(1)
 * (ring-buffer head advance).
 */
int tiku_kits_ds_pqueue_dequeue(struct tiku_kits_ds_pqueue *pq,
                                tiku_kits_ds_elem_t *value)
{
    uint8_t i;
    struct tiku_kits_ds_pqueue_level *lvl;

    if (pq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Scan from highest priority (level 0) to lowest -- the first
     * non-empty level wins, enforcing strict priority ordering. */
    for (i = 0; i < pq->n_levels; i++) {
        lvl = &pq->levels[i];
        if (lvl->count > 0) {
            *value = lvl->data[lvl->head];
            /* Advance head with wrap-around to consume the element */
            lvl->head = (lvl->head + 1) % lvl->capacity;
            lvl->count--;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_EMPTY;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Peek at the highest-priority element without removing it
 *
 * Same level scan as dequeue, but the head index is not advanced
 * so the element remains in place.  Useful for inspecting the next
 * element without committing to a removal.
 */
int tiku_kits_ds_pqueue_peek(const struct tiku_kits_ds_pqueue *pq,
                             tiku_kits_ds_elem_t *value)
{
    uint8_t i;
    const struct tiku_kits_ds_pqueue_level *lvl;

    if (pq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Scan from highest priority (level 0) to lowest -- same as
     * dequeue but without advancing the head pointer. */
    for (i = 0; i < pq->n_levels; i++) {
        lvl = &pq->levels[i];
        if (lvl->count > 0) {
            *value = lvl->data[lvl->head];
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_EMPTY;
}

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all levels of the priority queue
 *
 * Resets head, tail, and count for each active level.  The backing
 * data arrays are not zeroed for efficiency -- old values remain
 * in memory but are inaccessible through the public API since all
 * access functions check against count.
 */
int tiku_kits_ds_pqueue_clear(struct tiku_kits_ds_pqueue *pq)
{
    uint8_t i;

    if (pq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Reset ring-buffer state without zeroing data -- the count
     * check in enqueue/dequeue/peek makes stale data unreachable. */
    for (i = 0; i < pq->n_levels; i++) {
        pq->levels[i].head = 0;
        pq->levels[i].tail = 0;
        pq->levels[i].count = 0;
    }

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the total number of elements across all levels
 *
 * Sums the count fields of all active levels.  O(n_levels).  Safe
 * to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint16_t tiku_kits_ds_pqueue_size(const struct tiku_kits_ds_pqueue *pq)
{
    uint8_t i;
    uint16_t total;

    if (pq == NULL) {
        return 0;
    }

    total = 0;
    for (i = 0; i < pq->n_levels; i++) {
        total += pq->levels[i].count;
    }

    return total;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether all levels are empty
 *
 * Short-circuits as soon as any level with a non-zero count is
 * found, so the best case is O(1).  A NULL pointer is treated as
 * empty (returns 1) to be safe for defensive callers.
 */
int tiku_kits_ds_pqueue_empty(const struct tiku_kits_ds_pqueue *pq)
{
    uint8_t i;

    if (pq == NULL) {
        return 1;
    }

    for (i = 0; i < pq->n_levels; i++) {
        if (pq->levels[i].count > 0) {
            return 0;
        }
    }

    return 1;
}
