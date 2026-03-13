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
 * @brief Single-frequency Goertzel DFT filter
 *
 * Processes a block of N samples and computes the squared magnitude
 * of a single DFT bin. The filter state (s1, s2) is updated on each
 * push. After block_size samples the magnitude can be read.
 *
 * The coefficient is in Q14 fixed-point: the actual value is
 * coeff_q14 / 16384.
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
    int32_t  s1;          /**< Filter state: previous output */
    int32_t  s2;          /**< Filter state: two samples ago */
    int16_t  coeff_q14;   /**< 2*cos(2*pi*k/N) in Q14 */
    uint16_t count;       /**< Samples processed in current block */
    uint16_t block_size;  /**< N: samples per block */
};

/*---------------------------------------------------------------------------*/
/* Initialization                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Goertzel filter for a specific frequency bin
 * @param g          Goertzel filter to initialize
 * @param coeff_q14  Coefficient: round(2*cos(2*pi*k/N) * 16384)
 * @param block_size Number of samples per block (N, must be > 0)
 * @return TIKU_KITS_SIGFEATURES_OK or
 *         TIKU_KITS_SIGFEATURES_ERR_PARAM
 */
int tiku_kits_sigfeatures_goertzel_init(
    struct tiku_kits_sigfeatures_goertzel *g,
    int16_t coeff_q14,
    uint16_t block_size);

/**
 * @brief Reset the filter state for a new block
 * @param g Goertzel filter to reset
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 *
 * Clears s1, s2, and count. Call between blocks to start fresh.
 */
int tiku_kits_sigfeatures_goertzel_reset(
    struct tiku_kits_sigfeatures_goertzel *g);

/*---------------------------------------------------------------------------*/
/* Sample input                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push one sample through the Goertzel filter
 * @param g     Goertzel filter
 * @param value Sample value
 * @return TIKU_KITS_SIGFEATURES_OK or TIKU_KITS_SIGFEATURES_ERR_NULL
 *
 * Updates the internal state. After block_size samples have been
 * pushed, use magnitude_sq() to read the result.
 */
int tiku_kits_sigfeatures_goertzel_push(
    struct tiku_kits_sigfeatures_goertzel *g,
    tiku_kits_sigfeatures_elem_t value);

/*---------------------------------------------------------------------------*/
/* Queries                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check if a full block has been processed
 * @param g Goertzel filter
 * @return 1 if count >= block_size, 0 otherwise
 */
int tiku_kits_sigfeatures_goertzel_complete(
    const struct tiku_kits_sigfeatures_goertzel *g);

/**
 * @brief Compute the squared magnitude of the target DFT bin
 * @param g      Goertzel filter (must be complete)
 * @param result Output: |X[k]|^2 (not normalized)
 * @return TIKU_KITS_SIGFEATURES_OK or
 *         TIKU_KITS_SIGFEATURES_ERR_NODATA (block not complete)
 *
 * The result is proportional to the energy at the target frequency.
 * It is not normalized by block_size. To compare across different
 * block sizes, divide by N^2.
 *
 * After reading the result, call reset() before processing the
 * next block.
 */
int tiku_kits_sigfeatures_goertzel_magnitude_sq(
    const struct tiku_kits_sigfeatures_goertzel *g,
    int64_t *result);

/*---------------------------------------------------------------------------*/
/* Batch operation                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an entire block and return squared magnitude
 * @param g      Goertzel filter (will be reset internally)
 * @param src    Input samples (length must equal block_size)
 * @param len    Number of samples (must equal block_size)
 * @param result Output: |X[k]|^2
 * @return TIKU_KITS_SIGFEATURES_OK,
 *         TIKU_KITS_SIGFEATURES_ERR_NULL, or
 *         TIKU_KITS_SIGFEATURES_ERR_SIZE (len != block_size)
 *
 * Convenience function that resets, pushes all samples, and
 * computes the magnitude in a single call.
 */
int tiku_kits_sigfeatures_goertzel_block(
    struct tiku_kits_sigfeatures_goertzel *g,
    const tiku_kits_sigfeatures_elem_t *src, uint16_t len,
    int64_t *result);

#endif /* TIKU_KITS_SIGFEATURES_GOERTZEL_H_ */
