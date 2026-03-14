/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_sm.h - State machine table (2D transition lookup)
 *
 * Platform-independent table-driven state machine for embedded systems.
 * Transitions are stored in a static 2D array indexed by
 * [state][event]. Each transition specifies a next state and an
 * optional action callback. All storage is statically allocated.
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

#ifndef TIKU_KITS_DS_SM_H_
#define TIKU_KITS_DS_SM_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of states in the state machine.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_SM_MAX_STATES
#define TIKU_KITS_DS_SM_MAX_STATES 8
#endif

/**
 * Maximum number of events the state machine can handle.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_SM_MAX_EVENTS
#define TIKU_KITS_DS_SM_MAX_EVENTS 8
#endif

/** Sentinel value indicating no transition defined */
#define TIKU_KITS_DS_SM_NO_TRANSITION 0xFF

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Action function pointer type, called on transition
 *
 * Action functions take no arguments and return nothing. They
 * are invoked by tiku_kits_ds_sm_process() before updating the
 * current state. A NULL action means no callback is executed.
 */
typedef void (*tiku_kits_ds_sm_action_t)(void);

/**
 * @struct tiku_kits_ds_sm_transition
 * @brief Single transition entry: next state + optional action
 *
 * If next_state is SM_NO_TRANSITION, the transition is undefined
 * and tiku_kits_ds_sm_process() will return ERR_PARAM.
 */
struct tiku_kits_ds_sm_transition {
    uint8_t next_state;             /**< Target state, or
                                         SM_NO_TRANSITION        */
    tiku_kits_ds_sm_action_t action; /**< Callback, or NULL      */
};

/**
 * @struct tiku_kits_ds_sm
 * @brief Table-driven state machine with static storage
 *
 * The transition table is a 2D array indexed by [state][event].
 * current_state tracks the active state. n_states and n_events
 * record the user-configured dimensions (must be <= the compile-
 * time maximums).
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_sm sm;
 *   tiku_kits_ds_sm_init(&sm, 3, 2);
 *   tiku_kits_ds_sm_set_transition(&sm, 0, 0, 1, my_action);
 *   tiku_kits_ds_sm_process(&sm, 0);  // state 0 -> 1
 * @endcode
 */
struct tiku_kits_ds_sm {
    struct tiku_kits_ds_sm_transition
        table[TIKU_KITS_DS_SM_MAX_STATES]
             [TIKU_KITS_DS_SM_MAX_EVENTS];
    uint8_t current_state;  /**< Active state index              */
    uint8_t n_states;       /**< Number of valid states          */
    uint8_t n_events;       /**< Number of valid events          */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a state machine with the given dimensions
 *
 * All transitions are set to SM_NO_TRANSITION with NULL action.
 * current_state is set to 0.
 *
 * @param sm       State machine to initialize
 * @param n_states Number of states (1..SM_MAX_STATES)
 * @param n_events Number of events (1..SM_MAX_EVENTS)
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_PARAM
 */
int tiku_kits_ds_sm_init(struct tiku_kits_ds_sm *sm,
                          uint8_t n_states,
                          uint8_t n_events);

/*---------------------------------------------------------------------------*/
/* TRANSITION CONFIGURATION                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Define a transition in the state machine table
 *
 * Sets table[state][event] = { next_state, action }.
 *
 * @param sm         State machine
 * @param state      Source state (must be < n_states)
 * @param event      Event index (must be < n_events)
 * @param next_state Target state (must be < n_states)
 * @param action     Action callback, or NULL for no action
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_PARAM
 */
int tiku_kits_ds_sm_set_transition(struct tiku_kits_ds_sm *sm,
                                    uint8_t state,
                                    uint8_t event,
                                    uint8_t next_state,
                                    tiku_kits_ds_sm_action_t action);

/*---------------------------------------------------------------------------*/
/* EVENT PROCESSING                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an event: look up transition and execute
 *
 * Looks up table[current_state][event]. If the transition is
 * defined (next_state != SM_NO_TRANSITION), the action callback
 * is called (if non-NULL), and current_state is updated.
 *
 * @param sm    State machine
 * @param event Event index (must be < n_events)
 * @return TIKU_KITS_DS_OK if transition executed,
 *         TIKU_KITS_DS_ERR_PARAM if no transition defined or
 *         invalid event, or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_sm_process(struct tiku_kits_ds_sm *sm,
                             uint8_t event);

/*---------------------------------------------------------------------------*/
/* STATE ACCESS                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current state
 * @param sm State machine
 * @return Current state index, or 0 if sm is NULL
 */
uint8_t tiku_kits_ds_sm_get_state(
    const struct tiku_kits_ds_sm *sm);

/**
 * @brief Force the current state to a given value
 * @param sm    State machine
 * @param state New state (must be < n_states)
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_PARAM
 */
int tiku_kits_ds_sm_set_state(struct tiku_kits_ds_sm *sm,
                               uint8_t state);

/**
 * @brief Reset current state to 0 (initial state)
 * @param sm State machine
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_sm_reset(struct tiku_kits_ds_sm *sm);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all transitions and reset current state to 0
 *
 * All table entries are set to SM_NO_TRANSITION with NULL action.
 *
 * @param sm State machine to clear
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_sm_clear(struct tiku_kits_ds_sm *sm);

#endif /* TIKU_KITS_DS_SM_H_ */
