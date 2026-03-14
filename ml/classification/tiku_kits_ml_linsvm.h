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
#ifndef TIKU_KITS_ML_LINSVM_MAX_FEATURES
#define TIKU_KITS_ML_LINSVM_MAX_FEATURES 8
#endif

/**
 * Default number of fractional bits for fixed-point weights and
 * margin outputs. With shift=8, the resolution is ~0.004.
 * Override before including this header to change the default.
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
 * Fits y = sign(w . x + bias) using stochastic sub-gradient
 * descent on the hinge loss with L2 regularization.
 *
 * weights[0] is the bias term. weights[1..n_features] correspond to
 * each input feature. All weights are stored in Q(shift) fixed-point.
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
        /**< Weight vector: [0]=bias, [1..n]=feature weights (Q shift) */
    uint8_t n_features;    /**< Number of input features */
    uint8_t shift;         /**< Fixed-point fractional bits */
    int32_t learning_rate; /**< SGD learning rate (Q shift) */
    int32_t lambda;        /**< L2 regularization strength (Q shift) */
    uint16_t n;            /**< Number of training samples seen */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a linear SVM model
 * @param svm        Model to initialize
 * @param n_features Number of input features (1..MAX_FEATURES)
 * @param shift      Number of fixed-point fractional bits (1..30)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (invalid n_features or shift)
 *
 * All weights are zeroed. Learning rate defaults to ~0.1 in Q(shift),
 * lambda defaults to ~0.01 in Q(shift).
 */
int tiku_kits_ml_linsvm_init(struct tiku_kits_ml_linsvm *svm,
                               uint8_t n_features,
                               uint8_t shift);

/**
 * @brief Reset all weights and training count to zero
 * @param svm Model to reset
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Preserves n_features, shift, learning_rate, and lambda.
 */
int tiku_kits_ml_linsvm_reset(struct tiku_kits_ml_linsvm *svm);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the SGD learning rate
 * @param svm           Model
 * @param learning_rate Learning rate in Q(shift) fixed-point (must be > 0)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (learning_rate <= 0)
 *
 * A typical value for shift=8 is 26 (~0.1) or 3 (~0.01).
 */
int tiku_kits_ml_linsvm_set_lr(struct tiku_kits_ml_linsvm *svm,
                                 int32_t learning_rate);

/**
 * @brief Set the L2 regularization strength
 * @param svm    Model
 * @param lambda Regularization parameter in Q(shift) (must be >= 0)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (lambda < 0)
 *
 * Higher lambda produces a wider margin but may underfit.
 * Set to 0 to disable regularization. Typical: 3 (~0.01 in Q8).
 */
int tiku_kits_ml_linsvm_set_lambda(struct tiku_kits_ml_linsvm *svm,
                                     int32_t lambda);

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the model with one sample using SGD
 * @param svm Model
 * @param x   Feature vector of length n_features
 * @param y   Label: +1 or -1
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (y not +1 or -1)
 *
 * Performs one step of sub-gradient descent on the hinge loss:
 *   if y * f(x) < (1 << shift):
 *       w_j += lr * (y * x_j - lambda * w_j)   [margin violation]
 *   else:
 *       w_j -= lr * lambda * w_j                [regularization only]
 */
int tiku_kits_ml_linsvm_train(struct tiku_kits_ml_linsvm *svm,
                                const tiku_kits_ml_elem_t *x,
                                int8_t y);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the decision function f(x) = w . x + bias
 * @param svm    Model
 * @param x      Feature vector of length n_features
 * @param result Output: f(x) in Q(shift) fixed-point (signed margin)
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Positive result indicates class +1, negative indicates class -1.
 * The magnitude is proportional to the distance from the decision
 * boundary.
 */
int tiku_kits_ml_linsvm_decision(
    const struct tiku_kits_ml_linsvm *svm,
    const tiku_kits_ml_elem_t *x,
    int32_t *result);

/**
 * @brief Predict the binary class label (+1 or -1)
 * @param svm    Model
 * @param x      Feature vector of length n_features
 * @param result Output: +1 if f(x) >= 0, else -1
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
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
 * @param svm     Model
 * @param weights Output array of length (n_features + 1)
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 */
int tiku_kits_ml_linsvm_get_weights(
    const struct tiku_kits_ml_linsvm *svm,
    int32_t *weights);

/**
 * @brief Set the weight vector (for pre-trained model deployment)
 * @param svm     Model (must be initialized)
 * @param weights Input array of length (n_features + 1)
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 */
int tiku_kits_ml_linsvm_set_weights(
    struct tiku_kits_ml_linsvm *svm,
    const int32_t *weights);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of training samples seen
 * @param svm Model
 * @return Number of training samples, or 0 if svm is NULL
 */
uint16_t tiku_kits_ml_linsvm_count(
    const struct tiku_kits_ml_linsvm *svm);

#endif /* TIKU_KITS_ML_LINSVM_H_ */
