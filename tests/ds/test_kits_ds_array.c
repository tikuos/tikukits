/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_array.c - Array data structure tests
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
#include "tikukits/ds/array/tiku_kits_ds_array.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_init(void)
{
    struct tiku_kits_ds_array arr;
    int rc;

    TEST_GROUP_BEGIN("Array Init");

    rc = tiku_kits_ds_array_init(&arr, 16);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init cap=16 returns OK");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 0,
                "size is 0 after init");

    /* Max capacity accepted */
    rc = tiku_kits_ds_array_init(&arr, TIKU_KITS_DS_ARRAY_MAX_SIZE);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init max capacity OK");

    /* Zero capacity rejected */
    rc = tiku_kits_ds_array_init(&arr, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "zero capacity rejected");

    /* Oversized capacity rejected */
    rc = tiku_kits_ds_array_init(&arr, TIKU_KITS_DS_ARRAY_MAX_SIZE + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized capacity rejected");

    TEST_GROUP_END("Array Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: PUSH_BACK AND POP_BACK                                           */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_push_pop(void)
{
    struct tiku_kits_ds_array arr;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Array Push/Pop");

    tiku_kits_ds_array_init(&arr, 4);

    /* Push elements */
    rc = tiku_kits_ds_array_push_back(&arr, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 10 OK");
    rc = tiku_kits_ds_array_push_back(&arr, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 20 OK");
    rc = tiku_kits_ds_array_push_back(&arr, 30);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 30 OK");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 3,
                "size is 3 after 3 pushes");

    /* Pop elements (LIFO from back) */
    rc = tiku_kits_ds_array_pop_back(&arr, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop OK");
    TEST_ASSERT(val == 30, "popped value is 30");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 2,
                "size is 2 after pop");

    rc = tiku_kits_ds_array_pop_back(&arr, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "second pop OK");
    TEST_ASSERT(val == 20, "popped value is 20");

    /* Pop with NULL value pointer */
    rc = tiku_kits_ds_array_pop_back(&arr, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop with NULL value OK");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 0,
                "size is 0 after all pops");

    /* Pop from empty array */
    rc = tiku_kits_ds_array_pop_back(&arr, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "pop from empty returns ERR_EMPTY");

    TEST_GROUP_END("Array Push/Pop");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: SET AND GET                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_set_get(void)
{
    struct tiku_kits_ds_array arr;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Array Set/Get");

    tiku_kits_ds_array_init(&arr, 8);
    tiku_kits_ds_array_push_back(&arr, 100);
    tiku_kits_ds_array_push_back(&arr, 200);
    tiku_kits_ds_array_push_back(&arr, 300);

    /* Set element at valid index */
    rc = tiku_kits_ds_array_set(&arr, 1, 999);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "set index 1 OK");

    rc = tiku_kits_ds_array_get(&arr, 1, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "get index 1 OK");
    TEST_ASSERT(val == 999, "get returns updated value 999");

    /* Get other elements unchanged */
    tiku_kits_ds_array_get(&arr, 0, &val);
    TEST_ASSERT(val == 100, "index 0 still 100");
    tiku_kits_ds_array_get(&arr, 2, &val);
    TEST_ASSERT(val == 300, "index 2 still 300");

    /* Set out of bounds */
    rc = tiku_kits_ds_array_set(&arr, 3, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "set out of bounds rejected");

    /* Get out of bounds */
    rc = tiku_kits_ds_array_get(&arr, 3, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "get out of bounds rejected");

    TEST_GROUP_END("Array Set/Get");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: INSERT AND REMOVE                                                 */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_insert_remove(void)
{
    struct tiku_kits_ds_array arr;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Array Insert/Remove");

    tiku_kits_ds_array_init(&arr, 8);
    tiku_kits_ds_array_push_back(&arr, 10);
    tiku_kits_ds_array_push_back(&arr, 30);
    tiku_kits_ds_array_push_back(&arr, 40);

    /* Insert at index 1: [10, 20, 30, 40] */
    rc = tiku_kits_ds_array_insert(&arr, 1, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert at index 1 OK");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 4,
                "size is 4 after insert");

    tiku_kits_ds_array_get(&arr, 0, &val);
    TEST_ASSERT(val == 10, "index 0 is 10");
    tiku_kits_ds_array_get(&arr, 1, &val);
    TEST_ASSERT(val == 20, "index 1 is 20 (inserted)");
    tiku_kits_ds_array_get(&arr, 2, &val);
    TEST_ASSERT(val == 30, "index 2 is 30 (shifted)");
    tiku_kits_ds_array_get(&arr, 3, &val);
    TEST_ASSERT(val == 40, "index 3 is 40 (shifted)");

    /* Insert at beginning */
    rc = tiku_kits_ds_array_insert(&arr, 0, 5);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert at index 0 OK");
    tiku_kits_ds_array_get(&arr, 0, &val);
    TEST_ASSERT(val == 5, "index 0 is 5 (inserted at front)");

    /* Insert at end (same as push_back) */
    rc = tiku_kits_ds_array_insert(&arr, 5, 50);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert at end OK");
    tiku_kits_ds_array_get(&arr, 5, &val);
    TEST_ASSERT(val == 50, "index 5 is 50 (inserted at end)");

    /* Remove from middle: remove index 1 (was 10) */
    rc = tiku_kits_ds_array_remove(&arr, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "remove index 1 OK");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 5,
                "size is 5 after remove");
    tiku_kits_ds_array_get(&arr, 1, &val);
    TEST_ASSERT(val == 20, "index 1 shifted to 20 after remove");

    /* Remove out of bounds */
    rc = tiku_kits_ds_array_remove(&arr, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "remove out of bounds rejected");

    /* Insert out of bounds */
    rc = tiku_kits_ds_array_insert(&arr, 100, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "insert out of bounds rejected");

    TEST_GROUP_END("Array Insert/Remove");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: FIND                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_find(void)
{
    struct tiku_kits_ds_array arr;
    uint16_t idx;
    int rc;

    TEST_GROUP_BEGIN("Array Find");

    tiku_kits_ds_array_init(&arr, 8);
    tiku_kits_ds_array_push_back(&arr, 10);
    tiku_kits_ds_array_push_back(&arr, 20);
    tiku_kits_ds_array_push_back(&arr, 30);
    tiku_kits_ds_array_push_back(&arr, 20);

    /* Find existing value (first occurrence) */
    rc = tiku_kits_ds_array_find(&arr, 20, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "find 20 returns OK");
    TEST_ASSERT(idx == 1, "first occurrence of 20 at index 1");

    /* Find first element */
    rc = tiku_kits_ds_array_find(&arr, 10, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "find 10 returns OK");
    TEST_ASSERT(idx == 0, "10 found at index 0");

    /* Find last element */
    rc = tiku_kits_ds_array_find(&arr, 30, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "find 30 returns OK");
    TEST_ASSERT(idx == 2, "30 found at index 2");

    /* Find non-existent value */
    rc = tiku_kits_ds_array_find(&arr, 99, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "find 99 returns ERR_NOTFOUND");

    TEST_GROUP_END("Array Find");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: FILL                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_fill(void)
{
    struct tiku_kits_ds_array arr;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Array Fill");

    tiku_kits_ds_array_init(&arr, 8);

    rc = tiku_kits_ds_array_fill(&arr, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "fill returns OK");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 8,
                "size equals capacity after fill");

    /* Verify all elements are 42 */
    tiku_kits_ds_array_get(&arr, 0, &val);
    TEST_ASSERT(val == 42, "index 0 is 42");
    tiku_kits_ds_array_get(&arr, 4, &val);
    TEST_ASSERT(val == 42, "index 4 is 42");
    tiku_kits_ds_array_get(&arr, 7, &val);
    TEST_ASSERT(val == 42, "index 7 is 42");

    /* Fill with different value overwrites */
    rc = tiku_kits_ds_array_fill(&arr, -1);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "refill returns OK");
    tiku_kits_ds_array_get(&arr, 0, &val);
    TEST_ASSERT(val == -1, "index 0 is -1 after refill");

    TEST_GROUP_END("Array Fill");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: BOUNDS CHECKING                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_bounds_check(void)
{
    struct tiku_kits_ds_array arr;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Array Bounds Check");

    tiku_kits_ds_array_init(&arr, 4);
    tiku_kits_ds_array_push_back(&arr, 10);
    tiku_kits_ds_array_push_back(&arr, 20);

    /* Get beyond size but within capacity */
    rc = tiku_kits_ds_array_get(&arr, 2, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "get beyond size rejected");

    /* Set beyond size */
    rc = tiku_kits_ds_array_set(&arr, 2, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "set beyond size rejected");

    /* Fill to capacity, then push_back fails */
    tiku_kits_ds_array_push_back(&arr, 30);
    tiku_kits_ds_array_push_back(&arr, 40);
    rc = tiku_kits_ds_array_push_back(&arr, 50);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "push_back when full returns ERR_FULL");

    /* Insert when full */
    rc = tiku_kits_ds_array_insert(&arr, 0, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "insert when full returns ERR_FULL");

    TEST_GROUP_END("Array Bounds Check");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: CLEAR AND RESET                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_clear_reset(void)
{
    struct tiku_kits_ds_array arr;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Array Clear/Reset");

    tiku_kits_ds_array_init(&arr, 8);
    tiku_kits_ds_array_push_back(&arr, 10);
    tiku_kits_ds_array_push_back(&arr, 20);
    tiku_kits_ds_array_push_back(&arr, 30);

    rc = tiku_kits_ds_array_clear(&arr);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear returns OK");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 0,
                "size is 0 after clear");

    /* Pop from cleared array fails */
    rc = tiku_kits_ds_array_pop_back(&arr, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "pop after clear returns ERR_EMPTY");

    /* Can push after clear */
    rc = tiku_kits_ds_array_push_back(&arr, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push after clear OK");
    tiku_kits_ds_array_get(&arr, 0, &val);
    TEST_ASSERT(val == 99, "pushed value is 99");

    /* Re-init resets everything */
    rc = tiku_kits_ds_array_init(&arr, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "re-init returns OK");
    TEST_ASSERT(tiku_kits_ds_array_size(&arr) == 0,
                "size is 0 after re-init");

    TEST_GROUP_END("Array Clear/Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER INPUTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_null_inputs(void)
{
    struct tiku_kits_ds_array arr;
    tiku_kits_ds_elem_t val;
    uint16_t idx;
    int rc;

    TEST_GROUP_BEGIN("Array NULL Inputs");

    /* init with NULL */
    rc = tiku_kits_ds_array_init(NULL, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    /* set with NULL */
    rc = tiku_kits_ds_array_set(NULL, 0, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "set NULL rejected");

    /* get with NULL array */
    rc = tiku_kits_ds_array_get(NULL, 0, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "get NULL array rejected");

    /* get with NULL value */
    tiku_kits_ds_array_init(&arr, 4);
    tiku_kits_ds_array_push_back(&arr, 10);
    rc = tiku_kits_ds_array_get(&arr, 0, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "get NULL value rejected");

    /* push_back with NULL */
    rc = tiku_kits_ds_array_push_back(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "push_back NULL rejected");

    /* pop_back with NULL array */
    rc = tiku_kits_ds_array_pop_back(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "pop_back NULL rejected");

    /* insert with NULL */
    rc = tiku_kits_ds_array_insert(NULL, 0, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "insert NULL rejected");

    /* remove with NULL */
    rc = tiku_kits_ds_array_remove(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "remove NULL rejected");

    /* find with NULL array */
    rc = tiku_kits_ds_array_find(NULL, 42, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find NULL array rejected");

    /* find with NULL index */
    rc = tiku_kits_ds_array_find(&arr, 10, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find NULL index rejected");

    /* fill with NULL */
    rc = tiku_kits_ds_array_fill(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "fill NULL rejected");

    /* clear with NULL */
    rc = tiku_kits_ds_array_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear NULL rejected");

    /* size with NULL returns 0 */
    TEST_ASSERT(tiku_kits_ds_array_size(NULL) == 0,
                "size NULL returns 0");

    TEST_GROUP_END("Array NULL Inputs");
}
