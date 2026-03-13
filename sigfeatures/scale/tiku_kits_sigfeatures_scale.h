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
 * @brief Min-max scaler with precomputed range reciprocal
 *
 * Maps input values from [in_min, in_max] to [0, out_max] using:
 *
 *     scaled = (x - in_min) * out_max / (in_max - in_min)
 *
 * The division is precomputed as a fixed-point multiply:
 *
 *     inv_range_q = (out_max << shift) / (in_max - in_min)
 *     scaled = ((x - in_min) * inv_range_q) >> shift
 *
 * Values outside [in_min, in_max] are clamped to [0, out_max].
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
 *   // scaled ≈ 127
 * @endcode
 */
struct tiku_kits_sigfeatures_scale {
    tiku_kits_sigfeatures_elem_t in_min;  /**< Input lower bound */
    tiku_kits_sigfeatures_elem_t in_max;  /**< Input upper bound */
    int32_t inv_range_q;  /**< (out_max << shift) / (in_max - in_min) */
    int32_t out_max;      /**< Output maximum (e.g. 255, 32767) */
    uint8_t shift;        /**< Internal fixed-point precision bits */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a min-max scaler
 * @param s       Scaler to initialize
 * @param in_min  Input lower bound
 * @param in_max  Input upper bound (must be > in_min)
 * @param out_max Output upper bound (must be > 0)
 * @param shift   Internal fixed-point bits (1..30)
 * @return TIKU_KITS_SIGFEATURES_OK,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL, or
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM
 *
 * Precomputes inv_range_q = (out_max << shift) / (in_max - in_min).
 * Use shift=16 for good precision.
 */
int tiku_kits_sigfeatures_scale_init(
    struct tiku_kits_sigfeatures_scale *s,
    tiku_kits_sigfeatures_elem_t in_min,
    tiku_kits_sigfeatures_elem_t in_max,
    int32_t out_max,
    uint8_t shift);

/**
 * @brief Update the input range, recomputing the reciprocal
 * @param s      Scaler
 * @param in_min New input lower bound
 * @param in_max New input upper bound (must be > in_min)
 * @return TIKU_KITS_SIGFEATURES_OK or
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM
 *
 * Preserves out_max and shift from init(). Call this when the
 * observed signal range changes (e.g. from a sliding min/max
 * tracker).
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
 * @param s      Scaler
 * @param value  Input sample
 * @param result Output: scaled value in [0, out_max]
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 *
 * Values at or below in_min map to 0. Values at or above in_max
 * map to out_max. Intermediate values are linearly interpolated.
 */
int tiku_kits_sigfeatures_scale_normalize(
    const struct tiku_kits_sigfeatures_scale *s,
    tiku_kits_sigfeatures_elem_t value,
    int32_t *result);

/**
 * @brief Scale an entire buffer with clamping
 * @param s     Scaler
 * @param src   Input samples (length len)
 * @param dst   Output scaled values (length len)
 * @param len   Number of samples
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 */
int tiku_kits_sigfeatures_scale_normalize_batch(
    const struct tiku_kits_sigfeatures_scale *s,
    const tiku_kits_sigfeatures_elem_t *src,
    int32_t *dst,
    uint16_t len);

#endif /* TIKU_KITS_SIGFEATURES_SCALE_H_ */
