/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_statistics.h - Running statistics library
 *
 * Platform-independent library providing multiple streaming and windowed
 * statistics trackers for embedded systems:
 *
 *  - Windowed tracker  : circular-buffer mean, variance, min, max
 *  - Welford tracker   : overflow-safe running mean and variance
 *  - Minmax tracker    : O(1) sliding-window min/max (monotonic deque)
 *  - EWMA tracker      : exponentially weighted moving average
 *  - Energy tracker    : running mean-of-squares and RMS
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

#ifndef TIKU_KITS_STATISTICS_H_
#define TIKU_KITS_STATISTICS_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_maths.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum window size (number of samples) for windowed trackers.
 *
 * This compile-time constant defines the upper bound on the circular
 * buffer capacity used by both the windowed statistics tracker and
 * the sliding-window min/max tracker.  Each tracker instance reserves
 * this many sample slots in its static storage.  Choose a value that
 * balances memory usage against the largest analysis window your
 * application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_STATISTICS_MAX_WINDOW 64
 *   #include "tiku_kits_statistics.h"
 * @endcode
 */
#ifndef TIKU_KITS_STATISTICS_MAX_WINDOW
#define TIKU_KITS_STATISTICS_MAX_WINDOW 32
#endif

/**
 * @brief Element type for sample values.
 *
 * Defaults to int32_t, which is safe for integer-only targets with
 * no FPU (e.g. MSP430).  Change to int16_t to halve buffer memory
 * when sample values fit in 16 bits.
 *
 * Override before including this header to change the type:
 * @code
 *   #define TIKU_KITS_STATISTICS_ELEM_TYPE int16_t
 *   #include "tiku_kits_statistics.h"
 * @endcode
 */
#ifndef TIKU_KITS_STATISTICS_ELEM_TYPE
#define TIKU_KITS_STATISTICS_ELEM_TYPE int32_t
#endif

/**
 * @brief Default number of fractional bits for EWMA fixed-point alpha.
 *
 * With shift=8, alpha is an integer in [0, 256] representing a
 * fraction in [0.0, 1.0].  Higher shift gives finer alpha resolution
 * at the cost of a wider intermediate product.  The intermediate
 * multiply is performed in int64_t, so shift values up to 16 are
 * safe.
 *
 * Override before including this header to change the default:
 * @code
 *   #define TIKU_KITS_STATISTICS_EWMA_SHIFT 10
 *   #include "tiku_kits_statistics.h"
 * @endcode
 */
#ifndef TIKU_KITS_STATISTICS_EWMA_SHIFT
#define TIKU_KITS_STATISTICS_EWMA_SHIFT 8
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_statistics_elem_t
 *  @brief Element type used for all sample values
 */
typedef TIKU_KITS_STATISTICS_ELEM_TYPE tiku_kits_statistics_elem_t;

/*===========================================================================*/
/*                                                                           */
/* WINDOWED STATISTICS TRACKER                                               */
/*                                                                           */
/*===========================================================================*/

/**
 * @struct tiku_kits_statistics
 * @brief Sliding-window statistics tracker with static storage
 *
 * Maintains a circular buffer of the most recent samples.  When the
 * window is full, each push evicts the oldest sample and updates the
 * running sum in O(1), so the mean is always available without
 * rescanning the buffer.  Min, max, and variance are computed on
 * demand by scanning the buffer, which is efficient for the small
 * window sizes typical of embedded applications.
 *
 * Two counts are tracked:
 *   - @c capacity -- the runtime window size passed to init (must
 *     be <= TIKU_KITS_STATISTICS_MAX_WINDOW).
 *   - @c count -- the number of samples currently in the buffer.
 *     Grows from 0 to capacity, then stays at capacity as old
 *     samples are evicted.
 *
 * @note Element type is controlled by tiku_kits_statistics_elem_t
 *       (default int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_STATISTICS_ELEM_TYPE=int16_t to change it
 *       globally for all statistics sub-modules.
 *
 * Example:
 * @code
 *   struct tiku_kits_statistics stat;
 *   tiku_kits_statistics_elem_t mean;
 *
 *   tiku_kits_statistics_init(&stat, 8);
 *   tiku_kits_statistics_push(&stat, 100);
 *   tiku_kits_statistics_push(&stat, 200);
 *   tiku_kits_statistics_push(&stat, 150);
 *   tiku_kits_statistics_mean(&stat, &mean);  // mean = 150
 * @endcode
 */
