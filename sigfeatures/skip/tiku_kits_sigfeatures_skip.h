/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_skip.h - Skip (decimation) filter
 *
 * Outputs every Nth input sample, discarding the rest.  Useful
 * for down-sampling a high-rate sensor stream before feeding it
 * to a more expensive feature extractor.
 *
 * The filter counts incoming samples and latches every Nth one as
 * the output.  The caller checks the ready flag after each push to
 * determine whether a new output was produced.
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

#ifndef TIKU_KITS_SIGFEATURES_SKIP_H_
#define TIKU_KITS_SIGFEATURES_SKIP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_skip
 * @brief Skip (decimation) filter -- keeps every Nth sample
 *
 * Counts input samples and outputs one every @c interval pushes.
 * On each push the internal counter increments; when it reaches
 * @c interval, the sample is latched as the output, the counter
 * resets to zero, and the @c ready flag is set.  Between output
 * points, @c ready is cleared.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_skip s;
 *   tiku_kits_sigfeatures_elem_t result;
 *
 *   tiku_kits_sigfeatures_skip_init(&s, 4);
 *   // push samples 1..4
 *   tiku_kits_sigfeatures_skip_push(&s, 10);  // ready=0
 *   tiku_kits_sigfeatures_skip_push(&s, 20);  // ready=0
 *   tiku_kits_sigfeatures_skip_push(&s, 30);  // ready=0
 *   tiku_kits_sigfeatures_skip_push(&s, 40);  // ready=1, output=40
 *   tiku_kits_sigfeatures_skip_value(&s, &result); // result=40
 * @endcode
 */
struct tiku_kits_sigfeatures_skip {
    uint16_t interval; /**< Output every N samples (>= 1) */
    uint16_t counter;  /**< Counts up to interval */
    tiku_kits_sigfeatures_elem_t last; /**< Last output value */
    uint8_t  ready;    /**< 1 if last push produced output */
    uint16_t count;    /**< Total outputs produced */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a skip filter with the given decimation interval
 *
 * Sets the interval, zeros the counter, output, ready flag, and
 * output count.  Must be called before any push operations.
 *
 * @param s        Skip filter to initialize (must not be NULL)
 * @param interval Decimation interval (>= 1; output 1 in N)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if s is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if interval == 0
 */
int tiku_kits_sigfeatures_skip_init(
    struct tiku_kits_sigfeatures_skip *s,
    uint16_t interval);

/**
 * @brief Reset a skip filter, keeping the interval
 *
 * Clears the counter, output, ready flag, and output count.  The
 * interval setting is preserved.
 *
 * @param s Skip filter to reset (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if s is NULL
 */
int tiku_kits_sigfeatures_skip_reset(
    struct tiku_kits_sigfeatures_skip *s);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the skip filter
 *
 * Increments the internal counter.  When the counter reaches
 * interval, the sample is latched as the output, the counter
 * resets to 0, ready is set to 1, and the output count increments.
 * Otherwise ready is cleared.  O(1) per call.
 *
 * @param s      Skip filter (must not be NULL)
 * @param sample New input sample
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if s is NULL
 */
int tiku_kits_sigfeatures_skip_push(
    struct tiku_kits_sigfeatures_skip *s,
    tiku_kits_sigfeatures_elem_t sample);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether the last push produced an output
 *
 * NULL-safe: returns 0 if s is NULL.
 *
 * @param s Skip filter (may be NULL)
 * @return 1 if the last push produced output, 0 otherwise
 */
uint8_t tiku_kits_sigfeatures_skip_ready(
    const struct tiku_kits_sigfeatures_skip *s);

/**
 * @brief Retrieve the last output value
 *
 * Copies the most recently latched output into *result.  Fails
 * with ERR_NODATA if no output has been produced yet (count == 0).
 *
 * @param s      Skip filter (must not be NULL)
 * @param result Output pointer where the value is written
 *               (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if s or result is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA if count == 0
 */
int tiku_kits_sigfeatures_skip_value(
    const struct tiku_kits_sigfeatures_skip *s,
    tiku_kits_sigfeatures_elem_t *result);

/**
 * @brief Return the total number of outputs produced
 *
 * NULL-safe: returns 0 if s is NULL.
 *
 * @param s Skip filter (may be NULL)
 * @return Number of outputs produced, or 0 if s is NULL
 */
uint16_t tiku_kits_sigfeatures_skip_count(
    const struct tiku_kits_sigfeatures_skip *s);

#endif /* TIKU_KITS_SIGFEATURES_SKIP_H_ */
