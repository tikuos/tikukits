/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_sm.c - State machine table (2D transition lookup)
 *
 * Platform-independent implementation of a table-driven state machine.
 * All storage is statically allocated; no heap usage.
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_ds_sm.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set all transitions to undefined (NO_TRANSITION, NULL).
 *
 * Iterates over the full compile-time table dimensions (MAX_STATES
 * x MAX_EVENTS), not just the runtime dimensions, to ensure no
 * stale entries remain from a previous configuration.  This is a
 * deliberate choice to keep the struct in a clean state regardless
 * of prior contents.
 */
static void clear_table(struct tiku_kits_ds_sm *sm)
{
    uint8_t s;
    uint8_t e;

    for (s = 0; s < TIKU_KITS_DS_SM_MAX_STATES; s++) {
        for (e = 0; e < TIKU_KITS_DS_SM_MAX_EVENTS; e++) {
            sm->table[s][e].next_state =
                TIKU_KITS_DS_SM_NO_TRANSITION;
            sm->table[s][e].action = NULL;
        }
    }
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a state machine with the given dimensions.
 *
 * Validates the runtime dimensions against the compile-time limits,
 * stores them, resets current_state to 0, and fills every table
 * cell with SM_NO_TRANSITION / NULL so that undefined transitions
 * are safely rejected.
 */
int tiku_kits_ds_sm_init(struct tiku_kits_ds_sm *sm,
                          uint8_t n_states,
                          uint8_t n_events)
{
    if (sm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (n_states == 0 || n_states > TIKU_KITS_DS_SM_MAX_STATES ||
        n_events == 0 || n_events > TIKU_KITS_DS_SM_MAX_EVENTS) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    sm->n_states = n_states;
    sm->n_events = n_events;
    sm->current_state = 0;
    clear_table(sm);

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* TRANSITION CONFIGURATION                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Define a transition in the state machine table.
 *
 * O(1) -- a direct array write into table[state][event].  All
 * three indices (state, event, next_state) are bounds-checked
 * against the runtime dimensions to prevent out-of-range writes.
 */
int tiku_kits_ds_sm_set_transition(struct tiku_kits_ds_sm *sm,
                                    uint8_t state,
                                    uint8_t event,
                                    uint8_t next_state,
                                    tiku_kits_ds_sm_action_t action)
{
    if (sm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (state >= sm->n_states || event >= sm->n_events ||
        next_state >= sm->n_states) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    sm->table[state][event].next_state = next_state;
    sm->table[state][event].action = action;

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* EVENT PROCESSING                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an event: look up transition and execute.
 *
 * O(1) lookup into table[current_state][event].  The action
 * callback is invoked *before* current_state is updated so that
 * the callback can inspect the source state if needed.  If the
 * cell holds SM_NO_TRANSITION, the machine state is left unchanged
 * and ERR_PARAM is returned.
 */
int tiku_kits_ds_sm_process(struct tiku_kits_ds_sm *sm,
                             uint8_t event)
{
    struct tiku_kits_ds_sm_transition *tr;

    if (sm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (event >= sm->n_events) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    tr = &sm->table[sm->current_state][event];

    if (tr->next_state == TIKU_KITS_DS_SM_NO_TRANSITION) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    /* Execute action callback before state change so the callback
     * can still query the source state if needed. */
    if (tr->action != NULL) {
        tr->action();
    }

    /* Transition to next state */
    sm->current_state = tr->next_state;

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* STATE ACCESS                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current state.
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the raw state index.
 */
uint8_t tiku_kits_ds_sm_get_state(
    const struct tiku_kits_ds_sm *sm)
{
    if (sm == NULL) {
        return 0;
    }
    return sm->current_state;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Force the current state to a given value.
 *
 * Directly overwrites current_state without invoking any
 * transition action.  The new state is bounds-checked against
 * n_states to prevent an invalid index.
 */
int tiku_kits_ds_sm_set_state(struct tiku_kits_ds_sm *sm,
                               uint8_t state)
{
    if (sm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (state >= sm->n_states) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    sm->current_state = state;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset current state to 0 (initial state).
 *
 * Leaves the transition table untouched -- only the current_state
 * field is set to 0.  No action callback is invoked.
 */
int tiku_kits_ds_sm_reset(struct tiku_kits_ds_sm *sm)
{
    if (sm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    sm->current_state = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all transitions and reset current state to 0.
 *
 * Resets current_state and fills every table cell with
 * SM_NO_TRANSITION / NULL.  The runtime dimensions (n_states,
 * n_events) are preserved so the caller can reconfigure
 * transitions without calling init again.
 */
int tiku_kits_ds_sm_clear(struct tiku_kits_ds_sm *sm)
{
    if (sm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    sm->current_state = 0;
    clear_table(sm);

    return TIKU_KITS_DS_OK;
}