struct tiku_kits_statistics {
    /** Circular sample buffer (statically sized to MAX_WINDOW) */
    tiku_kits_statistics_elem_t
        buf[TIKU_KITS_STATISTICS_MAX_WINDOW];
    tiku_kits_statistics_elem_t sum;  /**< Running sum of all buffered samples */
    uint16_t head;      /**< Next write position in the circular buffer */
    uint16_t count;     /**< Number of valid samples currently stored */
    uint16_t capacity;  /**< Runtime window size set by init (<= MAX_WINDOW) */
};

/*---------------------------------------------------------------------------*/
/* Windowed -- initialization                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a statistics tracker with given window size
 *
 * Resets the tracker to an empty state: count and sum are zeroed,
 * the circular buffer is cleared, and the runtime window capacity
 * is set.  The buffer is zeroed via memset so that uninitialised
 * reads return a deterministic value.
 *
 * @param s        Statistics tracker to initialize (must not be NULL)
 * @param window   Window size (1..TIKU_KITS_STATISTICS_MAX_WINDOW)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if s is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if window is 0 or exceeds
 *         MAX_WINDOW
 */
int tiku_kits_statistics_init(struct tiku_kits_statistics *s,
                              uint16_t window);

/**
 * @brief Clear all samples, preserving the window size
 *
 * Resets count, head, sum, and zeros the buffer while keeping the
 * configured capacity.  Useful for restarting data collection
 * without a full re-init.
 *
 * @param s Statistics tracker to reset (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if s is NULL
 */
int tiku_kits_statistics_reset(struct tiku_kits_statistics *s);

/*---------------------------------------------------------------------------*/
/* Windowed -- sample input                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the window
 *
 * Writes @p value at the current head position.  If the window is
 * already full, the oldest sample is subtracted from the running sum
 * before being overwritten, so the sum always reflects exactly the
 * samples in the buffer.  O(1) per push.
 *
 * @param s     Statistics tracker (must not be NULL)
 * @param value Sample value to add
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if s is NULL
 */
