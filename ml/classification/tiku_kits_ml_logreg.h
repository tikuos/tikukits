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
 * @brief Element type for feature values.
 *
 * Defaults to int32_t for integer-only targets (no FPU required).
 * All ML sub-modules share this type so that feature vectors can be
 * passed between classifiers without casting.
 *
 * Override before including this header to change the type:
 * @code
 *   #define TIKU_KITS_ML_ELEM_TYPE int16_t
 *   #include "tiku_kits_ml_logreg.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * @brief Maximum number of input features.
 *
 * The weight vector has (MAX_FEATURES + 1) entries: weights[0] is
 * the bias and weights[1..n] are the feature weights.  Total static
 * memory for weights is (MAX_FEATURES + 1) * sizeof(int32_t) bytes.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_LOGREG_MAX_FEATURES 16
 *   #include "tiku_kits_ml_logreg.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_LOGREG_MAX_FEATURES
#define TIKU_KITS_ML_LOGREG_MAX_FEATURES 8
#endif

/**
 * @brief Default number of fractional bits for fixed-point weights
 *        and probability outputs.
 *
 * With shift=8 the resolution is ~0.004 (1/256).  Probabilities
 * are returned in the range [0, 1 << shift], where 1 << shift
 * represents 1.0.  Larger shift gives finer probability resolution
 * but reduces the representable weight range.
 *
 * Override before including this header to change the default:
 * @code
 *   #define TIKU_KITS_ML_LOGREG_SHIFT 10
 *   #include "tiku_kits_ml_logreg.h"
 * @endcode
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
 * Models P(y=1|x) = sigmoid(w0 + w1*x1 + ... + wn*xn) and trains
 * via stochastic gradient descent.  The sigmoid is approximated by
 * a 3-piece linear function to avoid floating-point math:
 *
 *     sigmoid(z) ~= 0          if z <= -4
 *                   z/8 + 0.5  if -4 < z < 4
 *                   1          if z >= 4
 *
 * The weight vector layout is:
 *   - @c weights[0] -- bias term (intercept)
 *   - @c weights[1..n_features] -- one weight per input feature
 *
 * All weights and the learning rate are stored in Q(shift)
 * fixed-point.  Probabilities are output in the same format,
 * clamped to [0, 1 << shift].
 *
 * @note Because all storage lives inside the struct, no heap
 *       allocation is needed -- just declare the model as a static
 *       or local variable.
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
        /**< Weight vector in Q(shift) fixed-point:
             [0] = bias, [1..n_features] = feature weights */
    uint8_t n_features;    /**< Dimensionality of input feature
                                vectors (set at init time) */
    uint8_t shift;         /**< Fixed-point fractional bits for all
                                Q-format values in this model */
    int32_t learning_rate; /**< SGD step size in Q(shift) (must be > 0);
                                default ~0.1 */
    uint16_t n;            /**< Total number of train() calls seen
                                (monotonically increasing) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a logistic regression model
 *
 * Zeros the weight vector, sets the feature dimensionality and
 * fixed-point shift, and applies a default learning rate of
 * approximately 0.1 in Q(shift): (1 << shift) / 10.  The default
 * is clamped to a minimum of 1.
 *
 * @param lg         Model to initialize (must not be NULL)
 * @param n_features Number of input features
 *                   (1..TIKU_KITS_ML_LOGREG_MAX_FEATURES)
 * @param shift      Number of fixed-point fractional bits (1..30)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lg is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if n_features is 0 or exceeds
 *         MAX_FEATURES, or shift is out of range
 */
int tiku_kits_ml_logreg_init(struct tiku_kits_ml_logreg *lg,
                              uint8_t n_features,
                              uint8_t shift);

/**
 * @brief Reset all weights and training count to zero
 *
 * Zeros the weight vector (bias + features) and resets the training
 * counter to 0.  Preserves n_features, shift, and learning_rate so
 * the model can be retrained with the same configuration.
 *
 * @param lg Model to reset (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lg is NULL
 */
