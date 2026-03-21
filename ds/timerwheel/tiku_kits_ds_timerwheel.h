/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_timerwheel.h - Interval Timer Wheel
 *
 * Efficient O(N) timer scheduling structure for embedded systems,
 * where N is TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS (a small compile-time
 * constant).  Supports one-shot and periodic timers with tick-based
 * deadlines.
 *
 * Design:
 *   - Flat array of timer entries -- no linked lists, no heap.
 *   - All storage lives inside the struct; declare it as a static or
 *     local variable.
 *   - tick() advances the internal clock and marks expired timers in
 *     a bitmask.  expired() returns the fired timer IDs and handles
 *     one-shot removal and periodic rescheduling.
 *   - Timer IDs are 1-based (slot index + 1).  ID 0 is reserved to
 *     mean "unused".
 *
 * Typical usage:
 * @code
 *   tiku_kits_ds_timerwheel_t tw;
 *   tiku_kits_ds_timerwheel_init(&tw);
 *
 *   uint8_t id;
 *   tiku_kits_ds_timerwheel_add(&tw, 10, 0, &id);   // one-shot, 10 ticks
 *   tiku_kits_ds_timerwheel_add(&tw, 5, 5, &id);    // periodic, every 5
 *
 *   // In your main loop:
 *   tiku_kits_ds_timerwheel_tick(&tw);
 *   uint8_t fired[TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS];
 *   uint8_t count;
 *   tiku_kits_ds_timerwheel_expired(&tw, fired, sizeof(fired), &count);
 *   for (uint8_t i = 0; i < count; i++) {
 *       // handle fired[i]
 *   }
 * @endcode
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

#ifndef TIKU_KITS_DS_TIMERWHEEL_H_
#define TIKU_KITS_DS_TIMERWHEEL_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of concurrent timers.
 *
 * Each timer wheel instance statically allocates this many entry
 * slots.  Must be <= 8 so that the fired bitmask fits in a uint8_t.
 *
 * Override before including this header:
 * @code
 *   #define TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS 4
 *   #include "tiku_kits_ds_timerwheel.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS
#define TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_timerwheel_entry
 * @brief A single timer entry within the wheel.
 *
 * Each entry records an absolute deadline tick, a repeat interval
 * (zero for one-shot timers), a 1-based ID, and an active flag.
 */
typedef struct {
    uint16_t deadline;  /**< Absolute tick when this timer fires */
    uint16_t interval;  /**< Repeat interval in ticks (0 = one-shot) */
    uint8_t  id;        /**< Timer ID: slot index + 1 (0 = unused) */
    uint8_t  active;    /**< 1 if timer is active, 0 if free */
} tiku_kits_ds_timerwheel_entry_t;

/**
 * @struct tiku_kits_ds_timerwheel
 * @brief Interval timer wheel with flat static storage.
 *
 * Contains a flat array of timer entries, a monotonic tick counter,
 * the number of currently active timers, and a bitmask of timers
 * that fired during the most recent tick() call.
 */
