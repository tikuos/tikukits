/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_zscore.c - Z-score normalization (fixed-point)
 *
 * Precomputes 1/stddev as a fixed-point reciprocal so each
 * normalization is a multiply + shift with no runtime division.
 * Intermediate products use int64_t for overflow safety on MSP430.
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

#include "tiku_kits_sigfeatures_zscore.h"

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a z-score normalizer
 *
 * Stores the mean and shift, then precomputes the fixed-point
 * reciprocal of stddev: inv_stddev_q = (1 << shift) / stddev.
 * The numerator is widened to int64_t to support large shift
 * values without overflow.
 */
int tiku_kits_sigfeatures_zscore_init(
    struct tiku_kits_sigfeatures_zscore *z,
    tiku_kits_sigfeatures_elem_t mean,
    tiku_kits_sigfeatures_elem_t stddev,
    uint8_t shift)
{
    if (z == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (stddev <= 0 || shift == 0 || shift > 30) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    z->mean = mean;
    z->shift = shift;

    /*
     * Precompute the reciprocal of stddev in Q(shift) fixed-point:
     *   inv_stddev_q = (1 << shift) / stddev
     *
     * Use int64_t for the numerator to support large shift values
     * without overflow.
     */
    z->inv_stddev_q = (int32_t)(((int64_t)1 << shift) / stddev);

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Update the mean and stddev, recomputing the reciprocal
 *
 * Replaces the stored mean and recomputes inv_stddev_q using the
 * shift value preserved from init().  Cheaper than a full re-init
 * when only the statistics have changed.
 */
int tiku_kits_sigfeatures_zscore_update(
    struct tiku_kits_sigfeatures_zscore *z,
    tiku_kits_sigfeatures_elem_t mean,
    tiku_kits_sigfeatures_elem_t stddev)
{
    if (z == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (stddev <= 0) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    z->mean = mean;
    z->inv_stddev_q = (int32_t)(((int64_t)1 << z->shift) / stddev);

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* NORMALIZATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Normalize a single value to its z-score
 *
 * Computes (value - mean) * inv_stddev_q in int64_t, then
 * truncates to int32_t.  The result is a signed Q(shift)
 * fixed-point integer representing the z-score.
 */
int tiku_kits_sigfeatures_zscore_normalize(
    const struct tiku_kits_sigfeatures_zscore *z,
    tiku_kits_sigfeatures_elem_t value,
    int32_t *result)
{
    int64_t diff;

    if (z == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    /*
     * z_score = (value - mean) * inv_stddev_q
     *         = (value - mean) * (1 << shift) / stddev
     *         = (value - mean) / stddev   [in Q(shift) format]
     *
     * Use int64_t for the multiply to prevent overflow.
     */
    diff = (int64_t)value - (int64_t)z->mean;
    *result = (int32_t)(diff * z->inv_stddev_q);

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Normalize an entire buffer to z-scores
 *
 * Applies (src[i] - mean) * inv_stddev_q for each element.  The
 * subtraction is widened to int64_t before the multiply to prevent
 * overflow.  O(len); forward iteration for sequential memory
 * access.
 */
int tiku_kits_sigfeatures_zscore_normalize_batch(
    const struct tiku_kits_sigfeatures_zscore *z,
    const tiku_kits_sigfeatures_elem_t *src,
    int32_t *dst,
    uint16_t len)
{
    uint16_t i;
    int64_t diff;

    if (z == NULL || src == NULL || dst == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    for (i = 0; i < len; i++) {
        /* Widen to int64_t before multiply to prevent overflow on
         * 16-bit targets. */
        diff = (int64_t)src[i] - (int64_t)z->mean;
        dst[i] = (int32_t)(diff * z->inv_stddev_q);
    }

    return TIKU_KITS_SIGFEATURES_OK;
}
