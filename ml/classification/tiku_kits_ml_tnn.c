/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_tnn.c - Tiny Neural Network classifier
 *
 * Platform-independent single-hidden-layer feedforward neural network.
 * Uses ReLU activation, one-hot targets, and SGD backpropagation.
 * All computation uses integer / fixed-point arithmetic with int64_t
 * intermediates. No heap allocation.
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

#include "tiku_kits_ml_tnn.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize hidden weights with small alternating values
 *
 * Sets w_hidden[j][i] to approximately +/-1/n_input in Q(shift),
 * alternating sign based on (j + i) & 1 to break symmetry.
 * Without this alternation every hidden neuron would start
 * identical and learn the same function.  Biases are set to zero.
 * Output weights are all zeroed so the network starts with no
 * class preference.  Scratch buffers h[] and o[] are also cleared.
 */
static void init_weights(struct tiku_kits_ml_tnn *tnn)
{
    uint8_t j, i, k;
    int32_t scale;

    /* Scale: ~1/n_input in Q(shift), minimum 1 */
    scale = (int32_t)((1 << tnn->shift) / (tnn->n_input + 1));
    if (scale < 1) {
        scale = 1;
    }

    /* Hidden layer: alternating ±scale, bias = 0 */
    for (j = 0; j < tnn->n_hidden; j++) {
        tnn->w_hidden[j][0] = 0;
        for (i = 1; i <= tnn->n_input; i++) {
            tnn->w_hidden[j][i] = ((j + i) & 1) ? scale : -scale;
        }
    }

    /* Output layer: all zero */
    for (k = 0; k < tnn->n_output; k++) {
        for (j = 0; j <= tnn->n_hidden; j++) {
            tnn->w_output[k][j] = 0;
        }
    }

    /* Clear scratch buffers */
    for (j = 0; j < tnn->n_hidden; j++) {
        tnn->h[j] = 0;
    }
    for (k = 0; k < tnn->n_output; k++) {
        tnn->o[k] = 0;
    }
}

/**
 * @brief Compute forward pass through the network
 *
 * Two-stage computation:
 *   Hidden: h[j] = ReLU( bias + sum_i(w_hidden[j][i+1] * x[i]) )
 *   Output: o[k] = bias + sum_j( (w_output[k][j+1] * h[j]) >> shift )
 *
 * The output-layer products are right-shifted by @c shift because
 * both w_output and h are Q(shift), so their product is Q(2*shift).
 * The shift brings the result back to Q(shift).  int64_t
 * accumulators prevent intermediate overflow.
 *
 * Results are stored in tnn->h[] and tnn->o[] (scratch buffers).
 */
