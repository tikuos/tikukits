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
 * @brief Element type for feature values.
 *
 * Defaults to int32_t for integer-only targets (no FPU required).
 * All ML sub-modules share this type so that feature vectors can be
 * passed between classifiers without casting.
 *
 * Override before including this header to change the type:
 * @code
 *   #define TIKU_KITS_ML_ELEM_TYPE int16_t
 *   #include "tiku_kits_ml_tnn.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * @brief Maximum number of input features.
 *
 * Determines the width of the hidden-layer weight matrix.  Each
 * hidden neuron stores (MAX_INPUT + 1) weights (bias + features).
 * Total hidden-layer memory is MAX_HIDDEN * (MAX_INPUT + 1) *
 * sizeof(int32_t) bytes.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_TNN_MAX_INPUT 16
 *   #include "tiku_kits_ml_tnn.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_TNN_MAX_INPUT
#define TIKU_KITS_ML_TNN_MAX_INPUT 8
#endif

/**
 * @brief Maximum number of hidden neurons.
 *
 * Determines the height of the hidden-layer weight matrix and the
 * width of the output-layer weight matrix.  Larger values allow
 * the network to learn more complex decision boundaries at the
 * cost of more memory and computation.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_TNN_MAX_HIDDEN 16
 *   #include "tiku_kits_ml_tnn.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_TNN_MAX_HIDDEN
#define TIKU_KITS_ML_TNN_MAX_HIDDEN 8
#endif

/**
 * @brief Maximum number of output neurons (classes).
 *
 * Must be >= 2 for classification.  Each output neuron stores
 * (MAX_HIDDEN + 1) weights (bias + hidden activations).
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_TNN_MAX_OUTPUT 8
 *   #include "tiku_kits_ml_tnn.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_TNN_MAX_OUTPUT
#define TIKU_KITS_ML_TNN_MAX_OUTPUT 4
#endif

/**
 * @brief Default number of fractional bits for fixed-point weights.
 *
 * With shift=8 the resolution is ~0.004 (1/256).  Weights,
 * activations, learning rate, and output scores all use this Q
 * format.  Larger shift gives finer weight precision but reduces
 * the representable integer range.
 *
 * Override before including this header to change the default:
 * @code
 *   #define TIKU_KITS_ML_TNN_SHIFT 10
 *   #include "tiku_kits_ml_tnn.h"
 * @endcode
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
 * Architecture:
 *
 *     input (n_input) --> hidden (n_hidden, ReLU) --> output (n_output)
 *                                                     argmax --> class
 *
 * Two weight matrices are stored:
 *   - @c w_hidden[j][0] is the bias for hidden neuron j.
 *     @c w_hidden[j][1..n_input] are the input-to-hidden weights.
 *   - @c w_output[k][0] is the bias for output neuron k.
 *     @c w_output[k][1..n_hidden] are the hidden-to-output weights.
 *
 * All weights are Q(shift) fixed-point.  Hidden activations use
 * ReLU (max(0, z)); output activations are linear.  Classification
 * is by argmax of the output layer.
 *
 * Two scratch buffers (@c h and @c o) hold intermediate activations
 * during forward and backward passes.  They are overwritten on each
 * call to train(), forward(), or predict().
 *
 * @note Hidden weights are initialized with small alternating values
 *       (~1/n_input in Q(shift)) to break symmetry.  Output weights
 *       and biases are zero-initialized.
 *
 * @note Because all storage lives inside the struct, no heap
 *       allocation is needed -- just declare the network as a static
 *       or local variable.
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
        /**< Hidden layer weights in Q(shift):
             [j][0] = bias, [j][1..n_input] = input weights */
    int32_t w_output[TIKU_KITS_ML_TNN_MAX_OUTPUT]
                    [TIKU_KITS_ML_TNN_MAX_HIDDEN + 1];
        /**< Output layer weights in Q(shift):
             [k][0] = bias, [k][1..n_hidden] = hidden weights */
    int32_t h[TIKU_KITS_ML_TNN_MAX_HIDDEN];
        /**< Hidden activations (scratch; overwritten each forward
             pass).  Post-ReLU values in Q(shift). */
    int32_t o[TIKU_KITS_ML_TNN_MAX_OUTPUT];
        /**< Output activations (scratch; overwritten each forward
             pass).  Linear values in Q(shift). */
    uint8_t n_input;       /**< Number of input features (set at init) */
    uint8_t n_hidden;      /**< Number of hidden neurons (set at init) */
    uint8_t n_output;      /**< Number of output neurons / classes
                                (set at init; must be >= 2) */
    uint8_t shift;         /**< Fixed-point fractional bits for all
                                Q-format values in this network */
    int32_t learning_rate; /**< SGD step size in Q(shift) (must be > 0);
                                default ~0.05 */
    uint16_t n;            /**< Total number of train() calls seen
                                (monotonically increasing) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a tiny neural network
 *
 * Validates all architecture parameters, sets default learning rate
 * (~0.05 in Q(shift)), and initializes weights via the symmetry-
 * breaking alternating pattern.  Output weights and biases are
 * zeroed.
 *
 * @param tnn      Network to initialize (must not be NULL)
 * @param n_input  Number of input features
 *                 (1..TIKU_KITS_ML_TNN_MAX_INPUT)
 * @param n_hidden Number of hidden neurons
 *                 (1..TIKU_KITS_ML_TNN_MAX_HIDDEN)
 * @param n_output Number of output classes
 *                 (2..TIKU_KITS_ML_TNN_MAX_OUTPUT)
 * @param shift    Number of fixed-point fractional bits (1..30)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if tnn is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if any dimension is out of range
 */