int tiku_kits_statistics_push(struct tiku_kits_statistics *s,
                              tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* Windowed -- queries                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the arithmetic mean of the current window
 *
 * Returns sum / count using integer division (truncation toward zero).
 * O(1) because the running sum is maintained incrementally by push().
 *
 * @param s      Statistics tracker (must not be NULL)
 * @param result Output pointer for the mean value (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if s or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_mean(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Compute the population variance of the current window
 *
 * Computes (1/n) * sum((x_i - mean)^2) by scanning the buffer.
 * O(n) where n is the current sample count.  Uses integer division,
 * so the result is truncated.  This is the population variance (not
 * sample variance with n-1 denominator).
 *
 * @param s      Statistics tracker (must not be NULL)
 * @param result Output pointer for the variance value (must not be
 *               NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if s or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_variance(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the minimum value in the current window
 *
 * Linear scan over the buffer.  O(n) where n is the current count.
 * For O(1) amortized min queries, use the minmax tracker instead.
 *
 * @param s      Statistics tracker (must not be NULL)
 * @param result Output pointer for the minimum value (must not be
 *               NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if s or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_min(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the maximum value in the current window
 *
 * Linear scan over the buffer.  O(n) where n is the current count.
 * For O(1) amortized max queries, use the minmax tracker instead.
 *
 * @param s      Statistics tracker (must not be NULL)
 * @param result Output pointer for the maximum value (must not be
 *               NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if s or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_max(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result);

/*---------------------------------------------------------------------------*/
/* Windowed -- utility                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of samples in the window
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 *
 * @param s Statistics tracker, or NULL
 * @return Number of valid samples currently stored, or 0 if s is NULL
 */
uint16_t tiku_kits_statistics_count(
    const struct tiku_kits_statistics *s);

/**
 * @brief Check if the window is full (count >= capacity)
 *
 * Safe to call with a NULL pointer -- returns 0 (not full).
 *
 * @param s Statistics tracker, or NULL
 * @return 1 if the window is full, 0 otherwise (including when s is
 *         NULL)
 */
int tiku_kits_statistics_full(
    const struct tiku_kits_statistics *s);

/**
 * @brief Return the configured window capacity
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 *
 * @param s Statistics tracker, or NULL
 * @return Window capacity set by init, or 0 if s is NULL
 */
uint16_t tiku_kits_statistics_capacity(
    const struct tiku_kits_statistics *s);

/*===========================================================================*/
/*                                                                           */
/* WELFORD RUNNING MEAN / VARIANCE                                           */
/*                                                                           */
/*===========================================================================*/

/**
 * @struct tiku_kits_statistics_welford
 * @brief Streaming mean and variance using Welford's online algorithm
 *
 * Computes running mean and variance incrementally without storing a
 * sample window.  Each push is O(1) with constant memory -- only
 * three scalars are maintained.  The algorithm avoids accumulator
 * overflow because:
 *   - @c mean stays bounded by the input range (updated with a
 *     delta / n correction).
 *   - @c m2 (sum of squared deviations) uses int64_t so that the
 *     delta products fit even for int32_t inputs.
 *
 * The mean update uses integer division, so there is approximately
 * 1 LSB of truncation error per push.  For most embedded use cases
 * (e.g. sensor averaging) this is acceptable.
 *
 * @note The Welford tracker provides running (cumulative) statistics
 *       over all samples ever pushed, not a sliding window.  For
 *       windowed statistics, use tiku_kits_statistics instead.
 *
 * Example:
 * @code
 *   struct tiku_kits_statistics_welford w;
 *   tiku_kits_statistics_elem_t mean, var;
 *
 *   tiku_kits_statistics_welford_init(&w);
 *   for (i = 0; i < n; i++) {
 *       tiku_kits_statistics_welford_push(&w, samples[i]);
 *   }
 *   tiku_kits_statistics_welford_mean(&w, &mean);
 *   tiku_kits_statistics_welford_variance(&w, &var);
 * @endcode
 */
struct tiku_kits_statistics_welford {
    tiku_kits_statistics_elem_t mean;  /**< Running mean (integer-truncated) */
    int64_t m2;    /**< Sum of squared deviations from mean (Welford M2) */
    uint32_t n;    /**< Total number of samples pushed so far */
};

/*---------------------------------------------------------------------------*/
/* Welford -- initialization                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Welford tracker to an empty state
 *
 * Sets mean, M2, and count to zero.  No window size is needed
 * because the Welford algorithm tracks cumulative statistics.
 *
 * @param w Welford tracker to initialize (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if w is NULL
 */
int tiku_kits_statistics_welford_init(
    struct tiku_kits_statistics_welford *w);

/**
 * @brief Reset a Welford tracker, clearing all samples
 *
 * Equivalent to a fresh init -- mean, M2, and count are zeroed.
 *
 * @param w Welford tracker to reset (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if w is NULL
 */
int tiku_kits_statistics_welford_reset(
    struct tiku_kits_statistics_welford *w);

/*---------------------------------------------------------------------------*/
/* Welford -- sample input                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the Welford tracker
 *
 * Updates the running mean and M2 accumulator in O(1) using
 * Welford's online algorithm:
 *   delta  = value - mean
 *   mean  += delta / n
 *   delta2 = value - mean   (using updated mean)
 *   M2    += delta * delta2
 *
 * @param w     Welford tracker (must not be NULL)
 * @param value Sample value
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if w is NULL
 */
int tiku_kits_statistics_welford_push(
    struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* Welford -- queries                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the running mean
 *
 * Returns the incrementally maintained mean.  O(1) -- just a copy.
 *
 * @param w      Welford tracker (must not be NULL)
 * @param result Output pointer for the mean value (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if w or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_welford_mean(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the running population variance
 *
 * Returns M2 / n, the population variance.  O(1) -- single integer
 * division.  For sample variance (n-1 denominator), the caller can
 * compute M2 / (n-1) externally.
 *
 * @param w      Welford tracker (must not be NULL)
 * @param result Output pointer for the variance value (must not be
 *               NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if w or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_welford_variance(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the running population standard deviation
 *
 * Computes floor(sqrt(variance)) via integer square root.  The
 * result is truncated, not rounded.  Uses the variance from
 * welford_variance() internally.
 *
 * @param w      Welford tracker (must not be NULL)
 * @param result Output pointer for the stddev value (must not be
 *               NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if w or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_welford_stddev(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Return the number of samples pushed
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 *
 * @param w Welford tracker, or NULL
 * @return Total sample count, or 0 if w is NULL
 */
uint32_t tiku_kits_statistics_welford_count(
    const struct tiku_kits_statistics_welford *w);

/*===========================================================================*/
/*                                                                           */
/* SLIDING-WINDOW MIN / MAX (MONOTONIC DEQUE)                                */
/*                                                                           */
/*===========================================================================*/

/**
 * @struct tiku_kits_statistics_minmax
 * @brief O(1) amortized sliding-window min and max tracker
 *
 * Uses two monotonic deques (one ascending for min, one descending
 * for max) backed by a circular sample buffer.  Each push:
 *   1. Expires front entries whose sequence number falls outside
 *      the window (age >= capacity via unsigned modular subtraction).
 *   2. Pops back entries that can never be the min/max again
 *      (because the new value dominates them).
 *   3. Pushes the current sequence number onto the back.
 *
 * This gives O(1) amortized min/max queries.  Sequence numbers are
 * uint16_t and may wrap; expiry uses unsigned subtraction, which
 * handles wrap-around correctly.
 *
 * Memory cost per instance:
 *   buf (MAX_WINDOW * elem_size) + 2 deques (MAX_WINDOW * 2 bytes
 *   each) + bookkeeping fields.
 *
 * @note For simple use cases where occasional O(n) min/max scans are
 *       acceptable, the basic windowed tracker (tiku_kits_statistics)
 *       is smaller and simpler.
 *
 * Example:
 * @code
 *   struct tiku_kits_statistics_minmax mm;
 *   tiku_kits_statistics_elem_t lo, hi;
 *
 *   tiku_kits_statistics_minmax_init(&mm, 8);
 *   tiku_kits_statistics_minmax_push(&mm, 50);
 *   tiku_kits_statistics_minmax_push(&mm, 20);
 *   tiku_kits_statistics_minmax_push(&mm, 80);
 *   tiku_kits_statistics_minmax_min(&mm, &lo);  // lo = 20
 *   tiku_kits_statistics_minmax_max(&mm, &hi);  // hi = 80
 * @endcode
 */
struct tiku_kits_statistics_minmax {
    /** Circular sample buffer (indexed by seq % capacity) */
    tiku_kits_statistics_elem_t
        buf[TIKU_KITS_STATISTICS_MAX_WINDOW];
    /** Min-deque: stores sequence numbers; front holds the minimum */
    uint16_t min_dq[TIKU_KITS_STATISTICS_MAX_WINDOW];
    /** Max-deque: stores sequence numbers; front holds the maximum */
    uint16_t max_dq[TIKU_KITS_STATISTICS_MAX_WINDOW];
    uint16_t min_front;  /**< Min-deque front index (circular) */
    uint16_t min_size;   /**< Min-deque current entry count */
    uint16_t max_front;  /**< Max-deque front index (circular) */
    uint16_t max_size;   /**< Max-deque current entry count */
    uint16_t seq;        /**< Monotonic sequence counter (wraps at 65535) */
    uint16_t count;      /**< Active samples in the window (<= capacity) */
    uint16_t capacity;   /**< Runtime window size set by init */
};

/*---------------------------------------------------------------------------*/
/* Minmax -- initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a sliding-window min/max tracker
 *
 * Resets the tracker to an empty state: deques are cleared, the
 * sequence counter is zeroed, and the buffer is zeroed via memset.
 *
 * @param m      Minmax tracker to initialize (must not be NULL)
 * @param window Window size (1..TIKU_KITS_STATISTICS_MAX_WINDOW)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if window is 0 or exceeds
 *         MAX_WINDOW
 */
int tiku_kits_statistics_minmax_init(
    struct tiku_kits_statistics_minmax *m,
    uint16_t window);

/**
 * @brief Reset a min/max tracker, clearing all samples
 *
 * Equivalent to a fresh init with the same capacity.  Deques and
 * buffer are cleared; the sequence counter is reset to zero.
 *
 * @param m Minmax tracker to reset (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m is NULL
 */
int tiku_kits_statistics_minmax_reset(
    struct tiku_kits_statistics_minmax *m);

/*---------------------------------------------------------------------------*/
/* Minmax -- sample input                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the min/max tracker
 *
 * Stores the sample, expires out-of-window deque entries, pops
 * back entries dominated by the new value, and pushes the current
 * sequence number onto both deques.  O(1) amortized -- each
 * element is pushed and popped at most once per deque.
 *
 * @param m     Minmax tracker (must not be NULL)
 * @param value Sample value
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m is NULL
 */
int tiku_kits_statistics_minmax_push(
    struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* Minmax -- queries                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the minimum value in the current window
 *
 * O(1) -- reads the sample at the front of the min-deque, which
 * is guaranteed to be the smallest value still in the window.
 *
 * @param m      Minmax tracker (must not be NULL)
 * @param result Output pointer for the minimum value (must not be
 *               NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_minmax_min(
    const struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the maximum value in the current window
 *
 * O(1) -- reads the sample at the front of the max-deque, which
 * is guaranteed to be the largest value still in the window.
 *
 * @param m      Minmax tracker (must not be NULL)
 * @param result Output pointer for the maximum value (must not be
 *               NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_minmax_max(
    const struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t *result);

/*---------------------------------------------------------------------------*/
/* Minmax -- utility                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of samples in the window
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 *
 * @param m Minmax tracker, or NULL
 * @return Number of active samples, or 0 if m is NULL
 */
uint16_t tiku_kits_statistics_minmax_count(
    const struct tiku_kits_statistics_minmax *m);

/**
 * @brief Check if the window is full (count >= capacity)
 *
 * Safe to call with a NULL pointer -- returns 0 (not full).
 *
 * @param m Minmax tracker, or NULL
 * @return 1 if the window is full, 0 otherwise (including when m is
 *         NULL)
 */
int tiku_kits_statistics_minmax_full(
    const struct tiku_kits_statistics_minmax *m);

/*===========================================================================*/
/*                                                                           */
/* EXPONENTIALLY WEIGHTED MOVING AVERAGE (EWMA)                              */
/*                                                                           */
/*===========================================================================*/

/**
 * @struct tiku_kits_statistics_ewma
 * @brief Single-state exponentially weighted moving average filter
 *
 * Computes a smoothed estimate of the input signal using one
 * multiply-accumulate per sample:
 *
 *     ewma += alpha * (x - ewma) >> shift
 *
 * The smoothing factor alpha is a fixed-point integer in
 * [0, 1 << shift].  Higher alpha tracks the input more closely
 * (less smoothing); lower alpha smooths more aggressively.  The
 * intermediate product is computed in int64_t to prevent overflow
 * on 16-bit targets.
 *
 * The first sample pushed initializes the EWMA directly (no startup
 * lag or transient).
 *
 * @note Only four bytes of state beyond the current value: alpha
 *       (uint16_t), shift (uint8_t), and a ready flag (uint8_t).
 *       This makes the EWMA tracker the lightest-weight smoother
 *       in the statistics library.
 *
 * Example:
 * @code
 *   struct tiku_kits_statistics_ewma e;
 *   tiku_kits_statistics_elem_t smoothed;
 *
 *   // alpha=32 with shift=8: smoothing ~12.5% per sample
 *   tiku_kits_statistics_ewma_init(&e, 32,
 *       TIKU_KITS_STATISTICS_EWMA_SHIFT);
 *   tiku_kits_statistics_ewma_push(&e, raw_sensor_value);
 *   tiku_kits_statistics_ewma_get(&e, &smoothed);
 * @endcode
 */
struct tiku_kits_statistics_ewma {
    tiku_kits_statistics_elem_t value;  /**< Current smoothed EWMA value */
    uint16_t alpha;  /**< Smoothing factor in [0, 1<<shift] */
    uint8_t shift;   /**< Number of fixed-point fractional bits */
    uint8_t ready;   /**< 1 after first sample has been pushed */
};

/*---------------------------------------------------------------------------*/
/* EWMA -- initialization                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an EWMA tracker
 *
 * Sets the smoothing parameters and clears the ready flag.  The
 * first call to push() will initialize the EWMA value directly
 * from the sample.
 *
 * @param e     EWMA tracker to initialize (must not be NULL)
 * @param alpha Smoothing factor (0..1<<shift).  Higher values track
 *              the input more closely; lower values smooth more.
 * @param shift Number of fixed-point fractional bits (0..16)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if shift > 16
 */
int tiku_kits_statistics_ewma_init(
    struct tiku_kits_statistics_ewma *e,
    uint16_t alpha, uint8_t shift);

/**
 * @brief Reset an EWMA tracker, clearing state
 *
 * Clears the ready flag and EWMA value while preserving alpha and
 * shift.  The next push() will re-initialize the EWMA from scratch.
 *
 * @param e EWMA tracker to reset (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e is NULL
 */
int tiku_kits_statistics_ewma_reset(
    struct tiku_kits_statistics_ewma *e);

/*---------------------------------------------------------------------------*/
/* EWMA -- sample input                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the EWMA tracker
 *
 * The first sample initializes the EWMA value directly (no startup
 * lag).  Subsequent samples apply the smoothing update:
 *   ewma += alpha * (value - ewma) >> shift
 *
 * The intermediate product is computed in int64_t for overflow safety.
 * O(1) per push.
 *
 * @param e     EWMA tracker (must not be NULL)
 * @param value Sample value
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e is NULL
 */
int tiku_kits_statistics_ewma_push(
    struct tiku_kits_statistics_ewma *e,
    tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* EWMA -- queries                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current EWMA value
 *
 * Returns the most recent smoothed value.  O(1) -- just a copy.
 * Fails if no samples have been pushed yet.
 *
 * @param e      EWMA tracker (must not be NULL)
 * @param result Output pointer for the current smoothed value (must
 *               not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_ewma_get(
    const struct tiku_kits_statistics_ewma *e,
    tiku_kits_statistics_elem_t *result);

/*===========================================================================*/
/*                                                                           */
/* RUNNING ENERGY / RMS                                                      */
/*                                                                           */
/*===========================================================================*/

/**
 * @struct tiku_kits_statistics_energy
 * @brief Streaming mean-of-squares and RMS tracker
 *
 * Tracks the running mean of x^2 (signal power) using an incremental
 * update similar to Welford's approach applied to x^2:
 *   mean_sq += (x*x - mean_sq) / n
 *
 * This avoids accumulator overflow because the mean stays bounded by
 * the largest x^2 value seen.  The intermediate x*x is computed in
 * int64_t, which accommodates the full int32_t input range (|x| < 2^31).
 *
 * RMS is derived via integer square root of mean_sq.
 *
 * Useful for vibration magnitude, audio envelope detection, and
 * feature normalization in embedded ML pipelines.
 *
 * @note Only 12 bytes of state: mean_sq (int64_t) + n (uint32_t).
 *       This is the most memory-efficient way to track signal power.
 *
 * Example:
 * @code
 *   struct tiku_kits_statistics_energy e;
 *   tiku_kits_statistics_elem_t rms;
 *
 *   tiku_kits_statistics_energy_init(&e);
 *   for (i = 0; i < n; i++) {
 *       tiku_kits_statistics_energy_push(&e, accel[i]);
 *   }
 *   tiku_kits_statistics_energy_rms(&e, &rms);
 * @endcode
 */
struct tiku_kits_statistics_energy {
    int64_t mean_sq;   /**< Running mean of x^2 (signal power) */
    uint32_t n;        /**< Total number of samples pushed */
};

/*---------------------------------------------------------------------------*/
/* Energy -- initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an energy/RMS tracker to an empty state
 *
 * Sets mean_sq and count to zero.  No window or buffer is needed --
 * the energy tracker computes cumulative statistics.
 *
 * @param e Energy tracker to initialize (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e is NULL
 */
int tiku_kits_statistics_energy_init(
    struct tiku_kits_statistics_energy *e);

/**
 * @brief Reset an energy tracker, clearing all samples
 *
 * Equivalent to a fresh init -- mean_sq and count are zeroed.
 *
 * @param e Energy tracker to reset (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e is NULL
 */
int tiku_kits_statistics_energy_reset(
    struct tiku_kits_statistics_energy *e);

/*---------------------------------------------------------------------------*/
/* Energy -- sample input                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the energy tracker
 *
 * Computes x^2 in int64_t and updates the running mean
 * incrementally: mean_sq += (x^2 - mean_sq) / n.  O(1) per push.
 *
 * @param e     Energy tracker (must not be NULL)
 * @param value Sample value
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e is NULL
 */
int tiku_kits_statistics_energy_push(
    struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* Energy -- queries                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the running mean of x^2 (signal power)
 *
 * Returns the incrementally maintained mean_sq truncated to the
 * element type.  O(1) -- just a cast and copy.
 *
 * @param e      Energy tracker (must not be NULL)
 * @param result Output pointer for mean(x^2) (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_energy_mean_sq(
    const struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the running RMS (root mean square)
 *
 * Computes floor(sqrt(mean_sq)) via a 64-bit integer square root.
 * The result is truncated, not rounded.  O(1) with no loops beyond
 * the sqrt bit-shift algorithm (32 iterations max).
 *
 * @param e      Energy tracker (must not be NULL)
 * @param result Output pointer for the RMS value (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if e or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if no samples have been pushed
 */
int tiku_kits_statistics_energy_rms(
    const struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Return the number of samples pushed
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 *
 * @param e Energy tracker, or NULL
 * @return Total sample count, or 0 if e is NULL
 */
uint32_t tiku_kits_statistics_energy_count(
    const struct tiku_kits_statistics_energy *e);

/*===========================================================================*/
/*                                                                           */
/* UTILITY                                                                   */
/*                                                                           */
/*===========================================================================*/

/**
 * @brief Integer square root (floor)
 *
 * Computes floor(sqrt(x)) using a bit-shifting non-restoring
 * algorithm.  No multiplication is needed -- only shifts, adds,
 * and comparisons -- making it efficient on multiply-challenged
 * targets.  Runs in O(b/2) iterations where b is the bit width of
 * the element type.
 *
 * @param x Non-negative input value (negative values return 0)
 * @return floor(sqrt(x)), or 0 if x <= 0
 */
tiku_kits_statistics_elem_t tiku_kits_statistics_isqrt(
    tiku_kits_statistics_elem_t x);

#endif /* TIKU_KITS_STATISTICS_H_ */
