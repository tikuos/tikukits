/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_nbayes.c - Categorical Naive Bayes classifier
 *
 * Platform-independent implementation of Naive Bayes classification
 * on discrete (binned) features. All computation uses integer
 * arithmetic with a fixed-point log2 approximation. No heap
 * allocation.
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

#include "tiku_kits_ml_nbayes.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Fixed-point log2 approximation
 *
 * Computes log2(x) in Q(shift) fixed-point using the identity
 * log2(x) = floor_log2(x) + frac, where frac is linearly
 * interpolated from the mantissa bits.  The integer part is the
 * position of the highest set bit; the fractional part is computed
 * as (x - 2^ilog) / 2^ilog in Q(shift).
 *
 * Accuracy: exact for powers of 2, ~0.09 max error for other
 * values.  This is sufficient for comparing log-likelihood scores
 * where only the relative ordering matters.
 */
static int32_t log2_q(uint32_t x, uint8_t shift)
{
    int32_t ilog = 0;
    uint32_t tmp = x;
    int32_t frac;

    if (x <= 1) {
        return 0;
    }

    /* Find integer part: position of highest set bit */
    while (tmp > 1) {
        tmp >>= 1;
        ilog++;
    }

    /*
     * Fractional part via linear interpolation:
     *   frac = (x - 2^ilog) / 2^ilog
     * In Q(shift):
     *   frac_q = ((x - 2^ilog) << shift) >> ilog
     */
    if (ilog <= 0) {
        frac = 0;
    } else {
        uint32_t mantissa = x - ((uint32_t)1 << ilog);
        if ((uint32_t)shift >= (uint32_t)ilog) {
            frac = (int32_t)(mantissa << (shift - ilog));
        } else {
            frac = (int32_t)(mantissa >> (ilog - shift));
        }
    }

    return (ilog << shift) + frac;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Naive Bayes model
 *
 * Validates all configuration parameters, then zeros the frequency
 * table and class counters using memset for efficiency.
 */
int tiku_kits_ml_nbayes_init(struct tiku_kits_ml_nbayes *nb,
                               uint8_t n_features,
                               uint8_t n_bins,
                               uint8_t n_classes,
                               uint8_t shift)
{
    if (nb == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (n_features == 0
        || n_features > TIKU_KITS_ML_NBAYES_MAX_FEATURES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (n_bins < 2 || n_bins > TIKU_KITS_ML_NBAYES_MAX_BINS) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (n_classes < 2
        || n_classes > TIKU_KITS_ML_NBAYES_MAX_CLASSES) {
        return TIKU_KITS_ML_ERR_PARAM;
    }
    if (shift < 1 || shift > 30) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    nb->n_features = n_features;
    nb->n_bins = n_bins;
    nb->n_classes = n_classes;
    nb->shift = shift;
    nb->n_total = 0;

    memset(nb->freq, 0, sizeof(nb->freq));
    memset(nb->class_count, 0, sizeof(nb->class_count));

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset all frequency counters without changing configuration
 *
 * Zeros the frequency table and class counters.  Preserves
 * n_features, n_bins, n_classes, and shift.
 */
int tiku_kits_ml_nbayes_reset(struct tiku_kits_ml_nbayes *nb)
{
    if (nb == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    memset(nb->freq, 0, sizeof(nb->freq));
    memset(nb->class_count, 0, sizeof(nb->class_count));
    nb->n_total = 0;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* TRAINING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Train the model with one sample
 *
 * Validates all bin indices before touching the frequency table so
 * that a partially-valid sample never causes a partial update.
 * O(n_features) for validation + O(n_features) for the update.
 */
int tiku_kits_ml_nbayes_train(struct tiku_kits_ml_nbayes *nb,
                                const uint8_t *x,
                                uint8_t label)
{
    uint8_t j;

    if (nb == NULL || x == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (label >= nb->n_classes) {
        return TIKU_KITS_ML_ERR_PARAM;
    }

    /* Validate ALL bin indices before committing any changes so
     * that a single out-of-range bin does not leave the model in
     * an inconsistent state. */
    for (j = 0; j < nb->n_features; j++) {
        if (x[j] >= nb->n_bins) {
            return TIKU_KITS_ML_ERR_PARAM;
        }
    }

    /* Update frequency table -- one increment per feature */
    for (j = 0; j < nb->n_features; j++) {
        nb->freq[label][j][x[j]]++;
    }
    nb->class_count[label]++;
    nb->n_total++;

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* PREDICTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get log-likelihood scores for all classes
 *
 * For each class c, computes the log-posterior score using Laplace
 * (add-1) smoothing in Q(shift) fixed-point.  The prior
 * log2(class_count+1) favours classes with more training data.
 * The likelihood sum adds log2(freq+1) for each feature.  The
 * normalization term subtracts n_features * log2(class_count+n_bins)
 * to account for the smoothed denominator.
 *
 * Out-of-range bin values in x are treated as unseen (freq = 0)
 * rather than triggering an error, so prediction is robust to
 * occasional quantization glitches.
 */
int tiku_kits_ml_nbayes_predict_log_proba(
    const struct tiku_kits_ml_nbayes *nb,
    const uint8_t *x,
    int32_t *scores)
{
    uint8_t c, j;

    if (nb == NULL || x == NULL || scores == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }
    if (nb->n_total == 0) {
        return TIKU_KITS_ML_ERR_SIZE;
    }

    /*
     * For each class c, compute:
     *
     *   score[c] = log2(class_count[c] + 1)
     *            + sum_j log2(freq[c][j][x[j]] + 1)
     *            - n_features * log2(class_count[c] + n_bins)
     *
     * The +1 in the numerator is Laplace smoothing.
     * The denominator uses (class_count + n_bins) to match
     * the Laplace-smoothed distribution.
     *
     * All log2 values are in Q(shift) fixed-point.
     */
    for (c = 0; c < nb->n_classes; c++) {
        int32_t s;
        uint16_t cc = nb->class_count[c];

        /* Prior: log2(class_count + 1) */
        s = log2_q((uint32_t)(cc + 1), nb->shift);

        /* Likelihood: sum of log2(freq + 1) */
        for (j = 0; j < nb->n_features; j++) {
            uint8_t bin = x[j];
            uint16_t f;

            if (bin >= nb->n_bins) {
                /* Out-of-range bin: treat as unseen (freq=0) */
                f = 0;
            } else {
                f = nb->freq[c][j][bin];
            }
            s += log2_q((uint32_t)(f + 1), nb->shift);
        }

        /* Normalization: - n_features * log2(class_count + n_bins) */
        s -= (int32_t)nb->n_features
             * log2_q((uint32_t)(cc + nb->n_bins), nb->shift);

        scores[c] = s;
    }

    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Classify a feature vector using Naive Bayes
 *
 * Delegates to predict_log_proba() and returns the argmax class.
 * Ties are broken by the lowest class index (deterministic).
 */
int tiku_kits_ml_nbayes_predict(const struct tiku_kits_ml_nbayes *nb,
                                  const uint8_t *x,
                                  uint8_t *result)
{
    int32_t scores[TIKU_KITS_ML_NBAYES_MAX_CLASSES];
    int32_t best_score;
    uint8_t best_class;
    uint8_t c;
    int rc;

    if (nb == NULL || x == NULL || result == NULL) {
        return TIKU_KITS_ML_ERR_NULL;
    }

    rc = tiku_kits_ml_nbayes_predict_log_proba(nb, x, scores);
    if (rc != TIKU_KITS_ML_OK) {
        return rc;
    }

    /* Find class with highest log-likelihood score */
    best_class = 0;
    best_score = scores[0];

    for (c = 1; c < nb->n_classes; c++) {
        if (scores[c] > best_score) {
            best_score = scores[c];
            best_class = c;
        }
    }

    *result = best_class;
    return TIKU_KITS_ML_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the total number of training samples
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint16_t tiku_kits_ml_nbayes_count(
    const struct tiku_kits_ml_nbayes *nb)
{
    if (nb == NULL) {
        return 0;
    }
    return nb->n_total;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of training samples for a specific class
 *
 * Safe to call with a NULL pointer or out-of-range class --
 * returns 0 in both cases rather than dereferencing.
 */
uint16_t tiku_kits_ml_nbayes_class_count(
    const struct tiku_kits_ml_nbayes *nb,
    uint8_t cls)
{
    if (nb == NULL || cls >= nb->n_classes) {
        return 0;
    }
    return nb->class_count[cls];
}
