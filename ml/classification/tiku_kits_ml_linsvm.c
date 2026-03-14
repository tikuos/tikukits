/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_linsvm.c - Linear Support Vector Machine classifier
 *
 * Platform-independent implementation of online binary linear SVM
 * using Pegasos-style stochastic sub-gradient descent on the hinge
 * loss with L2 regularization. All computation uses integer /
 * fixed-point arithmetic with int64_t intermediates.
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

#include "tiku_kits_ml_linsvm.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the linear combination z = w0 + w1*x1 + ... + wn*xn
 * @param svm Model with current weights
 * @param x   Feature vector of length n_features
 * @return z in Q(shift) fixed-point
 *
 * Weights are Q(shift) and features are plain integers, so each
 * product w_j * x_j is Q(shift). The sum is accumulated in int64_t
 * to prevent overflow, then truncated to int32_t.
 */
static int32_t dot_product(const struct tiku_kits_ml_linsvm *svm,
                           const tiku_kits_ml_elem_t *x)
{
    int64_t z = (int64_t)svm->weights[0]; /* bias */
    uint8_t j;

    for (j = 0; j < svm->n_features; j++) {
        z += (int64_t)svm->weights[j + 1] * (int64_t)x[j];
    }

    return (int32_t)z;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_init(struct tiku_kits_ml_linsvm *svm,
                               uint8_t n_features,
                               uint8_t shift)
{
    uint8_t i;

    if (svm == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (n_features == 0
        || n_features > TIKU_KITS_ML_LINSVM_MAX_FEATURES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (shift < 1 || shift > 30) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    svm->n_features = n_features;
    svm->shift = shift;
    svm->n = 0;

    /* Default learning rate: ~0.1 in Q(shift) */
    svm->learning_rate = (int32_t)((1 << shift) / 10);
    if (svm->learning_rate < 1) {
        svm->learning_rate = 1;
    }

    /* Default lambda: ~0.01 in Q(shift) */
    svm->lambda = (int32_t)((1 << shift) / 100);
    if (svm->lambda < 1) {
        svm->lambda = 1;
    }

    /* Zero all weights (bias + features) */
    for (i = 0; i <= n_features; i++) {
        svm->weights[i] = 0;
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_reset(struct tiku_kits_ml_linsvm *svm)
{
    uint8_t i;

    if (svm == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    for (i = 0; i <= svm->n_features; i++) {
        svm->weights[i] = 0;
    }
    svm->n = 0;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_set_lr(struct tiku_kits_ml_linsvm *svm,
                                 int32_t learning_rate)
{
    if (svm == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (learning_rate <= 0) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    svm->learning_rate = learning_rate;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_set_lambda(struct tiku_kits_ml_linsvm *svm,
                                     int32_t lambda)
{
    if (svm == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (lambda < 0) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    svm->lambda = lambda;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_train(struct tiku_kits_ml_linsvm *svm,
                                const tiku_kits_ml_elem_t *x,
                                int8_t y)
{
    int32_t f_x;
    int64_t margin;
    int32_t one_q;
    int64_t lr_q;
    uint8_t j;

    if (svm == NULL || x == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (y != 1 && y != -1) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    /* Forward pass: f(x) = w . x + bias */
    f_x = dot_product(svm, x);

    /* Margin = y * f(x), compared to 1.0 in Q(shift) */
    margin = (int64_t)y * (int64_t)f_x;
    one_q = (int32_t)(1 << svm->shift);
    lr_q = (int64_t)svm->learning_rate;

    if (margin < (int64_t)one_q) {
        /*
         * Margin violation: y * f(x) < 1
         * w_j += lr * (y * x_j - lambda * w_j / (1 << shift))
         * bias: w_0 += lr * (y - lambda * w_0 / (1 << shift))
         *
         * In fixed-point: lr and lambda are Q(shift), so
         *   lr * lambda * w_j needs >> shift to stay Q(shift)
         */
        int64_t reg;

        /* Update bias: w_0 += lr * y - lr * lambda * w_0 >> shift */
        reg = (lr_q * (int64_t)svm->lambda * (int64_t)svm->weights[0])
              >> svm->shift;
        svm->weights[0] += (int32_t)(lr_q * (int64_t)y - (reg >> svm->shift));

        /* Update feature weights */
        for (j = 0; j < svm->n_features; j++) {
            reg = (lr_q * (int64_t)svm->lambda
                   * (int64_t)svm->weights[j + 1]) >> svm->shift;
            svm->weights[j + 1] +=
                (int32_t)(lr_q * (int64_t)y * (int64_t)x[j]
                          - (reg >> svm->shift));
        }
    } else {
        /*
         * No margin violation: regularization only
         * w_j -= lr * lambda * w_j / (1 << shift)^2
         */
        int64_t reg;

        reg = (lr_q * (int64_t)svm->lambda * (int64_t)svm->weights[0])
              >> svm->shift;
        svm->weights[0] -= (int32_t)(reg >> svm->shift);

        for (j = 0; j < svm->n_features; j++) {
            reg = (lr_q * (int64_t)svm->lambda
                   * (int64_t)svm->weights[j + 1]) >> svm->shift;
            svm->weights[j + 1] -= (int32_t)(reg >> svm->shift);
        }
    }

    svm->n++;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_decision(
    const struct tiku_kits_ml_linsvm *svm,
    const tiku_kits_ml_elem_t *x,
    int32_t *result)
{
    if (svm == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    *result = dot_product(svm, x);

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_predict(
    const struct tiku_kits_ml_linsvm *svm,
    const tiku_kits_ml_elem_t *x,
    int8_t *result)
{
    int32_t margin;
    int rc;

    if (svm == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    rc = tiku_kits_ml_linsvm_decision(svm, x, &margin);
    if (rc != TIKU_KITS_ML_OK) {
        return rc;
    }

    *result = (margin >= 0) ? 1 : -1;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* WEIGHT ACCESS                                                             */
/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_get_weights(
    const struct tiku_kits_ml_linsvm *svm,
    int32_t *weights)
{
    uint8_t i;

    if (svm == NULL || weights == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    for (i = 0; i <= svm->n_features; i++) {
        weights[i] = svm->weights[i];
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ml_linsvm_set_weights(
    struct tiku_kits_ml_linsvm *svm,
    const int32_t *weights)
{
    uint8_t i;

    if (svm == NULL || weights == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    for (i = 0; i <= svm->n_features; i++) {
        svm->weights[i] = weights[i];
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ml_linsvm_count(
    const struct tiku_kits_ml_linsvm *svm)
{
    if (svm == NULL) {
        return 0;
    }
    return svm->n;
}
