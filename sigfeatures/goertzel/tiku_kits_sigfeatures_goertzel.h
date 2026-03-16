/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_goertzel.h - Goertzel single-frequency DFT
 *
 * Computes the energy at a specific frequency without a full FFT.
 * Much lower cost than FFT when only one or a few frequency bins are
 * needed. Ideal for tone detection, spectral band energy, and
 * frequency-domain features on MSP430.
 *
 * The algorithm uses a fixed-point Q14 coefficient derived from
 * 2 * cos(2 * pi * k / N), where k is the target bin index and N is
 * the block size. The coefficient must be precomputed offline:
 *
 *     coeff_q14 = round(2 * cos(2 * pi * target_freq / sample_rate
 *                               * block_size / block_size) * 16384)
 *
 * Or equivalently for bin index k:
 *     coeff_q14 = round(2 * cos(2 * pi * k / N) * 16384)
 *
 * Common values (N=64):
 *     k=1  -> coeff_q14 = 32729   (fs/64)
 *     k=2  -> coeff_q14 = 32610   (fs/32)
 *     k=4  -> coeff_q14 = 32138   (fs/16)
 *     k=8  -> coeff_q14 = 30274   (fs/8)
 *     k=16 -> coeff_q14 = 23170   (fs/4)
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

#ifndef TIKU_KITS_SIGFEATURES_GOERTZEL_H_
#define TIKU_KITS_SIGFEATURES_GOERTZEL_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_sigfeatures_goertzel
 * @brief Single-frequency Goertzel DFT filter with fixed-point arithmetic
 *
 * Implements the Goertzel algorithm to compute the squared magnitude
 * of a single DFT bin using only additions, one multiply, and a
 * shift per sample -- much cheaper than a full FFT when only one
 * (or a few) frequency bins are needed.  Ideal for tone detection,
 * spectral-band energy estimation, and frequency-domain feature
 * extraction on resource-constrained targets like the MSP430.
 *
 * The core recurrence maintained per sample is:
 *     s0 = x[n] + coeff * s1 - s2
 * where coeff = 2*cos(2*pi*k/N) is stored in Q14 fixed-point.
 * After N samples, the squared magnitude |X[k]|^2 is computed from
 * the final s1 and s2 values without any further trigonometric
 * operations.
 *
 * Two fields track filter state independently:
 *   - @c s1 / @c s2 -- the two-tap feedback registers, updated on
 *     every push.
 *   - @c count -- number of samples pushed in the current block.
 *     Once count reaches block_size, the magnitude is available.
 *
 * @note The coefficient @c coeff_q14 must be precomputed offline.
 *       See the file-level header comment for the formula and
 *       common values.
 *
 * Example:
 * @code
 *   struct tiku_kits_sigfeatures_goertzel g;
 *   int64_t mag_sq;
 *
 *   // Detect bin k=8 in a 64-sample block
 *   // coeff_q14 = round(2 * cos(2*pi*8/64) * 16384) = 30274
 *   tiku_kits_sigfeatures_goertzel_init(&g, 30274, 64);
 *
 *   for (i = 0; i < 64; i++) {
 *       tiku_kits_sigfeatures_goertzel_push(&g, samples[i]);
 *   }
 *   tiku_kits_sigfeatures_goertzel_magnitude_sq(&g, &mag_sq);
 * @endcode
 */
