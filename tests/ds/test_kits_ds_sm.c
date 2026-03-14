/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_sm.c - State machine table data structure tests
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
#include "tikukits/ds/sm/tiku_kits_ds_sm.h"

/*---------------------------------------------------------------------------*/
/* SHARED ACTION COUNTER                                                     */
/*---------------------------------------------------------------------------*/

static int action_counter;

static void action_increment(void)
{
    action_counter++;
}

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_init(void)
{
    struct tiku_kits_ds_sm sm;
    int rc;

    TEST_GROUP_BEGIN("SM Init");

    rc = tiku_kits_ds_sm_init(&sm, 4, 3);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init returns OK");
    TEST_ASSERT(sm.n_states == 4, "n_states is 4");
    TEST_ASSERT(sm.n_events == 3, "n_events is 3");
    TEST_ASSERT(sm.current_state == 0, "current_state is 0");

    /* Invalid dimensions */
    rc = tiku_kits_ds_sm_init(&sm, 0, 3);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "0 states rejected");

    rc = tiku_kits_ds_sm_init(&sm, 3, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "0 events rejected");

    rc = tiku_kits_ds_sm_init(&sm,
                               TIKU_KITS_DS_SM_MAX_STATES + 1,
                               2);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "too many states rejected");

    rc = tiku_kits_ds_sm_init(&sm, 2,
                               TIKU_KITS_DS_SM_MAX_EVENTS + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "too many events rejected");

    TEST_GROUP_END("SM Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: SET TRANSITION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_set_transition(void)
{
    struct tiku_kits_ds_sm sm;
    int rc;

    TEST_GROUP_BEGIN("SM Set Transition");

    tiku_kits_ds_sm_init(&sm, 3, 2);

    rc = tiku_kits_ds_sm_set_transition(&sm, 0, 0, 1, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "set_transition (0,0)->1 OK");

    rc = tiku_kits_ds_sm_set_transition(&sm, 1, 1, 2,
                                         action_increment);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "set_transition (1,1)->2 with action OK");

    /* Out-of-bounds state */
    rc = tiku_kits_ds_sm_set_transition(&sm, 3, 0, 0, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "source state out of range rejected");

    /* Out-of-bounds event */
    rc = tiku_kits_ds_sm_set_transition(&sm, 0, 2, 0, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "event out of range rejected");

    /* Out-of-bounds next_state */
    rc = tiku_kits_ds_sm_set_transition(&sm, 0, 0, 3, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "next_state out of range rejected");

    TEST_GROUP_END("SM Set Transition");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: PROCESS EVENT                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_process_event(void)
{
    struct tiku_kits_ds_sm sm;
    int rc;

    TEST_GROUP_BEGIN("SM Process Event");

    tiku_kits_ds_sm_init(&sm, 3, 2);

    /* State 0 --event 0--> State 1 */
    tiku_kits_ds_sm_set_transition(&sm, 0, 0, 1, NULL);
    /* State 1 --event 1--> State 2 */
    tiku_kits_ds_sm_set_transition(&sm, 1, 1, 2, NULL);

    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 0,
                "initial state is 0");

    rc = tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "process event 0 OK");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 1,
                "state is 1 after event 0");

    rc = tiku_kits_ds_sm_process(&sm, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "process event 1 OK");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 2,
                "state is 2 after event 1");

    TEST_GROUP_END("SM Process Event");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: ACTION CALLED                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_action_called(void)
{
    struct tiku_kits_ds_sm sm;
    int rc;

    TEST_GROUP_BEGIN("SM Action Called");

    tiku_kits_ds_sm_init(&sm, 2, 1);
    tiku_kits_ds_sm_set_transition(&sm, 0, 0, 1,
                                    action_increment);

    action_counter = 0;

    rc = tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "process returns OK");
    TEST_ASSERT(action_counter == 1,
                "action was called exactly once");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 1,
                "state updated after action");

    /* Process again with a self-loop to verify counter again */
    tiku_kits_ds_sm_set_transition(&sm, 1, 0, 1,
                                    action_increment);
    tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(action_counter == 2,
                "action called twice total");

    TEST_GROUP_END("SM Action Called");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: UNDEFINED TRANSITION                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_undefined_transition(void)
{
    struct tiku_kits_ds_sm sm;
    int rc;

    TEST_GROUP_BEGIN("SM Undefined Transition");

    tiku_kits_ds_sm_init(&sm, 3, 3);

    /* Only define one transition; others remain NO_TRANSITION */
    tiku_kits_ds_sm_set_transition(&sm, 0, 0, 1, NULL);

    /* Undefined transition: state 0, event 1 */
    rc = tiku_kits_ds_sm_process(&sm, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "undefined transition returns ERR_PARAM");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 0,
                "state unchanged on undefined transition");

    /* Invalid event index */
    rc = tiku_kits_ds_sm_process(&sm, 3);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "out-of-range event rejected");

    TEST_GROUP_END("SM Undefined Transition");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: STATE GET / SET                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_state_get_set(void)
{
    struct tiku_kits_ds_sm sm;
    int rc;

    TEST_GROUP_BEGIN("SM State Get Set");

    tiku_kits_ds_sm_init(&sm, 4, 2);

    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 0,
                "initial state is 0");

    rc = tiku_kits_ds_sm_set_state(&sm, 3);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "set_state 3 OK");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 3,
                "get_state returns 3");

    rc = tiku_kits_ds_sm_set_state(&sm, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "set_state 0 OK");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 0,
                "get_state returns 0");

    /* Out-of-bounds state */
    rc = tiku_kits_ds_sm_set_state(&sm, 4);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "set_state out of range rejected");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 0,
                "state unchanged after invalid set");

    TEST_GROUP_END("SM State Get Set");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: MULTI-STATE TRAVERSAL                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_multi_state(void)
{
    struct tiku_kits_ds_sm sm;

    TEST_GROUP_BEGIN("SM Multi State");

    tiku_kits_ds_sm_init(&sm, 4, 2);

    /* Build a chain: 0->1->2->3 on event 0 */
    tiku_kits_ds_sm_set_transition(&sm, 0, 0, 1, NULL);
    tiku_kits_ds_sm_set_transition(&sm, 1, 0, 2, NULL);
    tiku_kits_ds_sm_set_transition(&sm, 2, 0, 3, NULL);

    /* Event 1 loops back to 0 from any state */
    tiku_kits_ds_sm_set_transition(&sm, 1, 1, 0, NULL);
    tiku_kits_ds_sm_set_transition(&sm, 2, 1, 0, NULL);
    tiku_kits_ds_sm_set_transition(&sm, 3, 1, 0, NULL);

    /* Walk forward */
    tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 1,
                "step 1: state 1");

    tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 2,
                "step 2: state 2");

    tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 3,
                "step 3: state 3");

    /* Loop back */
    tiku_kits_ds_sm_process(&sm, 1);
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 0,
                "loop back to state 0");

    TEST_GROUP_END("SM Multi State");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: RESET / CLEAR                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_reset_clear(void)
{
    struct tiku_kits_ds_sm sm;
    int rc;

    TEST_GROUP_BEGIN("SM Reset Clear");

    tiku_kits_ds_sm_init(&sm, 3, 2);
    tiku_kits_ds_sm_set_transition(&sm, 0, 0, 1, NULL);
    tiku_kits_ds_sm_set_transition(&sm, 1, 0, 2, NULL);

    /* Move to state 2 */
    tiku_kits_ds_sm_process(&sm, 0);
    tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 2,
                "state is 2 before reset");

    /* Reset: state goes to 0 but transitions remain */
    rc = tiku_kits_ds_sm_reset(&sm);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 0,
                "state is 0 after reset");

    /* Transitions should still work */
    rc = tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "transition still works after reset");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 1,
                "state is 1 after process post-reset");

    /* Clear: state to 0 and transitions all removed */
    rc = tiku_kits_ds_sm_clear(&sm);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear returns OK");
    TEST_ASSERT(tiku_kits_ds_sm_get_state(&sm) == 0,
                "state is 0 after clear");

    rc = tiku_kits_ds_sm_process(&sm, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "transition gone after clear");

    TEST_GROUP_END("SM Reset Clear");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ds_sm_null_inputs(void)
{
    int rc;

    TEST_GROUP_BEGIN("SM NULL Inputs");

    rc = tiku_kits_ds_sm_init(NULL, 3, 2);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    rc = tiku_kits_ds_sm_set_transition(NULL, 0, 0, 1, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "set_transition NULL rejected");

    rc = tiku_kits_ds_sm_process(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "process NULL rejected");

    rc = tiku_kits_ds_sm_set_state(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "set_state NULL rejected");

    rc = tiku_kits_ds_sm_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "reset NULL rejected");

    rc = tiku_kits_ds_sm_clear(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear NULL rejected");

    TEST_ASSERT(tiku_kits_ds_sm_get_state(NULL) == 0,
                "get_state NULL returns 0");

    TEST_GROUP_END("SM NULL Inputs");
}
