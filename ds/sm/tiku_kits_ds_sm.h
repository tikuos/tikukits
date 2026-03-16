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
 * @brief Maximum number of states in the state machine.
 *
 * This compile-time constant defines the upper bound on the number
 * of distinct states any state machine instance can have.  The 2D
 * transition table reserves MAX_STATES rows, so choose a value that
 * balances memory usage against the largest state machine your
 * application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_SM_MAX_STATES 16
 *   #include "tiku_kits_ds_sm.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_SM_MAX_STATES
#define TIKU_KITS_DS_SM_MAX_STATES 8
#endif

/**
 * @brief Maximum number of events the state machine can handle.
 *
 * This compile-time constant defines the upper bound on the number
 * of distinct event types.  The 2D transition table reserves
 * MAX_EVENTS columns per state, so choose a value that balances
 * memory usage against the number of event types your application
 * requires.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_SM_MAX_EVENTS 16
 *   #include "tiku_kits_ds_sm.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_SM_MAX_EVENTS
#define TIKU_KITS_DS_SM_MAX_EVENTS 8
#endif

/**
 * @brief Sentinel value indicating no transition defined.
 *
 * When a table cell's next_state equals this value, the
 * transition is considered undefined and tiku_kits_ds_sm_process()
 * returns TIKU_KITS_DS_ERR_PARAM instead of performing a state
 * change.  This allows sparse transition tables where only the
 * valid (state, event) pairs need to be explicitly configured.
 */
#define TIKU_KITS_DS_SM_NO_TRANSITION 0xFF

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Action function pointer type, called on transition.
 *
 * Action functions take no arguments and return nothing.  They
 * are invoked by tiku_kits_ds_sm_process() *before* the current
 * state is updated to the next state, so within the callback
 * the machine still reports the source state.  A NULL action
 * pointer means no callback is executed for that transition.
 */
typedef void (*tiku_kits_ds_sm_action_t)(void);

/**
 * @struct tiku_kits_ds_sm_transition
 * @brief Single transition entry: next state + optional action.
 *
 * Each cell in the 2D transition table holds one of these entries.
 * If @c next_state equals TIKU_KITS_DS_SM_NO_TRANSITION, the
 * transition is considered undefined -- tiku_kits_ds_sm_process()
 * will return TIKU_KITS_DS_ERR_PARAM and leave the state unchanged.
 * Otherwise, the @c action callback (if non-NULL) is invoked and
 * then the machine moves to @c next_state.
 */
struct tiku_kits_ds_sm_transition {
    uint8_t next_state;             /**< Target state, or
                                         SM_NO_TRANSITION        */
    tiku_kits_ds_sm_action_t action; /**< Callback, or NULL      */
};

/**
 * @struct tiku_kits_ds_sm
 * @brief Table-driven state machine with static storage.
 *
 * A general-purpose finite state machine that stores transitions
 * in a 2D array indexed by [state][event].  Because all storage
 * lives inside the struct itself (the table, counters, and current
 * state), no heap allocation is needed -- just declare the state
 * machine as a static or local variable.
 *
 * Two dimension limits are tracked independently:
 *   - @c n_states -- the runtime number of states passed to init
 *     (must be <= TIKU_KITS_DS_SM_MAX_STATES).  This lets
 *     different state machine instances use different logical
 *     sizes while sharing the same compile-time backing table.
 *   - @c n_events -- the runtime number of event types passed to
 *     init (must be <= TIKU_KITS_DS_SM_MAX_EVENTS).
 *
 * @c current_state tracks the active state index, starting at 0
 * after initialization.  All transition lookups are O(1) since
 * the table is directly indexed.
 *
 * @note The transition table uses SM_NO_TRANSITION as a sentinel,
 *       which limits states to 254 (0..0xFE) per instance.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_sm sm;
 *   tiku_kits_ds_sm_init(&sm, 3, 2);
 *   tiku_kits_ds_sm_set_transition(&sm, 0, 0, 1, my_action);
 *   tiku_kits_ds_sm_set_transition(&sm, 1, 1, 2, NULL);
 *   tiku_kits_ds_sm_process(&sm, 0);  // state 0 -> 1, my_action()
 *   tiku_kits_ds_sm_process(&sm, 1);  // state 1 -> 2, no action
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
 * @brief Initialize a state machine with the given dimensions.
 *
 * Resets the state machine to an empty state: current_state is set
 * to 0 and every cell in the transition table is filled with
 * SM_NO_TRANSITION / NULL action, so that undefined transitions
 * are safely rejected by tiku_kits_ds_sm_process().
 *
 * @param sm       State machine to initialize (must not be NULL)
 * @param n_states Number of states (1..SM_MAX_STATES)
 * @param n_events Number of event types (1..SM_MAX_EVENTS)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sm is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if n_states or n_events is 0 or
 *         exceeds the compile-time maximum
 */
