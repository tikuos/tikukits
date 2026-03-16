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

/**
 * @brief Initialize a stack with the given capacity.
 *
 * Zeros the entire backing buffer so that any subsequent reads of
 * unpopulated slots return a deterministic value.  The runtime
 * capacity is clamped to TIKU_KITS_DS_STACK_MAX_SIZE at compile
 * time so that the static buffer is never overrun.
 */
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

    /* Zero the full static buffer, not just `capacity` slots, so
     * the struct is in a clean state regardless of prior contents. */
    memset(stk->data, 0, sizeof(stk->data));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* PUSH / POP / PEEK                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push an element onto the stack.
 *
 * O(1) -- simply writes into the next free slot (data[top]) and
 * bumps the top counter.  No element shifting required.
 */
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

/**
 * @brief Pop the top element from the stack.
 *
 * O(1) -- decrements top first, then copies the value out through
 * @p value.  The element's memory slot is not cleared; it becomes
 * inaccessible via the public API because all access functions
 * check against top.
 */
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

/**
 * @brief Read the top element without removing it.
 *
 * O(1) -- copies the value at data[top-1] out through @p value
 * without modifying the stack.  The caller owns the copy; the
 * stack contents remain untouched.
 */
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

/**
 * @brief Clear the stack by resetting top to zero.
 *
 * Logically removes all elements without zeroing the backing
 * buffer.  Old values remain in memory but are inaccessible
 * through the public API because all access functions bounds-check
 * against top.
 */
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

/**
 * @brief Return the current number of elements in the stack.
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the logical size (top), not the capacity.
 */
uint16_t tiku_kits_ds_stack_size(const struct tiku_kits_ds_stack *stk)
{
    if (stk == NULL) {
        return 0;
    }
    return stk->top;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check if the stack is full.
 *
 * Returns 1 when top has reached capacity.  Safe to call with a
 * NULL pointer -- returns 0.
 */
int tiku_kits_ds_stack_full(const struct tiku_kits_ds_stack *stk)
{
    if (stk == NULL) {
        return 0;
    }
    return (stk->top >= stk->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check if the stack is empty.
 *
 * Returns 1 when top is 0.  Safe to call with a NULL pointer --
 * returns 0.
 */
int tiku_kits_ds_stack_empty(const struct tiku_kits_ds_stack *stk)
{
    if (stk == NULL) {
        return 0;
    }
    return (stk->top == 0) ? 1 : 0;
}
