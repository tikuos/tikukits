/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_zscore.h - Z-score normalization (fixed-point)
 *
 * Computes (x - mean) / stddev in fixed-point arithmetic. The
 * reciprocal 1/stddev is precomputed at setup time so each
 * normalization is a single multiply + shift -- no runtime division.
 *
 * The output is a Q(shift) fixed-point int32_t. With shift=16 a
 * z-score of 1.5 is represented as 98304 (1.5 * 65536). To recover
 * the integer part use result >> shift.
 *
 * Designed to work with running statistics from the TikuKits maths
 * statistics library (Welford tracker, windowed statistics, etc.).
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

#ifndef TIKU_KITS_SIGFEATURES_ZSCORE_H_
#define TIKU_KITS_SIGFEATURES_ZSCORE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_zscore
 * @brief Fixed-point z-score normalizer
 *
 * Stores the mean, precomputed 1/stddev in Q(shift) fixed-point,
 * and the shift parameter. Each call to normalize() computes:
 *
 *     result = (x - mean) * inv_stddev_q
 *
 * which equals (x - mean) / stddev in Q(shift) representation.
 *
 * Higher shift values give better precision at the cost of larger
 * intermediate products (int64_t is used internally). shift=16 gives
 * about 0.002% resolution; shift=8 gives about 0.4%.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_zscore z;
 *   int32_t z_score;
 *
 *   // mean=100, stddev=20, shift=16
 *   tiku_kits_sigfeatures_zscore_init(&z, 100, 20, 16);
 *   tiku_kits_sigfeatures_zscore_normalize(&z, 130, &z_score);
 *   // z_score ≈ 98304 (1.5 in Q16)
 *   // integer part: z_score >> 16 = 1
 * @endcode
 */
struct tiku_kits_sigfeatures_zscore {
    tiku_kits_sigfeatures_elem_t mean;  /**< Population mean */
    int32_t inv_stddev_q;  /**< (1 << shift) / stddev, precomputed */
    uint8_t shift;         /**< Fixed-point fractional bits */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a z-score normalizer
 * @param z      Z-score normalizer to initialize
 * @param mean   Population mean (from running statistics)
 * @param stddev Population standard deviation (must be > 0)
 * @param shift  Fixed-point fractional bits (1..30)
 * @return TIKU_KITS_SIGFEATURES_OK,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM (stddev <= 0 or shift
 *         out of range)
 *
 * Precomputes inv_stddev_q = (1 << shift) / stddev.
 * Use shift=16 for good precision on MSP430.
 */
int tiku_kits_sigfeatures_zscore_init(
    struct tiku_kits_sigfeatures_zscore *z,
    tiku_kits_sigfeatures_elem_t mean,
    tiku_kits_sigfeatures_elem_t stddev,
    uint8_t shift);

/**
 * @brief Update the mean and stddev, recomputing 1/stddev
 * @param z      Z-score normalizer
 * @param mean   New population mean
 * @param stddev New population standard deviation (must be > 0)
 * @return TIKU_KITS_SIGFEATURES_OK or
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM (stddev <= 0)
 *
 * Call this periodically as running statistics are updated.
 * The shift value is preserved from init().
 */
int tiku_kits_sigfeatures_zscore_update(
    struct tiku_kits_sigfeatures_zscore *z,
    tiku_kits_sigfeatures_elem_t mean,
    tiku_kits_sigfeatures_elem_t stddev);

/*---------------------------------------------------------------------------*/
/* Normalization                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Normalize a single value to its z-score
 * @param z      Z-score normalizer
 * @param value  Input sample
 * @param result Output: (value - mean) / stddev in Q(shift) format
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 *
 * The result is a signed Q(shift) fixed-point integer:
 *   - Positive values indicate above-mean samples
 *   - Negative values indicate below-mean samples
 *   - Integer part: result >> shift
 *   - Fractional part: result & ((1 << shift) - 1)
 */
int tiku_kits_sigfeatures_zscore_normalize(
    const struct tiku_kits_sigfeatures_zscore *z,
    tiku_kits_sigfeatures_elem_t value,
    int32_t *result);

/**
 * @brief Normalize an entire buffer to z-scores
 * @param z       Z-score normalizer
 * @param src     Input samples (length len)
 * @param dst     Output z-scores in Q(shift) format (length len)
 * @param len     Number of samples
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 */
int tiku_kits_sigfeatures_zscore_normalize_batch(
    const struct tiku_kits_sigfeatures_zscore *z,
    const tiku_kits_sigfeatures_elem_t *src,
    int32_t *dst,
    uint16_t len);

#endif /* TIKU_KITS_SIGFEATURES_ZSCORE_H_ */
