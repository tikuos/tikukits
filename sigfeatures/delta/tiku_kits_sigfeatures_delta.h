/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_delta.h - First-order difference (delta)
 *
 * Computes x[n] - x[n-1] to capture the rate of change. Simple but
 * often a strong ML feature. Provides both a streaming tracker (push
 * one sample at a time) and a batch function for processing buffers.
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

#ifndef TIKU_KITS_SIGFEATURES_DELTA_H_
#define TIKU_KITS_SIGFEATURES_DELTA_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_delta
 * @brief Streaming first-order difference tracker
 *
 * Computes the first-order difference x[n] - x[n-1] incrementally as
 * samples arrive one at a time.  This is the discrete derivative of
 * the signal and is one of the most common features for detecting
 * rate-of-change, trend shifts, and motion onset.
 *
 * The tracker has a simple two-phase lifecycle:
 *   - After the first push, the @c prev field is seeded but no
 *     valid delta exists yet (@c ready == 0).
 *   - From the second push onward, @c last_delta holds the most
 *     recent difference and @c ready == 1.
 *
 * All storage is contained within the struct itself (no pointers,
 * no heap), so it can be declared as a static or local variable.
 *
 * @note For batch processing of an entire buffer in one call, use
 *       tiku_kits_sigfeatures_delta_compute() instead.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_delta d;
 *   tiku_kits_sigfeatures_elem_t result;
 *
 *   tiku_kits_sigfeatures_delta_init(&d);
 *   tiku_kits_sigfeatures_delta_push(&d, 100);  // seeds prev
 *   tiku_kits_sigfeatures_delta_push(&d, 130);  // delta = 30
 *   tiku_kits_sigfeatures_delta_get(&d, &result);  // result = 30
 * @endcode
 */
struct tiku_kits_sigfeatures_delta {
    tiku_kits_sigfeatures_elem_t prev;       /**< Previous sample value (x[n-1]) */
    tiku_kits_sigfeatures_elem_t last_delta; /**< Most recent delta (x[n] - x[n-1]) */
    uint8_t ready;   /**< 1 after at least two samples have been pushed */
};

/*---------------------------------------------------------------------------*/
/* Streaming -- initialization                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a delta tracker to its empty state
 *
 * Zeros prev, last_delta, and the ready flag so the tracker is
 * prepared to receive its first sample.  Must be called before
 * any push/get operations.
 *
 * @param d Delta tracker to initialize (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if d is NULL
 */
int tiku_kits_sigfeatures_delta_init(
    struct tiku_kits_sigfeatures_delta *d);

/**
 * @brief Reset a delta tracker, clearing all accumulated state
 *
 * Equivalent to calling init() again.  The tracker returns to its
 * pre-first-push state: ready is cleared and prev/last_delta are
 * zeroed.  Useful when the input stream changes context (e.g.
 * starting a new measurement window).
 *
 * @param d Delta tracker to reset (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if d is NULL
 */
int tiku_kits_sigfeatures_delta_reset(
    struct tiku_kits_sigfeatures_delta *d);

/*---------------------------------------------------------------------------*/
/* Streaming -- sample input                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample and compute the first-order difference
 *
 * On the very first push the tracker only stores the sample as
 * @c prev and sets ready to 1 -- no valid delta is produced yet.
 * From the second push onward, last_delta is updated to
 * value - prev, and the result can be retrieved with get().
 *
 * O(1) per call; no loops or allocations.
 *
 * @param d     Delta tracker (must not be NULL)
 * @param value New sample value (x[n])
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if d is NULL
 */
int tiku_kits_sigfeatures_delta_push(
    struct tiku_kits_sigfeatures_delta *d,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Streaming -- queries                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Retrieve the most recently computed delta value
 *
 * Copies last_delta into the caller-provided location pointed to
 * by @p result.  Fails if fewer than two samples have been pushed
 * (i.e. the tracker has not yet produced a valid difference).
 *
 * @param d      Delta tracker (must not be NULL)
 * @param result Output pointer where x[n] - x[n-1] is written
 *               (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if d or result is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA if fewer than 2 samples
 *         have been pushed
 */
int tiku_kits_sigfeatures_delta_get(
    const struct tiku_kits_sigfeatures_delta *d,
    tiku_kits_sigfeatures_elem_t *result);

/*---------------------------------------------------------------------------*/
/* Batch operation                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute first-order differences for an entire buffer
 *
 * Batch alternative to the streaming push/get interface.  Produces
 * (src_len - 1) output values where dst[i] = src[i+1] - src[i].
 * O(n) where n is src_len.
 *
 * The caller must provide a @p dst buffer of at least
 * (src_len - 1) elements.  The source and destination buffers
 * must not overlap.
 *
 * @param src     Input sample buffer (length src_len, must not be
 *                NULL)
 * @param src_len Number of input samples (must be >= 2)
 * @param dst     Output delta buffer (length >= src_len - 1, must
 *                not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if src or dst is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_SIZE if src_len < 2
 */
int tiku_kits_sigfeatures_delta_compute(
    const tiku_kits_sigfeatures_elem_t *src, uint16_t src_len,
    tiku_kits_sigfeatures_elem_t *dst);

#endif /* TIKU_KITS_SIGFEATURES_DELTA_H_ */
