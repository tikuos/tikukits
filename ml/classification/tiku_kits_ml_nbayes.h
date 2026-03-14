/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_nbayes.h - Categorical Naive Bayes classifier
 *
 * Platform-independent Naive Bayes classifier for embedded systems.
 * Operates on discrete (binned) features: each feature value is an
 * integer in [0, n_bins). The user is responsible for discretizing
 * continuous sensor readings before training/prediction.
 *
 * Training updates per-class frequency tables with O(1) cost per
 * sample. Prediction computes log-likelihood scores using a
 * fixed-point log2 approximation and Laplace (add-1) smoothing,
 * then returns the class with the highest score.
 *
 * Log-space scoring avoids the probability-product underflow problem
 * that would occur with raw probability multiplication:
 *
 *     score(c) = log P(c) + sum_j log P(x_j | c)
 *              = log2(count_c) + sum_j log2(freq[c][j][x_j] + 1)
 *                - n_features * log2(count_c + n_bins)
 *
 * All computation uses integer arithmetic. No FPU, no exp(), no
 * sqrt() required. All storage is statically allocated.
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

#ifndef TIKU_KITS_ML_NBAYES_H_
#define TIKU_KITS_ML_NBAYES_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ml.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of distinct class labels (0..MAX_CLASSES-1).
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_NBAYES_MAX_CLASSES
#define TIKU_KITS_ML_NBAYES_MAX_CLASSES 4
#endif

/**
 * Maximum number of input features.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_NBAYES_MAX_FEATURES
#define TIKU_KITS_ML_NBAYES_MAX_FEATURES 4
#endif

/**
 * Maximum number of discrete bins per feature.
 * Each feature value must be in [0, n_bins). Continuous sensor
 * readings should be quantized into bins before use.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_NBAYES_MAX_BINS
#define TIKU_KITS_ML_NBAYES_MAX_BINS 16
#endif

/**
 * Default number of fractional bits for fixed-point log-likelihood
 * scores. With shift=8, log2 resolution is ~0.004.
 * Override before including this header to change the default.
 */
#ifndef TIKU_KITS_ML_NBAYES_SHIFT
#define TIKU_KITS_ML_NBAYES_SHIFT 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ml_nbayes
 * @brief Categorical Naive Bayes classifier
 *
 * Stores per-class per-feature frequency tables and classifies
 * new samples by computing log-likelihood scores with Laplace
 * smoothing.
 *
 * Example:
 * @code
 *   struct tiku_kits_ml_nbayes nb;
 *   uint8_t cls;
 *
 *   tiku_kits_ml_nbayes_init(&nb, 2, 4, 2, 8);
 *   // 2 features, 4 bins each, 2 classes, Q8
 *
 *   // Train: feature bins as uint8_t arrays
 *   uint8_t a[] = {0, 1};
 *   tiku_kits_ml_nbayes_train(&nb, a, 0);
 *   uint8_t b[] = {3, 2};
 *   tiku_kits_ml_nbayes_train(&nb, b, 1);
 *
 *   // Predict
 *   uint8_t q[] = {0, 1};
 *   tiku_kits_ml_nbayes_predict(&nb, q, &cls);  // cls = 0
 * @endcode
 */
struct tiku_kits_ml_nbayes {
    uint16_t freq[TIKU_KITS_ML_NBAYES_MAX_CLASSES]
                 [TIKU_KITS_ML_NBAYES_MAX_FEATURES]
                 [TIKU_KITS_ML_NBAYES_MAX_BINS];
            /**< Frequency table: freq[class][feature][bin] */
    uint16_t class_count[TIKU_KITS_ML_NBAYES_MAX_CLASSES];
            /**< Number of training samples per class */
    uint16_t n_total;      /**< Total training samples seen */
    uint8_t n_features;    /**< Number of input features */
    uint8_t n_bins;        /**< Number of discrete bins per feature */
    uint8_t n_classes;     /**< Number of class labels */
    uint8_t shift;         /**< Fixed-point fractional bits for scores */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Naive Bayes model
 * @param nb         Model to initialize
 * @param n_features Number of input features (1..MAX_FEATURES)
 * @param n_bins     Number of bins per feature (2..MAX_BINS)
 * @param n_classes  Number of class labels (2..MAX_CLASSES)
 * @param shift      Fixed-point fractional bits (1..30)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM
 *
 * All frequency counters are zeroed.
 */
int tiku_kits_ml_nbayes_init(struct tiku_kits_ml_nbayes *nb,
                               uint8_t n_features,
                               uint8_t n_bins,
                               uint8_t n_classes,
                               uint8_t shift);

/**
 * @brief Reset all frequency counters
 * @param nb Model to reset
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Preserves n_features, n_bins, n_classes, and shift.
 */
int tiku_kits_ml_nbayes_reset(struct tiku_kits_ml_nbayes *nb);

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the model with one sample
 * @param nb    Model
 * @param x     Feature vector of length n_features (each in [0, n_bins))
 * @param label Class label (0..n_classes-1)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (label or bin value out of range)
 *
 * Increments freq[label][j][x[j]] for each feature j, and
 * class_count[label].
 */
int tiku_kits_ml_nbayes_train(struct tiku_kits_ml_nbayes *nb,
                                const uint8_t *x,
                                uint8_t label);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector
 * @param nb     Model (must have at least 1 training sample)
 * @param x      Feature vector of length n_features (each in [0, n_bins))
 * @param result Output: predicted class label
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_SIZE (no training data)
 *
 * Computes log-likelihood scores for each class using Laplace
 * smoothing and returns the class with the highest score.
 */
int tiku_kits_ml_nbayes_predict(const struct tiku_kits_ml_nbayes *nb,
                                  const uint8_t *x,
                                  uint8_t *result);

/**
 * @brief Get log-likelihood scores for all classes
 * @param nb     Model (must have at least 1 training sample)
 * @param x      Feature vector of length n_features
 * @param scores Output array of length n_classes (Q shift fixed-point)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_SIZE (no training data)
 *
 * scores[c] = log2(P(c)) + sum_j log2(P(x_j|c)) in Q(shift).
 * Higher score = more likely class.
 */
int tiku_kits_ml_nbayes_predict_log_proba(
    const struct tiku_kits_ml_nbayes *nb,
    const uint8_t *x,
    int32_t *scores);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the total number of training samples
 * @param nb Model
 * @return Total sample count, or 0 if nb is NULL
 */
uint16_t tiku_kits_ml_nbayes_count(
    const struct tiku_kits_ml_nbayes *nb);

/**
 * @brief Get the number of training samples for a specific class
 * @param nb  Model
 * @param cls Class label
 * @return Sample count for that class, or 0 if nb is NULL or cls
 *         is out of range
 */
uint16_t tiku_kits_ml_nbayes_class_count(
    const struct tiku_kits_ml_nbayes *nb,
    uint8_t cls);

#endif /* TIKU_KITS_ML_NBAYES_H_ */
