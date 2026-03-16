/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_linreg.c - Simple linear regression library
 *
 * Platform-independent implementation of streaming OLS linear
 * regression. All computation uses integer / fixed-point arithmetic
 * with int64_t accumulators. No heap allocation.
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

#include "tiku_kits_ml_linreg.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute slope numerator and denominator from accumulators
 *
 * Extracts the two components of the OLS slope formula:
 *   numerator   = n * Sxy - Sx * Sy
 *   denominator = n * Sxx - Sx * Sx
 *
 * Returns ERR_SIZE if fewer than 2 points have been pushed, or
 * ERR_SINGULAR if the denominator is zero (all x values identical).
 * This helper is reused by slope(), intercept(), predict(), and r2()
 * to avoid duplicating the accumulator arithmetic.
 */
static int slope_parts(const struct tiku_kits_ml_linreg *lr,
                       int64_t *num, int64_t *den)
{
    int64_t d;

    if (lr->n < 2) {
        return TIKU_KITS_ML_ERR_SIZE;
    }

    *num = (int64_t)lr->n * lr->sum_xy
           - lr->sum_x * lr->sum_y;

    d = (int64_t)lr->n * lr->sum_xx
        - lr->sum_x * lr->sum_x;

    if (d == 0) {
        return TIKU_KITS_ML_ERR_SINGULAR;
    }

    *den = d;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a linear regression model
 *
 * Zeros all five accumulators and the sample count.
 */
int tiku_kits_ml_linreg_init(struct tiku_kits_ml_linreg *lr,
                              uint8_t shift)
{
    if (lr == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (shift > 30) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    lr->sum_x = 0;
    lr->sum_y = 0;
    lr->sum_xx = 0;
    lr->sum_xy = 0;
    lr->sum_yy = 0;
    lr->n = 0;
    lr->shift = shift;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset a model, clearing all data points
 *
 * Zeros all accumulators and the sample count.  Preserves shift.
 */
int tiku_kits_ml_linreg_reset(struct tiku_kits_ml_linreg *lr)
{
    if (lr == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    lr->sum_x = 0;
    lr->sum_y = 0;
    lr->sum_xx = 0;
    lr->sum_xy = 0;
    lr->sum_yy = 0;
    lr->n = 0;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* DATA INPUT                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a data point (x, y) into the model
 *
 * Updates all five accumulators in O(1).  Intermediate products
 * are widened to int64_t before accumulation to prevent overflow
 * when x and y are large int32_t values.
 */
int tiku_kits_ml_linreg_push(struct tiku_kits_ml_linreg *lr,
                              tiku_kits_ml_elem_t x,
                              tiku_kits_ml_elem_t y)
{
    if (lr == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    lr->sum_x += (int64_t)x;
    lr->sum_y += (int64_t)y;
    lr->sum_xx += (int64_t)x * (int64_t)x;
    lr->sum_xy += (int64_t)x * (int64_t)y;
    lr->sum_yy += (int64_t)y * (int64_t)y;
    lr->n++;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* FITTED PARAMETERS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the fitted slope as a fixed-point value
 *
 * slope_q = (num << shift) / den, where num and den come from
 * slope_parts().  The left-shift applies the Q(shift) scaling.
 */
int tiku_kits_ml_linreg_slope(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result)
{
    int64_t num, den;
    int rc;

    if (lr == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    rc = slope_parts(lr, &num, &den);
    if (rc != TIKU_KITS_ML_OK) {
        return rc;
    }

    *result = (int32_t)((num << lr->shift) / den);
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the fitted intercept as a fixed-point value
 *
 * intercept = (Sy - slope * Sx) / n.  To avoid computing slope
 * explicitly, the formula is rearranged using the slope numerator
 * and denominator: inter_num = Sy * den - num * Sx, then
 * result = (inter_num << shift) / (n * den).
 */
int tiku_kits_ml_linreg_intercept(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result)
{
    int64_t num, den;
    int64_t inter_num;
    int rc;

    if (lr == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    rc = slope_parts(lr, &num, &den);
    if (rc != TIKU_KITS_ML_OK) {
        return rc;
    }

    /*
     * intercept = (sum_y - slope * sum_x) / n
     *           = (sum_y * den - num * sum_x) / (n * den)
     *
     * Scale to fixed-point by shifting numerator.
     */
    inter_num = lr->sum_y * den - num * lr->sum_x;
    *result = (int32_t)(
        (inter_num << lr->shift) / ((int64_t)lr->n * den));
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Predict y for a given x value
 *
 * Computes y = slope * x + intercept directly from accumulators
 * without extracting slope or intercept separately.  The algebra
 * simplifies to:
 *   y = (num * (n*x - Sx) + Sy * den) / (n * den)
 * which avoids any Q-format scaling -- the result is in the
 * original integer domain.
 */
int tiku_kits_ml_linreg_predict(
    const struct tiku_kits_ml_linreg *lr,
    tiku_kits_ml_elem_t x,
    tiku_kits_ml_elem_t *result)
{
    int64_t num, den;
    int64_t y_num;
    int rc;

    if (lr == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    rc = slope_parts(lr, &num, &den);
    if (rc != TIKU_KITS_ML_OK) {
        return rc;
    }

    /*
     * y = slope * x + intercept
     *   = (num/den) * x + (sum_y * den - num * sum_x) / (n * den)
     *   = (num * n * x + sum_y * den - num * sum_x) / (n * den)
     *   = (num * (n * x - sum_x) + sum_y * den) / (n * den)
     */
    y_num = num * ((int64_t)lr->n * (int64_t)x - lr->sum_x)
            + lr->sum_y * den;

    *result = (tiku_kits_ml_elem_t)(
        y_num / ((int64_t)lr->n * den));
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* GOODNESS OF FIT                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the R-squared (coefficient of determination)
 *
 * R2 = num^2 / (den * SSyy) where SSyy = n*Syy - Sy^2.
 * To produce a Q(shift) result, the numerator is shifted left.
 * To mitigate overflow when num^2 is very large, the division is
 * split: (num/den) * num, then the shift and final division by
 * SSyy.  The result is clamped to [0, 1 << shift] to guard
 * against rounding overshoot.
 */
int tiku_kits_ml_linreg_r2(
    const struct tiku_kits_ml_linreg *lr,
    int32_t *result)
{
    int64_t num, den;
    int64_t ss_yy;
    int rc;

    if (lr == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    rc = slope_parts(lr, &num, &den);
    if (rc != TIKU_KITS_ML_OK) {
        return rc;
    }

    /* SS_yy = n * sum_yy - sum_y^2 */
    ss_yy = (int64_t)lr->n * lr->sum_yy
            - lr->sum_y * lr->sum_y;

    if (ss_yy == 0) {
        /* All y values identical: R^2 is undefined, return 0 */
        *result = 0;
        return TIKU_KITS_ML_ERR_SINGULAR;
    }

    /*
     * R^2 = (n*Sxy - Sx*Sy)^2 / ((n*Sxx - Sx^2) * (n*Syy - Sy^2))
     *     = num^2 / (den * ss_yy)
     *
     * Scale by (1 << shift) for fixed-point output.
     * To avoid overflow: compute (num^2 << shift) / (den * ss_yy).
     * When num^2 is large, shift the numerator in parts.
     */
    *result = (int32_t)(
        ((num / den) * num << lr->shift) / ss_yy);

    /* Clamp to [0, 1<<shift] to guard against rounding overshoot */
    if (*result > (int32_t)(1 << lr->shift)) {
        *result = (int32_t)(1 << lr->shift);
    }
    if (*result < 0) {
        *result = 0;
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of data points pushed
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint16_t tiku_kits_ml_linreg_count(
    const struct tiku_kits_ml_linreg *lr)
{
    if (lr == NULL) {
        return 0;
    }
    return lr->n;
}
