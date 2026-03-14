/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_list.c - Linked list library tests
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
#include "tikukits/ds/list/tiku_kits_ds_list.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_init(void)
{
    struct tiku_kits_ds_list lst;
    int rc;

    TEST_GROUP_BEGIN("List Init");

    rc = tiku_kits_ds_list_init(&lst, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init cap=8 returns OK");
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 0, "size is 0");
    TEST_ASSERT(tiku_kits_ds_list_head(&lst) ==
                TIKU_KITS_DS_LIST_NONE, "head is NONE");

    /* Zero capacity rejected */
    rc = tiku_kits_ds_list_init(&lst, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "zero capacity rejected");

    /* Oversized capacity rejected */
    rc = tiku_kits_ds_list_init(&lst,
                                 TIKU_KITS_DS_LIST_MAX_NODES + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized capacity rejected");

    /* Max capacity accepted */
    rc = tiku_kits_ds_list_init(&lst, TIKU_KITS_DS_LIST_MAX_NODES);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "max capacity init OK");

    TEST_GROUP_END("List Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: PUSH FRONT AND POP FRONT                                          */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_push_front_pop(void)
{
    struct tiku_kits_ds_list lst;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("List Push Front / Pop");

    tiku_kits_ds_list_init(&lst, 4);

    /* Push 3 elements to front: list becomes [30, 20, 10] */
    rc = tiku_kits_ds_list_push_front(&lst, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push_front 10 OK");

    rc = tiku_kits_ds_list_push_front(&lst, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push_front 20 OK");

    rc = tiku_kits_ds_list_push_front(&lst, 30);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push_front 30 OK");

    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 3, "size is 3");

    /* Pop front: LIFO order 30, 20, 10 */
    rc = tiku_kits_ds_list_pop_front(&lst, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop_front OK");
    TEST_ASSERT(val == 30, "popped 30");

    rc = tiku_kits_ds_list_pop_front(&lst, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop_front OK");
    TEST_ASSERT(val == 20, "popped 20");

    rc = tiku_kits_ds_list_pop_front(&lst, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop_front OK");
    TEST_ASSERT(val == 10, "popped 10");

    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 0,
                "size is 0 after all pops");

    /* Pop from empty list */
    rc = tiku_kits_ds_list_pop_front(&lst, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "pop from empty returns ERR_EMPTY");

    /* Fill to capacity then overflow */
    tiku_kits_ds_list_init(&lst, 2);
    tiku_kits_ds_list_push_front(&lst, 1);
    tiku_kits_ds_list_push_front(&lst, 2);
    rc = tiku_kits_ds_list_push_front(&lst, 3);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "push to full list returns ERR_FULL");

    TEST_GROUP_END("List Push Front / Pop");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: PUSH BACK                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_push_back(void)
{
    struct tiku_kits_ds_list lst;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("List Push Back");

    tiku_kits_ds_list_init(&lst, 4);

    /* Push back: list becomes [10, 20, 30] */
    rc = tiku_kits_ds_list_push_back(&lst, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push_back 10 OK");

    rc = tiku_kits_ds_list_push_back(&lst, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push_back 20 OK");

    rc = tiku_kits_ds_list_push_back(&lst, 30);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push_back 30 OK");

    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 3, "size is 3");

    /* Pop front in FIFO order: 10, 20, 30 */
    rc = tiku_kits_ds_list_pop_front(&lst, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop OK");
    TEST_ASSERT(val == 10, "first is 10");

    rc = tiku_kits_ds_list_pop_front(&lst, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop OK");
    TEST_ASSERT(val == 20, "second is 20");

    rc = tiku_kits_ds_list_pop_front(&lst, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop OK");
    TEST_ASSERT(val == 30, "third is 30");

    /* Push back to empty list */
    tiku_kits_ds_list_init(&lst, 2);
    rc = tiku_kits_ds_list_push_back(&lst, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push_back to empty OK");
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 1, "size is 1");

    TEST_GROUP_END("List Push Back");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: INSERT AFTER                                                      */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_insert_after(void)
{
    struct tiku_kits_ds_list lst;
    tiku_kits_ds_elem_t val;
    uint8_t head_idx;
    uint8_t next_idx;
    int rc;

    TEST_GROUP_BEGIN("List Insert After");

    tiku_kits_ds_list_init(&lst, 8);

    /* Build list [10, 30] */
    tiku_kits_ds_list_push_back(&lst, 10);
    tiku_kits_ds_list_push_back(&lst, 30);

    /* Insert 20 after the head node (10) -> [10, 20, 30] */
    head_idx = tiku_kits_ds_list_head(&lst);
    rc = tiku_kits_ds_list_insert_after(&lst, head_idx, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert_after OK");
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 3, "size is 3");

    /* Verify order by traversal: 10 -> 20 -> 30 */
    tiku_kits_ds_list_get(&lst, head_idx, &val);
    TEST_ASSERT(val == 10, "head is 10");

    next_idx = tiku_kits_ds_list_next(&lst, head_idx);
    tiku_kits_ds_list_get(&lst, next_idx, &val);
    TEST_ASSERT(val == 20, "second is 20");

    next_idx = tiku_kits_ds_list_next(&lst, next_idx);
    tiku_kits_ds_list_get(&lst, next_idx, &val);
    TEST_ASSERT(val == 30, "third is 30");

    /* Insert after invalid index */
    rc = tiku_kits_ds_list_insert_after(&lst, 200, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "insert_after invalid index rejected");

    TEST_GROUP_END("List Insert After");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: REMOVE NODE                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_remove_node(void)
{
    struct tiku_kits_ds_list lst;
    tiku_kits_ds_elem_t val;
    uint8_t idx;
    int rc;

    TEST_GROUP_BEGIN("List Remove Node");

    tiku_kits_ds_list_init(&lst, 8);

    /* Build list [10, 20, 30] */
    tiku_kits_ds_list_push_back(&lst, 10);
    tiku_kits_ds_list_push_back(&lst, 20);
    tiku_kits_ds_list_push_back(&lst, 30);

    /* Remove middle node (value 20) */
    tiku_kits_ds_list_find(&lst, 20, &idx);
    rc = tiku_kits_ds_list_remove(&lst, idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "remove middle OK");
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 2,
                "size is 2 after remove");

    /* Verify 20 is gone */
    rc = tiku_kits_ds_list_find(&lst, 20, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "20 not found after remove");

    /* Remove head node (value 10) */
    tiku_kits_ds_list_find(&lst, 10, &idx);
    rc = tiku_kits_ds_list_remove(&lst, idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "remove head OK");
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 1, "size is 1");

    /* Only 30 remains */
    tiku_kits_ds_list_pop_front(&lst, &val);
    TEST_ASSERT(val == 30, "remaining element is 30");

    /* Remove from empty list */
    rc = tiku_kits_ds_list_remove(&lst, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "remove from empty returns ERR_BOUNDS");

    /* Remove invalid index */
    tiku_kits_ds_list_init(&lst, 4);
    tiku_kits_ds_list_push_back(&lst, 1);
    rc = tiku_kits_ds_list_remove(&lst, 200);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "remove invalid index returns ERR_BOUNDS");

    TEST_GROUP_END("List Remove Node");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: FIND                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_find(void)
{
    struct tiku_kits_ds_list lst;
    uint8_t idx;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("List Find");

    tiku_kits_ds_list_init(&lst, 8);

    /* Build list [10, 20, 30, 40] */
    tiku_kits_ds_list_push_back(&lst, 10);
    tiku_kits_ds_list_push_back(&lst, 20);
    tiku_kits_ds_list_push_back(&lst, 30);
    tiku_kits_ds_list_push_back(&lst, 40);

    /* Find existing values */
    rc = tiku_kits_ds_list_find(&lst, 10, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "find 10 OK");
    tiku_kits_ds_list_get(&lst, idx, &val);
    TEST_ASSERT(val == 10, "found node has value 10");

    rc = tiku_kits_ds_list_find(&lst, 30, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "find 30 OK");
    tiku_kits_ds_list_get(&lst, idx, &val);
    TEST_ASSERT(val == 30, "found node has value 30");

    /* Find non-existent value */
    rc = tiku_kits_ds_list_find(&lst, 99, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "find 99 returns ERR_NOTFOUND");

    /* Find in empty list */
    tiku_kits_ds_list_init(&lst, 4);
    rc = tiku_kits_ds_list_find(&lst, 10, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "find in empty returns ERR_NOTFOUND");

    TEST_GROUP_END("List Find");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: TRAVERSAL                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_traversal(void)
{
    struct tiku_kits_ds_list lst;
    tiku_kits_ds_elem_t val;
    tiku_kits_ds_elem_t expected[4] = {10, 20, 30, 40};
    uint8_t cur;
    uint16_t i;

    TEST_GROUP_BEGIN("List Traversal");

    tiku_kits_ds_list_init(&lst, 8);

    /* Build list [10, 20, 30, 40] */
    tiku_kits_ds_list_push_back(&lst, 10);
    tiku_kits_ds_list_push_back(&lst, 20);
    tiku_kits_ds_list_push_back(&lst, 30);
    tiku_kits_ds_list_push_back(&lst, 40);

    /* Traverse using head/next and verify order */
    cur = tiku_kits_ds_list_head(&lst);
    i = 0;
    while (cur != TIKU_KITS_DS_LIST_NONE && i < 4) {
        tiku_kits_ds_list_get(&lst, cur, &val);
        TEST_ASSERT(val == expected[i], "traversal order correct");
        cur = tiku_kits_ds_list_next(&lst, cur);
        i++;
    }
    TEST_ASSERT(i == 4, "traversed exactly 4 nodes");
    TEST_ASSERT(cur == TIKU_KITS_DS_LIST_NONE,
                "traversal ended at NONE");

    /* next() on the last node returns NONE */
    tiku_kits_ds_list_init(&lst, 4);
    tiku_kits_ds_list_push_back(&lst, 1);
    cur = tiku_kits_ds_list_head(&lst);
    TEST_ASSERT(
        tiku_kits_ds_list_next(&lst, cur) ==
            TIKU_KITS_DS_LIST_NONE,
        "next of single-node list is NONE");

    TEST_GROUP_END("List Traversal");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: CLEAR AND RESET                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_clear_reset(void)
{
    struct tiku_kits_ds_list lst;
    int rc;

    TEST_GROUP_BEGIN("List Clear/Reset");

    tiku_kits_ds_list_init(&lst, 4);

    /* Fill the list */
    tiku_kits_ds_list_push_back(&lst, 10);
    tiku_kits_ds_list_push_back(&lst, 20);
    tiku_kits_ds_list_push_back(&lst, 30);
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 3,
                "size is 3 before clear");

    /* Clear */
    rc = tiku_kits_ds_list_clear(&lst);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear returns OK");
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 0,
                "size is 0 after clear");
    TEST_ASSERT(tiku_kits_ds_list_head(&lst) ==
                TIKU_KITS_DS_LIST_NONE,
                "head is NONE after clear");

    /* Can re-use list after clear */
    rc = tiku_kits_ds_list_push_back(&lst, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "push_back after clear OK");
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 1,
                "size is 1 after re-use");

    /* Fill to capacity after clear to verify free list rebuilt */
    tiku_kits_ds_list_clear(&lst);
    tiku_kits_ds_list_push_back(&lst, 1);
    tiku_kits_ds_list_push_back(&lst, 2);
    tiku_kits_ds_list_push_back(&lst, 3);
    tiku_kits_ds_list_push_back(&lst, 4);
    TEST_ASSERT(tiku_kits_ds_list_size(&lst) == 4,
                "filled to capacity after clear");
    rc = tiku_kits_ds_list_push_back(&lst, 5);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "overflow after fill returns ERR_FULL");

    TEST_GROUP_END("List Clear/Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER INPUTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_null_inputs(void)
{
    struct tiku_kits_ds_list lst;
    tiku_kits_ds_elem_t val;
    uint8_t idx;
    int rc;

    TEST_GROUP_BEGIN("List NULL Inputs");

    tiku_kits_ds_list_init(&lst, 4);

    rc = tiku_kits_ds_list_init(NULL, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    rc = tiku_kits_ds_list_push_front(NULL, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "push_front NULL rejected");

    rc = tiku_kits_ds_list_push_back(NULL, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "push_back NULL rejected");

    rc = tiku_kits_ds_list_pop_front(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "pop_front NULL list rejected");

    rc = tiku_kits_ds_list_pop_front(&lst, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "pop_front NULL value rejected");

    rc = tiku_kits_ds_list_insert_after(NULL, 0, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "insert_after NULL rejected");

    rc = tiku_kits_ds_list_remove(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "remove NULL rejected");

    rc = tiku_kits_ds_list_find(NULL, 1, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find NULL list rejected");

    rc = tiku_kits_ds_list_find(&lst, 1, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find NULL idx rejected");

    rc = tiku_kits_ds_list_get(NULL, 0, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "get NULL list rejected");

    rc = tiku_kits_ds_list_get(&lst, 0, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "get NULL value rejected");

    rc = tiku_kits_ds_list_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear NULL rejected");

    TEST_ASSERT(tiku_kits_ds_list_size(NULL) == 0,
                "size NULL returns 0");
    TEST_ASSERT(tiku_kits_ds_list_head(NULL) ==
                TIKU_KITS_DS_LIST_NONE,
                "head NULL returns NONE");
    TEST_ASSERT(tiku_kits_ds_list_next(NULL, 0) ==
                TIKU_KITS_DS_LIST_NONE,
                "next NULL returns NONE");

    TEST_GROUP_END("List NULL Inputs");
}