struct tiku_kits_sigfeatures_goertzel {
    int32_t  s1;          /**< Filter state: previous output (s[n-1]) */
    int32_t  s2;          /**< Filter state: two samples ago (s[n-2]) */
    int16_t  coeff_q14;   /**< 2*cos(2*pi*k/N) scaled to Q14 */
    uint16_t count;       /**< Samples processed in current block so far */
    uint16_t block_size;  /**< N: total samples expected per block */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Goertzel filter for a specific frequency bin
 *
 * Stores the Q14 coefficient and block size, and resets the
 * feedback registers (s1, s2) and sample counter to zero.  Must
 * be called once before any push/magnitude operations.
 *
 * @param g          Goertzel filter to initialize (must not be NULL)
 * @param coeff_q14  Coefficient: round(2*cos(2*pi*k/N) * 16384).
 *                   Precompute this value offline for the target
 *                   frequency bin k and block size N.
 * @param block_size Number of samples per block (N, must be > 0)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if g is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM if block_size is 0
 */
int tiku_kits_sigfeatures_goertzel_init(
    struct tiku_kits_sigfeatures_goertzel *g,
    int16_t coeff_q14,
    uint16_t block_size);

/**
 * @brief Reset the filter state for a new block
 *
 * Clears the feedback registers (s1, s2) and sample counter to
 * zero while preserving the coefficient and block size.  Call this
 * between consecutive blocks to start a fresh DFT computation.
 *
 * @param g Goertzel filter to reset (must not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if g is NULL
 */
int tiku_kits_sigfeatures_goertzel_reset(
    struct tiku_kits_sigfeatures_goertzel *g);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push one sample through the Goertzel filter
 *
 * Applies the Goertzel recurrence:
 *     s0 = x[n] + (coeff_q14 * s1 >> 14) - s2
 * then shifts the feedback registers (s2 = s1, s1 = s0) and
 * increments the sample counter.  O(1) per call with one int64_t
 * multiply for overflow safety.
 *
 * After block_size samples have been pushed, call magnitude_sq()
 * to read the result.
 *
 * @param g     Goertzel filter (must not be NULL)
 * @param value Input sample (x[n])
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if g is NULL
 */
int tiku_kits_sigfeatures_goertzel_push(
    struct tiku_kits_sigfeatures_goertzel *g,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check if a full block of N samples has been processed
 *
 * Returns a boolean indicating whether count has reached
 * block_size, meaning the magnitude can now be read.  Safe to
 * call with a NULL pointer -- returns 0.
 *
 * @param g Goertzel filter, or NULL
 * @return 1 if count >= block_size, 0 otherwise (including NULL)
 */
int tiku_kits_sigfeatures_goertzel_complete(
    const struct tiku_kits_sigfeatures_goertzel *g);

/**
 * @brief Compute the squared magnitude of the target DFT bin
 *
 * Uses the final s1 and s2 values to compute |X[k]|^2 via:
 *     |X[k]|^2 = s1^2 + s2^2 - coeff * s1 * s2
 * The result is proportional to the energy at the target frequency
 * and is not normalized by N.  To compare across different block
 * sizes, divide by N^2.
 *
 * After reading the result, call reset() before processing the
 * next block.
 *
 * @param g      Goertzel filter (must not be NULL, block must be
 *               complete -- i.e. count >= block_size)
 * @param result Output pointer where |X[k]|^2 is written (must
 *               not be NULL).  Stored as int64_t because squared
 *               magnitudes can exceed 32-bit range.
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if g or result is NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA if the block is not
 *         yet complete
 */
int tiku_kits_sigfeatures_goertzel_magnitude_sq(
    const struct tiku_kits_sigfeatures_goertzel *g,
    int64_t *result);

/*---------------------------------------------------------------------------*/
/* Batch operation                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an entire block and return squared magnitude
 *
 * Convenience function that resets the filter, pushes all @p len
 * samples from @p src, and computes the squared magnitude in a
 * single call.  Equivalent to calling reset(), push() in a loop,
 * and magnitude_sq() manually.  O(N) where N is block_size.
 *
 * @param g      Goertzel filter (must not be NULL; will be reset
 *               internally before processing)
 * @param src    Input sample buffer (length must equal block_size,
 *               must not be NULL)
 * @param len    Number of samples (must equal block_size)
 * @param result Output pointer where |X[k]|^2 is written (must
 *               not be NULL)
 * @return TIKU_KITS_SIGFEATURES_OK on success,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL if g, src, or result is
 *         NULL,
 *         TIKU_KITS_SIGFEATURES_ERR_SIZE if len != block_size
 */
int tiku_kits_sigfeatures_goertzel_block(
    struct tiku_kits_sigfeatures_goertzel *g,
    const tiku_kits_sigfeatures_elem_t *src, uint16_t len,
    int64_t *result);

#endif /* TIKU_KITS_SIGFEATURES_GOERTZEL_H_ */
