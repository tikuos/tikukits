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
 * Tracks local maxima in a signal stream. A peak is confirmed when
 * the signal drops by at least @c hysteresis below the running
 * maximum. After confirmation the detector waits for the signal to
 * rise by @c hysteresis above the running minimum before searching
 * for the next peak.
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
    tiku_kits_sigfeatures_elem_t hysteresis;  /**< Deadband threshold */
    tiku_kits_sigfeatures_elem_t extreme;     /**< Running max or min */
    tiku_kits_sigfeatures_elem_t last_peak;   /**< Value of last peak */
    uint16_t extreme_idx;   /**< Sample index of current extreme */
    uint16_t last_peak_idx; /**< Sample index of last confirmed peak */
    uint16_t sample_idx;    /**< Running sample counter */
    uint16_t peak_count;    /**< Total confirmed peaks */
    uint8_t state;          /**< RISING or FALLING */
    uint8_t detected;       /**< 1 if peak confirmed on last push */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a peak detector
 * @param p          Peak detector to initialize
 * @param hysteresis Deadband: signal must drop by this much below a
 *                   maximum to confirm it as a peak (must be > 0)
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_PARAM
 */
int tiku_kits_sigfeatures_peak_init(
    struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t hysteresis);

/**
 * @brief Reset a peak detector, clearing all state
 * @param p Peak detector to reset
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 */
int tiku_kits_sigfeatures_peak_reset(
    struct tiku_kits_sigfeatures_peak *p);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the peak detector
 * @param p     Peak detector
 * @param value Sample value
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 *
 * After this call, use tiku_kits_sigfeatures_peak_detected() to check
 * if a peak was confirmed on this sample.
 */
int tiku_kits_sigfeatures_peak_push(
    struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check if a peak was detected on the last push
 * @param p Peak detector
 * @return 1 if a peak was confirmed, 0 otherwise
 */
int tiku_kits_sigfeatures_peak_detected(
    const struct tiku_kits_sigfeatures_peak *p);

/**
 * @brief Get the value of the most recently confirmed peak
 * @param p      Peak detector
 * @param result Output: peak value
 * @return TIKU_KITS_SIGFEATURES_OK or
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA (no peaks yet)
 */
int tiku_kits_sigfeatures_peak_last_value(
    const struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t *result);

/**
 * @brief Get the sample index of the most recently confirmed peak
 * @param p      Peak detector
 * @param result Output: sample index (0-based)
 * @return TIKU_KITS_SIGFEATURES_OK or
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA (no peaks yet)
 */
int tiku_kits_sigfeatures_peak_last_index(
    const struct tiku_kits_sigfeatures_peak *p,
    uint16_t *result);

/**
 * @brief Get the total number of confirmed peaks
 * @param p Peak detector
 * @return Peak count, or 0 if p is NULL
 */
uint16_t tiku_kits_sigfeatures_peak_count(
    const struct tiku_kits_sigfeatures_peak *p);

#endif /* TIKU_KITS_SIGFEATURES_PEAK_H_ */
