/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_logreg.h - Binary logistic regression library
 *
 * Platform-independent online logistic regression for embedded systems.
 * Fits a binary classifier P(y=1|x) = sigmoid(w0 + w1*x1 + ... + wn*xn)
 * using stochastic gradient descent, computed entirely with integer /
 * fixed-point arithmetic (no FPU required).
 *
 * Training samples are fed one at a time via tiku_kits_ml_logreg_train().
 * Weights are updated in-place using SGD. Pre-trained weights can also
 * be loaded directly for inference-only deployment.
 *
 * Probabilities and weights are returned as fixed-point integers with
 * configurable fractional bits (default 8). To recover the real value,
 * divide by (1 << shift):
 *
 *     real_prob = prob_q / (1 << shift)
 *
 * The sigmoid function is approximated using a 3-piece linear function:
 *
 *     sigmoid(z) ~= 0            if z <= -4
 *                   z/8 + 0.5    if -4 < z < 4
 *                   1            if z >= 4
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

#ifndef TIKU_KITS_ML_LOGREG_H_
#define TIKU_KITS_ML_LOGREG_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ml.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Element type for feature values.
 * Defaults to int32_t for integer-only targets (no FPU).
 * Must match the type defined in sibling ML modules.
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * Maximum number of input features.
 * The weight vector has (MAX_FEATURES + 1) entries to include the bias.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_LOGREG_MAX_FEATURES
#define TIKU_KITS_ML_LOGREG_MAX_FEATURES 8
#endif

/**
 * Default number of fractional bits for fixed-point weights and
 * probability outputs. With shift=8, the resolution is ~0.004.
 * Override before including this header to change the default.
 */
#ifndef TIKU_KITS_ML_LOGREG_SHIFT
#define TIKU_KITS_ML_LOGREG_SHIFT 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_ml_elem_t
 *  @brief Element type used for all feature values
 */
typedef TIKU_KITS_ML_ELEM_TYPE tiku_kits_ml_elem_t;

/**
 * @struct tiku_kits_ml_logreg
 * @brief Online binary logistic regression classifier
 *
 * Fits P(y=1|x) = sigmoid(w0 + w1*x1 + ... + wn*xn) using stochastic
 * gradient descent with a piecewise-linear sigmoid approximation.
 *
 * weights[0] is the bias term. weights[1..n_features] correspond to
 * each input feature. All weights are stored in Q(shift) fixed-point.
 *
 * Example:
 * @code
 *   struct tiku_kits_ml_logreg lg;
 *   int32_t prob_q;
 *   uint8_t cls;
 *
 *   tiku_kits_ml_logreg_init(&lg, 2, 8);
 *   tiku_kits_ml_logreg_set_lr(&lg, 26);  // ~0.1 in Q8
 *
 *   // Train: class 1 when x1+x2 > 0
 *   tiku_kits_ml_elem_t x0[] = {-10, -5};
 *   tiku_kits_ml_logreg_train(&lg, x0, 0);
 *   tiku_kits_ml_elem_t x1[] = {10, 5};
 *   tiku_kits_ml_logreg_train(&lg, x1, 1);
 *
 *   // Predict
 *   tiku_kits_ml_elem_t xt[] = {8, 3};
 *   tiku_kits_ml_logreg_predict_proba(&lg, xt, &prob_q);
 *   tiku_kits_ml_logreg_predict(&lg, xt, &cls);
 * @endcode
 */
