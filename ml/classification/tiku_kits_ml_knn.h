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
 * @brief Element type for feature values.
 *
 * Defaults to int32_t for integer-only targets (no FPU required).
 * All ML sub-modules share this type so that feature vectors can be
 * passed between classifiers without casting.
 *
 * Override before including this header to change the type:
 * @code
 *   #define TIKU_KITS_ML_ELEM_TYPE int16_t
 *   #include "tiku_kits_ml_knn.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_ELEM_TYPE
#define TIKU_KITS_ML_ELEM_TYPE int32_t
#endif

/**
 * @brief Maximum number of input features per sample.
 *
 * Determines the dimensionality of each stored feature vector.
 * Each sample reserves this many element slots in the static
 * data buffer, so choose a value that covers your data without
 * wasting memory.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_KNN_MAX_FEATURES 16
 *   #include "tiku_kits_ml_knn.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_KNN_MAX_FEATURES
#define TIKU_KITS_ML_KNN_MAX_FEATURES 8
#endif

/**
 * @brief Maximum number of stored training samples.
 *
 * When the buffer is full, new samples overwrite the oldest entry
 * (ring-buffer semantics), enabling online / sliding-window
 * operation without explicit buffer management.  Total static
 * memory usage is approximately
 * MAX_SAMPLES * (MAX_FEATURES * sizeof(elem_t) + 1) bytes.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_KNN_MAX_SAMPLES 64
 *   #include "tiku_kits_ml_knn.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_KNN_MAX_SAMPLES
#define TIKU_KITS_ML_KNN_MAX_SAMPLES 32
#endif

/**
 * @brief Maximum number of distinct class labels (0..MAX_CLASSES-1).
 *
 * Controls the size of the vote-tally array during prediction.
 * Labels must be integers in [0, MAX_CLASSES).
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_ML_KNN_MAX_CLASSES 16
 *   #include "tiku_kits_ml_knn.h"
 * @endcode
 */
#ifndef TIKU_KITS_ML_KNN_MAX_CLASSES
#define TIKU_KITS_ML_KNN_MAX_CLASSES 8
#endif

/**
 * @brief Default value of k (number of neighbors to consider).
 *
 * Using an odd value avoids ties in the common binary-classification
 * case.  Can be overridden at init time or changed later via
 * set_k().
 *
 * Override before including this header to change the default:
 * @code
 *   #define TIKU_KITS_ML_KNN_DEFAULT_K 5
 *   #include "tiku_kits_ml_knn.h"
 * @endcode
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
 * @brief k-Nearest Neighbors classifier with ring-buffer storage
 *
 * A lazy-learning classifier that stores training samples in a flat
 * static buffer and classifies query points by majority vote among
 * the k closest stored samples, using squared Euclidean distance.
 *
 * Two indices manage the ring buffer:
 *   - @c write_idx -- points to the next slot that will be written.
 *     Advances after each add() call and wraps at MAX_SAMPLES.
 *   - @c n_samples -- tracks how many distinct samples are stored
 *     (capped at MAX_SAMPLES).  Once full, add() overwrites the
 *     oldest sample without incrementing n_samples.
 *
 * @note Distance is computed as sum_j(a[j]-b[j])^2 in int64_t to
 *       prevent overflow on 16-bit targets.  No FPU or sqrt()
 *       required because only relative ordering matters.
 *
 * @note Because all storage lives inside the struct, no heap
 *       allocation is needed -- just declare the model as a static
 *       or local variable.
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
            /**< Feature vectors of stored samples, indexed by
                 sample number then feature dimension */
    uint8_t labels[TIKU_KITS_ML_KNN_MAX_SAMPLES];
            /**< Class labels of stored samples (0..MAX_CLASSES-1) */
    uint16_t n_samples;    /**< Number of valid samples stored
                                (capped at MAX_SAMPLES) */
    uint16_t write_idx;    /**< Next write position in the ring
                                buffer (wraps at MAX_SAMPLES) */
    uint8_t n_features;    /**< Dimensionality of feature vectors
                                (set at init time) */
    uint8_t k;             /**< Number of neighbors used for
                                majority voting */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a k-NN model
 *
 * Sets the feature dimensionality and neighbor count, then clears
 * the sample buffer.  Samples must be added via add() before
 * prediction is possible.
 *
 * @param knn        Model to initialize (must not be NULL)
 * @param n_features Number of input features
 *                   (1..TIKU_KITS_ML_KNN_MAX_FEATURES)
 * @param k          Number of neighbors for voting
 *                   (1..TIKU_KITS_ML_KNN_MAX_SAMPLES; odd values
 *                   recommended for binary classification)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if knn is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if n_features is 0 or exceeds
 *         MAX_FEATURES, or k is 0 or exceeds MAX_SAMPLES
 */