int tiku_kits_ml_logreg_reset(struct tiku_kits_ml_logreg *lg);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the SGD learning rate
 *
 * Controls the step size for each weight update.  Can be changed
 * between training calls to implement a learning-rate schedule.
 *
 * @param lg            Model (must not be NULL)
 * @param learning_rate Learning rate in Q(shift) fixed-point
 *                      (must be > 0; typical Q8 values: 26 for ~0.1,
 *                      3 for ~0.01)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lg is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if learning_rate <= 0
 */
int tiku_kits_ml_logreg_set_lr(struct tiku_kits_ml_logreg *lg,
                                int32_t learning_rate);

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the model with one sample using SGD
 *
 * Performs one step of stochastic gradient descent on the
 * cross-entropy loss with a piecewise-linear sigmoid:
 *   1. Compute z = w . x + bias  (dot product)
 *   2. Compute sig = sigmoid_approx(z)
 *   3. error = y_scaled - sig  (y_scaled = y << shift)
 *   4. Update: w_j += lr * error * x_j  (all in Q(shift))
 *
 * O(n_features) per call.
 *
 * @param lg Model (must not be NULL)
 * @param x  Feature vector of length n_features (must not be NULL)
 * @param y  Binary label: 0 or 1 (other values are rejected)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lg or x is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if y > 1
 */
int tiku_kits_ml_logreg_train(struct tiku_kits_ml_logreg *lg,
                               const tiku_kits_ml_elem_t *x,
                               uint8_t y);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Predict the probability P(y=1|x) as fixed-point
 *
 * Computes sigmoid(w . x + bias) using the piecewise-linear
 * approximation and writes the result clamped to [0, 1 << shift].
 * O(n_features) for the dot product plus O(1) for the sigmoid.
 *
 * @param lg     Model (must not be NULL)
 * @param x      Feature vector of length n_features
 *               (must not be NULL)
 * @param result Output pointer where the probability in Q(shift)
 *               is written (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lg, x, or result is NULL
 */
int tiku_kits_ml_logreg_predict_proba(
    const struct tiku_kits_ml_logreg *lg,
    const tiku_kits_ml_elem_t *x,
    int32_t *result);

/**
 * @brief Predict the binary class label (0 or 1)
 *
 * Evaluates predict_proba and thresholds at 0.5:
 * result = (prob_q >= 1 << (shift - 1)) ? 1 : 0.
 * O(n_features).
 *
 * @param lg     Model (must not be NULL)
 * @param x      Feature vector of length n_features
 *               (must not be NULL)
 * @param result Output pointer where 0 or 1 is written
 *               (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lg, x, or result is NULL
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
 *
 * Copies (n_features + 1) Q(shift) values into the caller's buffer.
 * weights[0] is the bias, weights[1..n] are feature weights.
 *
 * @param lg      Model (must not be NULL)
 * @param weights Output array; caller must provide space for at least
 *                (n_features + 1) int32_t entries (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lg or weights is NULL
 */
int tiku_kits_ml_logreg_get_weights(
    const struct tiku_kits_ml_logreg *lg,
    int32_t *weights);

/**
 * @brief Set the weight vector (for pre-trained model deployment)
 *
 * Loads externally computed weights so the model can be used for
 * inference without on-device training.  weights[0] must be the
 * bias and weights[1..n] the feature weights, all in Q(shift)
 * matching the model's configured shift.
 *
 * @param lg      Model (must be initialized; must not be NULL)
 * @param weights Input array of length (n_features + 1)
 *                (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if lg or weights is NULL
 */
int tiku_kits_ml_logreg_set_weights(
    struct tiku_kits_ml_logreg *lg,
    const int32_t *weights);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of training samples seen
 *
 * Returns the total number of train() calls.  Safe to call with a
 * NULL pointer -- returns 0.
 *
 * @param lg Model, or NULL
 * @return Number of training samples seen, or 0 if lg is NULL
 */
uint16_t tiku_kits_ml_logreg_count(
    const struct tiku_kits_ml_logreg *lg);

#endif /* TIKU_KITS_ML_LOGREG_H_ */