int tiku_kits_ml_tnn_init(struct tiku_kits_ml_tnn *tnn,
                            uint8_t n_input,
                            uint8_t n_hidden,
                            uint8_t n_output,
                            uint8_t shift);

/**
 * @brief Reset all weights and training count
 *
 * Re-initializes hidden weights with the alternating symmetry-
 * breaking pattern and zeros output weights.  Preserves n_input,
 * n_hidden, n_output, shift, and learning_rate so the network can
 * be retrained from scratch with the same architecture.
 *
 * @param tnn Network to reset (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if tnn is NULL
 */
int tiku_kits_ml_tnn_reset(struct tiku_kits_ml_tnn *tnn);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the SGD learning rate
 *
 * Controls the step size for each weight update during
 * backpropagation.  Can be changed between training calls to
 * implement a learning-rate schedule.
 *
 * @param tnn           Network (must not be NULL)
 * @param learning_rate Learning rate in Q(shift) fixed-point
 *                      (must be > 0; typical Q8 values: 13 for ~0.05,
 *                      6 for ~0.025)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if tnn is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if learning_rate <= 0
 */
int tiku_kits_ml_tnn_set_lr(struct tiku_kits_ml_tnn *tnn,
                              int32_t learning_rate);

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the network with one sample using backpropagation
 *
 * Performs one forward pass (computing hidden and output activations)
 * followed by one backward pass (SGD weight update).  The target is
 * one-hot encoded: target[k] = (1 << shift) if k == y, else 0.
 * The ReLU derivative (0 or 1) gates gradient flow through the
 * hidden layer.
 *
 * O(n_input * n_hidden + n_hidden * n_output) per call.
 *
 * @param tnn Network (must not be NULL)
 * @param x   Feature vector of length n_input (must not be NULL)
 * @param y   Class label (0..n_output - 1)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if tnn or x is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if y >= n_output
 */
int tiku_kits_ml_tnn_train(struct tiku_kits_ml_tnn *tnn,
                             const tiku_kits_ml_elem_t *x,
                             uint8_t y);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute raw output scores (forward pass only)
 *
 * Runs the forward pass and copies the output activations into the
 * caller's buffer.  The network's internal h[] and o[] scratch
 * buffers are overwritten.  Higher score indicates stronger class
 * membership.  O(n_input * n_hidden + n_hidden * n_output).
 *
 * @param tnn    Network (must not be NULL)
 * @param x      Feature vector of length n_input (must not be NULL)
 * @param scores Output array; caller must provide space for at least
 *               n_output int32_t entries (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if tnn, x, or scores is NULL
 */
int tiku_kits_ml_tnn_forward(
    struct tiku_kits_ml_tnn *tnn,
    const tiku_kits_ml_elem_t *x,
    int32_t *scores);

/**
 * @brief Predict the class label (argmax of output layer)
 *
 * Runs the forward pass and returns the index of the output neuron
 * with the highest activation.  O(n_input * n_hidden + n_hidden *
 * n_output).
 *
 * @param tnn    Network (must not be NULL)
 * @param x      Feature vector of length n_input (must not be NULL)
 * @param result Output pointer where the predicted class label
 *               (0..n_output - 1) is written (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if tnn, x, or result is NULL
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
 *
 * Copies the weight matrix row-by-row into a flat caller-supplied
 * array.  For layer 0 (hidden): n_hidden * (n_input + 1) elements.
 * For layer 1 (output): n_output * (n_hidden + 1) elements.
 * Each row starts with the bias, followed by the connection weights.
 *
 * @param tnn     Network (must not be NULL)
 * @param layer   0 = hidden layer, 1 = output layer
 * @param weights Output array; caller must provide sufficient space
 *                (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if tnn or weights is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if layer > 1
 */
int tiku_kits_ml_tnn_get_weights(
    const struct tiku_kits_ml_tnn *tnn,
    uint8_t layer,
    int32_t *weights);

/**
 * @brief Set weights for a layer (for pre-trained deployment)
 *
 * Loads externally computed weights so the network can be used for
 * inference without on-device training.  The input array layout
 * must match get_weights: row-major, bias first in each row, all
 * values in Q(shift).
 *
 * @param tnn     Network (must be initialized; must not be NULL)
 * @param layer   0 = hidden layer, 1 = output layer
 * @param weights Input array with the same layout as get_weights
 *                (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if tnn or weights is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if layer > 1
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
 *
 * Returns the total number of train() calls.  Safe to call with a
 * NULL pointer -- returns 0.
 *
 * @param tnn Network, or NULL
 * @return Number of training samples seen, or 0 if tnn is NULL
 */
uint16_t tiku_kits_ml_tnn_count(
    const struct tiku_kits_ml_tnn *tnn);

#endif /* TIKU_KITS_ML_TNN_H_ */
