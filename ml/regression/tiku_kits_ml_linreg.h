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
 * @brief Element type for x and y sample values.
 *
 * Defaults to int32_t for integer-only targets (no FPU required).
 * All ML sub-modules share this type.  Define as int16_t before
 * including if memory is very tight on a 16-bit target.
 *
 * Override before including this header to change the type:
 * @code
 *   #define TIKU_KITS_ML_ELEM_TYPE int16_t
 *   #include "tiku_kits_ml_linreg.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * @brief Default number of fractional bits for fixed-point
 *        slope and intercept outputs.
 *
 * With shift=8 the resolution is ~0.004 (1/256).  Slope and
 * intercept are returned as value * (1 << shift); divide by
 * (1 << shift) to recover the real value.  Larger shift gives
 * finer resolution but reduces the representable range.
 *
 * Override before including this header to change the default:
 * @code
 *   #define TIKU_KITS_ML_LINREG_SHIFT 10
 *   #include "tiku_kits_ml_linreg.h"
 * @endcode
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
 * @brief Streaming simple linear regression model (OLS)
 *
 * Fits y = slope * x + intercept using ordinary least squares by
 * maintaining five running accumulators: sum_x, sum_y, sum_xx,
 * sum_xy, and sum_yy.  These use int64_t to prevent overflow for
 * typical embedded int32_t sample values.  No sample buffer is
 * needed -- each push() is O(1) and fitted parameters are computed
 * on demand from the accumulators.
 *
 * At least two data points with distinct x values are required
 * before slope, intercept, predict, or R-squared can be queried.
 * If all x values are identical the denominator is zero and
 * ERR_SINGULAR is returned.
 *
 * @note Because all storage lives inside the struct, no heap
 *       allocation is needed -- just declare the model as a static
 *       or local variable.  Total struct size is 5 * 8 + 2 + 1 =
 *       43 bytes (plus padding).
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
 *   tiku_kits_ml_linreg_slope(&lr, &slope_q);        // 5120 = 20.0 in Q8
 *   tiku_kits_ml_linreg_intercept(&lr, &intercept_q); // 25600 = 100.0 in Q8
 *   tiku_kits_ml_linreg_predict(&lr, 15, &y_pred);   // y_pred = 400
 *   tiku_kits_ml_linreg_r2(&lr, &r2_q);              // 256 = 1.0 in Q8
 * @endcode
 */
