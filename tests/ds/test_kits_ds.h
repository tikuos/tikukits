/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds.h - TikuKits DS test interface
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

#ifndef TEST_KITS_DS_H_
#define TEST_KITS_DS_H_

#include <stdint.h>
#include <string.h>
#include "tests/tiku_test_harness.h"
#include "tikukits/ds/tiku_kits_ds.h"
#include "tikukits/ds/array/tiku_kits_ds_array.h"
#include "tikukits/ds/ringbuf/tiku_kits_ds_ringbuf.h"
#include "tikukits/ds/stack/tiku_kits_ds_stack.h"
#include "tikukits/ds/queue/tiku_kits_ds_queue.h"
#include "tikukits/ds/pqueue/tiku_kits_ds_pqueue.h"
#include "tikukits/ds/list/tiku_kits_ds_list.h"
#include "tikukits/ds/htable/tiku_kits_ds_htable.h"
#include "tikukits/ds/bitmap/tiku_kits_ds_bitmap.h"
#include "tikukits/ds/sortarray/tiku_kits_ds_sortarray.h"
#include "tikukits/ds/btree/tiku_kits_ds_btree.h"
#include "tikukits/ds/sm/tiku_kits_ds_sm.h"

/*---------------------------------------------------------------------------*/
/* ARRAY TESTS                                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_array_init(void);
void test_kits_ds_array_push_pop(void);
void test_kits_ds_array_set_get(void);
void test_kits_ds_array_insert_remove(void);
void test_kits_ds_array_find(void);
void test_kits_ds_array_fill(void);
void test_kits_ds_array_bounds_check(void);
void test_kits_ds_array_clear_reset(void);
void test_kits_ds_array_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* RING BUFFER TESTS                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ds_ringbuf_init(void);
void test_kits_ds_ringbuf_push_pop(void);
void test_kits_ds_ringbuf_wraparound(void);
void test_kits_ds_ringbuf_peek(void);
void test_kits_ds_ringbuf_full_empty(void);
void test_kits_ds_ringbuf_overwrite_check(void);
void test_kits_ds_ringbuf_count_tracking(void);
void test_kits_ds_ringbuf_clear_reset(void);
void test_kits_ds_ringbuf_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* STACK TESTS                                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_stack_init(void);
void test_kits_ds_stack_push_pop(void);
void test_kits_ds_stack_peek(void);
void test_kits_ds_stack_overflow(void);
void test_kits_ds_stack_underflow(void);
void test_kits_ds_stack_size_tracking(void);
void test_kits_ds_stack_lifo_order(void);
void test_kits_ds_stack_clear_reset(void);
void test_kits_ds_stack_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* QUEUE TESTS                                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_queue_init(void);
void test_kits_ds_queue_enqueue_dequeue(void);
void test_kits_ds_queue_fifo_order(void);
void test_kits_ds_queue_peek(void);
void test_kits_ds_queue_full_empty(void);
void test_kits_ds_queue_wraparound(void);
void test_kits_ds_queue_size_tracking(void);
void test_kits_ds_queue_clear_reset(void);
void test_kits_ds_queue_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* PRIORITY QUEUE TESTS                                                      */
/*---------------------------------------------------------------------------*/

void test_kits_ds_pqueue_init(void);
void test_kits_ds_pqueue_basic_priority(void);
void test_kits_ds_pqueue_multi_level(void);
void test_kits_ds_pqueue_dequeue_order(void);
void test_kits_ds_pqueue_peek(void);
void test_kits_ds_pqueue_full_level(void);
void test_kits_ds_pqueue_empty_check(void);
void test_kits_ds_pqueue_clear_reset(void);
void test_kits_ds_pqueue_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* LINKED LIST TESTS                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ds_list_init(void);
void test_kits_ds_list_push_front_pop(void);
void test_kits_ds_list_push_back(void);
void test_kits_ds_list_insert_after(void);
void test_kits_ds_list_remove_node(void);
void test_kits_ds_list_find(void);
void test_kits_ds_list_traversal(void);
void test_kits_ds_list_clear_reset(void);
void test_kits_ds_list_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* HASH TABLE TESTS                                                          */
/*---------------------------------------------------------------------------*/

void test_kits_ds_htable_init(void);
void test_kits_ds_htable_put_get(void);
void test_kits_ds_htable_update_existing(void);
void test_kits_ds_htable_remove_tombstone(void);
void test_kits_ds_htable_contains(void);
void test_kits_ds_htable_collision_handling(void);
void test_kits_ds_htable_full_table(void);
void test_kits_ds_htable_clear_reset(void);
void test_kits_ds_htable_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* BITMAP TESTS                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_init(void);
void test_kits_ds_bitmap_set_clear_test(void);
void test_kits_ds_bitmap_toggle(void);
void test_kits_ds_bitmap_set_all_clear_all(void);
void test_kits_ds_bitmap_count_set_clear(void);
void test_kits_ds_bitmap_find_first_set(void);
void test_kits_ds_bitmap_find_first_clear(void);
void test_kits_ds_bitmap_boundary_bits(void);
void test_kits_ds_bitmap_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* SORTED ARRAY TESTS                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sortarray_init(void);
void test_kits_ds_sortarray_insert_sorted(void);
void test_kits_ds_sortarray_remove_element(void);
void test_kits_ds_sortarray_find_binary_search(void);
void test_kits_ds_sortarray_get_by_index(void);
void test_kits_ds_sortarray_min_max(void);
void test_kits_ds_sortarray_duplicates(void);
void test_kits_ds_sortarray_contains(void);
void test_kits_ds_sortarray_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* B-TREE TESTS                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_btree_init(void);
void test_kits_ds_btree_insert_single(void);
void test_kits_ds_btree_insert_multiple(void);
void test_kits_ds_btree_search_found(void);
void test_kits_ds_btree_search_not_found(void);
void test_kits_ds_btree_min_max(void);
void test_kits_ds_btree_split_root(void);
void test_kits_ds_btree_height(void);
void test_kits_ds_btree_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* STATE MACHINE TESTS                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_init(void);
void test_kits_ds_sm_set_transition(void);
void test_kits_ds_sm_process_event(void);
void test_kits_ds_sm_action_called(void);
void test_kits_ds_sm_undefined_transition(void);
void test_kits_ds_sm_state_get_set(void);
void test_kits_ds_sm_multi_state(void);
void test_kits_ds_sm_reset_clear(void);
void test_kits_ds_sm_null_inputs(void);

#endif /* TEST_KITS_DS_H_ */