struct tiku_kits_ml_logreg {
    int32_t weights[TIKU_KITS_ML_LOGREG_MAX_FEATURES + 1];
        /**< Weight vector: [0]=bias, [1..n]=feature weights (Q shift) */
    uint8_t n_features;    /**< Number of input features */
    uint8_t shift;         /**< Fixed-point fractional bits */
    int32_t learning_rate; /**< SGD learning rate (Q shift) */
    uint16_t n;            /**< Number of training samples seen */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a logistic regression model
 * @param lg         Model to initialize
 * @param n_features Number of input features (1..MAX_FEATURES)
 * @param shift      Number of fixed-point fractional bits (0..30)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (invalid n_features or shift)
 *
 * All weights are zeroed. The learning rate is set to a default of
 * approximately 0.1 in the chosen Q format: (1 << shift) / 10.
 */
int tiku_kits_ml_logreg_init(struct tiku_kits_ml_logreg *lg,
                              uint8_t n_features,
                              uint8_t shift);

/**
 * @brief Reset all weights and training count to zero
 * @param lg Model to reset
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Preserves n_features, shift, and learning_rate.
 */
int tiku_kits_ml_logreg_reset(struct tiku_kits_ml_logreg *lg);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the SGD learning rate
 * @param lg            Model
 * @param learning_rate Learning rate in Q(shift) fixed-point (must be > 0)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (learning_rate <= 0)
 *
 * A typical value for shift=8 is 26 (~0.1) or 3 (~0.01).
 */
int tiku_kits_ml_logreg_set_lr(struct tiku_kits_ml_logreg *lg,
                                int32_t learning_rate);

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the model with one sample using SGD
 * @param lg Model
 * @param x  Feature vector of length n_features
 * @param y  Label: 0 or 1
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (y not 0 or 1)
 *
 * Performs one step of stochastic gradient descent:
 *   error = y_scaled - sigmoid(w . x + bias)
 *   w_j  += learning_rate * error * x_j  (scaled to Q format)
 *
 * The sigmoid is approximated by a 3-piece linear function for
 * integer-only computation.
 */
int tiku_kits_ml_logreg_train(struct tiku_kits_ml_logreg *lg,
                               const tiku_kits_ml_elem_t *x,
                               uint8_t y);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Predict the probability P(y=1|x) as fixed-point
 * @param lg     Model
 * @param x      Feature vector of length n_features
 * @param result Output: probability * (1 << shift), range [0, 1<<shift]
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Computes sigmoid(w . x + bias) using the piecewise-linear
 * approximation. The result is clamped to [0, 1 << shift].
 */
int tiku_kits_ml_logreg_predict_proba(
    const struct tiku_kits_ml_logreg *lg,
    const tiku_kits_ml_elem_t *x,
    int32_t *result);

/**
 * @brief Predict the binary class label (0 or 1)
 * @param lg     Model
 * @param x      Feature vector of length n_features
 * @param result Output: 0 if P(y=1|x) < 0.5, else 1
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Equivalent to: predict_proba >= (1 << (shift - 1)) ? 1 : 0
 */
int tiku_kits_ml_logreg_predict(
    const struct tiku_kits_ml_logreg *lg,
    const tiku_kits_ml_elem_t *x,
    uint8_t *result);

/*---------------------------------------------------------------------------*/
/* WEIGHT ACCESS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the weight vector (bias + feature weights)
 * @param lg      Model
 * @param weights Output array of length (n_features + 1)
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * weights[0] is the bias, weights[1..n] are feature weights.
 * All values are in Q(shift) fixed-point.
 */
int tiku_kits_ml_logreg_get_weights(
    const struct tiku_kits_ml_logreg *lg,
    int32_t *weights);

/**
 * @brief Set the weight vector (for pre-trained model deployment)
 * @param lg      Model (must be initialized)
 * @param weights Input array of length (n_features + 1)
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * weights[0] is the bias, weights[1..n] are feature weights.
 * All values must be in Q(shift) fixed-point matching the model's shift.
 */
int tiku_kits_ml_logreg_set_weights(
    struct tiku_kits_ml_logreg *lg,
    const int32_t *weights);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of training samples seen
 * @param lg Model
 * @return Number of training samples, or 0 if lg is NULL
 */
uint16_t tiku_kits_ml_logreg_count(
    const struct tiku_kits_ml_logreg *lg);

#endif /* TIKU_KITS_ML_LOGREG_H_ */