int tiku_kits_ml_knn_init(struct tiku_kits_ml_knn *knn,
                            uint8_t n_features,
                            uint8_t k);

/**
 * @brief Reset all stored samples without changing configuration
 *
 * Logically removes all samples by resetting n_samples and
 * write_idx to 0.  The backing data buffer is not zeroed for
 * efficiency -- old values remain but are inaccessible because
 * predict() only considers indices < n_samples.  Preserves
 * n_features and k.
 *
 * @param knn Model to reset (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if knn is NULL
 */
int tiku_kits_ml_knn_reset(struct tiku_kits_ml_knn *knn);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Change the number of neighbors used for voting
 *
 * Allows tuning k at run time without reinitializing or discarding
 * stored samples.  If k exceeds the current sample count,
 * predict() will automatically use min(k, n_samples).
 *
 * @param knn Model (must not be NULL)
 * @param k   New number of neighbors
 *            (1..TIKU_KITS_ML_KNN_MAX_SAMPLES)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if knn is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if k is 0 or exceeds MAX_SAMPLES
 */
int tiku_kits_ml_knn_set_k(struct tiku_kits_ml_knn *knn,
                              uint8_t k);

/*---------------------------------------------------------------------------*/
/* TRAINING (SAMPLE STORAGE)                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add one training sample to the model
 *
 * Copies the feature vector and label into the next ring-buffer
 * slot and advances the write pointer.  If the buffer is full,
 * the oldest sample is silently overwritten, enabling online /
 * sliding-window operation.  O(n_features) -- one copy per feature.
 *
 * @param knn   Model (must not be NULL)
 * @param x     Feature vector of length n_features (must not be NULL)
 * @param label Class label (0..TIKU_KITS_ML_KNN_MAX_CLASSES - 1)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if knn or x is NULL,
 *         TIKU_KITS_ML_ERR_PARAM if label >= MAX_CLASSES
 */
int tiku_kits_ml_knn_add(struct tiku_kits_ml_knn *knn,
                            const tiku_kits_ml_elem_t *x,
                            uint8_t label);

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector by majority vote of k neighbors
 *
 * Computes the squared Euclidean distance from the query to every
 * stored sample, maintains a sorted list of the min(k, n_samples)
 * nearest neighbors, then tallies votes per class.  The class with
 * the most votes wins; ties are broken in favour of the class whose
 * nearest representative is closest to the query point.
 *
 * O(n_samples * n_features) for distance computation plus
 * O(n_samples * k) for insertion into the sorted neighbor list.
 *
 * @param knn    Model with at least 1 stored sample
 *               (must not be NULL)
 * @param x      Feature vector of length n_features
 *               (must not be NULL)
 * @param result Output pointer where the predicted class label is
 *               written (must not be NULL)
 * @return TIKU_KITS_ML_OK on success,
 *         TIKU_KITS_ML_ERR_NULL if knn, x, or result is NULL,
 *         TIKU_KITS_ML_ERR_SIZE if no training samples are stored
 */
int tiku_kits_ml_knn_predict(const struct tiku_kits_ml_knn *knn,
                               const tiku_kits_ml_elem_t *x,
                               uint8_t *result);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of stored training samples
 *
 * Returns the logical sample count (capped at MAX_SAMPLES), not the
 * write index.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param knn Model, or NULL
 * @return Number of samples currently stored, or 0 if knn is NULL
 */
uint16_t tiku_kits_ml_knn_count(const struct tiku_kits_ml_knn *knn);

/**
 * @brief Get the current value of k
 *
 * Returns the configured neighbor count.  Safe to call with a NULL
 * pointer -- returns 0.
 *
 * @param knn Model, or NULL
 * @return Value of k, or 0 if knn is NULL
 */
uint8_t tiku_kits_ml_knn_get_k(const struct tiku_kits_ml_knn *knn);

#endif /* TIKU_KITS_ML_KNN_H_ */