struct tiku_kits_ml_linreg {
    int64_t sum_x;     /**< Running sum of x values (Sx) */
    int64_t sum_y;     /**< Running sum of y values (Sy) */
    int64_t sum_xx;    /**< Running sum of x*x (Sxx) */
    int64_t sum_xy;    /**< Running sum of x*y (Sxy) */
    int64_t sum_yy;    /**< Running sum of y*y (Syy, used for
                            R-squared computation) */
    uint16_t n;        /**< Number of data points pushed so far */
    uint8_t shift;     /**< Fixed-point fractional bits for slope,
                            intercept, and R-squared outputs */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a linear regression model
 *
 * Zeros all five accumulators and the sample count.  The shift
 * parameter controls the precision of slope and intercept outputs:
 * larger shift gives finer resolution but reduces the representable
 * integer range before overflow.
 *
 * @param lr    Model to initialize (must not be NULL)
 * @param shift Number of fixed-point fractional bits (0..30)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lr is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if shift > 30
 */
int tiku_kits_ml_linreg_init(struct tiku_kits_ml_linreg *lr,
                              uint8_t shift);

/**
 * @brief Reset a model, clearing all data points
 *
 * Zeros all accumulators and the sample count.  Preserves the
 * configured shift value so the model can be reused without
 * reinitialization.
 *
 * @param lr Model to reset (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lr is NULL
 */
int tiku_kits_ml_linreg_reset(struct tiku_kits_ml_linreg *lr);

/*---------------------------------------------------------------------------*/
/* DATA INPUT                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a data point (x, y) into the model
 *
 * Updates all five running accumulators (sum_x, sum_y, sum_xx,
 * sum_xy, sum_yy) and increments the sample count in O(1).
 * Intermediate products are computed in int64_t to prevent
 * overflow for int32_t inputs.
 *
 * @param lr Model (must not be NULL)
 * @param x  Independent variable value
 * @param y  Dependent variable value
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lr is NULL
 */
int tiku_kits_ml_linreg_push(struct tiku_kits_ml_linreg *lr,
                              tiku_kits_ml_elem_t x,
                              tiku_kits_ml_elem_t y);

/*---------------------------------------------------------------------------*/
/* FITTED PARAMETERS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the fitted slope as a fixed-point value
 *
 * Computes the OLS slope from the running accumulators:
 *   slope = (n * Sxy - Sx * Sy) / (n * Sxx - Sx^2)
 * and scales the result by (1 << shift) for fixed-point output.
 * To recover the real value: slope_real = result / (1 << shift).
 *
 * @param lr     Model with >= 2 points that have distinct x values
 *               (must not be NULL)
 * @param result Output pointer where slope * (1 << shift) is
 *               written (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lr or result is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if fewer than 2 points have been
 *         pushed,
 *         TIKU_KITS_ML_ERR_SINGULAR if all x values are identical
 */
int tiku_kits_ml_linreg_slope(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result);

/**
 * @brief Get the fitted intercept as a fixed-point value
 *
 * Computes the OLS intercept from the running accumulators:
 *   intercept = (Sy * den - num * Sx) / (n * den)
 * where num and den are the slope numerator and denominator.
 * The result is scaled by (1 << shift).
 *
 * @param lr     Model with >= 2 points that have distinct x values
 *               (must not be NULL)
 * @param result Output pointer where intercept * (1 << shift) is
 *               written (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lr or result is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if fewer than 2 points have been
 *         pushed,
 *         TIKU_KITS_ML_ERR_SINGULAR if all x values are identical
 */
int tiku_kits_ml_linreg_intercept(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Predict y for a given x value
 *
 * Computes y = slope * x + intercept using the running accumulators
 * directly (no intermediate slope/intercept extraction needed).
 * The result is in the original (unscaled) integer domain, truncated
 * by integer division.  O(1).
 *
 * @param lr     Model with >= 2 points that have distinct x values
 *               (must not be NULL)
 * @param x      Input x value (independent variable)
 * @param result Output pointer where the predicted y is written in
 *               the original scale, not Q-format (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lr or result is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if fewer than 2 points have been
 *         pushed,
 *         TIKU_KITS_ML_ERR_SINGULAR if all x values are identical
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
 *
 * R-squared measures how well the linear model fits the data.
 * A value of (1 << shift) indicates a perfect fit; 0 indicates
 * no linear relationship.  The result is clamped to [0, 1 << shift]
 * to guard against rounding overshoot.
 *
 * Computed as:
 *   R2 = (n*Sxy - Sx*Sy)^2 / ((n*Sxx - Sx^2) * (n*Syy - Sy^2))
 *
 * If all y values are identical (SSyy == 0), returns ERR_SINGULAR
 * and writes 0 to *result.
 *
 * @param lr     Model with >= 2 points that have distinct x values
 *               (must not be NULL)
 * @param result Output pointer where R2 * (1 << shift) is written,
 *               clamped to [0, 1 << shift] (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lr or result is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if fewer than 2 points have been
 *         pushed,
 *         TIKU_KITS_ML_ERR_SINGULAR if all x or y values are
 *         identical
 */
int tiku_kits_ml_linreg_r2(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of data points pushed
 *
 * Returns the total number of push() calls.  Safe to call with a
 * NULL pointer -- returns 0.
 *
 * @param lr Model, or NULL
 * @return Number of data points pushed, or 0 if lr is NULL
 */
uint16_t tiku_kits_ml_linreg_count(
    const struct tiku_kits_ml_linreg *lr);

#endif /* TIKU_KITS_ML_LINREG_H_ */
