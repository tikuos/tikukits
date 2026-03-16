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
 * @brief Fixed-point z-score normalizer with precomputed reciprocal
 *
 * Computes (x - mean) / stddev in fixed-point arithmetic by
 * replacing the runtime division with a precomputed multiply:
 *
 *     inv_stddev_q = (1 << shift) / stddev        (at init time)
 *     result       = (x - mean) * inv_stddev_q    (per sample)
 *
 * The output is a signed Q(shift) integer.  With shift=16, a
 * z-score of 1.5 is represented as 98304 (1.5 * 65536).  To
 * extract the integer part, right-shift by @c shift bits.
 *
 * Higher shift values give better fractional precision at the cost
 * of larger intermediate products (int64_t is used internally to
 * prevent overflow on 16-bit targets):
 *   - shift=16 -- about 0.002% resolution (default)
 *   - shift=8  -- about 0.4% resolution
 *
 * The normalizer is designed to pair with running-statistics
 * trackers (e.g. Welford, windowed mean/variance) that provide
 * periodically updated mean and stddev values.
 *
 * @note If stddev changes at runtime (e.g. from a sliding
 *       statistics tracker), call update() to recompute the
 *       reciprocal without a full re-init.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_zscore z;
 *   int32_t z_score;
 *
 *   // mean=100, stddev=20, shift=16
 *   tiku_kits_sigfeatures_zscore_init(&z, 100, 20, 16);
 *   tiku_kits_sigfeatures_zscore_normalize(&z, 130, &z_score);
 *   // z_score ~ 98304 (1.5 in Q16)
 *   // integer part: z_score >> 16 = 1
 * @endcode
 */
struct tiku_kits_sigfeatures_zscore {
    tiku_kits_sigfeatures_elem_t mean;  /**< Population mean used for centering */
    int32_t inv_stddev_q;  /**< Precomputed: (1 << shift) / stddev */
    uint8_t shift;         /**< Fixed-point fractional bits (1..30) */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a z-score normalizer
 *
 * Stores the population mean and shift value, then precomputes
 * the fixed-point reciprocal of stddev so that each subsequent
 * normalize() call is a single multiply -- no runtime division.
 *
 * @param z      Z-score normalizer to initialize (must not be
 *               NULL)
 * @param mean   Population mean (from running statistics or a
 *               known calibration value)
 * @param stddev Population standard deviation (must be > 0)
 * @param shift  Fixed-point fractional bits (1..30).  Higher
 *               values give finer resolution at the cost of
 *               larger intermediate products.  Use 16 for a
 *               good default on MSP430.
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if z is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if stddev <= 0 or
 *         shift is 0 or > 30
 */
int tiku_kits_sigfeatures_zscore_init(
    struct tiku_kits_sigfeatures_zscore *z,
    tiku_kits_sigfeatures_elem_t mean,
    tiku_kits_sigfeatures_elem_t stddev,
    uint8_t shift);

/**
 * @brief Update the mean and stddev, recomputing the reciprocal
 *
 * Replaces the stored mean and recomputes inv_stddev_q from the
 * new stddev using the shift value preserved from init().  Use
 * this when running statistics are updated periodically (e.g.
 * once per sliding window) to keep the normalizer current
 * without a full re-init.
 *
 * @param z      Z-score normalizer (must not be NULL)
 * @param mean   New population mean
 * @param stddev New population standard deviation (must be > 0)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if z is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if stddev <= 0
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
 *
 * Computes (value - mean) * inv_stddev_q, which equals
 * (value - mean) / stddev in Q(shift) fixed-point.  The
 * subtraction and multiply are performed in int64_t to prevent
 * overflow, then truncated to int32_t for the result.  O(1).
 *
 * @param z      Z-score normalizer (must not be NULL)
 * @param value  Input sample
 * @param result Output pointer where the z-score is written in
 *               Q(shift) format (must not be NULL).  Positive
 *               values indicate above-mean samples; negative
 *               values indicate below-mean.  Integer part:
 *               result >> shift.
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if z or result is NULL
 */
int tiku_kits_sigfeatures_zscore_normalize(
    const struct tiku_kits_sigfeatures_zscore *z,
    tiku_kits_sigfeatures_elem_t value,
    int32_t *result);

/**
 * @brief Normalize an entire buffer to z-scores
 *
 * Batch alternative to calling normalize() in a loop.  Applies
 * the same (x - mean) * inv_stddev_q computation to every element
 * of @p src and writes the Q(shift) results to @p dst.  O(len).
 *
 * @param z       Z-score normalizer (must not be NULL)
 * @param src     Input sample buffer (length len, must not be NULL)
 * @param dst     Output buffer for z-scores in Q(shift) format
 *                (length len, must not be NULL)
 * @param len     Number of samples to process
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if z, src, or dst is NULL
 */
int tiku_kits_sigfeatures_zscore_normalize_batch(
    const struct tiku_kits_sigfeatures_zscore *z,
    const tiku_kits_sigfeatures_elem_t *src,
    int32_t *dst,
    uint16_t len);

#endif /* TIKU_KITS_SIGFEATURES_ZSCORE_H_ */
