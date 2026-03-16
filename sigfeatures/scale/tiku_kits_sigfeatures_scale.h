/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_scale.h - Min-max scaling
 *
 * Maps values from [in_min, in_max] to [0, out_max] with clamping.
 * The range reciprocal is precomputed at setup so each normalization
 * is a multiply + shift -- cheaper than z-score and often good enough.
 *
 * Common out_max values:
 *   255   for uint8_t output  (TIKU_KITS_SIGFEATURES_SCALE_U8)
 *   32767 for Q15 output      (TIKU_KITS_SIGFEATURES_SCALE_Q15)
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

#ifndef TIKU_KITS_SIGFEATURES_SCALE_H_
#define TIKU_KITS_SIGFEATURES_SCALE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_SIGFEATURES_SCALE_TARGETS Common output ranges
 * @{ */
#define TIKU_KITS_SIGFEATURES_SCALE_U8    255    /**< uint8_t: [0, 255] */
#define TIKU_KITS_SIGFEATURES_SCALE_Q15   32767  /**< Q15: [0, 32767] */
/** @} */

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_scale
 * @brief Min-max scaler with precomputed fixed-point range reciprocal
 *
 * Maps input values from [in_min, in_max] to [0, out_max] using
 * linear interpolation with clamping at both boundaries:
 *
 *     scaled = (x - in_min) * out_max / (in_max - in_min)
 *
 * To avoid runtime division (expensive on MSP430), the reciprocal
 * of the input range is precomputed at init/update time as a
 * fixed-point constant:
 *
 *     inv_range_q = (out_max << shift) / (in_max - in_min)
 *
 * Each normalization then reduces to a single multiply and right
 * shift:
 *
 *     scaled = ((x - in_min) * inv_range_q) >> shift
 *
 * Values at or below in_min map to 0; values at or above in_max
 * map to out_max.  An additional post-shift clamp guards against
 * fixed-point rounding pushing the result outside [0, out_max].
 *
 * @note Use shift=16 for a good balance of precision and
 *       intermediate-product headroom on 32-bit accumulators.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_scale s;
 *   int32_t scaled;
 *
 *   // Map [0, 1023] -> [0, 255] (10-bit ADC to 8-bit)
 *   tiku_kits_sigfeatures_scale_init(&s, 0, 1023,
 *       TIKU_KITS_SIGFEATURES_SCALE_U8, 16);
 *   tiku_kits_sigfeatures_scale_normalize(&s, 512, &scaled);
 *   // scaled ~ 127
 * @endcode
 */
struct tiku_kits_sigfeatures_scale {
    tiku_kits_sigfeatures_elem_t in_min;  /**< Input lower bound (clamp floor) */
    tiku_kits_sigfeatures_elem_t in_max;  /**< Input upper bound (clamp ceiling) */
    int32_t inv_range_q;  /**< Precomputed: (out_max << shift) / range */
    int32_t out_max;      /**< Output maximum (e.g. 255, 32767) */
    uint8_t shift;        /**< Fixed-point fractional bits (1..30) */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a min-max scaler
 *
 * Stores the input and output range parameters and precomputes the
 * fixed-point range reciprocal so that subsequent normalize() calls
 * require only a multiply and shift.  Must be called before any
 * normalize operations.
 *
 * @param s       Scaler to initialize (must not be NULL)
 * @param in_min  Input lower bound
 * @param in_max  Input upper bound (must be strictly > in_min)
 * @param out_max Output upper bound (must be > 0; typical values
 *                are TIKU_KITS_SIGFEATURES_SCALE_U8 (255) or
 *                TIKU_KITS_SIGFEATURES_SCALE_Q15 (32767))
 * @param shift   Fixed-point fractional bits (1..30).  Higher
 *                values give better precision at the cost of
 *                larger intermediate products.  Use 16 for a good
 *                default.
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if s is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if in_max <= in_min,
 *         out_max <= 0, or shift is out of range
 */
int tiku_kits_sigfeatures_scale_init(
    struct tiku_kits_sigfeatures_scale *s,
    tiku_kits_sigfeatures_elem_t in_min,
    tiku_kits_sigfeatures_elem_t in_max,
    int32_t out_max,
    uint8_t shift);

/**
 * @brief Update the input range, recomputing the reciprocal
 *
 * Replaces in_min and in_max with the new values and recomputes
 * inv_range_q.  Preserves out_max and shift from the original
 * init() call.  Use this when the observed signal range changes
 * at runtime (e.g. from a sliding min/max tracker) to avoid a
 * full re-init.
 *
 * @param s      Scaler (must not be NULL)
 * @param in_min New input lower bound
 * @param in_max New input upper bound (must be strictly > in_min)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if s is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if in_max <= in_min
 */
int tiku_kits_sigfeatures_scale_update(
    struct tiku_kits_sigfeatures_scale *s,
    tiku_kits_sigfeatures_elem_t in_min,
    tiku_kits_sigfeatures_elem_t in_max);

/*---------------------------------------------------------------------------*/
/* Normalization                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Scale a single value to the output range with clamping
 *
 * Computes scaled = ((value - in_min) * inv_range_q) >> shift,
 * clamping the result to [0, out_max].  Values at or below in_min
 * map to 0; values at or above in_max map to out_max.
 * Intermediate values are linearly interpolated.  O(1) per call.
 *
 * @param s      Scaler (must not be NULL)
 * @param value  Input sample
 * @param result Output pointer where the scaled value in [0,
 *               out_max] is written (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if s or result is NULL
 */
int tiku_kits_sigfeatures_scale_normalize(
    const struct tiku_kits_sigfeatures_scale *s,
    tiku_kits_sigfeatures_elem_t value,
    int32_t *result);

/**
 * @brief Scale an entire buffer with clamping
 *
 * Batch alternative to calling normalize() in a loop.  Applies
 * the same linear mapping with clamping to every element of
 * @p src and writes the results to @p dst.  O(len) per call.
 *
 * @param s     Scaler (must not be NULL)
 * @param src   Input sample buffer (length len, must not be NULL)
 * @param dst   Output buffer for scaled values (length len, must
 *              not be NULL)
 * @param len   Number of samples to process
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if s, src, or dst is NULL
 */
int tiku_kits_sigfeatures_scale_normalize_batch(
    const struct tiku_kits_sigfeatures_scale *s,
    const tiku_kits_sigfeatures_elem_t *src,
    int32_t *dst,
    uint16_t len);

#endif /* TIKU_KITS_SIGFEATURES_SCALE_H_ */
