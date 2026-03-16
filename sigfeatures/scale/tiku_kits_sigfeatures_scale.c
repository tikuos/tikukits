/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_scale.c - Min-max scaling
 *
 * Maps [in_min, in_max] to [0, out_max] with a precomputed
 * fixed-point range reciprocal. Each normalization is a multiply +
 * shift + clamp. Intermediate products use int64_t for overflow
 * safety on MSP430.
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

#include "tiku_kits_sigfeatures_scale.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the fixed-point range reciprocal
 *
 * Calculates (out_max << shift) / range using int64_t for the
 * numerator to support large shift values without overflowing
 * the 32-bit result.
 *
 * @param out_max Output maximum value
 * @param range   in_max - in_min (must be > 0)
 * @param shift   Fixed-point fractional bits
 * @return (out_max << shift) / range, truncated to int32_t
 */
static int32_t compute_inv_range(int32_t out_max,
                                 tiku_kits_sigfeatures_elem_t range,
                                 uint8_t shift)
{
    return (int32_t)(((int64_t)out_max << shift) / range);
}

/**
 * @brief Scale and clamp a single value using the precomputed reciprocal
 *
 * Handles boundary clamping first (value <= in_min or >= in_max)
 * to avoid unnecessary multiplication.  For in-range values the
 * fixed-point multiply is widened to int64_t to prevent overflow,
 * and the result is clamped a second time to guard against
 * fixed-point rounding at the boundaries.
 */
static int32_t scale_one(const struct tiku_kits_sigfeatures_scale *s,
                         tiku_kits_sigfeatures_elem_t value)
{
    int64_t temp;
    int32_t scaled;

    if (value <= s->in_min) {
        return 0;
    }
    if (value >= s->in_max) {
        return s->out_max;
    }

    temp = (int64_t)(value - s->in_min) * s->inv_range_q;
    scaled = (int32_t)(temp >> s->shift);

    /* Clamp to handle fixed-point rounding at boundaries */
    if (scaled < 0) {
        scaled = 0;
    } else if (scaled > s->out_max) {
        scaled = s->out_max;
    }

    return scaled;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a min-max scaler
 *
 * Validates all parameters, stores the range and shift, and
 * precomputes the fixed-point range reciprocal via
 * compute_inv_range().  After this call the scaler is ready
 * for normalize() / normalize_batch().
 */
int tiku_kits_sigfeatures_scale_init(
    struct tiku_kits_sigfeatures_scale *s,
    tiku_kits_sigfeatures_elem_t in_min,
    tiku_kits_sigfeatures_elem_t in_max,
    int32_t out_max,
    uint8_t shift)
{
    tiku_kits_sigfeatures_elem_t range;

    if (s == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (in_max <= in_min || out_max <= 0
        || shift == 0 || shift > 30) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    range = in_max - in_min;

    s->in_min = in_min;
    s->in_max = in_max;
    s->out_max = out_max;
    s->shift = shift;
    s->inv_range_q = compute_inv_range(out_max, range, shift);

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Update the input range, recomputing the reciprocal
 *
 * Replaces in_min and in_max, then recomputes inv_range_q using
 * the existing out_max and shift from init().  Cheaper than a
 * full re-init when only the input range changes.
 */
int tiku_kits_sigfeatures_scale_update(
    struct tiku_kits_sigfeatures_scale *s,
    tiku_kits_sigfeatures_elem_t in_min,
    tiku_kits_sigfeatures_elem_t in_max)
{
    tiku_kits_sigfeatures_elem_t range;

    if (s == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (in_max <= in_min) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    range = in_max - in_min;

    s->in_min = in_min;
    s->in_max = in_max;
    s->inv_range_q = compute_inv_range(s->out_max, range, s->shift);

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* NORMALIZATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Scale a single value to the output range with clamping
 *
 * Delegates to the static scale_one() helper which performs the
 * boundary clamp, fixed-point multiply, shift, and post-shift
 * clamp.  O(1).
 */
int tiku_kits_sigfeatures_scale_normalize(
    const struct tiku_kits_sigfeatures_scale *s,
    tiku_kits_sigfeatures_elem_t value,
    int32_t *result)
{
    if (s == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    *result = scale_one(s, value);
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Scale an entire buffer with clamping
 *
 * Iterates over all len samples, calling scale_one() for each.
 * O(len).  The loop runs forward through both buffers for
 * sequential memory access.
 */
int tiku_kits_sigfeatures_scale_normalize_batch(
    const struct tiku_kits_sigfeatures_scale *s,
    const tiku_kits_sigfeatures_elem_t *src,
    int32_t *dst,
    uint16_t len)
{
    uint16_t i;

    if (s == NULL || src == NULL || dst == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    for (i = 0; i < len; i++) {
        dst[i] = scale_one(s, src[i]);
    }

    return TIKU_KITS_SIGFEATURES_OK;
}
