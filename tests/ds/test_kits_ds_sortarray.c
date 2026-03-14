/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_sortarray.c - Sorted array data structure tests
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
#include "tikukits/ds/sortarray/tiku_kits_ds_sortarray.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_init(void)
{
    struct tiku_kits_ds_sortarray sa;
    int rc;

    TEST_GROUP_BEGIN("SortArray Init");

    rc = tiku_kits_ds_sortarray_init(&sa, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init capacity 8 returns OK");
    TEST_ASSERT(tiku_kits_ds_sortarray_size(&sa) == 0,
                "size is 0 after init");
    TEST_ASSERT(sa.capacity == 8, "capacity is 8");

    /* Zero capacity rejected */
    rc = tiku_kits_ds_sortarray_init(&sa, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "zero capacity rejected");

    /* Oversized capacity rejected */
    rc = tiku_kits_ds_sortarray_init(&sa,
        TIKU_KITS_DS_SORTARRAY_MAX_SIZE + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized capacity rejected");

    /* Max capacity accepted */
    rc = tiku_kits_ds_sortarray_init(&sa,
        TIKU_KITS_DS_SORTARRAY_MAX_SIZE);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "max capacity init OK");

    /* Capacity of 1 accepted */
    rc = tiku_kits_ds_sortarray_init(&sa, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "capacity 1 init OK");

    TEST_GROUP_END("SortArray Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: INSERT SORTED ORDER                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_insert_sorted(void)
{
    struct tiku_kits_ds_sortarray sa;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("SortArray Insert Sorted");

    tiku_kits_ds_sortarray_init(&sa, 8);

    /* Insert in reverse order: 50, 30, 40, 10, 20 */
    tiku_kits_ds_sortarray_insert(&sa, 50);
    tiku_kits_ds_sortarray_insert(&sa, 30);
    tiku_kits_ds_sortarray_insert(&sa, 40);
    tiku_kits_ds_sortarray_insert(&sa, 10);
    tiku_kits_ds_sortarray_insert(&sa, 20);

    TEST_ASSERT(tiku_kits_ds_sortarray_size(&sa) == 5,
                "size is 5 after 5 inserts");

    /* Verify sorted order: [10, 20, 30, 40, 50] */
    tiku_kits_ds_sortarray_get(&sa, 0, &val);
    TEST_ASSERT(val == 10, "index 0 is 10");
    tiku_kits_ds_sortarray_get(&sa, 1, &val);
    TEST_ASSERT(val == 20, "index 1 is 20");
    tiku_kits_ds_sortarray_get(&sa, 2, &val);
    TEST_ASSERT(val == 30, "index 2 is 30");
    tiku_kits_ds_sortarray_get(&sa, 3, &val);
    TEST_ASSERT(val == 40, "index 3 is 40");
    tiku_kits_ds_sortarray_get(&sa, 4, &val);
    TEST_ASSERT(val == 50, "index 4 is 50");

    /* Insert when full */
    tiku_kits_ds_sortarray_init(&sa, 2);
    tiku_kits_ds_sortarray_insert(&sa, 1);
    tiku_kits_ds_sortarray_insert(&sa, 2);
    rc = tiku_kits_ds_sortarray_insert(&sa, 3);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "insert when full returns ERR_FULL");

    /* Insert negative values */
    tiku_kits_ds_sortarray_init(&sa, 8);
    tiku_kits_ds_sortarray_insert(&sa, -5);
    tiku_kits_ds_sortarray_insert(&sa, 10);
    tiku_kits_ds_sortarray_insert(&sa, -20);
    tiku_kits_ds_sortarray_insert(&sa, 0);

    tiku_kits_ds_sortarray_get(&sa, 0, &val);
    TEST_ASSERT(val == -20, "negative: index 0 is -20");
    tiku_kits_ds_sortarray_get(&sa, 1, &val);
    TEST_ASSERT(val == -5, "negative: index 1 is -5");
    tiku_kits_ds_sortarray_get(&sa, 2, &val);
    TEST_ASSERT(val == 0, "negative: index 2 is 0");
    tiku_kits_ds_sortarray_get(&sa, 3, &val);
    TEST_ASSERT(val == 10, "negative: index 3 is 10");

    TEST_GROUP_END("SortArray Insert Sorted");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: REMOVE ELEMENT                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_remove_element(void)
{
    struct tiku_kits_ds_sortarray sa;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("SortArray Remove");

    tiku_kits_ds_sortarray_init(&sa, 8);
    tiku_kits_ds_sortarray_insert(&sa, 10);
    tiku_kits_ds_sortarray_insert(&sa, 20);
    tiku_kits_ds_sortarray_insert(&sa, 30);
    tiku_kits_ds_sortarray_insert(&sa, 40);

    /* Remove middle element */
    rc = tiku_kits_ds_sortarray_remove(&sa, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "remove 20 returns OK");
    TEST_ASSERT(tiku_kits_ds_sortarray_size(&sa) == 3,
                "size is 3 after remove");

    /* Verify remaining: [10, 30, 40] */
    tiku_kits_ds_sortarray_get(&sa, 0, &val);
    TEST_ASSERT(val == 10, "index 0 is 10 after remove");
    tiku_kits_ds_sortarray_get(&sa, 1, &val);
    TEST_ASSERT(val == 30, "index 1 is 30 after remove");
    tiku_kits_ds_sortarray_get(&sa, 2, &val);
    TEST_ASSERT(val == 40, "index 2 is 40 after remove");

    /* Remove first element */
    tiku_kits_ds_sortarray_remove(&sa, 10);
    tiku_kits_ds_sortarray_get(&sa, 0, &val);
    TEST_ASSERT(val == 30, "index 0 is 30 after removing first");

    /* Remove last element */
    tiku_kits_ds_sortarray_remove(&sa, 40);
    TEST_ASSERT(tiku_kits_ds_sortarray_size(&sa) == 1,
                "size is 1 after removing last");

    /* Remove non-existent element */
    rc = tiku_kits_ds_sortarray_remove(&sa, 999);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "remove non-existent returns NOTFOUND");

    /* Remove from empty array */
    tiku_kits_ds_sortarray_clear(&sa);
    rc = tiku_kits_ds_sortarray_remove(&sa, 30);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "remove from empty returns NOTFOUND");

    TEST_GROUP_END("SortArray Remove");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: FIND WITH BINARY SEARCH                                           */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_find_binary_search(void)
{
    struct tiku_kits_ds_sortarray sa;
    uint16_t idx;
    int rc;

    TEST_GROUP_BEGIN("SortArray Find");

    tiku_kits_ds_sortarray_init(&sa, 8);
    tiku_kits_ds_sortarray_insert(&sa, 100);
    tiku_kits_ds_sortarray_insert(&sa, 200);
    tiku_kits_ds_sortarray_insert(&sa, 300);
    tiku_kits_ds_sortarray_insert(&sa, 400);
    tiku_kits_ds_sortarray_insert(&sa, 500);

    /* Find first element */
    rc = tiku_kits_ds_sortarray_find(&sa, 100, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "find 100 returns OK");
    TEST_ASSERT(idx == 0, "100 is at index 0");

    /* Find middle element */
    rc = tiku_kits_ds_sortarray_find(&sa, 300, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "find 300 returns OK");
    TEST_ASSERT(idx == 2, "300 is at index 2");

    /* Find last element */
    rc = tiku_kits_ds_sortarray_find(&sa, 500, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "find 500 returns OK");
    TEST_ASSERT(idx == 4, "500 is at index 4");

    /* Find non-existent element */
    rc = tiku_kits_ds_sortarray_find(&sa, 250, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "find 250 returns NOTFOUND");

    /* Find in empty array */
    tiku_kits_ds_sortarray_clear(&sa);
    rc = tiku_kits_ds_sortarray_find(&sa, 100, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "find in empty returns NOTFOUND");

    TEST_GROUP_END("SortArray Find");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: GET BY INDEX                                                      */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_get_by_index(void)
{
    struct tiku_kits_ds_sortarray sa;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("SortArray Get By Index");

    tiku_kits_ds_sortarray_init(&sa, 8);
    tiku_kits_ds_sortarray_insert(&sa, 5);
    tiku_kits_ds_sortarray_insert(&sa, 15);
    tiku_kits_ds_sortarray_insert(&sa, 25);

    rc = tiku_kits_ds_sortarray_get(&sa, 0, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "get index 0 returns OK");
    TEST_ASSERT(val == 5, "index 0 value is 5");

    rc = tiku_kits_ds_sortarray_get(&sa, 2, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "get index 2 returns OK");
    TEST_ASSERT(val == 25, "index 2 value is 25");

    /* Out-of-bounds */
    rc = tiku_kits_ds_sortarray_get(&sa, 3, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "get index 3 out-of-bounds");

    rc = tiku_kits_ds_sortarray_get(&sa, 100, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "get index 100 out-of-bounds");

    TEST_GROUP_END("SortArray Get By Index");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: MIN AND MAX                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_min_max(void)
{
    struct tiku_kits_ds_sortarray sa;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("SortArray Min/Max");

    tiku_kits_ds_sortarray_init(&sa, 8);

    /* Empty array */
    rc = tiku_kits_ds_sortarray_min(&sa, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "min on empty returns ERR_EMPTY");
    rc = tiku_kits_ds_sortarray_max(&sa, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "max on empty returns ERR_EMPTY");

    /* Single element */
    tiku_kits_ds_sortarray_insert(&sa, 42);
    tiku_kits_ds_sortarray_min(&sa, &val);
    TEST_ASSERT(val == 42, "min of single element is 42");
    tiku_kits_ds_sortarray_max(&sa, &val);
    TEST_ASSERT(val == 42, "max of single element is 42");

    /* Multiple elements */
    tiku_kits_ds_sortarray_insert(&sa, -10);
    tiku_kits_ds_sortarray_insert(&sa, 100);
    tiku_kits_ds_sortarray_insert(&sa, 0);

    tiku_kits_ds_sortarray_min(&sa, &val);
    TEST_ASSERT(val == -10, "min is -10");
    tiku_kits_ds_sortarray_max(&sa, &val);
    TEST_ASSERT(val == 100, "max is 100");

    TEST_GROUP_END("SortArray Min/Max");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: DUPLICATE VALUES                                                  */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_duplicates(void)
{
    struct tiku_kits_ds_sortarray sa;
    tiku_kits_ds_elem_t val;

    TEST_GROUP_BEGIN("SortArray Duplicates");

    tiku_kits_ds_sortarray_init(&sa, 8);

    /* Insert duplicates */
    tiku_kits_ds_sortarray_insert(&sa, 10);
    tiku_kits_ds_sortarray_insert(&sa, 10);
    tiku_kits_ds_sortarray_insert(&sa, 10);
    tiku_kits_ds_sortarray_insert(&sa, 5);
    tiku_kits_ds_sortarray_insert(&sa, 20);

    TEST_ASSERT(tiku_kits_ds_sortarray_size(&sa) == 5,
                "size is 5 with duplicates");

    /* Verify sorted: [5, 10, 10, 10, 20] */
    tiku_kits_ds_sortarray_get(&sa, 0, &val);
    TEST_ASSERT(val == 5, "index 0 is 5");
    tiku_kits_ds_sortarray_get(&sa, 1, &val);
    TEST_ASSERT(val == 10, "index 1 is 10");
    tiku_kits_ds_sortarray_get(&sa, 2, &val);
    TEST_ASSERT(val == 10, "index 2 is 10");
    tiku_kits_ds_sortarray_get(&sa, 3, &val);
    TEST_ASSERT(val == 10, "index 3 is 10");
    tiku_kits_ds_sortarray_get(&sa, 4, &val);
    TEST_ASSERT(val == 20, "index 4 is 20");

    /* Remove one duplicate -- only first occurrence removed */
    tiku_kits_ds_sortarray_remove(&sa, 10);
    TEST_ASSERT(tiku_kits_ds_sortarray_size(&sa) == 4,
                "size is 4 after removing one duplicate");
    tiku_kits_ds_sortarray_get(&sa, 1, &val);
    TEST_ASSERT(val == 10, "index 1 still 10 after removing one");
    tiku_kits_ds_sortarray_get(&sa, 2, &val);
    TEST_ASSERT(val == 10, "index 2 still 10 after removing one");

    /* Contains still finds remaining duplicates */
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 10) == 1,
                "still contains 10 after partial removal");

    /* Remove all duplicates */
    tiku_kits_ds_sortarray_remove(&sa, 10);
    tiku_kits_ds_sortarray_remove(&sa, 10);
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 10) == 0,
                "no longer contains 10 after full removal");

    TEST_GROUP_END("SortArray Duplicates");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: CONTAINS                                                          */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_contains(void)
{
    struct tiku_kits_ds_sortarray sa;

    TEST_GROUP_BEGIN("SortArray Contains");

    tiku_kits_ds_sortarray_init(&sa, 8);
    tiku_kits_ds_sortarray_insert(&sa, 10);
    tiku_kits_ds_sortarray_insert(&sa, 20);
    tiku_kits_ds_sortarray_insert(&sa, 30);

    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 10) == 1,
                "contains 10");
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 20) == 1,
                "contains 20");
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 30) == 1,
                "contains 30");
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 15) == 0,
                "does not contain 15");
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 0) == 0,
                "does not contain 0");
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 100) == 0,
                "does not contain 100");

    /* Contains on empty array */
    tiku_kits_ds_sortarray_clear(&sa);
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(&sa, 10) == 0,
                "empty array does not contain 10");

    /* Contains on NULL */
    TEST_ASSERT(tiku_kits_ds_sortarray_contains(NULL, 10) == 0,
                "NULL contains returns 0");

    TEST_GROUP_END("SortArray Contains");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER INPUTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_null_inputs(void)
{
    struct tiku_kits_ds_sortarray sa;
    tiku_kits_ds_elem_t val;
    uint16_t idx;
    int rc;

    TEST_GROUP_BEGIN("SortArray NULL Inputs");

    rc = tiku_kits_ds_sortarray_init(NULL, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    rc = tiku_kits_ds_sortarray_insert(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "insert NULL rejected");

    rc = tiku_kits_ds_sortarray_remove(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "remove NULL rejected");

    rc = tiku_kits_ds_sortarray_find(NULL, 42, &idx);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find NULL sa rejected");

    tiku_kits_ds_sortarray_init(&sa, 8);
    rc = tiku_kits_ds_sortarray_find(&sa, 42, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find NULL index rejected");

    rc = tiku_kits_ds_sortarray_get(NULL, 0, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "get NULL sa rejected");

    rc = tiku_kits_ds_sortarray_get(&sa, 0, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "get NULL value rejected");

    rc = tiku_kits_ds_sortarray_min(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "min NULL sa rejected");

    rc = tiku_kits_ds_sortarray_min(&sa, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "min NULL value rejected");

    rc = tiku_kits_ds_sortarray_max(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "max NULL sa rejected");

    rc = tiku_kits_ds_sortarray_max(&sa, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "max NULL value rejected");

    rc = tiku_kits_ds_sortarray_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear NULL rejected");

    TEST_ASSERT(tiku_kits_ds_sortarray_size(NULL) == 0,
                "size NULL returns 0");

    TEST_GROUP_END("SortArray NULL Inputs");
}
