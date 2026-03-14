/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_stack.h - Fixed-size LIFO stack
 *
 * Platform-independent stack library for embedded systems. All storage
 * is statically allocated using a compile-time maximum capacity. The
 * element type is configured via tiku_kits_ds_elem_t.
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

#ifndef TIKU_KITS_DS_STACK_H_
#define TIKU_KITS_DS_STACK_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of elements the stack can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_STACK_MAX_SIZE
#define TIKU_KITS_DS_STACK_MAX_SIZE 16
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_stack
 * @brief Fixed-capacity LIFO stack with static storage
 *
 * The top field indicates the next write position (0 when empty).
 * Push writes at data[top] and increments top; pop decrements top
 * and reads from data[top].
 *
 * Declare stacks as static or local variables -- no heap required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_stack stk;
 *   tiku_kits_ds_stack_init(&stk, 8);
 *   tiku_kits_ds_stack_push(&stk, 42);
 * @endcode
 */
struct tiku_kits_ds_stack {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_STACK_MAX_SIZE];
    uint16_t top;       /**< Next write position (== size) */
    uint16_t capacity;  /**< User-requested capacity */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a stack with the given capacity
 * @param stk      Stack to initialize
 * @param capacity Maximum number of elements (1..STACK_MAX_SIZE)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_stack_init(struct tiku_kits_ds_stack *stk,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* PUSH / POP / PEEK                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push an element onto the stack
 * @param stk   Stack
 * @param value Value to push
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_stack_push(struct tiku_kits_ds_stack *stk,
                            tiku_kits_ds_elem_t value);

/**
 * @brief Pop the top element from the stack
 * @param stk   Stack
 * @param value Output: popped element
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_stack_pop(struct tiku_kits_ds_stack *stk,
                           tiku_kits_ds_elem_t *value);

/**
 * @brief Read the top element without removing it
 * @param stk   Stack
 * @param value Output: top element
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_stack_peek(const struct tiku_kits_ds_stack *stk,
                            tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the stack (set top to 0)
 * @param stk Stack
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_stack_clear(struct tiku_kits_ds_stack *stk);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of elements
 * @param stk Stack
 * @return Current size, or 0 if stk is NULL
 */
uint16_t tiku_kits_ds_stack_size(const struct tiku_kits_ds_stack *stk);

/**
 * @brief Check if the stack is full
 * @param stk Stack
 * @return 1 if full, 0 otherwise (or if NULL)
 */
int tiku_kits_ds_stack_full(const struct tiku_kits_ds_stack *stk);

/**
 * @brief Check if the stack is empty
 * @param stk Stack
 * @return 1 if empty, 0 otherwise (or if NULL)
 */
int tiku_kits_ds_stack_empty(const struct tiku_kits_ds_stack *stk);

#endif /* TIKU_KITS_DS_STACK_H_ */
