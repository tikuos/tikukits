/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_distance.h - Distance and similarity metrics
 *
 * Core metrics for nearest-neighbor classifiers and clustering on
 * MSP430. All functions operate on fixed-length vectors with static
 * storage.
 *
 *  - Manhattan (L1)              : sum of absolute differences, no
 *                                  multiply needed
 *  - Squared Euclidean           : sum of squared differences, skip
 *                                  the sqrt (ordering preserved)
 *  - Dot product / cosine approx : MAC loop; equals cosine similarity
 *                                  when vectors are pre-normalized
 *  - Hamming (bitwise)           : XOR + popcount on byte arrays,
 *                                  useful for binary features and LSH
 *
 * Intermediate accumulation uses int64_t to prevent overflow on
 * 16-bit targets. No heap required.
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

#ifndef TIKU_KITS_DISTANCE_H_
#define TIKU_KITS_DISTANCE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_maths.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Element type for vector components.
 *
 * Defaults to int32_t, which is safe for integer-only targets with
 * no FPU (e.g. MSP430).  Change to int16_t to halve memory when
 * feature vectors contain small values (e.g. 8-bit sensor readings
 * sign-extended to 16 bits).
 *
 * Override before including this header to change the type:
 * @code
 *   #define TIKU_KITS_DISTANCE_ELEM_TYPE int16_t
 *   #include "tiku_kits_distance.h"
 * @endcode
 */
#ifndef TIKU_KITS_DISTANCE_ELEM_TYPE
#define TIKU_KITS_DISTANCE_ELEM_TYPE int32_t
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_distance_elem_t
 *  @brief Element type for vector components
 */
typedef TIKU_KITS_DISTANCE_ELEM_TYPE tiku_kits_distance_elem_t;

/*===========================================================================*/
/*                                                                           */
/* MANHATTAN DISTANCE (L1)                                                   */
/*                                                                           */
/*===========================================================================*/

/**
 * @brief Compute the Manhattan (L1) distance between two vectors
 *
 * Sums the absolute element-wise differences: sum |a[i] - b[i]|.
 * No multiplication is needed -- only subtraction and absolute value
 * -- making this the cheapest distance metric for integer targets.
 * Each difference is widened to int64_t before accumulation to prevent
 * overflow on 16-bit architectures even for long vectors.
 *
 * @param a      First vector, length @p len (must not be NULL)
 * @param b      Second vector, length @p len (must not be NULL)
 * @param len    Number of elements (must be > 0)
 * @param result Output pointer for the distance (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if a, b, or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if len == 0
 *
 * @code
 *   tiku_kits_distance_elem_t a[] = {1, 2, 3};
 *   tiku_kits_distance_elem_t b[] = {4, 0, 5};
 *   int64_t dist;
 *   tiku_kits_distance_manhattan(a, b, 3, &dist);
 *   // dist = |1-4| + |2-0| + |3-5| = 3 + 2 + 2 = 7
 * @endcode
 */
int tiku_kits_distance_manhattan(
    const tiku_kits_distance_elem_t *a,
    const tiku_kits_distance_elem_t *b,
    uint16_t len,
    int64_t *result);

/*===========================================================================*/
/*                                                                           */
/* SQUARED EUCLIDEAN DISTANCE                                                */
/*                                                                           */
/*===========================================================================*/

/**
 * @brief Compute the squared Euclidean distance between two vectors
 *
 * Sums the squared element-wise differences: sum (a[i] - b[i])^2.
 * The square root is intentionally omitted because comparing squared
 * distances preserves ordering for nearest-neighbor queries and saves
 * an expensive sqrt operation.  Each difference is widened to int64_t
 * before squaring, so overflow is safe even with int32_t elements.
 * On MSP430 the multiply uses the hardware multiplier via normal C
 * multiply.
 *
 * @param a      First vector, length @p len (must not be NULL)
 * @param b      Second vector, length @p len (must not be NULL)
 * @param len    Number of elements (must be > 0)
 * @param result Output pointer for the squared distance (must not be
 *               NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if a, b, or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if len == 0
 *
 * @code
 *   tiku_kits_distance_elem_t a[] = {1, 2, 3};
 *   tiku_kits_distance_elem_t b[] = {4, 0, 5};
 *   int64_t dist;
 *   tiku_kits_distance_euclidean_sq(a, b, 3, &dist);
 *   // dist = 9 + 4 + 4 = 17
 * @endcode
 */