int tiku_kits_ds_sm_init(struct tiku_kits_ds_sm *sm,
                          uint8_t n_states,
                          uint8_t n_events);

/*---------------------------------------------------------------------------*/
/* TRANSITION CONFIGURATION                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Define a transition in the state machine table.
 *
 * Writes the pair { @p next_state, @p action } into the table cell
 * at [state][event].  This is an O(1) operation -- a direct array
 * write.  Previously defined transitions at the same cell are
 * silently overwritten.
 *
 * @param sm         State machine (must not be NULL)
 * @param state      Source state index (must be < n_states)
 * @param event      Event index (must be < n_events)
 * @param next_state Target state index (must be < n_states)
 * @param action     Action callback invoked on transition, or NULL
 *                   if no callback is needed
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sm is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if state, event, or next_state
 *         is out of range
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
 * @brief Process an event: look up transition and execute.
 *
 * Performs an O(1) lookup in table[current_state][event].  If the
 * cell holds a valid transition (next_state != SM_NO_TRANSITION),
 * the action callback is called first (if non-NULL), and then
 * current_state is updated to the target state.  If the cell is
 * undefined, the state remains unchanged and an error is returned.
 *
 * @param sm    State machine (must not be NULL)
 * @param event Event index (must be < n_events)
 * @return TIKU_KITS_DS_OK if the transition was executed,
 *         TIKU_KITS_DS_ERR_NULL if sm is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if event is out of range or
 *         no transition is defined for the (current_state, event)
 *         pair
 */
int tiku_kits_ds_sm_process(struct tiku_kits_ds_sm *sm,
                             uint8_t event);

/*---------------------------------------------------------------------------*/
/* STATE ACCESS                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current state.
 *
 * Returns the active state index.  Safe to call with a NULL
 * pointer -- returns 0.
 *
 * @param sm State machine, or NULL
 * @return Current state index, or 0 if sm is NULL
 */
uint8_t tiku_kits_ds_sm_get_state(
    const struct tiku_kits_ds_sm *sm);

/**
 * @brief Force the current state to a given value.
 *
 * Directly overwrites current_state without firing any transition
 * action.  Useful for testing or for implementing external reset
 * logic that needs to jump to a specific state.
 *
 * @param sm    State machine (must not be NULL)
 * @param state New state index (must be < n_states)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sm is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if state >= n_states
 */
int tiku_kits_ds_sm_set_state(struct tiku_kits_ds_sm *sm,
                               uint8_t state);

/**
 * @brief Reset current state to 0 (initial state).
 *
 * Equivalent to tiku_kits_ds_sm_set_state(sm, 0) but without
 * the bounds check since state 0 is always valid.  The
 * transition table is left untouched.
 *
 * @param sm State machine (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sm is NULL
 */
int tiku_kits_ds_sm_reset(struct tiku_kits_ds_sm *sm);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all transitions and reset current state to 0.
 *
 * Every cell in the transition table is set to SM_NO_TRANSITION
 * with a NULL action, and current_state is reset to 0.  The
 * runtime dimensions (n_states, n_events) are preserved so the
 * caller can immediately reconfigure transitions without calling
 * init again.
 *
 * @param sm State machine to clear (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if sm is NULL
 */
int tiku_kits_ds_sm_clear(struct tiku_kits_ds_sm *sm);

#endif /* TIKU_KITS_DS_SM_H_ */
