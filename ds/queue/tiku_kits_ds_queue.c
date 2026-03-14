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
    memset(q->data, 0, sizeof(q->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* ENQUEUE / DEQUEUE                                                         */
/*---------------------------------------------------------------------------*/

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
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

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

uint16_t tiku_kits_ds_queue_size(const struct tiku_kits_ds_queue *q)
{
    if (q == NULL) {
        return 0;
    }
    return q->count;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_queue_full(const struct tiku_kits_ds_queue *q)
{
    if (q == NULL) {
        return 0;
    }
    return (q->count == q->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_queue_empty(const struct tiku_kits_ds_queue *q)
{
    if (q == NULL) {
        return 1;
    }
    return (q->count == 0) ? 1 : 0;
}
