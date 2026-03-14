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

    for (i = 0; i < n_levels; i++) {
        pq->levels[i].head = 0;
        pq->levels[i].tail = 0;
        pq->levels[i].count = 0;
        pq->levels[i].capacity = per_level;
        memset(pq->levels[i].data, 0, sizeof(pq->levels[i].data));
    }

    /* Zero out unused levels */
    for (i = n_levels; i < TIKU_KITS_DS_PQUEUE_MAX_LEVELS; i++) {
        memset(&pq->levels[i], 0, sizeof(pq->levels[i]));
    }

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* ENQUEUE / DEQUEUE                                                         */
/*---------------------------------------------------------------------------*/

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
    lvl->tail = (lvl->tail + 1) % lvl->capacity;
    lvl->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_pqueue_dequeue(struct tiku_kits_ds_pqueue *pq,
                                tiku_kits_ds_elem_t *value)
{
    uint8_t i;
    struct tiku_kits_ds_pqueue_level *lvl;

    if (pq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Scan from highest priority (level 0) to lowest */
    for (i = 0; i < pq->n_levels; i++) {
        lvl = &pq->levels[i];
        if (lvl->count > 0) {
            *value = lvl->data[lvl->head];
            lvl->head = (lvl->head + 1) % lvl->capacity;
            lvl->count--;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_EMPTY;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_pqueue_peek(const struct tiku_kits_ds_pqueue *pq,
                             tiku_kits_ds_elem_t *value)
{
    uint8_t i;
    const struct tiku_kits_ds_pqueue_level *lvl;

    if (pq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Scan from highest priority (level 0) to lowest */
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

int tiku_kits_ds_pqueue_clear(struct tiku_kits_ds_pqueue *pq)
{
    uint8_t i;

    if (pq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

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
