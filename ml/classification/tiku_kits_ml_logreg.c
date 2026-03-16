/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_logreg.c - Binary logistic regression library
 *
 * Platform-independent implementation of online binary logistic
 * regression using stochastic gradient descent. All computation uses
 * integer / fixed-point arithmetic with int64_t intermediates.
 * No heap allocation.
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

#include "tiku_kits_ml_logreg.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the linear combination z = w0 + w1*x1 + ... + wn*xn
 *
 * Weights are Q(shift) and features are plain integers, so each
 * product w_j * x_j is Q(shift).  The sum is accumulated in int64_t
 * to prevent overflow, then truncated to int32_t for the caller.
 */
static int32_t dot_product(const struct tiku_kits_ml_logreg *lg,
                           const tiku_kits_ml_elem_t *x)
{
    int64_t z = (int64_t)lg->weights[0]; /* bias */
    uint8_t j;

    for (j = 0; j < lg->n_features; j++) {
        z += (int64_t)lg->weights[j + 1] * (int64_t)x[j];
    }

    return (int32_t)z;
}

/**
 * @brief Piecewise-linear sigmoid approximation in fixed-point
 *
 * Three-segment approximation that avoids exp() entirely:
 *   - z <= -4:  return 0   (deep negative saturation)
 *   - -4 < z < 4:  return z/8 + 0.5   (linear region)
 *   - z >= 4:  return 1   (deep positive saturation)
 *
 * In Q(shift) arithmetic the boundaries become -(4 << shift) and
 * +(4 << shift), the slope becomes >> 3, and 0.5 becomes
 * (1 << (shift-1)).  A final clamp guards against rounding
 * overshoots at the segment boundaries.
 */
