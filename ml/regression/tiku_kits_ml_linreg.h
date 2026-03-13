/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_linreg.h - Simple linear regression library
 *
 * Platform-independent streaming linear regression for embedded systems.
 * Fits a model y = slope * x + intercept using ordinary least squares,
 * computed entirely with integer / fixed-point arithmetic (no FPU
 * required).
 *
 * Data points are pushed one at a time via tiku_kits_ml_linreg_push().
 * Fitted parameters (slope, intercept, R-squared) are computed on
 * demand from running accumulators -- no sample buffer is required.
 *
 * Slope and intercept are returned as fixed-point integers with
 * configurable fractional bits (default 8). To recover the real
 * value, divide by (1 << shift):
 *
 *     real_slope = slope_q / (1 << shift)
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

#ifndef TIKU_KITS_ML_LINREG_H_
#define TIKU_KITS_ML_LINREG_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ml.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Element type for x and y sample values.
 * Defaults to int32_t for integer-only targets (no FPU).
 * Define as int16_t before including if memory is very tight.
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * Default number of fractional bits for fixed-point slope/intercept.
 * With shift=8, the resolution is ~0.004 per step.
 * Override before including this header to change the default.
 */
#ifndef TIKU_KITS_ML_LINREG_SHIFT
#define TIKU_KITS_ML_LINREG_SHIFT 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_ml_elem_t
 *  @brief Element type used for all x/y sample values
 */
typedef TIKU_KITS_ML_ELEM_TYPE tiku_kits_ml_elem_t;

/**
 * @struct tiku_kits_ml_linreg
 * @brief Streaming simple linear regression model
 *
 * Fits y = slope * x + intercept by maintaining running sums of
 * x, y, x*x, x*y, and y*y. These accumulators use int64_t to
 * prevent overflow for typical embedded value ranges.
 *
 * At least two data points with distinct x values are required
 * before slope, intercept, predict, or R-squared can be queried.
 *
 * Example:
 * @code
 *   struct tiku_kits_ml_linreg lr;
 *   int32_t slope_q, intercept_q, r2_q;
 *   tiku_kits_ml_elem_t y_pred;
 *
 *   tiku_kits_ml_linreg_init(&lr, 8);
 *   tiku_kits_ml_linreg_push(&lr, 0, 100);
 *   tiku_kits_ml_linreg_push(&lr, 10, 300);
 *   tiku_kits_ml_linreg_push(&lr, 20, 500);
 *
 *   tiku_kits_ml_linreg_slope(&lr, &slope_q);      // 20.0 in Q8
 *   tiku_kits_ml_linreg_intercept(&lr, &intercept_q); // 100.0 in Q8
 *   tiku_kits_ml_linreg_predict(&lr, 15, &y_pred);  // y_pred = 400
 *   tiku_kits_ml_linreg_r2(&lr, &r2_q);             // 256 = 1.0 in Q8
 * @endcode
 */
struct tiku_kits_ml_linreg {
    int64_t sum_x;     /**< Running sum of x values */
    int64_t sum_y;     /**< Running sum of y values */
    int64_t sum_xx;    /**< Running sum of x*x */
    int64_t sum_xy;    /**< Running sum of x*y */
    int64_t sum_yy;    /**< Running sum of y*y (for R-squared) */
    uint16_t n;        /**< Number of data points pushed */
    uint8_t shift;     /**< Fixed-point fractional bits */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a linear regression model
 * @param lr    Model to initialize
 * @param shift Number of fixed-point fractional bits (0..30)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (shift > 30)
 *
 * All accumulators are zeroed. The shift parameter controls the
 * precision of slope and intercept outputs: larger shift gives
 * finer resolution but reduces the representable range.
 */
int tiku_kits_ml_linreg_init(struct tiku_kits_ml_linreg *lr,
                              uint8_t shift);

/**
 * @brief Reset a model, clearing all data points
 * @param lr Model to reset
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Preserves the configured shift value.
 */
int tiku_kits_ml_linreg_reset(struct tiku_kits_ml_linreg *lr);

/*---------------------------------------------------------------------------*/
/* DATA INPUT                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a data point (x, y) into the model
 * @param lr Model
 * @param x  Independent variable value
 * @param y  Dependent variable value
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Updates all running accumulators in O(1). Intermediate products
 * are computed in int64_t to prevent overflow for int32_t inputs.
 */
int tiku_kits_ml_linreg_push(struct tiku_kits_ml_linreg *lr,
                              tiku_kits_ml_elem_t x,
                              tiku_kits_ml_elem_t y);

/*---------------------------------------------------------------------------*/
/* FITTED PARAMETERS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the fitted slope as a fixed-point value
 * @param lr     Model (must have >= 2 points with distinct x)
 * @param result Output: slope * (1 << shift)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL,
 *         TIKU_KITS_ML_ERR_SIZE (< 2 points), or
 *         TIKU_KITS_ML_ERR_SINGULAR (all x values identical)
 *
 * The returned value is the OLS slope scaled by 2^shift.
 * To recover the real value: slope_real = result / (1 << shift).
 */
int tiku_kits_ml_linreg_slope(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result);

/**
 * @brief Get the fitted intercept as a fixed-point value
 * @param lr     Model (must have >= 2 points with distinct x)
 * @param result Output: intercept * (1 << shift)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL,
 *         TIKU_KITS_ML_ERR_SIZE (< 2 points), or
 *         TIKU_KITS_ML_ERR_SINGULAR (all x values identical)
 *
 * The returned value is the OLS intercept scaled by 2^shift.
 */
int tiku_kits_ml_linreg_intercept(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Predict y for a given x value
 * @param lr     Model (must have >= 2 points with distinct x)
 * @param x      Input x value
 * @param result Output: predicted y (in original scale, not Q-format)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL,
 *         TIKU_KITS_ML_ERR_SIZE (< 2 points), or
 *         TIKU_KITS_ML_ERR_SINGULAR (all x values identical)
 *
 * Computes y = slope * x + intercept using the running accumulators
 * directly. The result is in the original (unscaled) integer domain,
 * truncated by integer division.
 */
int tiku_kits_ml_linreg_predict(
    const struct tiku_kits_ml_linreg *lr,
    tiku_kits_ml_elem_t x,
    tiku_kits_ml_elem_t *result);

/*---------------------------------------------------------------------------*/
/* GOODNESS OF FIT                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the R-squared (coefficient of determination)
 * @param lr     Model (must have >= 2 points with distinct x)
 * @param result Output: R-squared * (1 << shift), range [0, 1<<shift]
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL,
 *         TIKU_KITS_ML_ERR_SIZE (< 2 points), or
 *         TIKU_KITS_ML_ERR_SINGULAR (all x or y values identical)
 *
 * R-squared measures how well the linear model fits the data.
 * A value of (1 << shift) indicates a perfect fit; 0 indicates
 * no linear relationship.
 *
 * Computed as: R2 = (n*Sxy - Sx*Sy)^2 / ((n*Sxx - Sx^2) * (n*Syy - Sy^2))
 */
int tiku_kits_ml_linreg_r2(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of data points pushed
 * @param lr Model
 * @return Number of data points, or 0 if lr is NULL
 */
uint16_t tiku_kits_ml_linreg_count(
    const struct tiku_kits_ml_linreg *lr);

#endif /* TIKU_KITS_ML_LINREG_H_ */
