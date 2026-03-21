/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_timerwheel.c - Interval Timer Wheel implementation
 *
 * Flat-array timer wheel with O(N) tick scanning, where N is
 * TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS (a small compile-time constant,
 * default 8).  No heap allocation; all storage is statically allocated
 * inside the timer wheel struct.
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

#include "tiku_kits_ds_timerwheel.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a timer wheel, clearing all timers.
 *
 * Zeros the entire timer array via memset so that all slots start
 * inactive with deterministic field values.  The tick counter and
 * fired bitmask are also cleared.
 */
int tiku_kits_ds_timerwheel_init(tiku_kits_ds_timerwheel_t *tw)
{
    if (tw == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    memset(tw->timers, 0, sizeof(tw->timers));
    tw->current_tick = 0;
    tw->num_active = 0;
    tw->fired = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset a timer wheel (identical to init).
 *
 * Delegates directly to init for a full reset of all state.
 */
int tiku_kits_ds_timerwheel_reset(tiku_kits_ds_timerwheel_t *tw)
{
    return tiku_kits_ds_timerwheel_init(tw);
}

/*---------------------------------------------------------------------------*/
/* TIMER MANAGEMENT                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add a new timer to the wheel.
 *
 * Scans the timer array for the first inactive slot.  Sets the
 * deadline to current_tick + delay, records the repeat interval,
 * assigns a 1-based ID (slot index + 1), and marks the slot active.
 */
int tiku_kits_ds_timerwheel_add(tiku_kits_ds_timerwheel_t *tw,
                                uint16_t delay,
                                uint16_t interval,
                                uint8_t *id_out)
{
    uint8_t i;

    if (tw == NULL || id_out == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (delay == 0) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    /* Find the first free slot. */
    for (i = 0; i < TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS; i++) {
        if (tw->timers[i].active == 0) {
            tw->timers[i].deadline = (uint16_t)(
                tw->current_tick + delay);
            tw->timers[i].interval = interval;
            tw->timers[i].id = (uint8_t)(i + 1);
            tw->timers[i].active = 1;
            tw->num_active++;
            *id_out = tw->timers[i].id;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_FULL;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Cancel an active timer by ID.
 *
 * Looks up the timer by converting the 1-based ID to a slot index.
 * If the slot is active and its ID matches, the timer is deactivated.
 */
int tiku_kits_ds_timerwheel_cancel(tiku_kits_ds_timerwheel_t *tw,
                                   uint8_t id)
{
    uint8_t idx;

    if (tw == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (id == 0 || id > TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    idx = (uint8_t)(id - 1);
    if (tw->timers[idx].active == 0 || tw->timers[idx].id != id) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    tw->timers[idx].active = 0;
    tw->timers[idx].id = 0;
    tw->num_active--;

    /* Also clear the fired bit in case it was marked this tick but
     * not yet retrieved via expired(). */
    tw->fired &= (uint8_t)~(1U << idx);

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* TICK PROCESSING                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Advance the wheel by one tick.
 *
 * Increments the tick counter, then scans all active timers.  Any
 * timer whose deadline matches the new tick is marked in the fired
 * bitmask (bit position = slot index).  The bitmask is cleared at
 * the start so that only timers firing on this tick are reported.
 */
int tiku_kits_ds_timerwheel_tick(tiku_kits_ds_timerwheel_t *tw)
{
    uint8_t i;

    if (tw == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    tw->current_tick++;
    tw->fired = 0;

    for (i = 0; i < TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS; i++) {
        if (tw->timers[i].active &&
            tw->timers[i].deadline == tw->current_tick) {
            tw->fired |= (uint8_t)(1U << i);
        }
    }

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Retrieve timer IDs that fired during the last tick().
 *
 * Iterates the fired bitmask.  For each set bit the corresponding
 * timer ID is written to @p ids.  One-shot timers (interval == 0)
 * are deactivated and their slot freed.  Periodic timers have their
 * deadline advanced by their interval for the next firing.  The
 * fired bitmask is cleared when done.
 */
int tiku_kits_ds_timerwheel_expired(tiku_kits_ds_timerwheel_t *tw,
                                    uint8_t *ids,
                                    uint8_t max_ids,
                                    uint8_t *count)
{
    uint8_t i;
    uint8_t n;

    if (tw == NULL || ids == NULL || count == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    n = 0;
    for (i = 0; i < TIKU_KITS_DS_TIMERWHEEL_MAX_TIMERS; i++) {
        if (tw->fired & (1U << i)) {
            /* Record the fired ID if there is room. */
            if (n < max_ids) {
                ids[n] = tw->timers[i].id;
            }
            n++;

            if (tw->timers[i].interval == 0) {
                /* One-shot: deactivate the timer. */
                tw->timers[i].active = 0;
                tw->timers[i].id = 0;
                tw->num_active--;
            } else {
                /* Periodic: reschedule for the next interval. */
                tw->timers[i].deadline = (uint16_t)(
                    tw->timers[i].deadline +
                    tw->timers[i].interval);
            }
        }
    }

    /* Report the number of fired timers (may exceed max_ids if the
     * caller's buffer was too small, but we still count them all). */
    *count = n;
    tw->fired = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of active timers.
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint8_t tiku_kits_ds_timerwheel_active(
    const tiku_kits_ds_timerwheel_t *tw)
{
    if (tw == NULL) {
        return 0;
    }
    return tw->num_active;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current tick counter.
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint16_t tiku_kits_ds_timerwheel_current_tick(
    const tiku_kits_ds_timerwheel_t *tw)
{
    if (tw == NULL) {
        return 0;
    }
    return tw->current_tick;
}
