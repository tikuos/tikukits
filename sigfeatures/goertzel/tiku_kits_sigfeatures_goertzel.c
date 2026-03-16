/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_goertzel.c - Goertzel single-frequency DFT
 *
 * Fixed-point Goertzel filter using Q14 coefficient. All intermediate
 * products use int64_t to prevent overflow on 16-bit targets.
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

#include "tiku_kits_sigfeatures_goertzel.h"

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a Goertzel filter for a specific frequency bin
 *
 * Stores the precomputed Q14 coefficient and block size, then zeros
 * the feedback registers (s1, s2) and sample counter so the filter
 * is ready to accept its first block of samples.
 */
int tiku_kits_sigfeatures_goertzel_init(
    struct tiku_kits_sigfeatures_goertzel *g,
    int16_t coeff_q14,
    uint16_t block_size)
{
    if (g == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (block_size == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    g->coeff_q14 = coeff_q14;
    g->block_size = block_size;
    g->s1 = 0;
    g->s2 = 0;
    g->count = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset filter state for a new block
 *
 * Clears s1, s2, and count while preserving the coefficient and
 * block size.  Call between consecutive blocks.
 */
int tiku_kits_sigfeatures_goertzel_reset(
    struct tiku_kits_sigfeatures_goertzel *g)
{
    if (g == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    g->s1 = 0;
    g->s2 = 0;
    g->count = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* SAMPLE INPUT                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push one sample through the Goertzel filter
 *
 * Applies the second-order IIR recurrence:
 *   s0 = x[n] + (coeff_q14 * s1 >> 14) - s2
 * then shifts s2 <- s1, s1 <- s0.  The multiplication is widened
 * to int64_t before the right-shift to prevent overflow on 16-bit
 * targets.
 */
int tiku_kits_sigfeatures_goertzel_push(
    struct tiku_kits_sigfeatures_goertzel *g,
    tiku_kits_sigfeatures_elem_t value)
{
    int32_t s0;

    if (g == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    /*
     * Goertzel recurrence:
     *   s0 = x[n] + coeff * s1 - s2
     *
     * coeff is in Q14, so multiply then shift right 14 to get
     * the integer result. Use int64_t for the intermediate product
     * to prevent overflow.
     */
    s0 = (int32_t)value
       + (int32_t)(((int64_t)g->coeff_q14 * g->s1) >> 14)
       - g->s2;

    g->s2 = g->s1;
    g->s1 = s0;
    g->count++;

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERIES                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check if a full block of N samples has been processed
 *
 * Returns 1 when count >= block_size, meaning the magnitude can
 * now be read.  Safe to call with a NULL pointer -- returns 0.
 */
int tiku_kits_sigfeatures_goertzel_complete(
    const struct tiku_kits_sigfeatures_goertzel *g)
{
    if (g == NULL) {
        return 0;
    }
    return (g->count >= g->block_size) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the squared magnitude of the target DFT bin
 *
 * Evaluates |X[k]|^2 = s1^2 + s2^2 - coeff*s1*s2 using the
 * final feedback register values.  The coeff*s1 product is
 * computed in Q14 then multiplied by s2 in int64_t to prevent
 * overflow.  The result is not normalized by N.
 */
int tiku_kits_sigfeatures_goertzel_magnitude_sq(
    const struct tiku_kits_sigfeatures_goertzel *g,
    int64_t *result)
{
    int64_t cs1;

    if (g == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (g->count < g->block_size) {
        return TIKU_KITS_SIGFEATURES_ERR_NODATA;
    }

    /*
     * |X[k]|^2 = s1^2 + s2^2 - coeff * s1 * s2
     *
     * Rearranged for fixed-point:
     *   cs1 = (coeff_q14 * s1) >> 14   (≈ coeff * s1)
     *   mag_sq = s1*s1 + s2*s2 - cs1*s2
     */
    cs1 = ((int64_t)g->coeff_q14 * g->s1) >> 14;

    *result = (int64_t)g->s1 * g->s1
            + (int64_t)g->s2 * g->s2
            - cs1 * g->s2;

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* BATCH OPERATION                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Process an entire block and return squared magnitude
 *
 * Convenience wrapper that resets the filter, pushes all len
 * samples through the Goertzel recurrence, and computes the
 * squared magnitude in a single call.  O(N).
 */
int tiku_kits_sigfeatures_goertzel_block(
    struct tiku_kits_sigfeatures_goertzel *g,
    const tiku_kits_sigfeatures_elem_t *src, uint16_t len,
    int64_t *result)
{
    uint16_t i;
    int rc;

    if (g == NULL || src == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (len != g->block_size) {
        return TIKU_KITS_SIGFEATURES_ERR_SIZE;
    }

    /* Reset feedback registers for the new block so previous
     * state does not leak into the current computation. */
    g->s1 = 0;
    g->s2 = 0;
    g->count = 0;

    for (i = 0; i < len; i++) {
        tiku_kits_sigfeatures_goertzel_push(g, src[i]);
    }

    rc = tiku_kits_sigfeatures_goertzel_magnitude_sq(g, result);
    return rc;
}
