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
 * @brief Maximum number of elements the stack can hold.
 *
 * This compile-time constant defines the upper bound on stack
 * capacity.  Each stack instance reserves this many element slots
 * in its static storage, so choose a value that balances memory
 * usage against the deepest stack your application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_STACK_MAX_SIZE 64
 *   #include "tiku_kits_ds_stack.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_STACK_MAX_SIZE
#define TIKU_KITS_DS_STACK_MAX_SIZE 16
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_stack
 * @brief Fixed-capacity LIFO stack with contiguous static storage.
 *
 * A last-in-first-out container that stores elements in a
 * contiguous block of statically allocated memory.  Because all
 * storage lives inside the struct itself, no heap allocation is
 * needed -- just declare the stack as a static or local variable.
 *
 * Two sizes are tracked independently:
 *   - @c capacity -- the runtime limit passed to init (must be
 *     <= TIKU_KITS_DS_STACK_MAX_SIZE).  This lets different stack
 *     instances use different logical sizes while sharing the same
 *     compile-time backing buffer.
 *   - @c top -- the index of the next free slot, which also equals
 *     the number of elements currently stored.  Push writes at
 *     data[top] and increments top; pop decrements top and reads
 *     from data[top].
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_stack stk;
 *   tiku_kits_ds_stack_init(&stk, 8);   // use 8 of 16 slots
 *   tiku_kits_ds_stack_push(&stk, 42);
 *   tiku_kits_ds_stack_push(&stk, 7);
 *   // stk now contains: [42, 7], top == 2
 *   tiku_kits_ds_elem_t v;
 *   tiku_kits_ds_stack_pop(&stk, &v);   // v == 7, top == 1
 * @endcode
 */
struct tiku_kits_ds_stack {
    tiku_kits_ds_elem_t data[TIKU_KITS_DS_STACK_MAX_SIZE];
    uint16_t top;       /**< Next write position (== current size) */
    uint16_t capacity;  /**< Runtime capacity set by init (<= MAX_SIZE) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a stack with the given capacity.
 *
 * Resets the stack to an empty state and sets the runtime capacity
 * limit.  The backing buffer is zeroed so that uninitialised reads
 * (before any push) return a deterministic value.
 *
 * @param stk      Stack to initialize (must not be NULL)
 * @param capacity Maximum number of elements (1..STACK_MAX_SIZE)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if stk is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds
 *         MAX_SIZE
 */
int tiku_kits_ds_stack_init(struct tiku_kits_ds_stack *stk,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* PUSH / POP / PEEK                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push an element onto the stack.
 *
 * Stores @p value at position @c top and increments top by one.
 * This is an O(1) operation since no shifting is required.  Fails
 * if the stack has already reached its capacity.
 *
 * @param stk   Stack (must not be NULL)
 * @param value Value to push
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if stk is NULL,
 *         TIKU_KITS_DS_ERR_FULL if top == capacity
 */
int tiku_kits_ds_stack_push(struct tiku_kits_ds_stack *stk,
                            tiku_kits_ds_elem_t value);

/**
 * @brief Pop the top element from the stack.
 *
 * Decrements top by one and copies the removed element into the
 * caller-provided location pointed to by @p value.  This is an
 * O(1) operation.  The element's memory slot is not cleared; it
 * becomes inaccessible through the public API since all access
 * functions check against top.  Fails if the stack is empty.
 *
 * @param stk   Stack (must not be NULL)
 * @param value Output pointer where the popped element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if stk or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if top == 0
 */
int tiku_kits_ds_stack_pop(struct tiku_kits_ds_stack *stk,
                           tiku_kits_ds_elem_t *value);

/**
 * @brief Read the top element without removing it.
 *
 * Copies the element at position top-1 into the caller-provided
 * location pointed to by @p value.  The stack is not modified.
 * This is an O(1) operation.  Fails if the stack is empty.
 *
 * @param stk   Stack (must not be NULL)
 * @param value Output pointer where the top element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if stk or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if top == 0
 */
int tiku_kits_ds_stack_peek(const struct tiku_kits_ds_stack *stk,
                            tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the stack by resetting top to zero.
 *
 * Logically removes all elements by setting top to 0.  The backing
 * buffer is not zeroed for efficiency -- old values remain in
 * memory but are inaccessible through the public API since all
 * access functions bounds-check against top.
 *
 * @param stk Stack (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if stk is NULL
 */
int tiku_kits_ds_stack_clear(struct tiku_kits_ds_stack *stk);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of elements in the stack.
 *
 * Returns the logical size (number of valid, populated elements),
 * not the capacity.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param stk Stack, or NULL
 * @return Number of elements currently stored, or 0 if stk is NULL
 */
uint16_t tiku_kits_ds_stack_size(const struct tiku_kits_ds_stack *stk);

/**
 * @brief Check if the stack is full.
 *
 * Returns 1 when top has reached capacity, meaning no more
 * elements can be pushed.  Safe to call with a NULL pointer --
 * returns 0.
 *
 * @param stk Stack, or NULL
 * @return 1 if the stack is full, 0 otherwise (or if stk is NULL)
 */
int tiku_kits_ds_stack_full(const struct tiku_kits_ds_stack *stk);

/**
 * @brief Check if the stack is empty.
 *
 * Returns 1 when top is 0, meaning no elements are stored.
 * Safe to call with a NULL pointer -- returns 0.
 *
 * @param stk Stack, or NULL
 * @return 1 if the stack is empty, 0 otherwise (or if stk is NULL)
 */
int tiku_kits_ds_stack_empty(const struct tiku_kits_ds_stack *stk);

#endif /* TIKU_KITS_DS_STACK_H_ */
