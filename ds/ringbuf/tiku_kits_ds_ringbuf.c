/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_ringbuf.c - Fixed-size circular buffer implementation
 *
 * Platform-independent implementation of a fixed-capacity ring buffer.
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

#include "tiku_kits_ds_ringbuf.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_ringbuf_init(struct tiku_kits_ds_ringbuf *rb,
                              uint16_t capacity)
{
    if (rb == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_RINGBUF_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->capacity = capacity;
    memset(rb->data, 0, sizeof(rb->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PUSH / POP / PEEK                                                         */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_ringbuf_push(struct tiku_kits_ds_ringbuf *rb,
                              tiku_kits_ds_elem_t value)
{
    if (rb == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (rb->count >= rb->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    rb->data[rb->tail] = value;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_ringbuf_pop(struct tiku_kits_ds_ringbuf *rb,
                             tiku_kits_ds_elem_t *value)
{
    if (rb == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (rb->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = rb->data[rb->head];
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_ringbuf_peek(const struct tiku_kits_ds_ringbuf *rb,
                              tiku_kits_ds_elem_t *value)
{
    if (rb == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (rb->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = rb->data[rb->head];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_ringbuf_clear(struct tiku_kits_ds_ringbuf *rb)
{
    if (rb == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_ringbuf_count(
    const struct tiku_kits_ds_ringbuf *rb)
{
    if (rb == NULL) {
        return 0;
    }
    return rb->count;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_ringbuf_full(const struct tiku_kits_ds_ringbuf *rb)
{
    if (rb == NULL) {
        return 0;
    }
    return (rb->count >= rb->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_ringbuf_empty(
    const struct tiku_kits_ds_ringbuf *rb)
{
    if (rb == NULL) {
        return 0;
    }
    return (rb->count == 0) ? 1 : 0;
}
