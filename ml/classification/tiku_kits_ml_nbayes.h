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
 * @brief Maximum number of distinct class labels (0..MAX_CLASSES-1).
 *
 * Controls the first dimension of the frequency table and the size
 * of the class_count array.  Total static memory for frequency
 * tables is MAX_CLASSES * MAX_FEATURES * MAX_BINS * sizeof(uint16_t).
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_NBAYES_MAX_CLASSES 8
 *   #include "tiku_kits_ml_nbayes.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_NBAYES_MAX_CLASSES
#define TIKU_KITS_ML_NBAYES_MAX_CLASSES 4
#endif

/**
 * @brief Maximum number of input features.
 *
 * Controls the second dimension of the frequency table.  Each
 * feature must be a discrete value in [0, n_bins).
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_NBAYES_MAX_FEATURES 8
 *   #include "tiku_kits_ml_nbayes.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_NBAYES_MAX_FEATURES
#define TIKU_KITS_ML_NBAYES_MAX_FEATURES 4
#endif

/**
 * @brief Maximum number of discrete bins per feature.
 *
 * Each feature value must be an integer in [0, n_bins).  Continuous
 * sensor readings should be quantized into bins before training or
 * prediction.  Controls the third dimension of the frequency table.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_NBAYES_MAX_BINS 32
 *   #include "tiku_kits_ml_nbayes.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_NBAYES_MAX_BINS
#define TIKU_KITS_ML_NBAYES_MAX_BINS 16
#endif

/**
 * @brief Default number of fractional bits for fixed-point
 *        log-likelihood scores.
 *
 * With shift=8 the log2 resolution is ~0.004 (1/256).  Scores are
 * returned in Q(shift) format; higher shift gives finer resolution
 * but narrower representable range.
 *
 * Override before including this header to change the default:
 * @code
 *   #define TIKU_KITS_ML_NBAYES_SHIFT 10
 *   #include "tiku_kits_ml_nbayes.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_NBAYES_SHIFT
#define TIKU_KITS_ML_NBAYES_SHIFT 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ml_nbayes
 * @brief Categorical Naive Bayes classifier with Laplace smoothing
 *
 * Stores per-class per-feature frequency tables in a 3-D static
 * array and classifies new samples by computing log-likelihood
 * scores in fixed-point.  Training is O(1) per sample (just
 * increment counters); prediction is O(n_classes * n_features).
 *
 * The scoring formula with Laplace (add-1) smoothing is:
 *
 *     score(c) = log2(count_c + 1)
 *              + sum_j log2(freq[c][j][x_j] + 1)
 *              - n_features * log2(count_c + n_bins)
 *
 * Log-space scoring avoids the probability-product underflow that
 * would occur with raw probability multiplication on integer-only
 * targets.
 *
 * @note Feature values are discrete integers in [0, n_bins).  The
 *       user is responsible for quantizing continuous sensor
 *       readings before training or prediction.
 *
 * @note Because all storage lives inside the struct, no heap
 *       allocation is needed.  Memory usage is dominated by the
 *       freq table: MAX_CLASSES * MAX_FEATURES * MAX_BINS *
 *       sizeof(uint16_t) bytes.
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
            /**< Frequency table: freq[class][feature][bin] counts
                 how many training samples of the given class had
                 the given bin value for that feature */
    uint16_t class_count[TIKU_KITS_ML_NBAYES_MAX_CLASSES];
            /**< Per-class training sample count (sum of all bin
                 increments for that class) */
    uint16_t n_total;      /**< Total training samples seen across
                                all classes */
    uint8_t n_features;    /**< Dimensionality of input vectors
                                (set at init time) */
    uint8_t n_bins;        /**< Number of discrete bins per feature
                                (set at init time) */
    uint8_t n_classes;     /**< Number of class labels
                                (set at init time) */
    uint8_t shift;         /**< Fixed-point fractional bits for
                                log-likelihood scores */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Naive Bayes model
 *
 * Validates all configuration parameters, zeros the frequency table
 * and class counters, and records the model dimensions.  Samples
 * must be added via train() before prediction is possible.
 *
 * @param nb         Model to initialize (must not be NULL)
 * @param n_features Number of input features
 *                   (1..TIKU_KITS_ML_NBAYES_MAX_FEATURES)
 * @param n_bins     Number of discrete bins per feature
 *                   (2..TIKU_KITS_ML_NBAYES_MAX_BINS)
 * @param n_classes  Number of class labels
 *                   (2..TIKU_KITS_ML_NBAYES_MAX_CLASSES)
 * @param shift      Fixed-point fractional bits for scores (1..30)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if nb is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if any dimension is out of range
 */