int tiku_kits_distance_euclidean_sq(
    const tiku_kits_distance_elem_t *a,
    const tiku_kits_distance_elem_t *b,
    uint16_t len,
    int64_t *result);

/*===========================================================================*/
/*                                                                           */
/* DOT PRODUCT / COSINE SIMILARITY                                           */
/*                                                                           */
/*===========================================================================*/

/**
 * @brief Compute the dot product of two vectors
 *
 * Performs a multiply-accumulate (MAC) loop: sum a[i] * b[i].  When
 * both vectors are pre-normalized to unit length (e.g. in fixed-point
 * Q15), the dot product equals cosine similarity.  All products are
 * widened to int64_t before accumulation for overflow safety.
 *
 * For cosine similarity on raw (unnormalized) vectors, use
 * tiku_kits_distance_cosine_sq() instead, which returns the
 * numerator and denominator separately to avoid division.
 *
 * @param a      First vector, length @p len (must not be NULL)
 * @param b      Second vector, length @p len (must not be NULL)
 * @param len    Number of elements (must be > 0)
 * @param result Output pointer for the dot product (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if a, b, or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if len == 0
 *
 * @code
 *   // Pre-normalized Q15 vectors
 *   tiku_kits_distance_elem_t a[] = {16384, 16384, 16384, 16384};
 *   tiku_kits_distance_elem_t b[] = {32767, 0, 0, 0};
 *   int64_t dot;
 *   tiku_kits_distance_dot(a, b, 4, &dot);
 * @endcode
 */
int tiku_kits_distance_dot(
    const tiku_kits_distance_elem_t *a,
    const tiku_kits_distance_elem_t *b,
    uint16_t len,
    int64_t *result);

/**
 * @brief Compute cosine similarity components for raw vectors
 *
 * Computes the three dot products needed for cosine similarity in a
 * single pass over the data, avoiding redundant memory traversals:
 *
 *   cosine similarity = dot_ab / sqrt(dot_aa * dot_bb)
 *
 * The caller receives all three components and can compare similarity
 * without performing the expensive sqrt/divide:
 *
 *   cos^2(theta) = dot_ab^2 / (dot_aa * dot_bb)
 *
 * Comparing cos^2 values preserves similarity ordering.  All products
 * are accumulated in int64_t for overflow safety.
 *
 * @param a      First vector, length @p len (must not be NULL)
 * @param b      Second vector, length @p len (must not be NULL)
 * @param len    Number of elements (must be > 0)
 * @param dot_ab Output: dot(a, b) = sum a[i]*b[i] (must not be NULL)
 * @param dot_aa Output: dot(a, a) = sum a[i]^2, squared magnitude of
 *               a (must not be NULL)
 * @param dot_bb Output: dot(b, b) = sum b[i]^2, squared magnitude of
 *               b (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if len == 0
 */
int tiku_kits_distance_cosine_sq(
    const tiku_kits_distance_elem_t *a,
    const tiku_kits_distance_elem_t *b,
    uint16_t len,
    int64_t *dot_ab,
    int64_t *dot_aa,
    int64_t *dot_bb);

/*===========================================================================*/
/*                                                                           */
/* HAMMING DISTANCE (BITWISE)                                                */
/*                                                                           */
/*===========================================================================*/

/**
 * @brief Compute the bitwise Hamming distance between two byte arrays
 *
 * XORs corresponding bytes and counts the number of set bits using a
 * nibble lookup table (16 bytes in flash) for fast popcount.  This
 * is extremely cheap -- no multiplications, just XOR and table
 * lookups -- making it ideal for binary feature vectors, locality-
 * sensitive hashing (LSH), and error counting.
 *
 * @param a      First byte array, length @p len (must not be NULL)
 * @param b      Second byte array, length @p len (must not be NULL)
 * @param len    Number of bytes to compare (must be > 0)
 * @param result Output pointer for the number of differing bits (must
 *               not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if a, b, or result is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if len == 0
 *
 * @code
 *   uint8_t a[] = {0xFF, 0x00, 0xAA};
 *   uint8_t b[] = {0x0F, 0x00, 0x55};
 *   uint32_t dist;
 *   tiku_kits_distance_hamming(a, b, 3, &dist);
 *   // dist = popcount(0xF0) + popcount(0x00) + popcount(0xFF)
 *   //      = 4 + 0 + 8 = 12
 * @endcode
 */
int tiku_kits_distance_hamming(
    const uint8_t *a,
    const uint8_t *b,
    uint16_t len,
    uint32_t *result);

#endif /* TIKU_KITS_DISTANCE_H_ */
