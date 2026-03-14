/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_tnn.h - Tiny Neural Network classifier
 *
 * Platform-independent single-hidden-layer feedforward neural network
 * for embedded classification. Uses ReLU activation in the hidden
 * layer, linear output, and trains via backpropagation with SGD.
 * All computation uses integer / fixed-point arithmetic (no FPU).
 *
 * Architecture:
 *
 *     input (n_input) -> hidden (n_hidden, ReLU) -> output (n_output)
 *
 * The network classifies by argmax of the output layer:
 *
 *     h_j = ReLU( w_hidden[j][0] + sum_i w_hidden[j][i+1] * x[i] )
 *     o_k = w_output[k][0] + sum_j w_output[k][j+1] * h_j
 *     class = argmax_k( o_k )
 *
 * Hidden weights are initialized with small alternating values to
 * break symmetry. Output weights are zero-initialized.
 *
 * Training uses one-hot target encoding and SGD backpropagation:
 *
 *     delta_o[k] = o[k] - target[k]
 *     delta_h[j] = relu'(h[j]) * sum_k( w_output[k][j+1] * delta_o[k] )
 *     w -= lr * gradient
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

#ifndef TIKU_KITS_ML_TNN_H_
#define TIKU_KITS_ML_TNN_H_

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
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * Maximum number of input features.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_TNN_MAX_INPUT
#define TIKU_KITS_ML_TNN_MAX_INPUT 8
#endif

/**
 * Maximum number of hidden neurons.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_TNN_MAX_HIDDEN
#define TIKU_KITS_ML_TNN_MAX_HIDDEN 8
#endif

/**
 * Maximum number of output neurons (classes).
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_TNN_MAX_OUTPUT
#define TIKU_KITS_ML_TNN_MAX_OUTPUT 4
#endif

/**
 * Default number of fractional bits for fixed-point weights.
 * With shift=8, the resolution is ~0.004.
 */
#ifndef TIKU_KITS_ML_TNN_SHIFT
#define TIKU_KITS_ML_TNN_SHIFT 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_ml_elem_t
 *  @brief Element type used for all feature values
 */
typedef TIKU_KITS_ML_ELEM_TYPE tiku_kits_ml_elem_t;

/**
 * @struct tiku_kits_ml_tnn
 * @brief Single-hidden-layer feedforward neural network
 *
 * Architecture: input -> ReLU hidden -> linear output -> argmax
 *
 * w_hidden[j][0] is the bias for hidden neuron j.
 * w_hidden[j][1..n_input] are feature weights for hidden neuron j.
 * w_output[k][0] is the bias for output neuron k.
 * w_output[k][1..n_hidden] are hidden-to-output weights for class k.
 *
 * All weights are Q(shift) fixed-point.
 *
 * Example:
 * @code
 *   struct tiku_kits_ml_tnn net;
 *
 *   tiku_kits_ml_tnn_init(&net, 2, 4, 2, 8);
 *   tiku_kits_ml_tnn_set_lr(&net, 13);  // ~0.05 in Q8
 *
 *   // Train: class 0 when both positive, class 1 when both negative
 *   tiku_kits_ml_elem_t x0[] = {5, 5};
 *   tiku_kits_ml_tnn_train(&net, x0, 0);
 *   tiku_kits_ml_elem_t x1[] = {-5, -5};
 *   tiku_kits_ml_tnn_train(&net, x1, 1);
 *
 *   // Predict
 *   uint8_t cls;
 *   tiku_kits_ml_elem_t xt[] = {4, 3};
 *   tiku_kits_ml_tnn_predict(&net, xt, &cls);  // cls = 0
 * @endcode
 */
