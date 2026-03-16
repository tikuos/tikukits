/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_peak.h - Peak detector with hysteresis
 *
 * Streaming peak detector that finds local maxima using a configurable
 * deadband (hysteresis). Essential for heartbeat/pulse detection, step
 * counting, and any application that needs to identify signal peaks
 * while rejecting noise.
 *
 * The detector uses a two-state machine:
 *   RISING  -- tracking the running maximum; a peak is confirmed when
 *              the signal drops by more than the hysteresis threshold
 *   FALLING -- tracking the running minimum; a new rise begins when
 *              the signal climbs by more than the hysteresis threshold
 *
 * All storage is statically allocated; no heap required.
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

#ifndef TIKU_KITS_SIGFEATURES_PEAK_H_
#define TIKU_KITS_SIGFEATURES_PEAK_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_SIGFEATURES_PEAK_STATE Peak Detector States
 * @{ */
#define TIKU_KITS_SIGFEATURES_PEAK_RISING   0  /**< Tracking maximum */
#define TIKU_KITS_SIGFEATURES_PEAK_FALLING  1  /**< Tracking minimum */
/** @} */

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_peak
 * @brief Streaming peak detector with hysteresis deadband
 *
 * Tracks local maxima in a signal stream using a two-state machine:
 *
 *   - RISING: the detector tracks the running maximum.  When the
 *     signal drops by more than @c hysteresis below that maximum,
 *     a peak is confirmed at the maximum's value and sample index.
 *   - FALLING: the detector tracks the running minimum.  When the
 *     signal rises by more than @c hysteresis above that minimum,
 *     the detector switches back to RISING and begins looking for
 *     the next peak.
 *
 * The hysteresis deadband prevents noise-induced false peaks: small
 * fluctuations around a local maximum are ignored unless the signal
 * moves far enough to be considered a genuine reversal.
 *
 * All storage is contained within the struct -- no pointers, no
 * heap -- so it can be declared as a static or local variable.
 *
 * @note The detected flag is valid only for the most recent push.
 *       It is cleared at the start of every push() call, so the
 *       caller must check it before the next push.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_peak p;
 *   tiku_kits_sigfeatures_peak_init(&p, 10);  // hysteresis = 10
 *
 *   for (i = 0; i < n; i++) {
 *       tiku_kits_sigfeatures_peak_push(&p, samples[i]);
 *       if (tiku_kits_sigfeatures_peak_detected(&p)) {
 *           // peak at last_value(), index last_index()
 *       }
 *   }
 * @endcode
 */
struct tiku_kits_sigfeatures_peak {
    tiku_kits_sigfeatures_elem_t hysteresis;  /**< Deadband threshold (always > 0) */
    tiku_kits_sigfeatures_elem_t extreme;     /**< Running max (RISING) or min (FALLING) */
    tiku_kits_sigfeatures_elem_t last_peak;   /**< Value of the most recently confirmed peak */
    uint16_t extreme_idx;   /**< Sample index where the current extreme was seen */
    uint16_t last_peak_idx; /**< Sample index of the most recently confirmed peak */
    uint16_t sample_idx;    /**< Monotonically increasing sample counter */
    uint16_t peak_count;    /**< Total number of confirmed peaks so far */
    uint8_t state;          /**< TIKU_KITS_SIGFEATURES_PEAK_RISING or _FALLING */
    uint8_t detected;       /**< 1 if a peak was confirmed on the most recent push */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a peak detector with the given hysteresis
 *
 * Sets the deadband threshold and resets all internal state (extreme,
 * counters, sample index, state machine) to their initial values.
 * The detector starts in the RISING state, ready to track a maximum.
 *
 * @param p          Peak detector to initialize (must not be NULL)
 * @param hysteresis Deadband threshold: the signal must drop by at
 *                   least this much below a running maximum to
 *                   confirm it as a peak (must be > 0)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if p is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if hysteresis <= 0
 */
int tiku_kits_sigfeatures_peak_init(
    struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t hysteresis);

/**
 * @brief Reset a peak detector, clearing all accumulated state
 *
 * Returns the detector to its initial post-init condition: RISING
 * state, all counters zeroed, no peaks recorded.  The hysteresis
 * value is preserved from init().
 *
 * @param p Peak detector to reset (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if p is NULL
 */
int tiku_kits_sigfeatures_peak_reset(
    struct tiku_kits_sigfeatures_peak *p);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the peak detector
 *
 * Updates the state machine with the new sample value.  In the
 * RISING state the running maximum is tracked; in the FALLING
 * state the running minimum is tracked.  State transitions occur
 * when the signal deviates from the current extreme by more than
 * the hysteresis threshold.  O(1) per call.
 *
 * The @c detected flag is set to 1 if a peak was confirmed on
 * this push and 0 otherwise.  The flag is only valid until the
 * next push() call.
 *
 * @param p     Peak detector (must not be NULL)
 * @param value New sample value
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if p is NULL
 */
int tiku_kits_sigfeatures_peak_push(
    struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check if a peak was detected on the most recent push
 *
 * Returns the value of the detected flag set by the last push()
 * call.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param p Peak detector, or NULL
 * @return 1 if a peak was confirmed on the last push, 0 otherwise
 *         (including NULL)
 */
int tiku_kits_sigfeatures_peak_detected(
    const struct tiku_kits_sigfeatures_peak *p);

/**
 * @brief Get the value of the most recently confirmed peak
 *
 * Copies the amplitude of the last confirmed peak into the
 * caller-provided location.  Fails if no peaks have been
 * confirmed yet (peak_count == 0).
 *
 * @param p      Peak detector (must not be NULL)
 * @param result Output pointer where the peak amplitude is written
 *               (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if p or result is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA if no peaks have been
 *         confirmed yet
 */
int tiku_kits_sigfeatures_peak_last_value(
    const struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t *result);

/**
 * @brief Get the sample index of the most recently confirmed peak
 *
 * Returns the 0-based sample index at which the last confirmed
 * peak was observed.  This is the index of the running maximum
 * that was later confirmed by a sufficient drop, not the index of
 * the sample that triggered the confirmation.
 *
 * @param p      Peak detector (must not be NULL)
 * @param result Output pointer where the sample index is written
 *               (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if p or result is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA if no peaks have been
 *         confirmed yet
 */
int tiku_kits_sigfeatures_peak_last_index(
    const struct tiku_kits_sigfeatures_peak *p,
    uint16_t *result);

/**
 * @brief Get the total number of confirmed peaks
 *
 * Returns the cumulative count of peaks confirmed since
 * initialization (or the last reset).  Safe to call with a NULL
 * pointer -- returns 0.
 *
 * @param p Peak detector, or NULL
 * @return Number of confirmed peaks, or 0 if p is NULL
 */
uint16_t tiku_kits_sigfeatures_peak_count(
    const struct tiku_kits_sigfeatures_peak *p);

#endif /* TIKU_KITS_SIGFEATURES_PEAK_H_ */
