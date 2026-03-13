/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_histogram.h - Fixed-width histogram / binning
 *
 * Accumulates samples into N fixed-width bins to build a distribution
 * estimate without storing raw data. Practical with N=8 or N=16 bins
 * on MSP430. Useful for signal classification, anomaly detection, and
 * compact feature representation.
 *
 * Bins cover [min_val, min_val + bin_width * num_bins). Samples below
 * min_val are counted as underflow; samples at or above the upper
 * bound are counted as overflow.
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

#ifndef TIKU_KITS_SIGFEATURES_HISTOGRAM_H_
#define TIKU_KITS_SIGFEATURES_HISTOGRAM_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of histogram bins.
 * Override before including this header to change the limit.
 * N=8 or N=16 is practical on MSP430; N=32 for richer distributions.
 */
#ifndef TIKU_KITS_SIGFEATURES_HISTOGRAM_MAX_BINS
#define TIKU_KITS_SIGFEATURES_HISTOGRAM_MAX_BINS 16
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_histogram
 * @brief Fixed-width histogram accumulator
 *
 * Maintains an array of bin counts plus underflow/overflow counters.
 * Bin k covers [min_val + k * bin_width, min_val + (k+1) * bin_width).
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_histogram h;
 *   // 8 bins covering [0, 800), each bin width 100
 *   tiku_kits_sigfeatures_histogram_init(&h, 8, 0, 100);
 *   tiku_kits_sigfeatures_histogram_push(&h, 150);  // bin 1
 *   tiku_kits_sigfeatures_histogram_push(&h, 350);  // bin 3
 *   tiku_kits_sigfeatures_histogram_push(&h, 900);  // overflow
 * @endcode
 */
struct tiku_kits_sigfeatures_histogram {
    uint16_t bins[TIKU_KITS_SIGFEATURES_HISTOGRAM_MAX_BINS];
        /**< Bin counts */
    tiku_kits_sigfeatures_elem_t bin_min;   /**< Lower bound of bin 0 */
    tiku_kits_sigfeatures_elem_t bin_width; /**< Width of each bin */
    uint8_t  num_bins;    /**< Active number of bins (<= MAX_BINS) */
    uint16_t total;       /**< Total samples pushed */
    uint16_t underflow;   /**< Samples below bin_min */
    uint16_t overflow;    /**< Samples >= bin_min + bin_width * num_bins */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a histogram
 * @param h         Histogram to initialize
 * @param num_bins  Number of bins (1..MAX_BINS)
 * @param min_val   Lower bound of the first bin
 * @param bin_width Width of each bin (must be > 0)
 * @return TIKU_KITS_SIGFEATURES_OK, TIKU_KITS_SIGFEATURES_ERR_NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_SIZE, or
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM
 *
 * Bins cover [min_val, min_val + bin_width * num_bins).
 */
int tiku_kits_sigfeatures_histogram_init(
    struct tiku_kits_sigfeatures_histogram *h,
    uint8_t num_bins,
    tiku_kits_sigfeatures_elem_t min_val,
    tiku_kits_sigfeatures_elem_t bin_width);

/**
 * @brief Reset a histogram, clearing all counts but keeping bin config
 * @param h Histogram to reset
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 */
int tiku_kits_sigfeatures_histogram_reset(
    struct tiku_kits_sigfeatures_histogram *h);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a sample into the histogram
 * @param h     Histogram
 * @param value Sample value
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 *
 * The appropriate bin is incremented. If the value falls outside the
 * bin range, the underflow or overflow counter is incremented instead.
 */
int tiku_kits_sigfeatures_histogram_push(
    struct tiku_kits_sigfeatures_histogram *h,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the count for a specific bin
 * @param h       Histogram
 * @param bin_idx Bin index (0-based)
 * @return Bin count, or 0 if out of range or h is NULL
 */
uint16_t tiku_kits_sigfeatures_histogram_get_bin(
    const struct tiku_kits_sigfeatures_histogram *h,
    uint8_t bin_idx);

/**
 * @brief Get the total number of samples pushed
 * @param h Histogram
 * @return Total count, or 0 if h is NULL
 */
uint16_t tiku_kits_sigfeatures_histogram_total(
    const struct tiku_kits_sigfeatures_histogram *h);

/**
 * @brief Get the underflow count (samples below min_val)
 * @param h Histogram
 * @return Underflow count, or 0 if h is NULL
 */
uint16_t tiku_kits_sigfeatures_histogram_underflow(
    const struct tiku_kits_sigfeatures_histogram *h);

/**
 * @brief Get the overflow count (samples above range)
 * @param h Histogram
 * @return Overflow count, or 0 if h is NULL
 */
uint16_t tiku_kits_sigfeatures_histogram_overflow(
    const struct tiku_kits_sigfeatures_histogram *h);

/**
 * @brief Find the bin with the highest count (mode)
 * @param h       Histogram
 * @param bin_idx Output: index of the mode bin
 * @return TIKU_KITS_SIGFEATURES_OK or
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA (no samples)
 *
 * If multiple bins share the maximum count, the lowest index wins.
 */
int tiku_kits_sigfeatures_histogram_mode_bin(
    const struct tiku_kits_sigfeatures_histogram *h,
    uint8_t *bin_idx);

#endif /* TIKU_KITS_SIGFEATURES_HISTOGRAM_H_ */
