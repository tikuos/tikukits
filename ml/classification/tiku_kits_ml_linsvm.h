/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_linsvm.h - Linear Support Vector Machine classifier
 *
 * Platform-independent online linear SVM for embedded systems.
 * Fits a binary classifier y = sign(w . x + bias) using stochastic
 * sub-gradient descent on the hinge loss with L2 regularization,
 * computed entirely with integer / fixed-point arithmetic (no FPU
 * required).
 *
 * Training samples are fed one at a time via tiku_kits_ml_linsvm_train()
 * with labels +1 or -1. Weights are updated in-place using the Pegasos
 * online SVM algorithm. Pre-trained weights can be loaded directly for
 * inference-only deployment.
 *
 * The decision function is:
 *
 *     f(x) = w0 + w1*x1 + ... + wn*xn
 *     class = f(x) >= 0 ? +1 : -1
 *
 * The margin is f(x) in Q(shift) fixed-point. The distance to the
 * decision boundary is proportional to |f(x)| / ||w||.
 *
 * Weight update rule (hinge loss with L2 regularization):
 *
 *     if y * f(x) < 1:
 *         w_j += lr * (y * x_j - lambda * w_j)
 *     else:
 *         w_j -= lr * lambda * w_j
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

#ifndef TIKU_KITS_ML_LINSVM_H_
#define TIKU_KITS_ML_LINSVM_H_

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
 *   #include "tiku_kits_ml_linsvm.h"
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
 *   #define TIKU_KITS_ML_LINSVM_MAX_FEATURES 16
 *   #include "tiku_kits_ml_linsvm.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_LINSVM_MAX_FEATURES
#define TIKU_KITS_ML_LINSVM_MAX_FEATURES 8
#endif

/**
 * @brief Default number of fractional bits for fixed-point weights
 *        and margin outputs.
 *
 * With shift=8 the resolution is ~0.004 (1/256).  Larger shift
 * gives finer weight precision but reduces the representable
 * integer range before overflow.  Weights, learning rate, lambda,
 * and margin outputs all use this Q format.
 *
 * Override before including this header to change the default:
 * @code
 *   #define TIKU_KITS_ML_LINSVM_SHIFT 10
 *   #include "tiku_kits_ml_linsvm.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_LINSVM_SHIFT
#define TIKU_KITS_ML_LINSVM_SHIFT 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_ml_elem_t
 *  @brief Element type used for all feature values
 */
typedef TIKU_KITS_ML_ELEM_TYPE tiku_kits_ml_elem_t;

/**
 * @struct tiku_kits_ml_linsvm
 * @brief Online binary linear SVM classifier
 *
 * Fits y = sign(w . x + bias) using Pegasos-style stochastic
 * sub-gradient descent on the hinge loss with L2 regularization.
 * All arithmetic is integer / fixed-point with int64_t intermediates
 * to prevent overflow.
 *
 * The weight vector layout is:
 *   - @c weights[0] -- bias term (intercept)
 *   - @c weights[1..n_features] -- one weight per input feature
 *
 * All weights, the learning rate, and lambda are stored in Q(shift)
 * fixed-point.  To recover a real-valued weight, divide by
 * (1 << shift).
 *
 * Training is online: samples are fed one at a time and weights are
 * updated in-place.  Pre-trained weights can also be loaded directly
 * via set_weights() for inference-only deployment.
 *
 * @note Because all storage lives inside the struct, no heap
 *       allocation is needed -- just declare the model as a static
 *       or local variable.
 *
 * Example:
 * @code
 *   struct tiku_kits_ml_linsvm svm;
 *   int8_t cls;
 *
 *   tiku_kits_ml_linsvm_init(&svm, 2, 8);
 *   tiku_kits_ml_linsvm_set_lr(&svm, 26);     // ~0.1 in Q8
 *   tiku_kits_ml_linsvm_set_lambda(&svm, 3);   // ~0.01 in Q8
 *
 *   // Train: class +1 when x1+x2 > 0
 *   tiku_kits_ml_elem_t x0[] = {-5, -3};
 *   tiku_kits_ml_linsvm_train(&svm, x0, -1);
 *   tiku_kits_ml_elem_t x1[] = {5, 3};
 *   tiku_kits_ml_linsvm_train(&svm, x1, 1);
 *
 *   // Predict
 *   tiku_kits_ml_elem_t xt[] = {4, 2};
 *   tiku_kits_ml_linsvm_predict(&svm, xt, &cls);  // cls = 1
 * @endcode
 */
