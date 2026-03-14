/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_stack.c - Stack data structure tests
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

#include "tests/tiku_test_harness.h"
#include "tikukits/ds/stack/tiku_kits_ds_stack.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_init(void)
{
    struct tiku_kits_ds_stack stk;
    int rc;

    TEST_GROUP_BEGIN("Stack Init");

    rc = tiku_kits_ds_stack_init(&stk, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init cap=8 returns OK");
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 0,
                "size is 0 after init");
    TEST_ASSERT(tiku_kits_ds_stack_empty(&stk) == 1,
                "stack is empty after init");
    TEST_ASSERT(tiku_kits_ds_stack_full(&stk) == 0,
                "stack is not full after init");

    /* Max capacity accepted */
    rc = tiku_kits_ds_stack_init(&stk, TIKU_KITS_DS_STACK_MAX_SIZE);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init max capacity OK");

    /* Zero capacity rejected */
    rc = tiku_kits_ds_stack_init(&stk, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "zero capacity rejected");

    /* Oversized capacity rejected */
    rc = tiku_kits_ds_stack_init(&stk,
                                 TIKU_KITS_DS_STACK_MAX_SIZE + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized capacity rejected");

    TEST_GROUP_END("Stack Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: PUSH AND POP                                                      */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_push_pop(void)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Stack Push/Pop");

    tiku_kits_ds_stack_init(&stk, 4);

    /* Push elements */
    rc = tiku_kits_ds_stack_push(&stk, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 10 OK");
    rc = tiku_kits_ds_stack_push(&stk, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 20 OK");
    rc = tiku_kits_ds_stack_push(&stk, 30);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 30 OK");
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 3,
                "size is 3 after 3 pushes");

    /* Pop elements (LIFO order) */
    rc = tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop OK");
    TEST_ASSERT(val == 30, "popped value is 30");

    rc = tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "second pop OK");
    TEST_ASSERT(val == 20, "popped value is 20");

    rc = tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "third pop OK");
    TEST_ASSERT(val == 10, "popped value is 10");

    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 0,
                "size is 0 after all pops");

    TEST_GROUP_END("Stack Push/Pop");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: PEEK                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_peek(void)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Stack Peek");

    tiku_kits_ds_stack_init(&stk, 4);

    /* Peek on empty stack */
    rc = tiku_kits_ds_stack_peek(&stk, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "peek empty returns ERR_EMPTY");

    /* Push and peek */
    tiku_kits_ds_stack_push(&stk, 42);
    rc = tiku_kits_ds_stack_peek(&stk, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "peek returns OK");
    TEST_ASSERT(val == 42, "peek value is 42");

    /* Peek does not remove element */
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 1,
                "size unchanged after peek");

    /* Push another, peek sees new top */
    tiku_kits_ds_stack_push(&stk, 99);
    tiku_kits_ds_stack_peek(&stk, &val);
    TEST_ASSERT(val == 99, "peek sees new top 99");

    TEST_GROUP_END("Stack Peek");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: OVERFLOW (PUSH WHEN FULL)                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_overflow(void)
{
    struct tiku_kits_ds_stack stk;
    int rc;
    uint16_t i;

    TEST_GROUP_BEGIN("Stack Overflow");

    tiku_kits_ds_stack_init(&stk, 4);

    /* Fill to capacity */
    for (i = 0; i < 4; i++) {
        rc = tiku_kits_ds_stack_push(&stk, (tiku_kits_ds_elem_t)i);
        TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push within cap OK");
    }

    TEST_ASSERT(tiku_kits_ds_stack_full(&stk) == 1,
                "stack is full at capacity");

    /* Push beyond capacity */
    rc = tiku_kits_ds_stack_push(&stk, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "push when full returns ERR_FULL");

    /* Size unchanged */
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 4,
                "size unchanged after failed push");

    TEST_GROUP_END("Stack Overflow");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: UNDERFLOW (POP WHEN EMPTY)                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_underflow(void)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Stack Underflow");

    tiku_kits_ds_stack_init(&stk, 4);

    /* Pop from empty */
    rc = tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "pop empty returns ERR_EMPTY");

    /* Push one, pop two */
    tiku_kits_ds_stack_push(&stk, 10);
    tiku_kits_ds_stack_pop(&stk, &val);
    rc = tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "second pop returns ERR_EMPTY");

    TEST_ASSERT(tiku_kits_ds_stack_empty(&stk) == 1,
                "stack is empty after underflow");

    TEST_GROUP_END("Stack Underflow");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: SIZE TRACKING                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_size_tracking(void)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t val;

    TEST_GROUP_BEGIN("Stack Size Tracking");

    tiku_kits_ds_stack_init(&stk, 8);

    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 0,
                "initial size is 0");

    tiku_kits_ds_stack_push(&stk, 1);
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 1,
                "size is 1 after push");

    tiku_kits_ds_stack_push(&stk, 2);
    tiku_kits_ds_stack_push(&stk, 3);
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 3,
                "size is 3 after 3 pushes");

    tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 2,
                "size is 2 after pop");

    tiku_kits_ds_stack_pop(&stk, &val);
    tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 0,
                "size is 0 after all pops");

    TEST_GROUP_END("Stack Size Tracking");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: LIFO ORDER                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_lifo_order(void)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t val;
    int correct;
    uint16_t i;

    TEST_GROUP_BEGIN("Stack LIFO Order");

    tiku_kits_ds_stack_init(&stk, 8);

    /* Push 1..5 */
    for (i = 1; i <= 5; i++) {
        tiku_kits_ds_stack_push(&stk, (tiku_kits_ds_elem_t)i);
    }

    /* Pop must return 5..1 */
    correct = 1;
    for (i = 5; i >= 1; i--) {
        tiku_kits_ds_stack_pop(&stk, &val);
        if (val != (tiku_kits_ds_elem_t)i) {
            correct = 0;
        }
    }
    TEST_ASSERT(correct == 1, "LIFO order 5,4,3,2,1 correct");

    /* Interleaved push/pop */
    tiku_kits_ds_stack_push(&stk, 100);
    tiku_kits_ds_stack_push(&stk, 200);
    tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(val == 200, "interleaved pop returns 200");

    tiku_kits_ds_stack_push(&stk, 300);
    tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(val == 300, "interleaved pop returns 300");

    tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(val == 100, "final pop returns 100");

    TEST_GROUP_END("Stack LIFO Order");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: CLEAR AND RESET                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_clear_reset(void)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Stack Clear/Reset");

    tiku_kits_ds_stack_init(&stk, 8);
    tiku_kits_ds_stack_push(&stk, 10);
    tiku_kits_ds_stack_push(&stk, 20);
    tiku_kits_ds_stack_push(&stk, 30);

    rc = tiku_kits_ds_stack_clear(&stk);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear returns OK");
    TEST_ASSERT(tiku_kits_ds_stack_size(&stk) == 0,
                "size is 0 after clear");
    TEST_ASSERT(tiku_kits_ds_stack_empty(&stk) == 1,
                "stack is empty after clear");

    /* Pop from cleared stack fails */
    rc = tiku_kits_ds_stack_pop(&stk, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "pop after clear returns ERR_EMPTY");

    /* Can push after clear */
    rc = tiku_kits_ds_stack_push(&stk, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push after clear OK");
    tiku_kits_ds_stack_peek(&stk, &val);
    TEST_ASSERT(val == 99, "pushed value is 99");

    TEST_GROUP_END("Stack Clear/Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER INPUTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_null_inputs(void)
{
    struct tiku_kits_ds_stack stk;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Stack NULL Inputs");

    /* init with NULL */
    rc = tiku_kits_ds_stack_init(NULL, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    /* push with NULL */
    rc = tiku_kits_ds_stack_push(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "push NULL rejected");

    /* pop with NULL stack */
    rc = tiku_kits_ds_stack_pop(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "pop NULL stack rejected");

    /* pop with NULL value */
    tiku_kits_ds_stack_init(&stk, 4);
    tiku_kits_ds_stack_push(&stk, 10);
    rc = tiku_kits_ds_stack_pop(&stk, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "pop NULL value rejected");

    /* peek with NULL stack */
    rc = tiku_kits_ds_stack_peek(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "peek NULL stack rejected");

    /* peek with NULL value */
    rc = tiku_kits_ds_stack_peek(&stk, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "peek NULL value rejected");

    /* clear with NULL */
    rc = tiku_kits_ds_stack_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear NULL rejected");

    /* size with NULL returns 0 */
    TEST_ASSERT(tiku_kits_ds_stack_size(NULL) == 0,
                "size NULL returns 0");

    /* full with NULL returns 0 */
    TEST_ASSERT(tiku_kits_ds_stack_full(NULL) == 0,
                "full NULL returns 0");

    /* empty with NULL returns 0 */
    TEST_ASSERT(tiku_kits_ds_stack_empty(NULL) == 0,
                "empty NULL returns 0");

    TEST_GROUP_END("Stack NULL Inputs");
}
