/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_queue.c - FIFO queue tests
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
#include "tikukits/ds/queue/tiku_kits_ds_queue.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_init(void)
{
    struct tiku_kits_ds_queue q;
    int rc;

    TEST_GROUP_BEGIN("Queue Init");

    rc = tiku_kits_ds_queue_init(&q, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init capacity 8 returns OK");
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 0, "size is 0 after init");
    TEST_ASSERT(tiku_kits_ds_queue_empty(&q) == 1, "queue is empty");
    TEST_ASSERT(tiku_kits_ds_queue_full(&q) == 0, "queue is not full");

    /* Max capacity accepted */
    rc = tiku_kits_ds_queue_init(&q, TIKU_KITS_DS_QUEUE_MAX_SIZE);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init max capacity returns OK");

    /* Zero capacity rejected */
    rc = tiku_kits_ds_queue_init(&q, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM, "zero capacity rejected");

    /* Oversized capacity rejected */
    rc = tiku_kits_ds_queue_init(&q, TIKU_KITS_DS_QUEUE_MAX_SIZE + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized capacity rejected");

    TEST_GROUP_END("Queue Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: ENQUEUE AND DEQUEUE                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_enqueue_dequeue(void)
{
    struct tiku_kits_ds_queue q;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Queue Enqueue/Dequeue");

    tiku_kits_ds_queue_init(&q, 4);

    /* Enqueue a single element */
    rc = tiku_kits_ds_queue_enqueue(&q, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "enqueue 42 returns OK");
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 1, "size is 1");

    /* Dequeue it back */
    rc = tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "dequeue returns OK");
    TEST_ASSERT(val == 42, "dequeued value is 42");
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 0, "size is 0 after dequeue");

    /* Dequeue from empty queue */
    rc = tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "dequeue from empty returns ERR_EMPTY");

    TEST_GROUP_END("Queue Enqueue/Dequeue");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: FIFO ORDER                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_fifo_order(void)
{
    struct tiku_kits_ds_queue q;
    tiku_kits_ds_elem_t val;

    TEST_GROUP_BEGIN("Queue FIFO Order");

    tiku_kits_ds_queue_init(&q, 8);

    /* Enqueue 10, 20, 30 */
    tiku_kits_ds_queue_enqueue(&q, 10);
    tiku_kits_ds_queue_enqueue(&q, 20);
    tiku_kits_ds_queue_enqueue(&q, 30);

    /* Dequeue should return 10, 20, 30 in order */
    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 10, "first dequeue is 10");

    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 20, "second dequeue is 20");

    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 30, "third dequeue is 30");

    TEST_ASSERT(tiku_kits_ds_queue_empty(&q) == 1,
                "queue empty after all dequeued");

    TEST_GROUP_END("Queue FIFO Order");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: PEEK                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_peek(void)
{
    struct tiku_kits_ds_queue q;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Queue Peek");

    tiku_kits_ds_queue_init(&q, 4);

    /* Peek on empty queue */
    rc = tiku_kits_ds_queue_peek(&q, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "peek on empty returns ERR_EMPTY");

    /* Enqueue and peek */
    tiku_kits_ds_queue_enqueue(&q, 77);
    rc = tiku_kits_ds_queue_peek(&q, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "peek returns OK");
    TEST_ASSERT(val == 77, "peeked value is 77");

    /* Peek does not remove the element */
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 1,
                "size unchanged after peek");

    /* Peek again gives the same value */
    tiku_kits_ds_queue_peek(&q, &val);
    TEST_ASSERT(val == 77, "second peek still returns 77");

    /* Enqueue another, peek still returns head */
    tiku_kits_ds_queue_enqueue(&q, 88);
    tiku_kits_ds_queue_peek(&q, &val);
    TEST_ASSERT(val == 77, "peek returns head after second enqueue");

    TEST_GROUP_END("Queue Peek");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: FULL AND EMPTY                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_full_empty(void)
{
    struct tiku_kits_ds_queue q;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Queue Full/Empty");

    tiku_kits_ds_queue_init(&q, 3);

    TEST_ASSERT(tiku_kits_ds_queue_empty(&q) == 1, "empty after init");
    TEST_ASSERT(tiku_kits_ds_queue_full(&q) == 0, "not full after init");

    /* Fill to capacity */
    tiku_kits_ds_queue_enqueue(&q, 1);
    tiku_kits_ds_queue_enqueue(&q, 2);
    tiku_kits_ds_queue_enqueue(&q, 3);

    TEST_ASSERT(tiku_kits_ds_queue_full(&q) == 1, "full at capacity");
    TEST_ASSERT(tiku_kits_ds_queue_empty(&q) == 0, "not empty when full");

    /* Enqueue on full queue */
    rc = tiku_kits_ds_queue_enqueue(&q, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "enqueue on full returns ERR_FULL");

    /* Dequeue one -- no longer full */
    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(tiku_kits_ds_queue_full(&q) == 0,
                "not full after one dequeue");
    TEST_ASSERT(tiku_kits_ds_queue_empty(&q) == 0,
                "not empty with 2 elements");

    TEST_GROUP_END("Queue Full/Empty");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: WRAPAROUND                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_wraparound(void)
{
    struct tiku_kits_ds_queue q;
    tiku_kits_ds_elem_t val;

    TEST_GROUP_BEGIN("Queue Wraparound");

    tiku_kits_ds_queue_init(&q, 4);

    /* Fill queue: 10, 20, 30, 40 */
    tiku_kits_ds_queue_enqueue(&q, 10);
    tiku_kits_ds_queue_enqueue(&q, 20);
    tiku_kits_ds_queue_enqueue(&q, 30);
    tiku_kits_ds_queue_enqueue(&q, 40);

    /* Dequeue 2 elements to advance head */
    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 10, "first dequeue is 10");
    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 20, "second dequeue is 20");

    /* Enqueue 2 more -- tail wraps around */
    tiku_kits_ds_queue_enqueue(&q, 50);
    tiku_kits_ds_queue_enqueue(&q, 60);

    TEST_ASSERT(tiku_kits_ds_queue_full(&q) == 1,
                "full after wraparound enqueue");

    /* Dequeue all -- verify correct FIFO order */
    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 30, "wrapped dequeue 1 is 30");

    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 40, "wrapped dequeue 2 is 40");

    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 50, "wrapped dequeue 3 is 50");

    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 60, "wrapped dequeue 4 is 60");

    TEST_ASSERT(tiku_kits_ds_queue_empty(&q) == 1,
                "empty after full wraparound");

    TEST_GROUP_END("Queue Wraparound");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: SIZE TRACKING                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_size_tracking(void)
{
    struct tiku_kits_ds_queue q;
    tiku_kits_ds_elem_t val;

    TEST_GROUP_BEGIN("Queue Size Tracking");

    tiku_kits_ds_queue_init(&q, 8);

    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 0, "size 0 after init");

    tiku_kits_ds_queue_enqueue(&q, 1);
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 1, "size 1 after enqueue");

    tiku_kits_ds_queue_enqueue(&q, 2);
    tiku_kits_ds_queue_enqueue(&q, 3);
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 3, "size 3 after 3 enqueues");

    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 2,
                "size 2 after one dequeue");

    tiku_kits_ds_queue_dequeue(&q, &val);
    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 0,
                "size 0 after all dequeued");

    /* Interleaved enqueue/dequeue */
    tiku_kits_ds_queue_enqueue(&q, 10);
    tiku_kits_ds_queue_enqueue(&q, 20);
    tiku_kits_ds_queue_dequeue(&q, &val);
    tiku_kits_ds_queue_enqueue(&q, 30);
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 2,
                "size 2 after interleaved ops");

    TEST_GROUP_END("Queue Size Tracking");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: CLEAR RESET                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_clear_reset(void)
{
    struct tiku_kits_ds_queue q;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Queue Clear/Reset");

    tiku_kits_ds_queue_init(&q, 8);

    /* Fill some elements */
    tiku_kits_ds_queue_enqueue(&q, 100);
    tiku_kits_ds_queue_enqueue(&q, 200);
    tiku_kits_ds_queue_enqueue(&q, 300);

    /* Clear resets the queue */
    rc = tiku_kits_ds_queue_clear(&q);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear returns OK");
    TEST_ASSERT(tiku_kits_ds_queue_size(&q) == 0, "size 0 after clear");
    TEST_ASSERT(tiku_kits_ds_queue_empty(&q) == 1, "empty after clear");

    /* Dequeue after clear should fail */
    rc = tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "dequeue after clear returns ERR_EMPTY");

    /* Queue is usable again after clear */
    rc = tiku_kits_ds_queue_enqueue(&q, 999);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "enqueue after clear returns OK");

    tiku_kits_ds_queue_dequeue(&q, &val);
    TEST_ASSERT(val == 999, "dequeued value after clear is 999");

    TEST_GROUP_END("Queue Clear/Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL INPUTS                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_null_inputs(void)
{
    struct tiku_kits_ds_queue q;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("Queue NULL Inputs");

    tiku_kits_ds_queue_init(&q, 4);

    /* Init with NULL pointer */
    rc = tiku_kits_ds_queue_init(NULL, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL, "init NULL rejected");

    /* Enqueue with NULL queue */
    rc = tiku_kits_ds_queue_enqueue(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL, "enqueue NULL q rejected");

    /* Dequeue with NULL queue */
    rc = tiku_kits_ds_queue_dequeue(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL, "dequeue NULL q rejected");

    /* Dequeue with NULL value pointer */
    tiku_kits_ds_queue_enqueue(&q, 1);
    rc = tiku_kits_ds_queue_dequeue(&q, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "dequeue NULL value rejected");

    /* Peek with NULL queue */
    rc = tiku_kits_ds_queue_peek(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL, "peek NULL q rejected");

    /* Peek with NULL value pointer */
    rc = tiku_kits_ds_queue_peek(&q, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "peek NULL value rejected");

    /* Clear with NULL */
    rc = tiku_kits_ds_queue_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL, "clear NULL rejected");

    /* Query functions with NULL */
    TEST_ASSERT(tiku_kits_ds_queue_size(NULL) == 0,
                "size NULL returns 0");
    TEST_ASSERT(tiku_kits_ds_queue_full(NULL) == 0,
                "full NULL returns 0");
    TEST_ASSERT(tiku_kits_ds_queue_empty(NULL) == 1,
                "empty NULL returns 1");

    TEST_GROUP_END("Queue NULL Inputs");
}