int tiku_kits_ml_nbayes_init(struct tiku_kits_ml_nbayes *nb,
                               uint8_t n_features,
                               uint8_t n_bins,
                               uint8_t n_classes,
                               uint8_t shift);

/**
 * @brief Reset all frequency counters without changing configuration
 *
 * Zeros the frequency table and class counters, and resets n_total
 * to 0.  Preserves n_features, n_bins, n_classes, and shift so
 * the model can be retrained from scratch.
 *
 * @param nb Model to reset (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if nb is NULL
 */
int tiku_kits_ml_nbayes_reset(struct tiku_kits_ml_nbayes *nb);

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the model with one sample
 *
 * Validates the label and all bin values first (no partial update
 * on error), then increments freq[label][j][x[j]] for each feature
 * j and bumps class_count[label] and n_total.  O(n_features) per
 * call.
 *
 * @param nb    Model (must not be NULL)
 * @param x     Feature vector of length n_features; each element
 *              must be in [0, n_bins) (must not be NULL)
 * @param label Class label (0..n_classes - 1)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if nb or x is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if label >= n_classes or any
 *         x[j] >= n_bins
 */
int tiku_kits_ml_nbayes_train(struct tiku_kits_ml_nbayes *nb,
                                const uint8_t *x,
                                uint8_t label);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector using Naive Bayes
 *
 * Computes log-likelihood scores for every class via
 * predict_log_proba() and returns the class with the highest
 * score.  O(n_classes * n_features).
 *
 * @param nb     Model with at least 1 training sample
 *               (must not be NULL)
 * @param x      Feature vector of length n_features; each element
 *               must be in [0, n_bins) (must not be NULL)
 * @param result Output pointer where the predicted class label is
 *               written (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if nb, x, or result is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if no training data has been added
 */
int tiku_kits_ml_nbayes_predict(const struct tiku_kits_ml_nbayes *nb,
                                  const uint8_t *x,
                                  uint8_t *result);

/**
 * @brief Get log-likelihood scores for all classes
 *
 * For each class c, computes:
 *
 *     scores[c] = log2(class_count[c] + 1)
 *               + sum_j log2(freq[c][j][x_j] + 1)
 *               - n_features * log2(class_count[c] + n_bins)
 *
 * All log2 values are in Q(shift) fixed-point.  Higher score means
 * more likely class.  O(n_classes * n_features).
 *
 * @param nb     Model with at least 1 training sample
 *               (must not be NULL)
 * @param x      Feature vector of length n_features
 *               (must not be NULL)
 * @param scores Output array; caller must provide space for at least
 *               n_classes int32_t entries (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if nb, x, or scores is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if no training data has been added
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
 *
 * Returns the total sample count across all classes.  Safe to call
 * with a NULL pointer -- returns 0.
 *
 * @param nb Model, or NULL
 * @return Total sample count, or 0 if nb is NULL
 */
uint16_t tiku_kits_ml_nbayes_count(
    const struct tiku_kits_ml_nbayes *nb);

/**
 * @brief Get the number of training samples for a specific class
 *
 * Returns the per-class sample count.  Safe to call with a NULL
 * pointer or out-of-range class -- returns 0 in both cases.
 *
 * @param nb  Model, or NULL
 * @param cls Class label (0..n_classes - 1)
 * @return Sample count for that class, or 0 if nb is NULL or cls
 *         is out of range
 */
uint16_t tiku_kits_ml_nbayes_class_count(
    const struct tiku_kits_ml_nbayes *nb,
    uint8_t cls);

#endif /* TIKU_KITS_ML_NBAYES_H_ */