struct tiku_kits_ml_linsvm {
    int32_t weights[TIKU_KITS_ML_LINSVM_MAX_FEATURES + 1];
        /**< Weight vector in Q(shift) fixed-point:
             [0] = bias, [1..n_features] = feature weights */
    uint8_t n_features;    /**< Dimensionality of input feature
                                vectors (set at init time) */
    uint8_t shift;         /**< Fixed-point fractional bits for all
                                Q-format values in this model */
    int32_t learning_rate; /**< SGD step size in Q(shift) (must be > 0);
                                default ~0.1 */
    int32_t lambda;        /**< L2 regularization strength in Q(shift)
                                (>= 0); higher values widen the margin
                                but may underfit */
    uint16_t n;            /**< Total number of train() calls seen
                                (monotonically increasing) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a linear SVM model
 *
 * Zeros the weight vector, sets the feature dimensionality and
 * fixed-point shift, and applies default hyperparameters:
 * learning rate ~0.1 and lambda ~0.01 in Q(shift).  A model must
 * be trained (or have weights loaded) before prediction is useful.
 *
 * @param svm        Model to initialize (must not be NULL)
 * @param n_features Number of input features
 *                   (1..TIKU_KITS_ML_LINSVM_MAX_FEATURES)
 * @param shift      Number of fixed-point fractional bits (1..30)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if n_features is 0 or exceeds
 *         MAX_FEATURES, or shift is out of range
 */
int tiku_kits_ml_linsvm_init(struct tiku_kits_ml_linsvm *svm,
                               uint8_t n_features,
                               uint8_t shift);

/**
 * @brief Reset all weights and training count to zero
 *
 * Zeros the weight vector (bias + features) and resets the training
 * count to 0.  Preserves n_features, shift, learning_rate, and
 * lambda so the model can be retrained with the same configuration.
 *
 * @param svm Model to reset (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm is NULL
 */
int tiku_kits_ml_linsvm_reset(struct tiku_kits_ml_linsvm *svm);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the SGD learning rate
 *
 * Controls the step size for each weight update.  Can be changed
 * between training calls to implement a learning-rate schedule.
 *
 * @param svm           Model (must not be NULL)
 * @param learning_rate Learning rate in Q(shift) fixed-point
 *                      (must be > 0; typical Q8 values: 26 for ~0.1,
 *                      3 for ~0.01)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if learning_rate <= 0
 */
int tiku_kits_ml_linsvm_set_lr(struct tiku_kits_ml_linsvm *svm,
                                 int32_t learning_rate);

/**
 * @brief Set the L2 regularization strength
 *
 * Higher lambda produces a wider margin but may underfit the
 * training data.  Set to 0 to disable regularization entirely.
 *
 * @param svm    Model (must not be NULL)
 * @param lambda Regularization parameter in Q(shift) fixed-point
 *               (must be >= 0; typical Q8 value: 3 for ~0.01)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if lambda < 0
 */
int tiku_kits_ml_linsvm_set_lambda(struct tiku_kits_ml_linsvm *svm,
                                     int32_t lambda);

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the model with one sample using SGD on the hinge loss
 *
 * Performs one step of Pegasos-style sub-gradient descent.  If the
 * sample violates the margin (y * f(x) < 1 in Q(shift)), weights
 * are updated towards the correct side while also applying L2
 * regularization.  If the margin is satisfied, only regularization
 * is applied.  O(n_features) per call.
 *
 * @param svm Model (must not be NULL)
 * @param x   Feature vector of length n_features (must not be NULL)
 * @param y   Label: +1 or -1 (other values are rejected)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm or x is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if y is not +1 or -1
 */
int tiku_kits_ml_linsvm_train(struct tiku_kits_ml_linsvm *svm,
                                const tiku_kits_ml_elem_t *x,
                                int8_t y);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the decision function f(x) = w . x + bias
 *
 * Returns the signed margin in Q(shift) fixed-point.  A positive
 * result indicates class +1, a negative result indicates class -1.
 * The magnitude is proportional to the distance from the decision
 * boundary (modulo ||w||).  O(n_features) for the dot product.
 *
 * @param svm    Model (must not be NULL)
 * @param x      Feature vector of length n_features
 *               (must not be NULL)
 * @param result Output pointer where f(x) in Q(shift) is written
 *               (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm, x, or result is NULL
 */
int tiku_kits_ml_linsvm_decision(
    const struct tiku_kits_ml_linsvm *svm,
    const tiku_kits_ml_elem_t *x,
    int32_t *result);

/**
 * @brief Predict the binary class label (+1 or -1)
 *
 * Evaluates the decision function and thresholds at zero:
 * f(x) >= 0 yields +1, otherwise -1.  O(n_features).
 *
 * @param svm    Model (must not be NULL)
 * @param x      Feature vector of length n_features
 *               (must not be NULL)
 * @param result Output pointer where +1 or -1 is written
 *               (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm, x, or result is NULL
 */
int tiku_kits_ml_linsvm_predict(
    const struct tiku_kits_ml_linsvm *svm,
    const tiku_kits_ml_elem_t *x,
    int8_t *result);

/*---------------------------------------------------------------------------*/
/* WEIGHT ACCESS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the weight vector (bias + feature weights)
 *
 * Copies (n_features + 1) Q(shift) values into the caller's buffer.
 * weights[0] is the bias, weights[1..n] are feature weights.
 *
 * @param svm     Model (must not be NULL)
 * @param weights Output array; caller must provide space for at least
 *                (n_features + 1) int32_t entries (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm or weights is NULL
 */
int tiku_kits_ml_linsvm_get_weights(
    const struct tiku_kits_ml_linsvm *svm,
    int32_t *weights);

/**
 * @brief Set the weight vector (for pre-trained model deployment)
 *
 * Loads externally computed weights so the model can be used for
 * inference without on-device training.  weights[0] must be the
 * bias and weights[1..n] the feature weights, all in Q(shift)
 * matching the model's configured shift.
 *
 * @param svm     Model (must be initialized; must not be NULL)
 * @param weights Input array of length (n_features + 1)
 *                (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if svm or weights is NULL
 */
int tiku_kits_ml_linsvm_set_weights(
    struct tiku_kits_ml_linsvm *svm,
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
 * @param svm Model, or NULL
 * @return Number of training samples seen, or 0 if svm is NULL
 */
uint16_t tiku_kits_ml_linsvm_count(
    const struct tiku_kits_ml_linsvm *svm);

#endif /* TIKU_KITS_ML_LINSVM_H_ */
