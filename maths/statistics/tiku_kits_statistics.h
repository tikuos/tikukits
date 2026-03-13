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
 * Maximum window size (number of samples) for windowed trackers.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_STATISTICS_MAX_WINDOW
#define TIKU_KITS_STATISTICS_MAX_WINDOW 32
#endif

/**
 * Element type for sample values.
 * Defaults to int32_t for integer-only targets (no FPU).
 * Define as int16_t before including if memory is very tight.
 */
#ifndef TIKU_KITS_STATISTICS_ELEM_TYPE
#define TIKU_KITS_STATISTICS_ELEM_TYPE int32_t
#endif

/**
 * Default number of fractional bits for EWMA fixed-point alpha.
 * With shift=8, alpha ranges from 0 (no update) to 256 (track input).
 * Override before including this header to change the default.
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
 * Maintains a circular buffer of the most recent samples. When the
 * window is full, the oldest sample is evicted on each push. Mean,
 * variance, min, and max are computed over the current window contents.
 *
 * A running sum is maintained for O(1) mean computation. Min, max, and
 * variance are computed on demand by scanning the buffer, which is
 * efficient for typical embedded window sizes.
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
    /** Circular sample buffer */
    tiku_kits_statistics_elem_t
        buf[TIKU_KITS_STATISTICS_MAX_WINDOW];
    tiku_kits_statistics_elem_t sum;  /**< Running sum */
    uint16_t head;      /**< Next write position in buffer */
    uint16_t count;     /**< Current number of samples */
    uint16_t capacity;  /**< Window size (<= MAX_WINDOW) */
};

/*---------------------------------------------------------------------------*/
/* Windowed -- initialization                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a statistics tracker with given window size
 * @param s        Statistics tracker to initialize
 * @param window   Window size (1..TIKU_KITS_STATISTICS_MAX_WINDOW)
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_SIZE
 */
int tiku_kits_statistics_init(struct tiku_kits_statistics *s,
                              uint16_t window);

/**
 * @brief Clear all samples, preserving the window size
 * @param s Statistics tracker to reset
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 */
int tiku_kits_statistics_reset(struct tiku_kits_statistics *s);

/*---------------------------------------------------------------------------*/
/* Windowed -- sample input                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the window
 * @param s     Statistics tracker
 * @param value Sample value to add
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 *
 * If the window is full, the oldest sample is evicted and the running
 * sum is updated accordingly.
 */
