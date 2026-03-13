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
 * @param lr  Model with accumulated sums
 * @param num Output: n*sum_xy - sum_x*sum_y
 * @param den Output: n*sum_xx - sum_x*sum_x
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_SIZE, or
 *         TIKU_KITS_ML_ERR_SINGULAR
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

uint16_t tiku_kits_ml_linreg_count(
    const struct tiku_kits_ml_linreg *lr)
{
    if (lr == NULL) {
        return 0;
    }
    return lr->n;
}
