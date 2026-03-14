/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_knn.h - k-Nearest Neighbors classifier
 *
 * Platform-independent k-NN classifier for embedded systems.
 * Stores training samples in a static buffer and classifies new
 * points by majority vote among the k nearest neighbors, using
 * squared Euclidean distance (no FPU or sqrt required).
 *
 * Training samples are added one at a time via tiku_kits_ml_knn_add().
 * When the buffer is full, new samples overwrite the oldest (ring
 * buffer), enabling online / sliding-window operation.
 *
 * Distance is computed as:
 *
 *     d(a,b) = sum_j (a[j] - b[j])^2
 *
 * using int64_t accumulation to prevent overflow on 16-bit targets.
 *
 * For prediction, the k nearest stored samples are found via a
 * single linear scan with a partial-sort (selection) step. The
 * class with the most votes wins; ties are broken by the nearest
 * neighbor among tied classes.
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

#ifndef TIKU_KITS_ML_KNN_H_
#define TIKU_KITS_ML_KNN_H_

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
 * Maximum number of input features per sample.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_KNN_MAX_FEATURES
#define TIKU_KITS_ML_KNN_MAX_FEATURES 8
#endif

/**
 * Maximum number of stored training samples.
 * When full, new samples overwrite the oldest (ring buffer).
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_KNN_MAX_SAMPLES
#define TIKU_KITS_ML_KNN_MAX_SAMPLES 32
#endif

/**
 * Maximum number of distinct class labels (0..MAX_CLASSES-1).
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_ML_KNN_MAX_CLASSES
#define TIKU_KITS_ML_KNN_MAX_CLASSES 8
#endif

/**
 * Default value of k (number of neighbors to consider).
 * Must be odd to avoid ties in the common binary case.
 * Override before including this header to change the default.
 */
#ifndef TIKU_KITS_ML_KNN_DEFAULT_K
#define TIKU_KITS_ML_KNN_DEFAULT_K 3
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_ml_elem_t
 *  @brief Element type used for all feature values
 */
typedef TIKU_KITS_ML_ELEM_TYPE tiku_kits_ml_elem_t;

/**
 * @struct tiku_kits_ml_knn
 * @brief k-Nearest Neighbors classifier
 *
 * Stores training data in a flat static buffer and classifies
 * query points by majority vote among the k closest stored samples.
 *
 * Example:
 * @code
 *   struct tiku_kits_ml_knn knn;
 *   uint8_t cls;
 *
 *   tiku_kits_ml_knn_init(&knn, 2, 3);  // 2 features, k=3
 *
 *   // Add training data
 *   tiku_kits_ml_elem_t a[] = {1, 1};
 *   tiku_kits_ml_knn_add(&knn, a, 0);
 *   tiku_kits_ml_elem_t b[] = {2, 2};
 *   tiku_kits_ml_knn_add(&knn, b, 0);
 *   tiku_kits_ml_elem_t c[] = {10, 10};
 *   tiku_kits_ml_knn_add(&knn, c, 1);
 *
 *   // Predict
 *   tiku_kits_ml_elem_t q[] = {3, 3};
 *   tiku_kits_ml_knn_predict(&knn, q, &cls);  // cls = 0
 * @endcode
 */
struct tiku_kits_ml_knn {
    tiku_kits_ml_elem_t
        data[TIKU_KITS_ML_KNN_MAX_SAMPLES]
            [TIKU_KITS_ML_KNN_MAX_FEATURES];
            /**< Feature vectors of stored samples */
    uint8_t labels[TIKU_KITS_ML_KNN_MAX_SAMPLES];
            /**< Class labels of stored samples */
    uint16_t n_samples;    /**< Number of samples currently stored */
    uint16_t write_idx;    /**< Next write position (ring buffer) */
    uint8_t n_features;    /**< Number of input features */
    uint8_t k;             /**< Number of neighbors for voting */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a k-NN model
 * @param knn        Model to initialize
 * @param n_features Number of input features (1..MAX_FEATURES)
 * @param k          Number of neighbors (1..MAX_SAMPLES, should be odd)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (invalid n_features or k)
 *
 * All stored samples are cleared. k must be >= 1.
 */
int tiku_kits_ml_knn_init(struct tiku_kits_ml_knn *knn,
                            uint8_t n_features,
                            uint8_t k);

/**
 * @brief Reset all stored samples
 * @param knn Model to reset
 * @return TIKU_KITS_ML_OK or TIKU_KITS_ML_ERR_NULL
 *
 * Preserves n_features and k.
 */
int tiku_kits_ml_knn_reset(struct tiku_kits_ml_knn *knn);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Change the value of k
 * @param knn Model
 * @param k   New number of neighbors (1..MAX_SAMPLES)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (k == 0 or k > MAX_SAMPLES)
 */
int tiku_kits_ml_knn_set_k(struct tiku_kits_ml_knn *knn,
                              uint8_t k);

/*---------------------------------------------------------------------------*/
/* TRAINING (SAMPLE STORAGE)                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add one training sample
 * @param knn   Model
 * @param x     Feature vector of length n_features
 * @param label Class label (0..MAX_CLASSES-1)
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_PARAM (label >= MAX_CLASSES)
 *
 * If the buffer is full, the oldest sample is overwritten
 * (ring-buffer semantics).
 */
int tiku_kits_ml_knn_add(struct tiku_kits_ml_knn *knn,
                            const tiku_kits_ml_elem_t *x,
                            uint8_t label);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector
 * @param knn    Model (must have at least 1 stored sample)
 * @param x      Feature vector of length n_features
 * @param result Output: predicted class label
 * @return TIKU_KITS_ML_OK, TIKU_KITS_ML_ERR_NULL, or
 *         TIKU_KITS_ML_ERR_SIZE (no training samples stored)
 *
 * Finds min(k, n_samples) nearest neighbors by squared Euclidean
 * distance and returns the majority class. Ties are broken by the
 * class whose nearest representative is closest to the query.
 */
int tiku_kits_ml_knn_predict(const struct tiku_kits_ml_knn *knn,
                               const tiku_kits_ml_elem_t *x,
                               uint8_t *result);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of stored training samples
 * @param knn Model
 * @return Number of samples, or 0 if knn is NULL
 */
uint16_t tiku_kits_ml_knn_count(const struct tiku_kits_ml_knn *knn);

/**
 * @brief Get the current value of k
 * @param knn Model
 * @return Value of k, or 0 if knn is NULL
 */
uint8_t tiku_kits_ml_knn_get_k(const struct tiku_kits_ml_knn *knn);

#endif /* TIKU_KITS_ML_KNN_H_ */
