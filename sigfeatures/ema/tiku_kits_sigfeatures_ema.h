/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_ema.h - Exponential Moving Average filter
 *
 * Integer-only EMA using a power-of-two smoothing factor.  The
 * alpha parameter is expressed as a right-shift so the update
 * reduces to a subtract and shift -- no division, no floats.
 *
 *   value += ((sample << 16) - value) >> alpha_shift
 *
 * Larger alpha_shift values produce heavier smoothing (slower
 * response).  The internal accumulator is scaled by 2^16 for
 * fixed-point precision; the descaled result is returned by
 * tiku_kits_sigfeatures_ema_value().
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

#ifndef TIKU_KITS_SIGFEATURES_EMA_H_
#define TIKU_KITS_SIGFEATURES_EMA_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum allowed alpha_shift value
 *
 * Limits the smoothing factor to prevent excessively slow response
 * and to keep the fixed-point arithmetic well-behaved within the
 * 32-bit accumulator.  Override before including this header to
 * change the limit.
 */
#ifndef TIKU_KITS_SIGFEATURES_EMA_SHIFT_MAX
#define TIKU_KITS_SIGFEATURES_EMA_SHIFT_MAX 15
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_ema
 * @brief Streaming exponential moving average filter
 *
 * Maintains a fixed-point accumulator that tracks the EMA of the
 * input signal.  The smoothing weight alpha is 1 / 2^alpha_shift,
 * so larger shift values produce heavier smoothing.
 *
 * Internal value is scaled by 2^16.  The first sample seeds the
 * accumulator directly (no smoothing on the first sample).
 * Subsequent samples apply the recurrence:
 *
 *     value += (((int32_t)sample << 16) - value) >> alpha_shift
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_ema ema;
 *   tiku_kits_sigfeatures_elem_t result;
 *
 *   tiku_kits_sigfeatures_ema_init(&ema, 3);   // alpha = 1/8
 *   tiku_kits_sigfeatures_ema_push(&ema, 100);  // seeds value
 *   tiku_kits_sigfeatures_ema_push(&ema, 200);  // smoothed
 *   tiku_kits_sigfeatures_ema_value(&ema, &result);
 * @endcode
 */
struct tiku_kits_sigfeatures_ema {
    uint8_t  alpha_shift; /**< Right-shift for alpha (1..EMA_SHIFT_MAX) */
    int32_t  value;       /**< Current filtered value, scaled by 2^16 */
    uint16_t count;       /**< Number of samples processed */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an EMA filter with the given smoothing factor
 *
 * Sets alpha_shift, zeros the accumulator and sample count.  Must
 * be called before any push/value operations.
 *
 * @param ema         EMA filter to initialize (must not be NULL)
 * @param alpha_shift Smoothing factor as right-shift
 *                    (1..TIKU_KITS_SIGFEATURES_EMA_SHIFT_MAX)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if ema is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if alpha_shift is
 *         out of range
 */
int tiku_kits_sigfeatures_ema_init(
    struct tiku_kits_sigfeatures_ema *ema,
    uint8_t alpha_shift);

/**
 * @brief Reset an EMA filter, keeping the current alpha_shift
 *
 * Clears the accumulator and sample count so the filter returns
 * to its pre-first-push state.  The alpha_shift setting is
 * preserved.
 *
 * @param ema EMA filter to reset (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if ema is NULL
 */
int tiku_kits_sigfeatures_ema_reset(
    struct tiku_kits_sigfeatures_ema *ema);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the EMA filter
 *
 * On the first push the accumulator is seeded directly:
 *     value = (int32_t)sample << 16
 *
 * On subsequent pushes the EMA recurrence is applied:
 *     value += (((int32_t)sample << 16) - value) >> alpha_shift
 *
 * O(1) per call; no loops or allocations.
 *
 * @param ema    EMA filter (must not be NULL)
 * @param sample New input sample
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if ema is NULL
 */
int tiku_kits_sigfeatures_ema_push(
    struct tiku_kits_sigfeatures_ema *ema,
    tiku_kits_sigfeatures_elem_t sample);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Retrieve the current filtered value (descaled)
 *
 * Returns the EMA accumulator right-shifted by 16 to remove the
 * fixed-point scaling.  Fails with ERR_NODATA if no samples have
 * been pushed yet.
 *
 * @param ema    EMA filter (must not be NULL)
 * @param result Output pointer where the descaled value is written
 *               (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if ema or result is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA if count == 0
 */
int tiku_kits_sigfeatures_ema_value(
    const struct tiku_kits_sigfeatures_ema *ema,
    tiku_kits_sigfeatures_elem_t *result);

/**
 * @brief Return the number of samples processed
 *
 * NULL-safe: returns 0 if ema is NULL.
 *
 * @param ema EMA filter (may be NULL)
 * @return Number of samples pushed, or 0 if ema is NULL
 */
uint16_t tiku_kits_sigfeatures_ema_count(
    const struct tiku_kits_sigfeatures_ema *ema);

#endif /* TIKU_KITS_SIGFEATURES_EMA_H_ */
