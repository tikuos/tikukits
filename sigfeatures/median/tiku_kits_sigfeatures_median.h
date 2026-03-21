/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_median.h - Sliding window median filter
 *
 * Maintains a circular buffer of the most recent samples and
 * computes the median on demand via an insertion sort into a
 * scratch array.  Effective at removing impulse noise while
 * preserving edges -- a common pre-processing step for sensor
 * signals on resource-constrained devices.
 *
 * The window size must be odd (so the median is unambiguous) and
 * at most TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE.
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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_SIGFEATURES_MEDIAN_H_
#define TIKU_KITS_SIGFEATURES_MEDIAN_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum sliding window size
 *
 * Determines the size of the statically allocated circular buffer
 * and sorted scratch array.  Keep small on RAM-constrained targets.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE
#define TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE 7
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_median
 * @brief Sliding window median filter
 *
 * Stores the most recent @c size samples in a circular buffer and
 * computes the median by copying into a scratch array and running
 * an insertion sort.  The window size must be odd and in the range
 * [1, TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE].
 *
 * Before the window is full (count < size), the median is computed
 * over the samples received so far.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_median m;
 *   tiku_kits_sigfeatures_elem_t result;
 *
 *   tiku_kits_sigfeatures_median_init(&m, 3);
 *   tiku_kits_sigfeatures_median_push(&m, 10);
 *   tiku_kits_sigfeatures_median_push(&m, 50);
 *   tiku_kits_sigfeatures_median_push(&m, 20);
 *   tiku_kits_sigfeatures_median_value(&m, &result);
 *   // result = 20 (median of {10, 50, 20})
 * @endcode
 */
struct tiku_kits_sigfeatures_median {
    /** Circular sample buffer */
    tiku_kits_sigfeatures_elem_t
        window[TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE];
    /** Sorted scratch for median finding */
    tiku_kits_sigfeatures_elem_t
        sorted[TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE];
    uint8_t size;  /**< Configured window size (odd, 1..MAX_SIZE) */
    uint8_t count; /**< Samples received so far (capped at size) */
    uint8_t head;  /**< Circular buffer write position */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a median filter with the given window size
 *
 * Sets the window size, zeros the circular buffer and count.
 * The window_size must be odd, at least 1, and at most
 * TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE.
 *
 * @param m           Median filter to initialize (must not be NULL)
 * @param window_size Sliding window size (must be odd, 1..MAX_SIZE)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if m is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if window_size is even,
 *         0, or > MAX_SIZE
 */
int tiku_kits_sigfeatures_median_init(
    struct tiku_kits_sigfeatures_median *m,
    uint8_t window_size);

/**
 * @brief Reset a median filter, keeping the window size
 *
 * Clears the circular buffer and count so the filter returns to
 * its pre-first-push state.  The window size is preserved.
 *
 * @param m Median filter to reset (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if m is NULL
 */
int tiku_kits_sigfeatures_median_reset(
    struct tiku_kits_sigfeatures_median *m);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the median filter
 *
 * Writes the sample to the circular buffer at the current head
 * position and advances head modulo size.  If count < size, count
 * is incremented.  O(1) per call.
 *
 * @param m      Median filter (must not be NULL)
 * @param sample New input sample
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if m is NULL
 */
int tiku_kits_sigfeatures_median_push(
    struct tiku_kits_sigfeatures_median *m,
    tiku_kits_sigfeatures_elem_t sample);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Retrieve the current median value
 *
 * Copies the active portion of the circular buffer into the sorted
 * scratch array, runs an insertion sort, and returns the middle
 * element.  O(count^2) due to insertion sort, but count is bounded
 * by MAX_SIZE (default 7) so this is fast in practice.
 *
 * @param m      Median filter (must not be NULL)
 * @param result Output pointer where the median is written
 *               (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if m or result is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA if count == 0
 */
int tiku_kits_sigfeatures_median_value(
    struct tiku_kits_sigfeatures_median *m,
    tiku_kits_sigfeatures_elem_t *result);

/**
 * @brief Return the number of samples in the window
 *
 * NULL-safe: returns 0 if m is NULL.
 *
 * @param m Median filter (may be NULL)
 * @return Number of samples currently in the window, or 0 if NULL
 */
uint8_t tiku_kits_sigfeatures_median_count(
    const struct tiku_kits_sigfeatures_median *m);

#endif /* TIKU_KITS_SIGFEATURES_MEDIAN_H_ */
