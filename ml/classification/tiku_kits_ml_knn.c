/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_knn.c - k-Nearest Neighbors classifier
 *
 * Platform-independent implementation of k-NN classification.
 * All computation uses integer arithmetic with int64_t intermediates
 * for distance accumulation. No heap allocation.
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

#include "tiku_kits_ml_knn.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute squared Euclidean distance between two feature vectors
 *
 * Accumulates sum_j (a[j] - b[j])^2 in int64_t to prevent overflow.
 * On a 16-bit target with int32_t features, a single squared
 * difference can reach 2^62, so int64_t is essential.  The result
 * is always non-negative.  No sqrt() is needed because k-NN only
 * requires relative distance ordering, not absolute distances.
 */
static int64_t sq_distance(const tiku_kits_ml_elem_t *a,
                           const tiku_kits_ml_elem_t *b,
                           uint8_t n)
{
    int64_t sum = 0;
    int64_t diff;
    uint8_t j;

    for (j = 0; j < n; j++) {
        diff = (int64_t)a[j] - (int64_t)b[j];
        sum += diff * diff;
    }

    return sum;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a k-NN model
 *
 * Validates n_features and k, then clears the ring-buffer state.
 * The backing data buffer is not explicitly zeroed for efficiency;
 * n_samples = 0 ensures no stale data is accessed.
 */
int tiku_kits_ml_knn_init(struct tiku_kits_ml_knn *knn,
                            uint8_t n_features,
                            uint8_t k)
{
    if (knn == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (n_features == 0
        || n_features > TIKU_KITS_ML_KNN_MAX_FEATURES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (k == 0 || k > TIKU_KITS_ML_KNN_MAX_SAMPLES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    knn->n_features = n_features;
    knn->k = k;
    knn->n_samples = 0;
    knn->write_idx = 0;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset all stored samples without changing configuration
 *
 * Logically removes all samples by resetting the counters.
 * Preserves n_features and k.
 */
int tiku_kits_ml_knn_reset(struct tiku_kits_ml_knn *knn)
{
    if (knn == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    knn->n_samples = 0;
    knn->write_idx = 0;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Change the number of neighbors used for voting
 *
 * Updates k in place.  No stored data is modified or invalidated.
 */
int tiku_kits_ml_knn_set_k(struct tiku_kits_ml_knn *knn,
                              uint8_t k)
{
    if (knn == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (k == 0 || k > TIKU_KITS_ML_KNN_MAX_SAMPLES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    knn->k = k;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* TRAINING (SAMPLE STORAGE)                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Add one training sample to the ring buffer
 *
 * Copies the feature vector element-by-element into the current
 * write slot, stores the label, then advances the write pointer.
 * When the buffer is full the oldest sample is silently overwritten
 * (ring-buffer semantics).  O(n_features) per call.
 */
int tiku_kits_ml_knn_add(struct tiku_kits_ml_knn *knn,
                            const tiku_kits_ml_elem_t *x,
                            uint8_t label)
{
    uint8_t j;

    if (knn == NULL || x == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (label >= TIKU_KITS_ML_KNN_MAX_CLASSES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    /* Copy feature vector into the current ring-buffer slot */
    for (j = 0; j < knn->n_features; j++) {
        knn->data[knn->write_idx][j] = x[j];
    }
    knn->labels[knn->write_idx] = label;

    /* Advance write pointer, wrapping at the buffer boundary */
    knn->write_idx++;
    if (knn->write_idx >= TIKU_KITS_ML_KNN_MAX_SAMPLES) {
        knn->write_idx = 0;
    }

    /* Track sample count -- caps at MAX_SAMPLES once the buffer
     * is full; after that, add() only overwrites old entries. */
    if (knn->n_samples < TIKU_KITS_ML_KNN_MAX_SAMPLES) {
        knn->n_samples++;
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector by majority vote of k neighbors
 *
 * Algorithm overview:
 *   1. Compute squared Euclidean distance from the query to every
 *      stored sample -- O(n_samples * n_features).
 *   2. Maintain a sorted array of the k nearest neighbors using
 *      insertion sort.  For each new distance, if the array is not
 *      full or the distance beats the current k-th nearest, shift
 *      elements right and insert in sorted position -- O(k) per
 *      candidate, O(n_samples * k) total.
 *   3. Tally votes per class among the k nearest neighbors, also
 *      tracking the nearest representative per class for tie-breaking.
 *   4. Return the class with the most votes.  Ties are broken by
 *      whichever tied class has its nearest representative closest
 *      to the query.
 */
int tiku_kits_ml_knn_predict(const struct tiku_kits_ml_knn *knn,
                               const tiku_kits_ml_elem_t *x,
                               uint8_t *result)
{
    int64_t nn_dist[TIKU_KITS_ML_KNN_MAX_SAMPLES];
    uint8_t nn_label[TIKU_KITS_ML_KNN_MAX_SAMPLES];
    uint16_t nn_count;
    uint16_t actual_k;
    uint16_t i;
    uint16_t j;

    /* Vote tallying */
    uint8_t votes[TIKU_KITS_ML_KNN_MAX_CLASSES];
    int64_t nearest_dist[TIKU_KITS_ML_KNN_MAX_CLASSES];
    uint8_t best_class;
    uint8_t best_votes;
    int64_t best_nearest;

    if (knn == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (knn->n_samples == 0) {
        return TIKU_KITS_ML_ERR_SIZE;
    }

    /* Effective k is min(k, n_samples) */
    actual_k = knn->k;
    if (actual_k > knn->n_samples) {
        actual_k = knn->n_samples;
    }

    /*
     * Build a sorted list of the k nearest neighbors.
     * For each stored sample, compute distance and insert into
     * the sorted nn_dist/nn_label arrays if it belongs in the
     * top k. The array is kept sorted in ascending distance order.
     */
    nn_count = 0;

    for (i = 0; i < knn->n_samples; i++) {
        int64_t d = sq_distance(x, knn->data[i], knn->n_features);

        if (nn_count < actual_k) {
            /* Array not full yet: insert in sorted position */
            j = nn_count;
            while (j > 0 && nn_dist[j - 1] > d) {
                nn_dist[j] = nn_dist[j - 1];
                nn_label[j] = nn_label[j - 1];
                j--;
            }
            nn_dist[j] = d;
            nn_label[j] = knn->labels[i];
            nn_count++;
        } else if (d < nn_dist[nn_count - 1]) {
            /* Closer than the current k-th nearest: replace it */
            j = nn_count - 1;
            while (j > 0 && nn_dist[j - 1] > d) {
                nn_dist[j] = nn_dist[j - 1];
                nn_label[j] = nn_label[j - 1];
                j--;
            }
            nn_dist[j] = d;
            nn_label[j] = knn->labels[i];
        }
    }

    /* Tally votes and track nearest representative per class */
    memset(votes, 0, sizeof(votes));
    for (i = 0; i < TIKU_KITS_ML_KNN_MAX_CLASSES; i++) {
        nearest_dist[i] = INT64_MAX;
    }

    for (i = 0; i < nn_count; i++) {
        uint8_t cls = nn_label[i];
        if (cls < TIKU_KITS_ML_KNN_MAX_CLASSES) {
            votes[cls]++;
            if (nn_dist[i] < nearest_dist[cls]) {
                nearest_dist[cls] = nn_dist[i];
            }
        }
    }

    /* Find class with most votes; break ties by nearest distance */
    best_class = 0;
    best_votes = 0;
    best_nearest = INT64_MAX;

    for (i = 0; i < TIKU_KITS_ML_KNN_MAX_CLASSES; i++) {
        if (votes[i] > best_votes
            || (votes[i] == best_votes
                && votes[i] > 0
                && nearest_dist[i] < best_nearest)) {
            best_class = (uint8_t)i;
            best_votes = votes[i];
            best_nearest = nearest_dist[i];
        }
    }

    *result = best_class;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of stored training samples
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the logical sample count, not the
 * write index.
 */
uint16_t tiku_kits_ml_knn_count(const struct tiku_kits_ml_knn *knn)
{
    if (knn == NULL) {
        return 0;
    }
    return knn->n_samples;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current value of k
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint8_t tiku_kits_ml_knn_get_k(const struct tiku_kits_ml_knn *knn)
{
    if (knn == NULL) {
        return 0;
    }
    return knn->k;
}
