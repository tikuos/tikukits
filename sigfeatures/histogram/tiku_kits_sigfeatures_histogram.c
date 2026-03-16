/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_histogram.c - Fixed-width histogram / binning
 *
 * Accumulates samples into N fixed-width bins. O(1) per push.
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

#include "tiku_kits_sigfeatures_histogram.h"

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a histogram with the given bin configuration
 *
 * Validates parameters, stores the binning configuration, and
 * zeros all counters including the full static bins[] array via
 * memset so that unused trailing slots are deterministic.
 */
int tiku_kits_sigfeatures_histogram_init(
    struct tiku_kits_sigfeatures_histogram *h,
    uint8_t num_bins,
    tiku_kits_sigfeatures_elem_t min_val,
    tiku_kits_sigfeatures_elem_t bin_width)
{
    if (h == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (num_bins == 0
        || num_bins > TIKU_KITS_SIGFEATURES_HISTOGRAM_MAX_BINS) {
        return TIKU_KITS_SIGFEATURES_ERR_SIZE;
    }
    if (bin_width <= 0) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    h->num_bins = num_bins;
    h->bin_min = min_val;
    h->bin_width = bin_width;
    h->total = 0;
    h->underflow = 0;
    h->overflow = 0;
    memset(h->bins, 0, sizeof(h->bins));
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset a histogram, clearing all counts but keeping bin config
 *
 * Zeros total, underflow, overflow, and the full bins[] array.
 * Preserves num_bins, bin_min, and bin_width from the last init().
 */
int tiku_kits_sigfeatures_histogram_reset(
    struct tiku_kits_sigfeatures_histogram *h)
{
    if (h == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    h->total = 0;
    h->underflow = 0;
    h->overflow = 0;
    memset(h->bins, 0, sizeof(h->bins));
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* SAMPLE INPUT                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a sample into the histogram
 *
 * Computes the target bin via integer division of the offset by
 * bin_width.  O(1) per call.  Underflow and overflow are handled
 * before and after the division respectively to avoid unnecessary
 * computation on out-of-range samples.
 */
int tiku_kits_sigfeatures_histogram_push(
    struct tiku_kits_sigfeatures_histogram *h,
    tiku_kits_sigfeatures_elem_t value)
{
    tiku_kits_sigfeatures_elem_t offset;
    uint16_t bin_idx;

    if (h == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    h->total++;

    /* Fast path for underflow -- skip the division entirely */
    if (value < h->bin_min) {
        h->underflow++;
        return TIKU_KITS_SIGFEATURES_OK;
    }

    /* Integer division to find the bin index */
    offset = value - h->bin_min;
    bin_idx = (uint16_t)(offset / h->bin_width);

    if (bin_idx >= h->num_bins) {
        h->overflow++;
    } else {
        h->bins[bin_idx]++;
    }

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERIES                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the count for a specific bin
 *
 * Returns the count at bin_idx, or 0 if h is NULL or the index
 * is out of range.  No error code -- the zero return doubles as
 * a safe fallback.
 */
uint16_t tiku_kits_sigfeatures_histogram_get_bin(
    const struct tiku_kits_sigfeatures_histogram *h,
    uint8_t bin_idx)
{
    if (h == NULL || bin_idx >= h->num_bins) {
        return 0;
    }
    return h->bins[bin_idx];
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the total number of samples pushed
 *
 * Returns the aggregate count of all pushes (bins + underflow +
 * overflow).  Returns 0 if h is NULL.
 */
uint16_t tiku_kits_sigfeatures_histogram_total(
    const struct tiku_kits_sigfeatures_histogram *h)
{
    if (h == NULL) {
        return 0;
    }
    return h->total;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the underflow count (samples below bin_min)
 *
 * Returns the number of samples that were strictly less than
 * bin_min.  Returns 0 if h is NULL.
 */
uint16_t tiku_kits_sigfeatures_histogram_underflow(
    const struct tiku_kits_sigfeatures_histogram *h)
{
    if (h == NULL) {
        return 0;
    }
    return h->underflow;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the overflow count (samples at or above upper bound)
 *
 * Returns the number of samples that were >= bin_min + bin_width *
 * num_bins.  Returns 0 if h is NULL.
 */
uint16_t tiku_kits_sigfeatures_histogram_overflow(
    const struct tiku_kits_sigfeatures_histogram *h)
{
    if (h == NULL) {
        return 0;
    }
    return h->overflow;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Find the bin with the highest count (the mode bin)
 *
 * Linear scan over all active bins tracking the running maximum.
 * O(num_bins).  Ties are broken in favour of the lowest index
 * because the comparison uses strict greater-than.
 */
int tiku_kits_sigfeatures_histogram_mode_bin(
    const struct tiku_kits_sigfeatures_histogram *h,
    uint8_t *bin_idx)
{
    uint8_t best;
    uint16_t best_count;
    uint8_t i;

    if (h == NULL || bin_idx == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (h->total == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_NODATA;
    }

    best = 0;
    best_count = h->bins[0];

    /* Forward scan -- first maximum wins (stable tie-break) */
    for (i = 1; i < h->num_bins; i++) {
        if (h->bins[i] > best_count) {
            best_count = h->bins[i];
            best = i;
        }
    }

    *bin_idx = best;
    return TIKU_KITS_SIGFEATURES_OK;
}