typedef struct {
    /** Timer entry slots (statically allocated) */
    tiku_kits_ds_timerwheel_entry_t
        timers[TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS];
    uint16_t current_tick;  /**< Monotonic tick counter */
    uint8_t  num_active;    /**< Number of active timers */
    uint8_t  fired;         /**< Bitmask of timers that fired on
                                 the last tick() call */
} tiku_kits_ds_timerwheel_t;

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a timer wheel, clearing all timers.
 *
 * Resets the tick counter to zero, deactivates all timer slots, and
 * clears the fired bitmask.
 *
 * @param tw  Timer wheel to initialize (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if tw is NULL
 */
int tiku_kits_ds_timerwheel_init(tiku_kits_ds_timerwheel_t *tw);

/**
 * @brief Reset a timer wheel (identical to init).
 *
 * Convenience alias that clears all state exactly as init does.
 *
 * @param tw  Timer wheel to reset (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if tw is NULL
 */
int tiku_kits_ds_timerwheel_reset(tiku_kits_ds_timerwheel_t *tw);

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add a new timer to the wheel.
 *
 * Allocates the first free slot and schedules the timer to fire
 * after @p delay ticks from the current tick.  If @p interval is
 * non-zero the timer repeats every @p interval ticks after firing;
 * if zero the timer is one-shot and auto-removed after it fires.
 *
 * The assigned timer ID (1-based) is written to @p id_out.
 *
 * @param tw       Timer wheel (must not be NULL)
 * @param delay    Number of ticks until first firing (must be > 0)
 * @param interval Repeat interval in ticks (0 = one-shot)
 * @param id_out   Output: assigned timer ID (must not be NULL)
 *
 * @return TIKU_KITS_DS_OK on success
 * @return TIKU_KITS_DS_ERR_NULL if tw or id_out is NULL
 * @return TIKU_KITS_DS_ERR_PARAM if delay is 0
 * @return TIKU_KITS_DS_ERR_FULL if no free slots remain
 */
int tiku_kits_ds_timerwheel_add(tiku_kits_ds_timerwheel_t *tw,
                                uint16_t delay,
                                uint16_t interval,
                                uint8_t *id_out);

/**
 * @brief Cancel an active timer by ID.
 *
 * Deactivates the timer with the given ID.  The slot becomes
 * available for future add() calls.
 *
 * @param tw  Timer wheel (must not be NULL)
 * @param id  Timer ID to cancel (1-based)
 *
 * @return TIKU_KITS_DS_OK on success
 * @return TIKU_KITS_DS_ERR_NULL if tw is NULL
 * @return TIKU_KITS_DS_ERR_NOTFOUND if no active timer has this ID
 */
int tiku_kits_ds_timerwheel_cancel(tiku_kits_ds_timerwheel_t *tw,
                                   uint8_t id);

/*---------------------------------------------------------------------------*/
/* TICK PROCESSING                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Advance the wheel by one tick.
 *
 * Increments the internal tick counter and scans all active timers.
 * Any timer whose deadline matches the new tick is marked in an
 * internal fired bitmask.  Call expired() after tick() to retrieve
 * the fired timer IDs.
 *
 * @param tw  Timer wheel (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if tw is NULL
 */
int tiku_kits_ds_timerwheel_tick(tiku_kits_ds_timerwheel_t *tw);

/**
 * @brief Retrieve timer IDs that fired during the last tick().
 *
 * Iterates the internal fired bitmask and writes the IDs of expired
 * timers into @p ids (up to @p max_ids entries).  One-shot timers
 * are automatically deactivated; periodic timers are rescheduled to
 * fire again after their interval.  The fired bitmask is cleared.
 *
 * @param tw       Timer wheel (must not be NULL)
 * @param ids      Output array for fired timer IDs (must not be NULL)
 * @param max_ids  Capacity of @p ids array
 * @param count    Output: number of IDs written (must not be NULL)
 *
 * @return TIKU_KITS_DS_OK on success
 * @return TIKU_KITS_DS_ERR_NULL if tw, ids, or count is NULL
 */
int tiku_kits_ds_timerwheel_expired(tiku_kits_ds_timerwheel_t *tw,
                                    uint8_t *ids,
                                    uint8_t max_ids,
                                    uint8_t *count);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of active timers.
 *
 * Safe to call with a NULL pointer -- returns 0.
 *
 * @param tw  Timer wheel, or NULL
 * @return Number of active timers, or 0 if tw is NULL
 */
uint8_t tiku_kits_ds_timerwheel_active(
    const tiku_kits_ds_timerwheel_t *tw);

/**
 * @brief Return the current tick counter.
 *
 * Safe to call with a NULL pointer -- returns 0.
 *
 * @param tw  Timer wheel, or NULL
 * @return Current tick value, or 0 if tw is NULL
 */
uint16_t tiku_kits_ds_timerwheel_current_tick(
    const tiku_kits_ds_timerwheel_t *tw);

#endif /* TIKU_KITS_DS_TIMERWHEEL_H_ */
