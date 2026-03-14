/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_stack.c - Fixed-size LIFO stack implementation
 *
 * Platform-independent implementation of a fixed-capacity LIFO stack.
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

#include "tiku_kits_ds_stack.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_stack_init(struct tiku_kits_ds_stack *stk,
                            uint16_t capacity)
{
    if (stk == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_STACK_MAX_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    stk->top = 0;
    stk->capacity = capacity;
    memset(stk->data, 0, sizeof(stk->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PUSH / POP / PEEK                                                         */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_stack_push(struct tiku_kits_ds_stack *stk,
                            tiku_kits_ds_elem_t value)
{
    if (stk == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (stk->top >= stk->capacity) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    stk->data[stk->top] = value;
    stk->top++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_stack_pop(struct tiku_kits_ds_stack *stk,
                           tiku_kits_ds_elem_t *value)
{
    if (stk == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (stk->top == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    stk->top--;
    *value = stk->data[stk->top];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_stack_peek(const struct tiku_kits_ds_stack *stk,
                            tiku_kits_ds_elem_t *value)
{
    if (stk == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (stk->top == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    *value = stk->data[stk->top - 1];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_stack_clear(struct tiku_kits_ds_stack *stk)
{
    if (stk == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    stk->top = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_stack_size(const struct tiku_kits_ds_stack *stk)
{
    if (stk == NULL) {
        return 0;
    }
    return stk->top;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_stack_full(const struct tiku_kits_ds_stack *stk)
{
    if (stk == NULL) {
        return 0;
    }
    return (stk->top >= stk->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_stack_empty(const struct tiku_kits_ds_stack *stk)
{
    if (stk == NULL) {
        return 0;
    }
    return (stk->top == 0) ? 1 : 0;
}