static void compute_forward(struct tiku_kits_ml_tnn *tnn,
                            const tiku_kits_ml_elem_t *x)
{
    uint8_t j, i, k;
    int64_t acc;

    /* Hidden layer */
    for (j = 0; j < tnn->n_hidden; j++) {
        acc = (int64_t)tnn->w_hidden[j][0]; /* bias */
        for (i = 0; i < tnn->n_input; i++) {
            acc += (int64_t)tnn->w_hidden[j][i + 1] * (int64_t)x[i];
        }
        /* ReLU activation */
        tnn->h[j] = ((int32_t)acc > 0) ? (int32_t)acc : 0;
    }

    /* Output layer: h[] is Q(shift), w_output is Q(shift) */
    for (k = 0; k < tnn->n_output; k++) {
        acc = (int64_t)tnn->w_output[k][0]; /* bias */
        for (j = 0; j < tnn->n_hidden; j++) {
            acc += ((int64_t)tnn->w_output[k][j + 1]
                    * (int64_t)tnn->h[j]) >> tnn->shift;
        }
        tnn->o[k] = (int32_t)acc;
    }
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a tiny neural network
 *
 * Validates architecture parameters, sets the default learning rate
 * to ~0.05 in Q(shift), and initializes weights via init_weights().
 */
int tiku_kits_ml_tnn_init(struct tiku_kits_ml_tnn *tnn,
                            uint8_t n_input,
                            uint8_t n_hidden,
                            uint8_t n_output,
                            uint8_t shift)
{
    if (tnn == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (n_input == 0
        || n_input > TIKU_KITS_ML_TNN_MAX_INPUT) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (n_hidden == 0
        || n_hidden > TIKU_KITS_ML_TNN_MAX_HIDDEN) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (n_output < 2
        || n_output > TIKU_KITS_ML_TNN_MAX_OUTPUT) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (shift < 1 || shift > 30) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    tnn->n_input = n_input;
    tnn->n_hidden = n_hidden;
    tnn->n_output = n_output;
    tnn->shift = shift;
    tnn->n = 0;

    /* Default learning rate: ~0.05 in Q(shift) */
    tnn->learning_rate = (int32_t)((1 << shift) / 20);
    if (tnn->learning_rate < 1) {
        tnn->learning_rate = 1;
    }

    init_weights(tnn);

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset all weights and training count
 *
 * Re-runs init_weights() and resets the training counter.
 * Preserves architecture and learning rate.
 */
int tiku_kits_ml_tnn_reset(struct tiku_kits_ml_tnn *tnn)
{
    if (tnn == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    init_weights(tnn);
    tnn->n = 0;

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
int tiku_kits_ml_tnn_set_lr(struct tiku_kits_ml_tnn *tnn,
                              int32_t learning_rate)
{
    if (tnn == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (learning_rate <= 0) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    tnn->learning_rate = learning_rate;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the network with one sample using backpropagation
 *
 * Forward pass computes hidden and output activations.  Backward
 * pass updates hidden weights FIRST (before output weights change)
 * so that error back-propagation uses the original output weights.
 *
 * Hidden layer gradient:
 *   err_h[j] = sum_k( w_output[k][j+1] * delta_o[k] ) >> shift
 *   delta_h[j] = (h[j] > 0) ? err_h[j] : 0   (ReLU gate)
 *
 * Output layer gradient:
 *   delta_o[k] = o[k] - target[k]
 *   target[k] = one_q if k == y, else 0
 *
 * All intermediate products use int64_t to prevent overflow.
 */
int tiku_kits_ml_tnn_train(struct tiku_kits_ml_tnn *tnn,
                             const tiku_kits_ml_elem_t *x,
                             uint8_t y)
{
    uint8_t j, i, k;
    int32_t one_q;
    int64_t lr_q;
    int32_t delta_o;
    int64_t lr_delta;
    int64_t err_sum;
    int32_t delta_h;

    if (tnn == NULL || x == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (y >= tnn->n_output) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    /* Forward pass */
    compute_forward(tnn, x);

    one_q = (int32_t)(1 << tnn->shift);
    lr_q = (int64_t)tnn->learning_rate;

    /*
     * Backward pass: hidden layer FIRST (before output weights change)
     *
     * err_h[j] = sum_k( w_output[k][j+1] * delta_o[k] ) >> shift
     * delta_h[j] = (h[j] > 0) ? err_h[j] : 0   (ReLU derivative)
     *
     * Must run before output weight updates so that error
     * back-propagation uses the original (un-modified) output weights.
     *
     * Update hidden weights:
     *   lr_delta = (lr * delta_h) >> shift
     *   w_hidden[j][0] -= lr_delta                    (bias)
     *   w_hidden[j][i+1] -= lr_delta * x[i]           (features)
     */
    for (j = 0; j < tnn->n_hidden; j++) {
        /* Skip if ReLU was off (derivative = 0) */
        if (tnn->h[j] <= 0) {
            continue;
        }

        /* Compute error propagated from output layer */
        err_sum = 0;
        for (k = 0; k < tnn->n_output; k++) {
            delta_o = tnn->o[k] - ((k == y) ? one_q : 0);
            err_sum += ((int64_t)tnn->w_output[k][j + 1]
                        * (int64_t)delta_o) >> tnn->shift;
        }
        delta_h = (int32_t)err_sum;

        lr_delta = (lr_q * (int64_t)delta_h) >> tnn->shift;

        /* Bias update */
        tnn->w_hidden[j][0] -= (int32_t)lr_delta;

        /* Input-to-hidden weight updates */
        for (i = 0; i < tnn->n_input; i++) {
            tnn->w_hidden[j][i + 1] -=
                (int32_t)(lr_delta * (int64_t)x[i]);
        }
    }

    /*
     * Backward pass: output layer
     *
     * delta_o[k] = o[k] - target[k]
     * target[k] = one_q if k == y, else 0
     *
     * Update output weights:
     *   lr_delta = (lr * delta_o) >> shift
     *   w_output[k][0] -= lr_delta                    (bias)
     *   w_output[k][j+1] -= (lr_delta * h[j]) >> shift
     */
    for (k = 0; k < tnn->n_output; k++) {
        delta_o = tnn->o[k] - ((k == y) ? one_q : 0);
        lr_delta = (lr_q * (int64_t)delta_o) >> tnn->shift;

        /* Bias update */
        tnn->w_output[k][0] -= (int32_t)lr_delta;

        /* Hidden-to-output weight updates */
        for (j = 0; j < tnn->n_hidden; j++) {
            tnn->w_output[k][j + 1] -=
                (int32_t)((lr_delta * (int64_t)tnn->h[j])
                          >> tnn->shift);
        }
    }

    tnn->n++;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute raw output scores (forward pass only)
 *
 * Runs compute_forward() and copies the output activations into
 * the caller's buffer.
 */
int tiku_kits_ml_tnn_forward(
    struct tiku_kits_ml_tnn *tnn,
    const tiku_kits_ml_elem_t *x,
    int32_t *scores)
{
    uint8_t k;

    if (tnn == NULL || x == NULL || scores == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    compute_forward(tnn, x);

    for (k = 0; k < tnn->n_output; k++) {
        scores[k] = tnn->o[k];
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Predict the class label (argmax of output layer)
 *
 * Runs the forward pass, then scans the output activations to find
 * the neuron with the highest value.
 */
int tiku_kits_ml_tnn_predict(
    struct tiku_kits_ml_tnn *tnn,
    const tiku_kits_ml_elem_t *x,
    uint8_t *result)
{
    uint8_t k, best;
    int32_t best_score;

    if (tnn == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    compute_forward(tnn, x);

    /* Argmax */
    best = 0;
    best_score = tnn->o[0];
    for (k = 1; k < tnn->n_output; k++) {
        if (tnn->o[k] > best_score) {
            best_score = tnn->o[k];
            best = k;
        }
    }

    *result = best;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* WEIGHT ACCESS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get weights for a layer
 *
 * Copies the weight matrix row-by-row into the caller's flat array.
 * The layout is: [neuron_0_bias, neuron_0_w1, ..., neuron_1_bias, ...].
 */
int tiku_kits_ml_tnn_get_weights(
    const struct tiku_kits_ml_tnn *tnn,
    uint8_t layer,
    int32_t *weights)
{
    uint8_t r, c, cols, rows;
    uint16_t idx;

    if (tnn == NULL || weights == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (layer > 1) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    idx = 0;
    if (layer == 0) {
        rows = tnn->n_hidden;
        cols = tnn->n_input + 1;
        for (r = 0; r < rows; r++) {
            for (c = 0; c < cols; c++) {
                weights[idx++] = tnn->w_hidden[r][c];
            }
        }
    } else {
        rows = tnn->n_output;
        cols = tnn->n_hidden + 1;
        for (r = 0; r < rows; r++) {
            for (c = 0; c < cols; c++) {
                weights[idx++] = tnn->w_output[r][c];
            }
        }
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Set weights for a layer (for pre-trained deployment)
 *
 * Copies the caller's flat array into the weight matrix row-by-row.
 * The layout must match get_weights: bias first in each row.
 */
int tiku_kits_ml_tnn_set_weights(
    struct tiku_kits_ml_tnn *tnn,
    uint8_t layer,
    const int32_t *weights)
{
    uint8_t r, c, cols, rows;
    uint16_t idx;

    if (tnn == NULL || weights == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (layer > 1) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    idx = 0;
    if (layer == 0) {
        rows = tnn->n_hidden;
        cols = tnn->n_input + 1;
        for (r = 0; r < rows; r++) {
            for (c = 0; c < cols; c++) {
                tnn->w_hidden[r][c] = weights[idx++];
            }
        }
    } else {
        rows = tnn->n_output;
        cols = tnn->n_hidden + 1;
        for (r = 0; r < rows; r++) {
            for (c = 0; c < cols; c++) {
                tnn->w_output[r][c] = weights[idx++];
            }
        }
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
uint16_t tiku_kits_ml_tnn_count(
    const struct tiku_kits_ml_tnn *tnn)
{
    if (tnn == NULL) {
        return 0;
    }
    return tnn->n;
}
