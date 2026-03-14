/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_array.c - Fixed-size contiguous array implementation
 *
 * Platform-independent implementation of a fixed-capacity array.
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

#include "tiku_kits_ds_array.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_init(struct tiku_kits_ds_array *arr,
                            uint16_t capacity)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_ARRAY_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    arr->size = 0;
    arr->capacity = capacity;
    memset(arr->data, 0, sizeof(arr->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_set(struct tiku_kits_ds_array *arr,
                           uint16_t index,
                           tiku_kits_ds_elem_t value)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= arr->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    arr->data[index] = value;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_get(const struct tiku_kits_ds_array *arr,
                           uint16_t index,
                           tiku_kits_ds_elem_t *value)
{
    if (arr == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= arr->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    *value = arr->data[index];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERTION / REMOVAL                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_push_back(struct tiku_kits_ds_array *arr,
                                 tiku_kits_ds_elem_t value)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (arr->size >= arr->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    arr->data[arr->size] = value;
    arr->size++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_pop_back(struct tiku_kits_ds_array *arr,
                                tiku_kits_ds_elem_t *value)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (arr->size == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    arr->size--;
    if (value != NULL) {
        *value = arr->data[arr->size];
    }
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_insert(struct tiku_kits_ds_array *arr,
                              uint16_t index,
                              tiku_kits_ds_elem_t value)
{
    uint16_t i;

    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index > arr->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }
    if (arr->size >= arr->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    /* Shift elements right to make room */
    for (i = arr->size; i > index; i--) {
        arr->data[i] = arr->data[i - 1];
    }

    arr->data[index] = value;
    arr->size++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_remove(struct tiku_kits_ds_array *arr,
                              uint16_t index)
{
    uint16_t i;

    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (index >= arr->size) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    /* Shift elements left to fill the gap */
    for (i = index; i < arr->size - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }

    arr->size--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_find(const struct tiku_kits_ds_array *arr,
                            tiku_kits_ds_elem_t value,
                            uint16_t *index)
{
    uint16_t i;

    if (arr == NULL || index == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    for (i = 0; i < arr->size; i++) {
        if (arr->data[i] == value) {
            *index = i;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_NOTFOUND;
}

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_fill(struct tiku_kits_ds_array *arr,
                            tiku_kits_ds_elem_t value)
{
    uint16_t i;

    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    for (i = 0; i < arr->capacity; i++) {
        arr->data[i] = value;
    }

    arr->size = arr->capacity;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_array_clear(struct tiku_kits_ds_array *arr)
{
    if (arr == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    arr->size = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_array_size(const struct tiku_kits_ds_array *arr)
{
    if (arr == NULL) {
        return 0;
    }
    return arr->size;
}