struct tiku_kits_ml_tnn {
    int32_t w_hidden[TIKU_KITS_ML_TNN_MAX_HIDDEN]
                    [TIKU_KITS_ML_TNN_MAX_INPUT + 1];
        /**< Hidden layer weights: [j][0]=bias, [j][1..n]=input weights */
    int32_t w_output[TIKU_KITS_ML_TNN_MAX_OUTPUT]
                    [TIKU_KITS_ML_TNN_MAX_HIDDEN + 1];
        /**< Output layer weights: [k][0]=bias, [k][1..n]=hidden weights */
    int32_t h[TIKU_KITS_ML_TNN_MAX_HIDDEN];
        /**< Hidden activations (scratch buffer for forward/backward) */
    int32_t o[TIKU_KITS_ML_TNN_MAX_OUTPUT];
        /**< Output activations (scratch buffer for forward/backward) */
    uint8_t n_input;       /**< Number of input features */
    uint8_t n_hidden;      /**< Number of hidden neurons */
    uint8_t n_output;      /**< Number of output neurons (classes) */
    uint8_t shift;         /**< Fixed-point fractional bits */
    int32_t learning_rate; /**< SGD learning rate (Q shift) */
    uint16_t n;            /**< Number of training samples seen */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a tiny neural network
 * @param tnn      Network to initialize
 * @param n_input  Number of input features (1..MAX_INPUT)
 * @param n_hidden Number of hidden neurons (1..MAX_HIDDEN)
 * @param n_output Number of output classes (2..MAX_OUTPUT)
 * @param shift    Number of fixed-point fractional bits (1..30)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM
 *
 * Hidden weights are initialized with small alternating values
 * (~1/n_input in Q(shift)) to break symmetry. Output weights and
 * biases are zeroed. Learning rate defaults to ~0.05 in Q(shift).
 */
int tiku_kits_ml_tnn_init(struct tiku_kits_ml_tnn *tnn,
                            uint8_t n_input,
                            uint8_t n_hidden,
                            uint8_t n_output,
                            uint8_t shift);

/**
 * @brief Reset all weights and training count
 * @param tnn Network to reset
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Re-initializes hidden weights with the alternating pattern and
 * zeros output weights. Preserves n_input, n_hidden, n_output,
 * shift, and learning_rate.
 */
int tiku_kits_ml_tnn_reset(struct tiku_kits_ml_tnn *tnn);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the SGD learning rate
 * @param tnn           Network
 * @param learning_rate Learning rate in Q(shift) fixed-point (must be > 0)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM
 *
 * Typical value for shift=8: 13 (~0.05) or 6 (~0.025).
 */
int tiku_kits_ml_tnn_set_lr(struct tiku_kits_ml_tnn *tnn,
                              int32_t learning_rate);

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the network with one sample using backpropagation
 * @param tnn Network
 * @param x   Feature vector of length n_input
 * @param y   Class label: 0 .. n_output-1
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (y >= n_output)
 *
 * Performs one forward pass and one backward pass (SGD step).
 * Uses one-hot encoding for the target: target[k] = (k == y).
 * ReLU derivative is applied in the hidden layer.
 */
int tiku_kits_ml_tnn_train(struct tiku_kits_ml_tnn *tnn,
                             const tiku_kits_ml_elem_t *x,
                             uint8_t y);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute raw output scores (forward pass only)
 * @param tnn    Network
 * @param x      Feature vector of length n_input
 * @param scores Output array of length n_output (Q(shift) values)
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Higher score indicates stronger class membership.
 */
int tiku_kits_ml_tnn_forward(
    struct tiku_kits_ml_tnn *tnn,
    const tiku_kits_ml_elem_t *x,
    int32_t *scores);

/**
 * @brief Predict the class label (argmax of output layer)
 * @param tnn    Network
 * @param x      Feature vector of length n_input
 * @param result Output: class label 0 .. n_output-1
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 */
int tiku_kits_ml_tnn_predict(
    struct tiku_kits_ml_tnn *tnn,
    const tiku_kits_ml_elem_t *x,
    uint8_t *result);

/*---------------------------------------------------------------------------*/
/* WEIGHT ACCESS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get weights for a layer
 * @param tnn     Network
 * @param layer   0 = hidden, 1 = output
 * @param weights Output array:
 *                layer 0: n_hidden * (n_input + 1) elements
 *                layer 1: n_output * (n_hidden + 1) elements
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (invalid layer)
 */
int tiku_kits_ml_tnn_get_weights(
    const struct tiku_kits_ml_tnn *tnn,
    uint8_t layer,
    int32_t *weights);

/**
 * @brief Set weights for a layer (for pre-trained deployment)
 * @param tnn     Network (must be initialized)
 * @param layer   0 = hidden, 1 = output
 * @param weights Input array (same layout as get_weights)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (invalid layer)
 */
int tiku_kits_ml_tnn_set_weights(
    struct tiku_kits_ml_tnn *tnn,
    uint8_t layer,
    const int32_t *weights);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of training samples seen
 * @param tnn Network
 * @return Number of training samples, or 0 if tnn is NULL
 */
uint16_t tiku_kits_ml_tnn_count(
    const struct tiku_kits_ml_tnn *tnn);

#endif /* TIKU_KITS_ML_TNN_H_ */
