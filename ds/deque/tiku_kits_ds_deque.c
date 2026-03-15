/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_deque.c - Double-ended queue (circular-array based)
 *
 * Platform-independent implementation of a circular-buffer deque.
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

#include "tiku_kits_ds_deque.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_init(struct tiku_kits_ds_deque *dq,
                            uint16_t capacity)
{
    if (dq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_DEQUE_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    dq->head = 0;
    dq->count = 0;
    dq->capacity = capacity;
    memset(dq->data, 0, sizeof(dq->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PUSH / POP                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_push_front(struct tiku_kits_ds_deque *dq,
                                  tiku_kits_ds_elem_t value)
{
    if (dq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count >= dq->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    dq->head = (dq->head - 1 + dq->capacity) % dq->capacity;
    dq->data[dq->head] = value;
    dq->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_push_back(struct tiku_kits_ds_deque *dq,
                                 tiku_kits_ds_elem_t value)
{
    uint16_t tail;

    if (dq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count >= dq->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    tail = (dq->head + dq->count) % dq->capacity;
    dq->data[tail] = value;
    dq->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_pop_front(struct tiku_kits_ds_deque *dq,
                                 tiku_kits_ds_elem_t *value)
{
    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = dq->data[dq->head];
    dq->head = (dq->head + 1) % dq->capacity;
    dq->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_pop_back(struct tiku_kits_ds_deque *dq,
                                tiku_kits_ds_elem_t *value)
{
    uint16_t back;

    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    back = (dq->head + dq->count - 1) % dq->capacity;
    *value = dq->data[back];
    dq->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PEEK                                                                      */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_peek_front(
    const struct tiku_kits_ds_deque *dq,
    tiku_kits_ds_elem_t *value)
{
    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = dq->data[dq->head];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_peek_back(
    const struct tiku_kits_ds_deque *dq,
    tiku_kits_ds_elem_t *value)
{
    uint16_t back;

    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (dq->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    back = (dq->head + dq->count - 1) % dq->capacity;
    *value = dq->data[back];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* RANDOM ACCESS                                                             */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_get(
    const struct tiku_kits_ds_deque *dq,
    uint16_t index,
    tiku_kits_ds_elem_t *value)
{
    uint16_t phys;

    if (dq == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= dq->count) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    phys = (dq->head + index) % dq->capacity;
    *value = dq->data[phys];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_clear(struct tiku_kits_ds_deque *dq)
{
    if (dq == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    dq->head = 0;
    dq->count = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_deque_count(
    const struct tiku_kits_ds_deque *dq)
{
    if (dq == NULL) {
        return 0;
    }
    return dq->count;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_full(
    const struct tiku_kits_ds_deque *dq)
{
    if (dq == NULL) {
        return 0;
    }
    return (dq->count == dq->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_deque_empty(
    const struct tiku_kits_ds_deque *dq)
{
    if (dq == NULL) {
        return 0;
    }
    return (dq->count == 0) ? 1 : 0;
}
