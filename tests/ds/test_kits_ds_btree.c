/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_btree.c - B-Tree data structure tests
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
#include "tikukits/ds/btree/tiku_kits_ds_btree.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_init(void)
{
    struct tiku_kits_ds_btree bt;
    int rc;

    TEST_GROUP_BEGIN("BTree Init");

    rc = tiku_kits_ds_btree_init(&bt);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init returns OK");
    TEST_ASSERT(bt.root == TIKU_KITS_DS_BTREE_NONE,
                "root is NONE after init");
    TEST_ASSERT(bt.n_nodes == 0, "n_nodes is 0 after init");
    TEST_ASSERT(bt.next_free == 0, "next_free is 0 after init");
    TEST_ASSERT(tiku_kits_ds_btree_count(&bt) == 0,
                "count is 0 after init");
    TEST_ASSERT(tiku_kits_ds_btree_height(&bt) == 0,
                "height is 0 after init");

    TEST_GROUP_END("BTree Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: INSERT SINGLE                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_insert_single(void)
{
    struct tiku_kits_ds_btree bt;
    int rc;

    TEST_GROUP_BEGIN("BTree Insert Single");

    tiku_kits_ds_btree_init(&bt);

    rc = tiku_kits_ds_btree_insert(&bt, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert 42 returns OK");
    TEST_ASSERT(tiku_kits_ds_btree_count(&bt) == 1,
                "count is 1 after single insert");
    TEST_ASSERT(bt.root != TIKU_KITS_DS_BTREE_NONE,
                "root is not NONE after insert");

    rc = tiku_kits_ds_btree_search(&bt, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "search 42 finds it");

    TEST_GROUP_END("BTree Insert Single");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: INSERT MULTIPLE                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_insert_multiple(void)
{
    struct tiku_kits_ds_btree bt;
    int rc;

    TEST_GROUP_BEGIN("BTree Insert Multiple");

    tiku_kits_ds_btree_init(&bt);

    rc = tiku_kits_ds_btree_insert(&bt, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert 10 OK");

    rc = tiku_kits_ds_btree_insert(&bt, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert 20 OK");

    rc = tiku_kits_ds_btree_insert(&bt, 5);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert 5 OK");

    rc = tiku_kits_ds_btree_insert(&bt, 15);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert 15 OK");

    rc = tiku_kits_ds_btree_insert(&bt, 25);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert 25 OK");

    rc = tiku_kits_ds_btree_insert(&bt, 30);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert 30 OK");

    rc = tiku_kits_ds_btree_insert(&bt, 35);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "insert 35 OK");

    TEST_ASSERT(tiku_kits_ds_btree_count(&bt) == 7,
                "count is 7 after 7 inserts");

    /* Verify all keys can be found */
    TEST_ASSERT(tiku_kits_ds_btree_search(&bt, 10) ==
                TIKU_KITS_DS_OK, "search 10 found");
    TEST_ASSERT(tiku_kits_ds_btree_search(&bt, 20) ==
                TIKU_KITS_DS_OK, "search 20 found");
    TEST_ASSERT(tiku_kits_ds_btree_search(&bt, 5) ==
                TIKU_KITS_DS_OK, "search 5 found");
    TEST_ASSERT(tiku_kits_ds_btree_search(&bt, 15) ==
                TIKU_KITS_DS_OK, "search 15 found");
    TEST_ASSERT(tiku_kits_ds_btree_search(&bt, 25) ==
                TIKU_KITS_DS_OK, "search 25 found");
    TEST_ASSERT(tiku_kits_ds_btree_search(&bt, 30) ==
                TIKU_KITS_DS_OK, "search 30 found");
    TEST_ASSERT(tiku_kits_ds_btree_search(&bt, 35) ==
                TIKU_KITS_DS_OK, "search 35 found");

    TEST_GROUP_END("BTree Insert Multiple");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: SEARCH FOUND                                                      */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_search_found(void)
{
    struct tiku_kits_ds_btree bt;
    int rc;

    TEST_GROUP_BEGIN("BTree Search Found");

    tiku_kits_ds_btree_init(&bt);
    tiku_kits_ds_btree_insert(&bt, 100);
    tiku_kits_ds_btree_insert(&bt, 200);
    tiku_kits_ds_btree_insert(&bt, 300);

    rc = tiku_kits_ds_btree_search(&bt, 100);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "search 100 returns OK");

    rc = tiku_kits_ds_btree_search(&bt, 200);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "search 200 returns OK");

    rc = tiku_kits_ds_btree_search(&bt, 300);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "search 300 returns OK");

    TEST_GROUP_END("BTree Search Found");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: SEARCH NOT FOUND                                                  */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_search_not_found(void)
{
    struct tiku_kits_ds_btree bt;
    int rc;

    TEST_GROUP_BEGIN("BTree Search Not Found");

    tiku_kits_ds_btree_init(&bt);

    /* Search empty tree */
    rc = tiku_kits_ds_btree_search(&bt, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "search empty tree returns ERR_EMPTY");

    /* Search populated tree for missing key */
    tiku_kits_ds_btree_insert(&bt, 10);
    tiku_kits_ds_btree_insert(&bt, 20);
    tiku_kits_ds_btree_insert(&bt, 30);

    rc = tiku_kits_ds_btree_search(&bt, 15);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "search 15 not found");

    rc = tiku_kits_ds_btree_search(&bt, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "search 99 not found");

    rc = tiku_kits_ds_btree_search(&bt, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "search 1 not found");

    TEST_GROUP_END("BTree Search Not Found");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: MIN / MAX                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_min_max(void)
{
    struct tiku_kits_ds_btree bt;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("BTree Min Max");

    tiku_kits_ds_btree_init(&bt);

    /* Min/max on empty tree */
    rc = tiku_kits_ds_btree_min(&bt, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "min on empty returns ERR_EMPTY");

    rc = tiku_kits_ds_btree_max(&bt, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "max on empty returns ERR_EMPTY");

    /* Populate tree */
    tiku_kits_ds_btree_insert(&bt, 50);
    tiku_kits_ds_btree_insert(&bt, 20);
    tiku_kits_ds_btree_insert(&bt, 80);
    tiku_kits_ds_btree_insert(&bt, 10);
    tiku_kits_ds_btree_insert(&bt, 60);
    tiku_kits_ds_btree_insert(&bt, 90);
    tiku_kits_ds_btree_insert(&bt, 5);

    rc = tiku_kits_ds_btree_min(&bt, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "min returns OK");
    TEST_ASSERT(val == 5, "min value is 5");

    rc = tiku_kits_ds_btree_max(&bt, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "max returns OK");
    TEST_ASSERT(val == 90, "max value is 90");

    TEST_GROUP_END("BTree Min Max");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: SPLIT ROOT                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_split_root(void)
{
    struct tiku_kits_ds_btree bt;
    int rc;
    uint8_t i;

    /*
     * With ORDER=3, MAX_KEYS=5. Inserting 6+ keys forces at
     * least one root split. Insert 10 keys to force multiple
     * splits and verify all remain searchable.
     */
    static const tiku_kits_ds_elem_t keys[] = {
        10, 20, 30, 40, 50, 60, 70, 80, 90, 100
    };
    static const uint8_t n = sizeof(keys) / sizeof(keys[0]);

    TEST_GROUP_BEGIN("BTree Split Root");

    tiku_kits_ds_btree_init(&bt);

    for (i = 0; i < n; i++) {
        rc = tiku_kits_ds_btree_insert(&bt, keys[i]);
        TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                    "insert ascending key OK");
    }

    TEST_ASSERT(tiku_kits_ds_btree_count(&bt) == n,
                "count matches inserted keys");

    /* Verify all keys are still findable after splits */
    for (i = 0; i < n; i++) {
        rc = tiku_kits_ds_btree_search(&bt, keys[i]);
        TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                    "key still found after splits");
    }

    /* Height must be > 1 since root was split */
    TEST_ASSERT(tiku_kits_ds_btree_height(&bt) > 1,
                "height > 1 after root split");

    TEST_GROUP_END("BTree Split Root");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: HEIGHT                                                            */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_height(void)
{
    struct tiku_kits_ds_btree bt;

    TEST_GROUP_BEGIN("BTree Height");

    tiku_kits_ds_btree_init(&bt);

    /* Empty tree: height 0 */
    TEST_ASSERT(tiku_kits_ds_btree_height(&bt) == 0,
                "empty tree height is 0");

    /* Single key: height 1 */
    tiku_kits_ds_btree_insert(&bt, 42);
    TEST_ASSERT(tiku_kits_ds_btree_height(&bt) == 1,
                "single node height is 1");

    /* Fill root (5 keys for ORDER=3) -- still height 1 */
    tiku_kits_ds_btree_insert(&bt, 10);
    tiku_kits_ds_btree_insert(&bt, 20);
    tiku_kits_ds_btree_insert(&bt, 50);
    tiku_kits_ds_btree_insert(&bt, 60);
    TEST_ASSERT(tiku_kits_ds_btree_height(&bt) == 1,
                "full root (5 keys) still height 1");

    /* 6th key forces split -- height becomes 2 */
    tiku_kits_ds_btree_insert(&bt, 70);
    TEST_ASSERT(tiku_kits_ds_btree_height(&bt) == 2,
                "height 2 after root split");

    TEST_GROUP_END("BTree Height");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_null_inputs(void)
{
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("BTree NULL Inputs");

    rc = tiku_kits_ds_btree_init(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    rc = tiku_kits_ds_btree_insert(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "insert NULL rejected");

    rc = tiku_kits_ds_btree_search(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "search NULL rejected");

    rc = tiku_kits_ds_btree_min(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "min NULL bt rejected");

    rc = tiku_kits_ds_btree_max(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "max NULL bt rejected");

    {
        struct tiku_kits_ds_btree bt;
        tiku_kits_ds_btree_init(&bt);
        tiku_kits_ds_btree_insert(&bt, 10);

        rc = tiku_kits_ds_btree_min(&bt, NULL);
        TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                    "min NULL value rejected");

        rc = tiku_kits_ds_btree_max(&bt, NULL);
        TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                    "max NULL value rejected");
    }

    rc = tiku_kits_ds_btree_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear NULL rejected");

    TEST_ASSERT(tiku_kits_ds_btree_count(NULL) == 0,
                "count NULL returns 0");

    TEST_ASSERT(tiku_kits_ds_btree_height(NULL) == 0,
                "height NULL returns 0");

    TEST_GROUP_END("BTree NULL Inputs");
}
