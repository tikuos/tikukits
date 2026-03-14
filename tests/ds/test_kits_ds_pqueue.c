/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_pqueue.c - Priority queue tests
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
#include "tikukits/ds/pqueue/tiku_kits_ds_pqueue.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_init(void)
{
    struct tiku_kits_ds_pqueue pq;
    int rc;

    TEST_GROUP_BEGIN("PQueue Init");

    rc = tiku_kits_ds_pqueue_init(&pq, 3, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init 3 levels, 4 per level OK");
    TEST_ASSERT(tiku_kits_ds_pqueue_size(&pq) == 0, "size is 0 after init");
    TEST_ASSERT(tiku_kits_ds_pqueue_empty(&pq) == 1, "pqueue is empty");

    /* Max levels accepted */
    rc = tiku_kits_ds_pqueue_init(&pq,
                                  TIKU_KITS_DS_PQUEUE_MAX_LEVELS,
                                  TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init max levels/per_level OK");

    /* Zero levels rejected */
    rc = tiku_kits_ds_pqueue_init(&pq, 0, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM, "zero levels rejected");

    /* Zero per_level rejected */
    rc = tiku_kits_ds_pqueue_init(&pq, 3, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM, "zero per_level rejected");

    /* Oversized levels rejected */
    rc = tiku_kits_ds_pqueue_init(&pq,
                                  TIKU_KITS_DS_PQUEUE_MAX_LEVELS + 1, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized levels rejected");

    /* Oversized per_level rejected */
    rc = tiku_kits_ds_pqueue_init(&pq, 3,
                                  TIKU_KITS_DS_PQUEUE_MAX_PER_LEVEL + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized per_level rejected");

    TEST_GROUP_END("PQueue Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: BASIC PRIORITY                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_basic_priority(void)
{
    struct tiku_kits_ds_pqueue pq;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("PQueue Basic Priority");

    tiku_kits_ds_pqueue_init(&pq, 3, 4);

    /* Enqueue low priority first, then high priority */
    rc = tiku_kits_ds_pqueue_enqueue(&pq, 100, 2);  /* low */
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "enqueue priority 2 OK");

    rc = tiku_kits_ds_pqueue_enqueue(&pq, 999, 0);  /* high */
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "enqueue priority 0 OK");

    /* Dequeue should return high-priority item first */
    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 999, "dequeue returns highest priority (999)");

    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 100, "dequeue returns low priority (100)");

    TEST_GROUP_END("PQueue Basic Priority");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: MULTI-LEVEL                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_multi_level(void)
{
    struct tiku_kits_ds_pqueue pq;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("PQueue Multi-Level");

    tiku_kits_ds_pqueue_init(&pq, 4, 4);

    /* Fill multiple levels */
    tiku_kits_ds_pqueue_enqueue(&pq, 30, 3);  /* lowest */
    tiku_kits_ds_pqueue_enqueue(&pq, 10, 1);
    tiku_kits_ds_pqueue_enqueue(&pq, 20, 2);
    tiku_kits_ds_pqueue_enqueue(&pq, 00, 0);  /* highest */

    TEST_ASSERT(tiku_kits_ds_pqueue_size(&pq) == 4, "total size is 4");

    /* Dequeue order should be: 0 (pri 0), 10 (pri 1), 20 (pri 2),
     * 30 (pri 3) */
    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 0, "dequeue 1st: priority 0 value");

    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 10, "dequeue 2nd: priority 1 value");

    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 20, "dequeue 3rd: priority 2 value");

    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 30, "dequeue 4th: priority 3 value");

    /* Invalid priority rejected */
    rc = tiku_kits_ds_pqueue_enqueue(&pq, 99, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "enqueue invalid priority rejected");

    TEST_GROUP_END("PQueue Multi-Level");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: DEQUEUE ORDER (FIFO WITHIN SAME LEVEL)                            */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_dequeue_order(void)
{
    struct tiku_kits_ds_pqueue pq;
    tiku_kits_ds_elem_t val;

    TEST_GROUP_BEGIN("PQueue Dequeue Order");

    tiku_kits_ds_pqueue_init(&pq, 2, 4);

    /* Enqueue multiple items at same priority level */
    tiku_kits_ds_pqueue_enqueue(&pq, 11, 1);
    tiku_kits_ds_pqueue_enqueue(&pq, 12, 1);
    tiku_kits_ds_pqueue_enqueue(&pq, 13, 1);

    /* Enqueue one high-priority item */
    tiku_kits_ds_pqueue_enqueue(&pq, 1, 0);

    /* High priority comes first */
    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 1, "high priority dequeued first");

    /* Then level 1 items in FIFO order */
    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 11, "level 1 FIFO: first is 11");

    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 12, "level 1 FIFO: second is 12");

    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 13, "level 1 FIFO: third is 13");

    TEST_ASSERT(tiku_kits_ds_pqueue_empty(&pq) == 1,
                "empty after all dequeued");

    TEST_GROUP_END("PQueue Dequeue Order");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: PEEK                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_peek(void)
{
    struct tiku_kits_ds_pqueue pq;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("PQueue Peek");

    tiku_kits_ds_pqueue_init(&pq, 3, 4);

    /* Peek on empty pqueue */
    rc = tiku_kits_ds_pqueue_peek(&pq, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "peek on empty returns ERR_EMPTY");

    /* Add low priority, peek returns it */
    tiku_kits_ds_pqueue_enqueue(&pq, 50, 2);
    rc = tiku_kits_ds_pqueue_peek(&pq, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "peek returns OK");
    TEST_ASSERT(val == 50, "peek returns low-pri value 50");

    /* Add high priority, peek now returns that */
    tiku_kits_ds_pqueue_enqueue(&pq, 5, 0);
    tiku_kits_ds_pqueue_peek(&pq, &val);
    TEST_ASSERT(val == 5, "peek returns high-pri value 5");

    /* Peek does not remove */
    TEST_ASSERT(tiku_kits_ds_pqueue_size(&pq) == 2,
                "size unchanged after peek");

    TEST_GROUP_END("PQueue Peek");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: FULL LEVEL                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_full_level(void)
{
    struct tiku_kits_ds_pqueue pq;
    int rc;
    uint8_t i;

    TEST_GROUP_BEGIN("PQueue Full Level");

    tiku_kits_ds_pqueue_init(&pq, 2, 4);

    /* Fill level 0 to capacity */
    for (i = 0; i < 4; i++) {
        rc = tiku_kits_ds_pqueue_enqueue(&pq, (tiku_kits_ds_elem_t)i, 0);
        TEST_ASSERT(rc == TIKU_KITS_DS_OK, "enqueue to level 0 OK");
    }

    /* Level 0 is full */
    rc = tiku_kits_ds_pqueue_enqueue(&pq, 99, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "enqueue to full level 0 returns ERR_FULL");

    /* Level 1 still has room */
    rc = tiku_kits_ds_pqueue_enqueue(&pq, 99, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "enqueue to level 1 still OK");

    TEST_ASSERT(tiku_kits_ds_pqueue_size(&pq) == 5,
                "total size is 5 (4 + 1)");

    TEST_GROUP_END("PQueue Full Level");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: EMPTY CHECK                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_empty_check(void)
{
    struct tiku_kits_ds_pqueue pq;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("PQueue Empty Check");

    tiku_kits_ds_pqueue_init(&pq, 3, 4);

    TEST_ASSERT(tiku_kits_ds_pqueue_empty(&pq) == 1,
                "empty after init");

    tiku_kits_ds_pqueue_enqueue(&pq, 42, 1);
    TEST_ASSERT(tiku_kits_ds_pqueue_empty(&pq) == 0,
                "not empty after enqueue");

    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(tiku_kits_ds_pqueue_empty(&pq) == 1,
                "empty after dequeue last element");

    /* Dequeue from empty pqueue */
    rc = tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "dequeue from empty returns ERR_EMPTY");

    TEST_GROUP_END("PQueue Empty Check");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: CLEAR RESET                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_clear_reset(void)
{
    struct tiku_kits_ds_pqueue pq;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("PQueue Clear/Reset");

    tiku_kits_ds_pqueue_init(&pq, 3, 4);

    /* Populate multiple levels */
    tiku_kits_ds_pqueue_enqueue(&pq, 10, 0);
    tiku_kits_ds_pqueue_enqueue(&pq, 20, 1);
    tiku_kits_ds_pqueue_enqueue(&pq, 30, 2);
    TEST_ASSERT(tiku_kits_ds_pqueue_size(&pq) == 3,
                "size 3 before clear");

    /* Clear */
    rc = tiku_kits_ds_pqueue_clear(&pq);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear returns OK");
    TEST_ASSERT(tiku_kits_ds_pqueue_size(&pq) == 0,
                "size 0 after clear");
    TEST_ASSERT(tiku_kits_ds_pqueue_empty(&pq) == 1,
                "empty after clear");

    /* Dequeue after clear should fail */
    rc = tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "dequeue after clear returns ERR_EMPTY");

    /* Usable again after clear */
    rc = tiku_kits_ds_pqueue_enqueue(&pq, 777, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "enqueue after clear returns OK");

    tiku_kits_ds_pqueue_dequeue(&pq, &val);
    TEST_ASSERT(val == 777, "dequeued value after clear is 777");

    TEST_GROUP_END("PQueue Clear/Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL INPUTS                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_null_inputs(void)
{
    struct tiku_kits_ds_pqueue pq;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("PQueue NULL Inputs");

    tiku_kits_ds_pqueue_init(&pq, 3, 4);

    /* Init with NULL pointer */
    rc = tiku_kits_ds_pqueue_init(NULL, 3, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL, "init NULL rejected");

    /* Enqueue with NULL pqueue */
    rc = tiku_kits_ds_pqueue_enqueue(NULL, 42, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "enqueue NULL pq rejected");

    /* Dequeue with NULL pqueue */
    rc = tiku_kits_ds_pqueue_dequeue(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "dequeue NULL pq rejected");

    /* Dequeue with NULL value pointer */
    tiku_kits_ds_pqueue_enqueue(&pq, 1, 0);
    rc = tiku_kits_ds_pqueue_dequeue(&pq, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "dequeue NULL value rejected");

    /* Peek with NULL pqueue */
    rc = tiku_kits_ds_pqueue_peek(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "peek NULL pq rejected");

    /* Peek with NULL value pointer */
    rc = tiku_kits_ds_pqueue_peek(&pq, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "peek NULL value rejected");

    /* Clear with NULL */
    rc = tiku_kits_ds_pqueue_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL, "clear NULL rejected");

    /* Query functions with NULL */
    TEST_ASSERT(tiku_kits_ds_pqueue_size(NULL) == 0,
                "size NULL returns 0");
    TEST_ASSERT(tiku_kits_ds_pqueue_empty(NULL) == 1,
                "empty NULL returns 1");

    TEST_GROUP_END("PQueue NULL Inputs");
}
