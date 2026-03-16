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
 * @brief Maximum window size (number of samples) for the ZCR tracker.
 *
 * This compile-time constant defines the upper bound on the
 * sliding-window capacity.  Each ZCR instance reserves this many
 * element slots in its circular buffer, so choose a value that
 * balances memory usage against the longest window your
 * application needs.  64 samples is a common default for
 * vibration / audio classification on MSP430.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_SIGFEATURES_ZCR_MAX_WINDOW 128
 *   #include "tiku_kits_sigfeatures_zcr.h"
 * @endcode
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
 * Counts sign changes between consecutive samples inside a
 * fixed-length sliding window.  The crossing count is maintained
 * incrementally: each push() adds at most one crossing (between
 * the new sample and its predecessor) and, when the window is
 * full, subtracts the crossing that is lost by evicting the
 * oldest sample.  This gives O(1) per-sample cost with zero
 * extra RAM beyond the circular buffer.
 *
 * A zero crossing is defined as two consecutive samples having
 * different signs.  Zero is treated as non-negative: a transition
 * from a negative value to zero (or vice versa) counts as a
 * crossing; a transition between zero and a positive value does
 * not.
 *
 * @note The window must contain at least 2 samples for the
 *       crossing count to be meaningful (a single sample has no
 *       predecessor).
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
    /** Circular sample buffer (statically sized to MAX_WINDOW) */
    tiku_kits_sigfeatures_elem_t
        buf[TIKU_KITS_SIGFEATURES_ZCR_MAX_WINDOW];
    uint16_t head;       /**< Next write position in the circular buffer */
    uint16_t count;      /**< Number of samples currently in the window */
    uint16_t capacity;   /**< Runtime window size (<= MAX_WINDOW) */
    uint16_t crossings;  /**< Current zero-crossing count within the window */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a zero-crossing rate tracker
 *
 * Sets the window capacity, zeros the circular buffer via memset,
 * and resets the head pointer, sample count, and crossing count.
 * The minimum window size is 2 because a single sample has no
 * predecessor to compare against.
 *
 * @param z      ZCR tracker to initialize (must not be NULL)
 * @param window Window size
 *               (2..TIKU_KITS_SIGFEATURES_ZCR_MAX_WINDOW)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if z is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_SIZE if window < 2 or
 *         exceeds MAX_WINDOW
 */
int tiku_kits_sigfeatures_zcr_init(
    struct tiku_kits_sigfeatures_zcr *z,
    uint16_t window);

/**
 * @brief Reset a ZCR tracker, clearing all samples and crossings
 *
 * Zeros the circular buffer, head pointer, sample count, and
 * crossing count while preserving the window capacity from
 * init().  Useful for starting a new measurement window without
 * re-initializing.
 *
 * @param z ZCR tracker to reset (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if z is NULL
 */
int tiku_kits_sigfeatures_zcr_reset(
    struct tiku_kits_sigfeatures_zcr *z);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the ZCR tracker
 *
 * Incrementally updates the crossing count in O(1).  When the
 * window is full the oldest sample is evicted and the crossing
 * between it and its successor is subtracted.  Then the crossing
 * between the current newest sample and the incoming value is
 * checked and, if a sign change occurred, added.  The first sample
 * pushed is a special case: it has no predecessor, so no crossing
 * is counted.
 *
 * @param z     ZCR tracker (must not be NULL)
 * @param value New sample value (zero is treated as non-negative)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if z is NULL
 */
int tiku_kits_sigfeatures_zcr_push(
    struct tiku_kits_sigfeatures_zcr *z,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current zero-crossing count within the window
 *
 * Returns the number of sign changes between consecutive samples
 * currently in the sliding window.  Safe to call with a NULL
 * pointer -- returns 0.
 *
 * @param z ZCR tracker, or NULL
 * @return Number of zero crossings, or 0 if z is NULL
 */
uint16_t tiku_kits_sigfeatures_zcr_crossings(
    const struct tiku_kits_sigfeatures_zcr *z);

/**
 * @brief Get the current number of samples in the window
 *
 * Returns the logical count of samples stored in the circular
 * buffer (always <= capacity).  Safe to call with a NULL pointer
 * -- returns 0.
 *
 * @param z ZCR tracker, or NULL
 * @return Number of samples currently in the window, or 0 if z
 *         is NULL
 */
uint16_t tiku_kits_sigfeatures_zcr_count(
    const struct tiku_kits_sigfeatures_zcr *z);

/**
 * @brief Check if the sliding window is full
 *
 * Returns a boolean indicating whether the number of samples
 * has reached the window capacity.  Once full, subsequent
 * push() calls evict the oldest sample before inserting the
 * new one.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param z ZCR tracker, or NULL
 * @return 1 if count >= capacity, 0 otherwise (including NULL)
 */
int tiku_kits_sigfeatures_zcr_full(
    const struct tiku_kits_sigfeatures_zcr *z);

#endif /* TIKU_KITS_SIGFEATURES_ZCR_H_ */
