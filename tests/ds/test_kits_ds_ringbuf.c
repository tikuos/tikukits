/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_ringbuf.c - Ring buffer data structure tests
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
#include "tikukits/ds/ringbuf/tiku_kits_ds_ringbuf.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_init(void)
{
    struct tiku_kits_ds_ringbuf rb;
    int rc;

    TEST_GROUP_BEGIN("RingBuf Init");

    rc = tiku_kits_ds_ringbuf_init(&rb, 16);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init cap=16 returns OK");
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 0,
                "count is 0 after init");
    TEST_ASSERT(tiku_kits_ds_ringbuf_empty(&rb) == 1,
                "buffer is empty after init");
    TEST_ASSERT(tiku_kits_ds_ringbuf_full(&rb) == 0,
                "buffer is not full after init");

    /* Max capacity accepted */
    rc = tiku_kits_ds_ringbuf_init(&rb,
                                    TIKU_KITS_DS_RINGBUF_MAX_SIZE);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init max capacity OK");

    /* Zero capacity rejected */
    rc = tiku_kits_ds_ringbuf_init(&rb, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "zero capacity rejected");

    /* Oversized capacity rejected */
    rc = tiku_kits_ds_ringbuf_init(
        &rb, TIKU_KITS_DS_RINGBUF_MAX_SIZE + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized capacity rejected");

    TEST_GROUP_END("RingBuf Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: PUSH AND POP                                                      */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_push_pop(void)
{
    struct tiku_kits_ds_ringbuf rb;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("RingBuf Push/Pop");

    tiku_kits_ds_ringbuf_init(&rb, 4);

    /* Push elements */
    rc = tiku_kits_ds_ringbuf_push(&rb, 10);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 10 OK");
    rc = tiku_kits_ds_ringbuf_push(&rb, 20);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 20 OK");
    rc = tiku_kits_ds_ringbuf_push(&rb, 30);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 30 OK");
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 3,
                "count is 3 after 3 pushes");

    /* Pop in FIFO order */
    rc = tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "pop OK");
    TEST_ASSERT(val == 10, "first pop returns 10 (FIFO)");

    rc = tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "second pop OK");
    TEST_ASSERT(val == 20, "second pop returns 20");

    rc = tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "third pop OK");
    TEST_ASSERT(val == 30, "third pop returns 30");

    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 0,
                "count is 0 after all pops");

    /* Pop from empty buffer */
    rc = tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "pop empty returns ERR_EMPTY");

    TEST_GROUP_END("RingBuf Push/Pop");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: WRAPAROUND                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_wraparound(void)
{
    struct tiku_kits_ds_ringbuf rb;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("RingBuf Wraparound");

    tiku_kits_ds_ringbuf_init(&rb, 4);

    /* Fill buffer: [10, 20, 30, 40] */
    tiku_kits_ds_ringbuf_push(&rb, 10);
    tiku_kits_ds_ringbuf_push(&rb, 20);
    tiku_kits_ds_ringbuf_push(&rb, 30);
    tiku_kits_ds_ringbuf_push(&rb, 40);

    /* Pop two: head advances past index 1 */
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 10, "pop 10");
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 20, "pop 20");

    /* Push two more: tail wraps around */
    rc = tiku_kits_ds_ringbuf_push(&rb, 50);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 50 wraps OK");
    rc = tiku_kits_ds_ringbuf_push(&rb, 60);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push 60 wraps OK");

    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 4,
                "count is 4 after wraparound");
    TEST_ASSERT(tiku_kits_ds_ringbuf_full(&rb) == 1,
                "buffer is full after wraparound");

    /* Pop all: order should be 30, 40, 50, 60 */
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 30, "wrapped pop returns 30");
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 40, "wrapped pop returns 40");
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 50, "wrapped pop returns 50");
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 60, "wrapped pop returns 60");

    TEST_ASSERT(tiku_kits_ds_ringbuf_empty(&rb) == 1,
                "buffer empty after full drain");

    TEST_GROUP_END("RingBuf Wraparound");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: PEEK                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_peek(void)
{
    struct tiku_kits_ds_ringbuf rb;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("RingBuf Peek");

    tiku_kits_ds_ringbuf_init(&rb, 4);

    /* Peek on empty buffer */
    rc = tiku_kits_ds_ringbuf_peek(&rb, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "peek empty returns ERR_EMPTY");

    /* Push and peek */
    tiku_kits_ds_ringbuf_push(&rb, 42);
    rc = tiku_kits_ds_ringbuf_peek(&rb, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "peek returns OK");
    TEST_ASSERT(val == 42, "peek value is 42");

    /* Peek does not remove element */
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 1,
                "count unchanged after peek");

    /* Push another, peek still sees head (oldest) */
    tiku_kits_ds_ringbuf_push(&rb, 99);
    tiku_kits_ds_ringbuf_peek(&rb, &val);
    TEST_ASSERT(val == 42, "peek still returns oldest (42)");

    /* Pop head, peek sees next */
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    tiku_kits_ds_ringbuf_peek(&rb, &val);
    TEST_ASSERT(val == 99, "peek returns 99 after pop");

    TEST_GROUP_END("RingBuf Peek");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: FULL AND EMPTY STATE                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_full_empty(void)
{
    struct tiku_kits_ds_ringbuf rb;
    tiku_kits_ds_elem_t val;
    int rc;
    uint16_t i;

    TEST_GROUP_BEGIN("RingBuf Full/Empty");

    tiku_kits_ds_ringbuf_init(&rb, 4);

    TEST_ASSERT(tiku_kits_ds_ringbuf_empty(&rb) == 1,
                "empty at start");
    TEST_ASSERT(tiku_kits_ds_ringbuf_full(&rb) == 0,
                "not full at start");

    /* Fill to capacity */
    for (i = 0; i < 4; i++) {
        tiku_kits_ds_ringbuf_push(&rb, (tiku_kits_ds_elem_t)i);
    }

    TEST_ASSERT(tiku_kits_ds_ringbuf_full(&rb) == 1,
                "full at capacity");
    TEST_ASSERT(tiku_kits_ds_ringbuf_empty(&rb) == 0,
                "not empty at capacity");

    /* Push when full fails */
    rc = tiku_kits_ds_ringbuf_push(&rb, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "push when full returns ERR_FULL");

    /* Pop one: no longer full */
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(tiku_kits_ds_ringbuf_full(&rb) == 0,
                "not full after pop");
    TEST_ASSERT(tiku_kits_ds_ringbuf_empty(&rb) == 0,
                "not empty after one pop");

    TEST_GROUP_END("RingBuf Full/Empty");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: OVERWRITE CHECK (NO SILENT OVERWRITE)                             */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_overwrite_check(void)
{
    struct tiku_kits_ds_ringbuf rb;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("RingBuf Overwrite Check");

    tiku_kits_ds_ringbuf_init(&rb, 3);

    /* Fill buffer */
    tiku_kits_ds_ringbuf_push(&rb, 10);
    tiku_kits_ds_ringbuf_push(&rb, 20);
    tiku_kits_ds_ringbuf_push(&rb, 30);

    /* Attempt to push -- must fail, not silently overwrite */
    rc = tiku_kits_ds_ringbuf_push(&rb, 40);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_FULL,
                "push on full returns error (no overwrite)");

    /* Original data intact */
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 10, "oldest element 10 preserved");
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 20, "element 20 preserved");
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(val == 30, "element 30 preserved");

    TEST_GROUP_END("RingBuf Overwrite Check");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: COUNT TRACKING                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_count_tracking(void)
{
    struct tiku_kits_ds_ringbuf rb;
    tiku_kits_ds_elem_t val;

    TEST_GROUP_BEGIN("RingBuf Count Tracking");

    tiku_kits_ds_ringbuf_init(&rb, 8);

    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 0,
                "initial count is 0");

    tiku_kits_ds_ringbuf_push(&rb, 1);
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 1,
                "count is 1 after push");

    tiku_kits_ds_ringbuf_push(&rb, 2);
    tiku_kits_ds_ringbuf_push(&rb, 3);
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 3,
                "count is 3 after 3 pushes");

    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 2,
                "count is 2 after pop");

    tiku_kits_ds_ringbuf_pop(&rb, &val);
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 0,
                "count is 0 after all pops");

    /* Push/pop cycle with wraparound */
    tiku_kits_ds_ringbuf_push(&rb, 10);
    tiku_kits_ds_ringbuf_push(&rb, 20);
    tiku_kits_ds_ringbuf_pop(&rb, &val);
    tiku_kits_ds_ringbuf_push(&rb, 30);
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 2,
                "count is 2 after mixed ops");

    TEST_GROUP_END("RingBuf Count Tracking");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: CLEAR AND RESET                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_clear_reset(void)
{
    struct tiku_kits_ds_ringbuf rb;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("RingBuf Clear/Reset");

    tiku_kits_ds_ringbuf_init(&rb, 8);
    tiku_kits_ds_ringbuf_push(&rb, 10);
    tiku_kits_ds_ringbuf_push(&rb, 20);
    tiku_kits_ds_ringbuf_push(&rb, 30);

    rc = tiku_kits_ds_ringbuf_clear(&rb);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear returns OK");
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 0,
                "count is 0 after clear");
    TEST_ASSERT(tiku_kits_ds_ringbuf_empty(&rb) == 1,
                "buffer is empty after clear");

    /* Pop from cleared buffer fails */
    rc = tiku_kits_ds_ringbuf_pop(&rb, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_EMPTY,
                "pop after clear returns ERR_EMPTY");

    /* Can push after clear */
    rc = tiku_kits_ds_ringbuf_push(&rb, 99);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "push after clear OK");
    tiku_kits_ds_ringbuf_peek(&rb, &val);
    TEST_ASSERT(val == 99, "pushed value is 99");

    /* Re-init resets everything */
    rc = tiku_kits_ds_ringbuf_init(&rb, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "re-init returns OK");
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(&rb) == 0,
                "count is 0 after re-init");

    TEST_GROUP_END("RingBuf Clear/Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER INPUTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_null_inputs(void)
{
    struct tiku_kits_ds_ringbuf rb;
    tiku_kits_ds_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("RingBuf NULL Inputs");

    /* init with NULL */
    rc = tiku_kits_ds_ringbuf_init(NULL, 8);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    /* push with NULL */
    rc = tiku_kits_ds_ringbuf_push(NULL, 42);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "push NULL rejected");

    /* pop with NULL buffer */
    rc = tiku_kits_ds_ringbuf_pop(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "pop NULL buffer rejected");

    /* pop with NULL value */
    tiku_kits_ds_ringbuf_init(&rb, 4);
    tiku_kits_ds_ringbuf_push(&rb, 10);
    rc = tiku_kits_ds_ringbuf_pop(&rb, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "pop NULL value rejected");

    /* peek with NULL buffer */
    rc = tiku_kits_ds_ringbuf_peek(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "peek NULL buffer rejected");

    /* peek with NULL value */
    rc = tiku_kits_ds_ringbuf_peek(&rb, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "peek NULL value rejected");

    /* clear with NULL */
    rc = tiku_kits_ds_ringbuf_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear NULL rejected");

    /* count with NULL returns 0 */
    TEST_ASSERT(tiku_kits_ds_ringbuf_count(NULL) == 0,
                "count NULL returns 0");

    /* full with NULL returns 0 */
    TEST_ASSERT(tiku_kits_ds_ringbuf_full(NULL) == 0,
                "full NULL returns 0");

    /* empty with NULL returns 0 */
    TEST_ASSERT(tiku_kits_ds_ringbuf_empty(NULL) == 0,
                "empty NULL returns 0");

    TEST_GROUP_END("RingBuf NULL Inputs");
}
