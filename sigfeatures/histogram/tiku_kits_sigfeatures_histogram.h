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
 * @brief Maximum number of histogram bins the accumulator can hold.
 *
 * This compile-time constant defines the upper bound on the number
 * of bins.  Each histogram instance reserves this many uint16_t
 * counters in its static storage, so choose a value that balances
 * memory usage against distribution resolution.
 *
 * N=8 or N=16 is practical on MSP430 (16--32 bytes of bin
 * storage).  N=32 gives richer distributions at the cost of
 * 64 bytes.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_SIGFEATURES_HISTOGRAM_MAX_BINS 32
 *   #include "tiku_kits_sigfeatures_histogram.h"
 * @endcode
 */
#ifndef TIKU_KITS_SIGFEATURES_HISTOGRAM_MAX_BINS
#define TIKU_KITS_SIGFEATURES_HISTOGRAM_MAX_BINS 16
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_histogram
 * @brief Fixed-width histogram accumulator with underflow/overflow tracking
 *
 * Accumulates samples into N fixed-width bins to build a
 * distribution estimate without storing raw data.  This is a
 * compact representation suitable for signal classification,
 * anomaly detection, and feature vectors on memory-constrained
 * embedded targets.
 *
 * Bin k covers the half-open interval
 *   [bin_min + k * bin_width,  bin_min + (k+1) * bin_width).
 * Samples below bin_min increment @c underflow; samples at or
 * above the upper bound increment @c overflow.  The total count
 * (underflow + sum(bins) + overflow) is tracked in @c total for
 * quick normalisation.
 *
 * @note The bin array is statically sized to MAX_BINS.  The
 *       runtime @c num_bins (passed to init) may be smaller;
 *       unused trailing slots are ignored.
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
        /**< Per-bin sample counts (only indices 0..num_bins-1 are active) */
    tiku_kits_sigfeatures_elem_t bin_min;   /**< Lower bound of bin 0 */
    tiku_kits_sigfeatures_elem_t bin_width; /**< Width of each bin (constant) */
    uint8_t  num_bins;    /**< Active number of bins (<= MAX_BINS) */
    uint16_t total;       /**< Total samples pushed (all bins + over/underflow) */
    uint16_t underflow;   /**< Count of samples below bin_min */
    uint16_t overflow;    /**< Count of samples >= bin_min + bin_width * num_bins */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a histogram with the given bin configuration
 *
 * Configures the number of bins, range origin, and bin width, then
 * zeros all counters (bins, total, underflow, overflow).  After
 * init the histogram covers the range
 * [min_val, min_val + bin_width * num_bins).
 *
 * @param h         Histogram to initialize (must not be NULL)
 * @param num_bins  Number of bins (1..HISTOGRAM_MAX_BINS)
 * @param min_val   Lower bound of the first bin
 * @param bin_width Width of each bin (must be > 0)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if h is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_SIZE if num_bins is 0 or
 *         exceeds MAX_BINS,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if bin_width <= 0
 */
int tiku_kits_sigfeatures_histogram_init(
    struct tiku_kits_sigfeatures_histogram *h,
    uint8_t num_bins,
    tiku_kits_sigfeatures_elem_t min_val,
    tiku_kits_sigfeatures_elem_t bin_width);

/**
 * @brief Reset a histogram, clearing all counts but keeping bin config
 *
 * Zeros all bin counters, total, underflow, and overflow while
 * preserving num_bins, bin_min, and bin_width.  Useful for starting
 * a new accumulation window without reconfiguring the binning
 * parameters.
 *
 * @param h Histogram to reset (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if h is NULL
 */
int tiku_kits_sigfeatures_histogram_reset(
    struct tiku_kits_sigfeatures_histogram *h);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a sample into the histogram
 *
 * Determines which bin the sample falls into via integer division:
 *     bin_idx = (value - bin_min) / bin_width
 * and increments the corresponding counter.  If the value is below
 * bin_min the underflow counter is incremented; if the computed
 * bin index is >= num_bins the overflow counter is incremented.
 * The total counter is always incremented.
 *
 * O(1) per call -- a single subtraction and division.
 *
 * @param h     Histogram (must not be NULL)
 * @param value Sample value to accumulate
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if h is NULL
 */
int tiku_kits_sigfeatures_histogram_push(
    struct tiku_kits_sigfeatures_histogram *h,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the count for a specific bin
 *
 * Returns the number of samples that fell into bin @p bin_idx.
 * Safe to call with a NULL pointer or an out-of-range index --
 * returns 0 in both cases.
 *
 * @param h       Histogram, or NULL
 * @param bin_idx Bin index (0-based, must be < num_bins)
 * @return Bin count, or 0 if h is NULL or bin_idx >= num_bins
 */
uint16_t tiku_kits_sigfeatures_histogram_get_bin(
    const struct tiku_kits_sigfeatures_histogram *h,
    uint8_t bin_idx);

/**
 * @brief Get the total number of samples pushed
 *
 * Returns the sum of all bin counts plus underflow and overflow.
 * Safe to call with a NULL pointer -- returns 0.
 *
 * @param h Histogram, or NULL
 * @return Total sample count, or 0 if h is NULL
 */
uint16_t tiku_kits_sigfeatures_histogram_total(
    const struct tiku_kits_sigfeatures_histogram *h);

/**
 * @brief Get the underflow count (samples below bin_min)
 *
 * Returns the number of samples that were strictly less than
 * bin_min.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param h Histogram, or NULL
 * @return Underflow count, or 0 if h is NULL
 */
uint16_t tiku_kits_sigfeatures_histogram_underflow(
    const struct tiku_kits_sigfeatures_histogram *h);

/**
 * @brief Get the overflow count (samples at or above the upper bound)
 *
 * Returns the number of samples that were >= bin_min + bin_width *
 * num_bins.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param h Histogram, or NULL
 * @return Overflow count, or 0 if h is NULL
 */
uint16_t tiku_kits_sigfeatures_histogram_overflow(
    const struct tiku_kits_sigfeatures_histogram *h);

/**
 * @brief Find the bin with the highest count (the mode bin)
 *
 * Scans all active bins and returns the index of the one with the
 * largest count.  If multiple bins share the same maximum count,
 * the lowest index wins (stable tie-breaking).  O(num_bins).
 *
 * @param h       Histogram (must not be NULL)
 * @param bin_idx Output pointer where the mode bin index is
 *                written (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if h or bin_idx is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA if no samples have
 *         been pushed (total == 0)
 */
int tiku_kits_sigfeatures_histogram_mode_bin(
    const struct tiku_kits_sigfeatures_histogram *h,
    uint8_t *bin_idx);

#endif /* TIKU_KITS_SIGFEATURES_HISTOGRAM_H_ */