static int32_t sigmoid_approx(int32_t z_q, uint8_t shift)
{
    int32_t lo = -(int32_t)(4 << shift);
    int32_t hi = (int32_t)(4 << shift);
    int32_t one = (int32_t)(1 << shift);
    int32_t half = (int32_t)(1 << (shift - 1));
    int32_t result;

    if (z_q <= lo) {
        return 0;
    }
    if (z_q >= hi) {
        return one;
    }

    /* Linear region: z/8 + 0.5 */
    result = (z_q >> 3) + half;

    /* Clamp to valid range */
    if (result < 0) {
        result = 0;
    }
    if (result > one) {
        result = one;
    }

    return result;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a logistic regression model
 *
 * Validates parameters, zeros the weight vector, and sets the
 * default learning rate to ~0.1 in Q(shift).  The default is
 * clamped to a minimum of 1 quantum.
 */
int tiku_kits_ml_logreg_init(struct tiku_kits_ml_logreg *lg,
                              uint8_t n_features,
                              uint8_t shift)
{
    uint8_t i;

    if (lg == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (n_features == 0
        || n_features > TIKU_KITS_ML_LOGREG_MAX_FEATURES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (shift < 1 || shift > 30) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    lg->n_features = n_features;
    lg->shift = shift;
    lg->n = 0;

    /* Default learning rate: ~0.1 in Q(shift) */
    lg->learning_rate = (int32_t)((1 << shift) / 10);
    if (lg->learning_rate < 1) {
        lg->learning_rate = 1;
    }

    /* Zero all weights (bias + features) */
    for (i = 0; i <= n_features; i++) {
        lg->weights[i] = 0;
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset all weights and training count to zero
 *
 * Preserves n_features, shift, and learning_rate.
 */
int tiku_kits_ml_logreg_reset(struct tiku_kits_ml_logreg *lg)
{
    uint8_t i;

    if (lg == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    for (i = 0; i <= lg->n_features; i++) {
        lg->weights[i] = 0;
    }
    lg->n = 0;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the SGD learning rate
 *
 * Stores the new learning rate for subsequent train() calls.
 */
int tiku_kits_ml_logreg_set_lr(struct tiku_kits_ml_logreg *lg,
                                int32_t learning_rate)
{
    if (lg == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (learning_rate <= 0) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    lg->learning_rate = learning_rate;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the model with one sample using SGD
 *
 * Computes the forward pass (dot product + sigmoid), then performs
 * one gradient step.  The error is computed as y_scaled - sigmoid(z)
 * in Q(shift), then scaled by the learning rate.  Bias update uses
 * implicit x=1; feature weight updates multiply lr_error by x[j].
 * All intermediate products use int64_t to prevent overflow.
 */
int tiku_kits_ml_logreg_train(struct tiku_kits_ml_logreg *lg,
                               const tiku_kits_ml_elem_t *x,
                               uint8_t y)
{
    int32_t z_q, sig_q, error;
    int64_t lr_error;
    uint8_t j;

    if (lg == NULL || x == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (y > 1) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    /* Forward pass: z = w . x + bias */
    z_q = dot_product(lg, x);

    /* Sigmoid approximation */
    sig_q = sigmoid_approx(z_q, lg->shift);

    /*
     * Gradient descent step.
     *
     * error = y_scaled - sigmoid(z), in Q(shift)
     *   where y_scaled = y * (1 << shift)
     *
     * Weight update for feature j:
     *   w_j += lr * error * x_j
     *
     * In fixed-point:
     *   lr_error = (lr_q * error) >> shift   [Q(shift)]
     *   w_j += (int32_t)(lr_error * x_j)     [Q(shift)]
     *
     * For the bias (x = 1):
     *   w_0 += (int32_t)lr_error
     */
    error = (int32_t)((uint32_t)y << lg->shift) - sig_q;
    lr_error = ((int64_t)lg->learning_rate * (int64_t)error)
               >> lg->shift;

    /* Update bias (implicit x = 1) */
    lg->weights[0] += (int32_t)lr_error;

    /* Update feature weights */
    for (j = 0; j < lg->n_features; j++) {
        lg->weights[j + 1] += (int32_t)(lr_error * (int64_t)x[j]);
    }

    lg->n++;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Predict the probability P(y=1|x) as fixed-point
 *
 * Computes the dot product and applies the piecewise-linear
 * sigmoid.  Result is in [0, 1 << shift].
 */
int tiku_kits_ml_logreg_predict_proba(
    const struct tiku_kits_ml_logreg *lg,
    const tiku_kits_ml_elem_t *x,
    int32_t *result)
{
    int32_t z_q;

    if (lg == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    z_q = dot_product(lg, x);
    *result = sigmoid_approx(z_q, lg->shift);

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Predict the binary class label (0 or 1)
 *
 * Evaluates predict_proba and thresholds at the midpoint
 * (1 << (shift-1)), which represents 0.5.
 */
int tiku_kits_ml_logreg_predict(
    const struct tiku_kits_ml_logreg *lg,
    const tiku_kits_ml_elem_t *x,
    uint8_t *result)
{
    int32_t prob_q;
    int rc;

    if (lg == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    rc = tiku_kits_ml_logreg_predict_proba(lg, x, &prob_q);
    if (rc != TIKU_KITS_ML_OK) {
        return rc;
    }

    *result = (prob_q >= (int32_t)(1 << (lg->shift - 1))) ? 1 : 0;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* WEIGHT ACCESS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the weight vector (bias + feature weights)
 *
 * Copies (n_features + 1) entries into the caller's buffer.
 */
int tiku_kits_ml_logreg_get_weights(
    const struct tiku_kits_ml_logreg *lg,
    int32_t *weights)
{
    uint8_t i;

    if (lg == NULL || weights == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    for (i = 0; i <= lg->n_features; i++) {
        weights[i] = lg->weights[i];
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Set the weight vector (for pre-trained model deployment)
 *
 * Copies (n_features + 1) entries from the caller's buffer.
 */
int tiku_kits_ml_logreg_set_weights(
    struct tiku_kits_ml_logreg *lg,
    const int32_t *weights)
{
    uint8_t i;

    if (lg == NULL || weights == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    for (i = 0; i <= lg->n_features; i++) {
        lg->weights[i] = weights[i];
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of training samples seen
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint16_t tiku_kits_ml_logreg_count(
    const struct tiku_kits_ml_logreg *lg)
{
    if (lg == NULL) {
        return 0;
    }
    return lg->n;
}
