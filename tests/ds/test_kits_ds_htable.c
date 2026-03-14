/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_htable.c - Hash table library tests
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
#include "tikukits/ds/htable/tiku_kits_ds_htable.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_init(void)
{
    struct tiku_kits_ds_htable ht;
    int rc;

    TEST_GROUP_BEGIN("HTable Init");

    rc = tiku_kits_ds_htable_init(&ht, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init cap=8 returns OK");
    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 0,
                "count is 0");

    /* Zero capacity rejected */
    rc = tiku_kits_ds_htable_init(&ht, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "zero capacity rejected");

    /* Oversized capacity rejected */
    rc = tiku_kits_ds_htable_init(&ht,
                                   TIKU_KITS_DS_HTABLE_MAX_SIZE + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized capacity rejected");

    /* Max capacity accepted */
    rc = tiku_kits_ds_htable_init(&ht,
                                   TIKU_KITS_DS_HTABLE_MAX_SIZE);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "max capacity init OK");

    /* All entries should be EMPTY after init */
    TEST_ASSERT(tiku_kits_ds_htable_contains(&ht, 0) == 0,
                "key 0 not present after init");

    TEST_GROUP_END("HTable Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: PUT AND GET                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_put_get(void)
{
    struct tiku_kits_ds_htable ht;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("HTable Put/Get");

    tiku_kits_ds_htable_init(&ht, 8);

    /* Insert key-value pairs */
    rc = tiku_kits_ds_htable_put(&ht, 100, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "put key=100 OK");

    rc = tiku_kits_ds_htable_put(&ht, 200, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "put key=200 OK");

    rc = tiku_kits_ds_htable_put(&ht, 300, 30);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "put key=300 OK");

    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 3,
                "count is 3");

    /* Retrieve values */
    rc = tiku_kits_ds_htable_get(&ht, 100, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "get key=100 OK");
    TEST_ASSERT(val == 10, "value for 100 is 10");

    rc = tiku_kits_ds_htable_get(&ht, 200, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "get key=200 OK");
    TEST_ASSERT(val == 20, "value for 200 is 20");

    rc = tiku_kits_ds_htable_get(&ht, 300, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "get key=300 OK");
    TEST_ASSERT(val == 30, "value for 300 is 30");

    /* Get non-existent key */
    rc = tiku_kits_ds_htable_get(&ht, 999, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "get missing key returns ERR_NOTFOUND");

    TEST_GROUP_END("HTable Put/Get");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: UPDATE EXISTING KEY                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_update_existing(void)
{
    struct tiku_kits_ds_htable ht;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("HTable Update Existing");

    tiku_kits_ds_htable_init(&ht, 8);

    /* Insert then update same key */
    tiku_kits_ds_htable_put(&ht, 42, 100);
    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 1,
                "count is 1 after insert");

    rc = tiku_kits_ds_htable_put(&ht, 42, 200);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "update returns OK");
    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 1,
                "count still 1 after update");

    rc = tiku_kits_ds_htable_get(&ht, 42, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "get updated key OK");
    TEST_ASSERT(val == 200, "value updated to 200");

    /* Update again */
    tiku_kits_ds_htable_put(&ht, 42, -50);
    tiku_kits_ds_htable_get(&ht, 42, &val);
    TEST_ASSERT(val == -50, "value updated to -50");
    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 1,
                "count still 1");

    TEST_GROUP_END("HTable Update Existing");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: REMOVE WITH TOMBSTONE                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_remove_tombstone(void)
{
    struct tiku_kits_ds_htable ht;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("HTable Remove Tombstone");

    tiku_kits_ds_htable_init(&ht, 8);

    tiku_kits_ds_htable_put(&ht, 10, 100);
    tiku_kits_ds_htable_put(&ht, 20, 200);
    tiku_kits_ds_htable_put(&ht, 30, 300);

    /* Remove middle key */
    rc = tiku_kits_ds_htable_remove(&ht, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "remove key=20 OK");
    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 2,
                "count is 2 after remove");

    /* Removed key is no longer found */
    rc = tiku_kits_ds_htable_get(&ht, 20, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "removed key not found");

    /* Other keys still accessible */
    rc = tiku_kits_ds_htable_get(&ht, 10, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "key=10 still OK");
    TEST_ASSERT(val == 100, "key=10 value intact");

    rc = tiku_kits_ds_htable_get(&ht, 30, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "key=30 still OK");
    TEST_ASSERT(val == 300, "key=30 value intact");

    /* Remove non-existent key */
    rc = tiku_kits_ds_htable_remove(&ht, 999);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "remove missing key returns ERR_NOTFOUND");

    /* Re-insert at tombstone slot */
    rc = tiku_kits_ds_htable_put(&ht, 20, 999);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "re-insert at tombstone OK");
    tiku_kits_ds_htable_get(&ht, 20, &val);
    TEST_ASSERT(val == 999, "re-inserted value is 999");

    TEST_GROUP_END("HTable Remove Tombstone");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: CONTAINS                                                          */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_contains(void)
{
    struct tiku_kits_ds_htable ht;

    TEST_GROUP_BEGIN("HTable Contains");

    tiku_kits_ds_htable_init(&ht, 8);

    tiku_kits_ds_htable_put(&ht, 55, 1);
    tiku_kits_ds_htable_put(&ht, 77, 2);

    TEST_ASSERT(tiku_kits_ds_htable_contains(&ht, 55) == 1,
                "contains 55 is true");
    TEST_ASSERT(tiku_kits_ds_htable_contains(&ht, 77) == 1,
                "contains 77 is true");
    TEST_ASSERT(tiku_kits_ds_htable_contains(&ht, 99) == 0,
                "contains 99 is false");

    /* After removal */
    tiku_kits_ds_htable_remove(&ht, 55);
    TEST_ASSERT(tiku_kits_ds_htable_contains(&ht, 55) == 0,
                "contains 55 is false after remove");

    /* NULL table */
    TEST_ASSERT(tiku_kits_ds_htable_contains(NULL, 55) == 0,
                "contains NULL returns 0");

    TEST_GROUP_END("HTable Contains");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: COLLISION HANDLING                                                 */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_collision_handling(void)
{
    struct tiku_kits_ds_htable ht;
    tiku_kits_ds_elem_t val;
    uint16_t i;
    int rc;

    TEST_GROUP_BEGIN("HTable Collision Handling");

    /* Use small capacity to force collisions */
    tiku_kits_ds_htable_init(&ht, 4);

    /* Insert 4 keys -- guaranteed collisions with cap=4 */
    rc = tiku_kits_ds_htable_put(&ht, 1, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "put key=1 OK");

    rc = tiku_kits_ds_htable_put(&ht, 5, 50);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "put key=5 OK");

    rc = tiku_kits_ds_htable_put(&ht, 9, 90);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "put key=9 OK");

    rc = tiku_kits_ds_htable_put(&ht, 13, 130);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "put key=13 OK");

    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 4,
                "count is 4");

    /* Verify all keys retrievable despite collisions */
    rc = tiku_kits_ds_htable_get(&ht, 1, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK && val == 10,
                "get key=1 returns 10");

    rc = tiku_kits_ds_htable_get(&ht, 5, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK && val == 50,
                "get key=5 returns 50");

    rc = tiku_kits_ds_htable_get(&ht, 9, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK && val == 90,
                "get key=9 returns 90");

    rc = tiku_kits_ds_htable_get(&ht, 13, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK && val == 130,
                "get key=13 returns 130");

    /* Remove one, verify others still work */
    tiku_kits_ds_htable_remove(&ht, 5);
    rc = tiku_kits_ds_htable_get(&ht, 9, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK && val == 90,
                "key=9 OK after removing key=5 (tombstone)");

    rc = tiku_kits_ds_htable_get(&ht, 13, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK && val == 130,
                "key=13 OK after removing key=5 (tombstone)");

    TEST_GROUP_END("HTable Collision Handling");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: FULL TABLE                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_full_table(void)
{
    struct tiku_kits_ds_htable ht;
    tiku_kits_ds_elem_t val;
    uint16_t i;
    int rc;

    TEST_GROUP_BEGIN("HTable Full Table");

    tiku_kits_ds_htable_init(&ht, 4);

    /* Fill to capacity */
    for (i = 0; i < 4; i++) {
        rc = tiku_kits_ds_htable_put(&ht, i + 1,
                                      (tiku_kits_ds_elem_t)(i * 10));
        TEST_ASSERT(rc == TIKU_KITS_DS_OK, "fill OK");
    }
    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 4,
                "count is 4 (full)");

    /* Overflow: insert new key into full table */
    rc = tiku_kits_ds_htable_put(&ht, 99, 999);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "insert to full table returns ERR_FULL");

    /* Update existing key in full table is still OK */
    rc = tiku_kits_ds_htable_put(&ht, 2, 777);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "update in full table OK");
    tiku_kits_ds_htable_get(&ht, 2, &val);
    TEST_ASSERT(val == 777, "updated value in full table");

    /* Remove one, then insert should succeed */
    tiku_kits_ds_htable_remove(&ht, 1);
    rc = tiku_kits_ds_htable_put(&ht, 99, 999);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "insert after remove in full table OK");
    tiku_kits_ds_htable_get(&ht, 99, &val);
    TEST_ASSERT(val == 999, "new key value correct");

    TEST_GROUP_END("HTable Full Table");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: CLEAR AND RESET                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_clear_reset(void)
{
    struct tiku_kits_ds_htable ht;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("HTable Clear/Reset");

    tiku_kits_ds_htable_init(&ht, 8);

    tiku_kits_ds_htable_put(&ht, 10, 100);
    tiku_kits_ds_htable_put(&ht, 20, 200);
    tiku_kits_ds_htable_put(&ht, 30, 300);
    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 3,
                "count is 3 before clear");

    rc = tiku_kits_ds_htable_clear(&ht);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear returns OK");
    TEST_ASSERT(tiku_kits_ds_htable_count(&ht) == 0,
                "count is 0 after clear");

    /* Previous keys no longer found */
    rc = tiku_kits_ds_htable_get(&ht, 10, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "key=10 not found after clear");

    TEST_ASSERT(tiku_kits_ds_htable_contains(&ht, 20) == 0,
                "key=20 not contained after clear");

    /* Can re-use table after clear */
    rc = tiku_kits_ds_htable_put(&ht, 50, 500);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "put after clear OK");
    tiku_kits_ds_htable_get(&ht, 50, &val);
    TEST_ASSERT(val == 500, "value after clear correct");

    TEST_GROUP_END("HTable Clear/Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER INPUTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_null_inputs(void)
{
    struct tiku_kits_ds_htable ht;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("HTable NULL Inputs");

    tiku_kits_ds_htable_init(&ht, 8);

    rc = tiku_kits_ds_htable_init(NULL, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    rc = tiku_kits_ds_htable_put(NULL, 1, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "put NULL rejected");

    rc = tiku_kits_ds_htable_get(NULL, 1, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "get NULL ht rejected");

    rc = tiku_kits_ds_htable_get(&ht, 1, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "get NULL value rejected");

    rc = tiku_kits_ds_htable_remove(NULL, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "remove NULL rejected");

    rc = tiku_kits_ds_htable_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear NULL rejected");

    TEST_ASSERT(tiku_kits_ds_htable_count(NULL) == 0,
                "count NULL returns 0");

    TEST_ASSERT(tiku_kits_ds_htable_contains(NULL, 1) == 0,
                "contains NULL returns 0");

    TEST_GROUP_END("HTable NULL Inputs");
}
