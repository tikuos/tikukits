/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_zcr.h - Zero-crossing rate feature
 *
 * Counts sign changes in a sliding window of samples. Cheap to compute
 * and useful for classifying vibration/audio signals. Zero crossings
 * are tracked incrementally as samples are pushed, giving O(1)
 * per-sample cost.
 *
 * A zero crossing occurs when consecutive samples have different signs.
 * Values of zero are treated as non-negative.
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

#ifndef TIKU_KITS_SIGFEATURES_ZCR_H_
#define TIKU_KITS_SIGFEATURES_ZCR_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum window size (number of samples) for the ZCR tracker.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_SIGFEATURES_ZCR_MAX_WINDOW
#define TIKU_KITS_SIGFEATURES_ZCR_MAX_WINDOW 64
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_zcr
 * @brief Sliding-window zero-crossing rate tracker
 *
 * Maintains a circular buffer of samples and an incrementally
 * updated crossing count. When the window is full, the oldest
 * sample is evicted and the crossing count is adjusted.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_zcr z;
 *   tiku_kits_sigfeatures_zcr_init(&z, 8);
 *   tiku_kits_sigfeatures_zcr_push(&z, 10);
 *   tiku_kits_sigfeatures_zcr_push(&z, -5);
 *   tiku_kits_sigfeatures_zcr_push(&z, 3);
 *   // crossings = 2 (10->-5 and -5->3)
 * @endcode
 */
struct tiku_kits_sigfeatures_zcr {
    /** Circular sample buffer */
    tiku_kits_sigfeatures_elem_t
        buf[TIKU_KITS_SIGFEATURES_ZCR_MAX_WINDOW];
    uint16_t head;       /**< Next write position */
    uint16_t count;      /**< Current number of samples */
    uint16_t capacity;   /**< Window size (<= MAX_WINDOW) */
    uint16_t crossings;  /**< Current zero-crossing count */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a zero-crossing rate tracker
 * @param z      ZCR tracker to initialize
 * @param window Window size (2..TIKU_KITS_SIGFEATURES_ZCR_MAX_WINDOW)
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_SIZE
 */
int tiku_kits_sigfeatures_zcr_init(
    struct tiku_kits_sigfeatures_zcr *z,
    uint16_t window);

/**
 * @brief Reset a ZCR tracker, clearing all samples
 * @param z ZCR tracker to reset
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 */
int tiku_kits_sigfeatures_zcr_reset(
    struct tiku_kits_sigfeatures_zcr *z);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the ZCR tracker
 * @param z     ZCR tracker
 * @param value Sample value
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 *
 * Incrementally updates the crossing count in O(1). When the window
 * is full the oldest sample is evicted and its boundary crossing
 * (if any) is subtracted.
 */
int tiku_kits_sigfeatures_zcr_push(
    struct tiku_kits_sigfeatures_zcr *z,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current zero-crossing count
 * @param z ZCR tracker
 * @return Number of zero crossings in the window, or 0 if z is NULL
 */
uint16_t tiku_kits_sigfeatures_zcr_crossings(
    const struct tiku_kits_sigfeatures_zcr *z);

/**
 * @brief Get the current number of samples in the window
 * @param z ZCR tracker
 * @return Number of samples, or 0 if z is NULL
 */
uint16_t tiku_kits_sigfeatures_zcr_count(
    const struct tiku_kits_sigfeatures_zcr *z);

/**
 * @brief Check if the window is full
 * @param z ZCR tracker
 * @return 1 if full, 0 otherwise
 */
int tiku_kits_sigfeatures_zcr_full(
    const struct tiku_kits_sigfeatures_zcr *z);

#endif /* TIKU_KITS_SIGFEATURES_ZCR_H_ */