int tiku_kits_statistics_push(struct tiku_kits_statistics *s,
                              tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* Windowed -- queries                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the arithmetic mean of the current window
 * @param s      Statistics tracker
 * @param result Output: mean value (integer division)
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 */
int tiku_kits_statistics_mean(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Compute the population variance of the current window
 * @param s      Statistics tracker
 * @param result Output: variance value (integer division)
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 *
 * Computes (1/n) * sum((x_i - mean)^2) by scanning the buffer.
 */
int tiku_kits_statistics_variance(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the minimum value in the current window
 * @param s      Statistics tracker
 * @param result Output: minimum value
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 */
int tiku_kits_statistics_min(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the maximum value in the current window
 * @param s      Statistics tracker
 * @param result Output: maximum value
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 */
int tiku_kits_statistics_max(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result);

/*---------------------------------------------------------------------------*/
/* Windowed -- utility                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of samples in the window
 * @param s Statistics tracker
 * @return Number of samples, or 0 if s is NULL
 */
uint16_t tiku_kits_statistics_count(
    const struct tiku_kits_statistics *s);

/**
 * @brief Check if the window is full
 * @param s Statistics tracker
 * @return 1 if full, 0 otherwise
 */
int tiku_kits_statistics_full(
    const struct tiku_kits_statistics *s);

/**
 * @brief Get the configured window capacity
 * @param s Statistics tracker
 * @return Window capacity, or 0 if s is NULL
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
 * sample window. Each push is O(1) with no risk of accumulator
 * overflow -- the mean stays bounded by the input range, and the M2
 * accumulator uses int64_t for safe delta products.
 *
 * The mean update uses integer division, so there is ~1 LSB of
 * truncation error. For most embedded use cases this is acceptable.
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
    tiku_kits_statistics_elem_t mean;  /**< Running mean */
    int64_t m2;    /**< Sum of squared deviations from mean */
    uint32_t n;    /**< Sample count */
};

/*---------------------------------------------------------------------------*/
/* Welford -- initialization                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Welford tracker
 * @param w Welford tracker to initialize
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 */
int tiku_kits_statistics_welford_init(
    struct tiku_kits_statistics_welford *w);

/**
 * @brief Reset a Welford tracker, clearing all samples
 * @param w Welford tracker to reset
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 */
int tiku_kits_statistics_welford_reset(
    struct tiku_kits_statistics_welford *w);

/*---------------------------------------------------------------------------*/
/* Welford -- sample input                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the Welford tracker
 * @param w     Welford tracker
 * @param value Sample value
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 *
 * Updates the running mean and M2 accumulator in O(1).
 */
int tiku_kits_statistics_welford_push(
    struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* Welford -- queries                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the running mean
 * @param w      Welford tracker
 * @param result Output: mean value
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 */
int tiku_kits_statistics_welford_mean(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the running population variance
 * @param w      Welford tracker
 * @param result Output: variance = M2 / n
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 */
int tiku_kits_statistics_welford_variance(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the running population standard deviation
 * @param w      Welford tracker
 * @param result Output: stddev = isqrt(variance)
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 *
 * Uses integer square root, so the result is floor(sqrt(variance)).
 */
int tiku_kits_statistics_welford_stddev(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the number of samples pushed
 * @param w Welford tracker
 * @return Sample count, or 0 if w is NULL
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
 * for max) backed by a circular sample buffer. Each push evicts
 * expired entries and maintains the deque invariant, giving O(1)
 * amortized min/max queries over the window.
 *
 * Sequence numbers (uint16_t, wrapping) track sample age for
 * correct expiry via unsigned modular subtraction.
 *
 * Memory cost: buf + 2 deques = 3 * MAX_WINDOW * elem_size +
 * 2 * MAX_WINDOW * 2 bytes + overhead.
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
    /** Circular sample buffer */
    tiku_kits_statistics_elem_t
        buf[TIKU_KITS_STATISTICS_MAX_WINDOW];
    /** Min-deque: sequence numbers, front holds the minimum */
    uint16_t min_dq[TIKU_KITS_STATISTICS_MAX_WINDOW];
    /** Max-deque: sequence numbers, front holds the maximum */
    uint16_t max_dq[TIKU_KITS_STATISTICS_MAX_WINDOW];
    uint16_t min_front;  /**< Min-deque front index */
    uint16_t min_size;   /**< Min-deque entry count */
    uint16_t max_front;  /**< Max-deque front index */
    uint16_t max_size;   /**< Max-deque entry count */
    uint16_t seq;        /**< Monotonic sequence counter */
    uint16_t count;      /**< Active samples (<= capacity) */
    uint16_t capacity;   /**< Window size */
};

/*---------------------------------------------------------------------------*/
/* Minmax -- initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a sliding-window min/max tracker
 * @param m      Minmax tracker to initialize
 * @param window Window size (1..TIKU_KITS_STATISTICS_MAX_WINDOW)
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_SIZE
 */
int tiku_kits_statistics_minmax_init(
    struct tiku_kits_statistics_minmax *m,
    uint16_t window);

/**
 * @brief Reset a min/max tracker, clearing all samples
 * @param m Minmax tracker to reset
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 */
int tiku_kits_statistics_minmax_reset(
    struct tiku_kits_statistics_minmax *m);

/*---------------------------------------------------------------------------*/
/* Minmax -- sample input                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the min/max tracker
 * @param m     Minmax tracker
 * @param value Sample value
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 *
 * Updates both monotonic deques in O(1) amortized time.
 */
int tiku_kits_statistics_minmax_push(
    struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* Minmax -- queries                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the minimum value in the current window
 * @param m      Minmax tracker
 * @param result Output: minimum value
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 */
int tiku_kits_statistics_minmax_min(
    const struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the maximum value in the current window
 * @param m      Minmax tracker
 * @param result Output: maximum value
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 */
int tiku_kits_statistics_minmax_max(
    const struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t *result);

/*---------------------------------------------------------------------------*/
/* Minmax -- utility                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of samples in the window
 * @param m Minmax tracker
 * @return Number of samples, or 0 if m is NULL
 */
uint16_t tiku_kits_statistics_minmax_count(
    const struct tiku_kits_statistics_minmax *m);

/**
 * @brief Check if the window is full
 * @param m Minmax tracker
 * @return 1 if full, 0 otherwise
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
 * The smoothing factor alpha is a fixed-point integer in [0, 1<<shift].
 * Higher alpha tracks the input more closely (less smoothing); lower
 * alpha smooths more aggressively. The intermediate product is
 * computed in int64_t to prevent overflow.
 *
 * The first sample pushed initializes the EWMA directly (no lag).
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
    tiku_kits_statistics_elem_t value;  /**< Current EWMA */
    uint16_t alpha;  /**< Smoothing factor [0..1<<shift] */
    uint8_t shift;   /**< Fixed-point fractional bits */
    uint8_t ready;   /**< 1 after first sample pushed */
};

/*---------------------------------------------------------------------------*/
/* EWMA -- initialization                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an EWMA tracker
 * @param e     EWMA tracker to initialize
 * @param alpha Smoothing factor (0..1<<shift)
 * @param shift Number of fractional bits
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (shift > 16)
 */
int tiku_kits_statistics_ewma_init(
    struct tiku_kits_statistics_ewma *e,
    uint16_t alpha, uint8_t shift);

/**
 * @brief Reset an EWMA tracker, clearing state
 * @param e EWMA tracker to reset
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 */
int tiku_kits_statistics_ewma_reset(
    struct tiku_kits_statistics_ewma *e);

/*---------------------------------------------------------------------------*/
/* EWMA -- sample input                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the EWMA tracker
 * @param e     EWMA tracker
 * @param value Sample value
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 *
 * The first sample initializes the EWMA value directly.
 * Subsequent samples apply the smoothing update.
 */
int tiku_kits_statistics_ewma_push(
    struct tiku_kits_statistics_ewma *e,
    tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* EWMA -- queries                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current EWMA value
 * @param e      EWMA tracker
 * @param result Output: current smoothed value
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
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
 * update that avoids accumulator overflow. The intermediate x*x is
 * computed in int64_t. RMS is derived via integer square root.
 *
 * Useful for vibration magnitude, audio envelope detection, and
 * feature normalization.
 *
 * Note: for int32_t inputs, |x| must be < 2^31 for x*x to fit in
 * int64_t. This covers the full int32_t range.
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
    int64_t mean_sq;   /**< Running mean of x^2 */
    uint32_t n;        /**< Sample count */
};

/*---------------------------------------------------------------------------*/
/* Energy -- initialization                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an energy/RMS tracker
 * @param e Energy tracker to initialize
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 */
int tiku_kits_statistics_energy_init(
    struct tiku_kits_statistics_energy *e);

/**
 * @brief Reset an energy tracker, clearing all samples
 * @param e Energy tracker to reset
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 */
int tiku_kits_statistics_energy_reset(
    struct tiku_kits_statistics_energy *e);

/*---------------------------------------------------------------------------*/
/* Energy -- sample input                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the energy tracker
 * @param e     Energy tracker
 * @param value Sample value
 * @return TIKU_KITS_MATHS_OK or TIKU_KITS_MATHS_ERR_NULL
 *
 * Updates the running mean of x^2 incrementally.
 */
int tiku_kits_statistics_energy_push(
    struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t value);

/*---------------------------------------------------------------------------*/
/* Energy -- queries                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the running mean of x^2 (signal power)
 * @param e      Energy tracker
 * @param result Output: mean(x^2), truncated to element type
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 */
int tiku_kits_statistics_energy_mean_sq(
    const struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the running RMS (root mean square)
 * @param e      Energy tracker
 * @param result Output: floor(sqrt(mean(x^2)))
 * @return TIKU_KITS_MATHS_OK, TIKU_KITS_MATHS_ERR_NULL, or
 *         TIKU_KITS_MATHS_ERR_SIZE (if no samples)
 *
 * Uses integer square root; result is floor(sqrt(mean_sq)).
 */
int tiku_kits_statistics_energy_rms(
    const struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t *result);

/**
 * @brief Get the number of samples pushed
 * @param e Energy tracker
 * @return Sample count, or 0 if e is NULL
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
 * @param x Non-negative input value
 * @return floor(sqrt(x)), or 0 if x <= 0
 *
 * Uses a bit-shifting non-restoring algorithm with no
 * multiplication. Suitable for all supported element types.
 */
tiku_kits_statistics_elem_t tiku_kits_statistics_isqrt(
    tiku_kits_statistics_elem_t x);

#endif /* TIKU_KITS_STATISTICS_H_ */
