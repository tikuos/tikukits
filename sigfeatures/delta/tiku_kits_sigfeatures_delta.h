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
 * Stores the previous sample and computes the difference on each
 * push. The first push only seeds the state; a valid delta is
 * available from the second push onward.
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
    tiku_kits_sigfeatures_elem_t prev;       /**< Previous sample */
    tiku_kits_sigfeatures_elem_t last_delta; /**< Most recent delta */
    uint8_t ready;   /**< 1 after at least two samples pushed */
};

/*---------------------------------------------------------------------------*/
/* Streaming -- initialization                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a delta tracker
 * @param d Delta tracker to initialize
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 */
int tiku_kits_sigfeatures_delta_init(
    struct tiku_kits_sigfeatures_delta *d);

/**
 * @brief Reset a delta tracker, clearing state
 * @param d Delta tracker to reset
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 */
int tiku_kits_sigfeatures_delta_reset(
    struct tiku_kits_sigfeatures_delta *d);

/*---------------------------------------------------------------------------*/
/* Streaming -- sample input                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample and compute the delta
 * @param d     Delta tracker
 * @param value Sample value
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 *
 * The first push seeds the tracker. From the second push onward,
 * the delta (value - prev) is stored and available via get().
 */
int tiku_kits_sigfeatures_delta_push(
    struct tiku_kits_sigfeatures_delta *d,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Streaming -- queries                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the most recent delta
 * @param d      Delta tracker
 * @param result Output: x[n] - x[n-1]
 * @return TIKU_KITS_SIGFEATURES_OK or
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA (fewer than 2 samples)
 */
int tiku_kits_sigfeatures_delta_get(
    const struct tiku_kits_sigfeatures_delta *d,
    tiku_kits_sigfeatures_elem_t *result);

/*---------------------------------------------------------------------------*/
/* Batch operation                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute first-order differences for an entire buffer
 * @param src     Input samples (length src_len)
 * @param src_len Number of input samples (must be >= 2)
 * @param dst     Output deltas (length src_len - 1)
 * @return TIKU_KITS_SIGFEATURES_OK,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL, or
 *         TIKU_KITS_SIGFEATURES_ERR_SIZE (src_len < 2)
 *
 * dst[i] = src[i+1] - src[i] for i in 0..src_len-2.
 * The caller must provide a dst buffer of at least (src_len - 1)
 * elements.
 */
int tiku_kits_sigfeatures_delta_compute(
    const tiku_kits_sigfeatures_elem_t *src, uint16_t src_len,
    tiku_kits_sigfeatures_elem_t *dst);

#endif /* TIKU_KITS_SIGFEATURES_DELTA_H_ */
