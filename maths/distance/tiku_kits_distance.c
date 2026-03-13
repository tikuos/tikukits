/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_distance.c - Distance and similarity metrics
 *
 * Platform-independent implementation of common distance and
 * similarity metrics for nearest-neighbor classifiers and clustering.
 * All accumulation uses int64_t to prevent overflow on 16-bit targets.
 * No heap usage.
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

#include "tiku_kits_distance.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Nibble popcount lookup table (16 bytes in flash)
 *
 * nibble_popcount[x] = number of 1-bits in the 4-bit value x.
 */
static const uint8_t nibble_popcount[16] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

/**
 * @brief Count the number of 1-bits in a byte
 * @param x Byte value
 * @return Number of set bits (0..8)
 */
static uint8_t popcount8(uint8_t x)
{
    return nibble_popcount[x & 0x0F] + nibble_popcount[x >> 4];
}

/*===========================================================================*/
/*                                                                           */
/* MANHATTAN DISTANCE (L1)                                                   */
/*                                                                           */
/*===========================================================================*/

int tiku_kits_distance_manhattan(
    const tiku_kits_distance_elem_t *a,
    const tiku_kits_distance_elem_t *b,
    uint16_t len,
    int64_t *result)
{
    int64_t sum;
    int64_t diff;
    uint16_t i;

    if (a == NULL || b == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (len == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    sum = 0;
    for (i = 0; i < len; i++) {
        diff = (int64_t)a[i] - (int64_t)b[i];
        sum += (diff >= 0) ? diff : -diff;
    }

    *result = sum;
    return TIKU_KITS_MATHS_OK;
}

/*===========================================================================*/
/*                                                                           */
/* SQUARED EUCLIDEAN DISTANCE                                                */
/*                                                                           */
/*===========================================================================*/

int tiku_kits_distance_euclidean_sq(
    const tiku_kits_distance_elem_t *a,
    const tiku_kits_distance_elem_t *b,
    uint16_t len,
    int64_t *result)
{
    int64_t sum;
    int64_t diff;
    uint16_t i;

    if (a == NULL || b == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (len == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    sum = 0;
    for (i = 0; i < len; i++) {
        diff = (int64_t)a[i] - (int64_t)b[i];
        sum += diff * diff;
    }

    *result = sum;
    return TIKU_KITS_MATHS_OK;
}

/*===========================================================================*/
/*                                                                           */
/* DOT PRODUCT / COSINE SIMILARITY                                           */
/*                                                                           */
/*===========================================================================*/

int tiku_kits_distance_dot(
    const tiku_kits_distance_elem_t *a,
    const tiku_kits_distance_elem_t *b,
    uint16_t len,
    int64_t *result)
{
    int64_t sum;
    uint16_t i;

    if (a == NULL || b == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (len == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    sum = 0;
    for (i = 0; i < len; i++) {
        sum += (int64_t)a[i] * (int64_t)b[i];
    }

    *result = sum;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_distance_cosine_sq(
    const tiku_kits_distance_elem_t *a,
    const tiku_kits_distance_elem_t *b,
    uint16_t len,
    int64_t *dot_ab,
    int64_t *dot_aa,
    int64_t *dot_bb)
{
    int64_t ab;
    int64_t aa;
    int64_t bb;
    uint16_t i;

    if (a == NULL || b == NULL
        || dot_ab == NULL || dot_aa == NULL || dot_bb == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (len == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    ab = 0;
    aa = 0;
    bb = 0;

    for (i = 0; i < len; i++) {
        ab += (int64_t)a[i] * (int64_t)b[i];
        aa += (int64_t)a[i] * (int64_t)a[i];
        bb += (int64_t)b[i] * (int64_t)b[i];
    }

    *dot_ab = ab;
    *dot_aa = aa;
    *dot_bb = bb;
    return TIKU_KITS_MATHS_OK;
}

/*===========================================================================*/
/*                                                                           */
/* HAMMING DISTANCE (BITWISE)                                                */
/*                                                                           */
/*===========================================================================*/

int tiku_kits_distance_hamming(
    const uint8_t *a,
    const uint8_t *b,
    uint16_t len,
    uint32_t *result)
{
    uint32_t dist;
    uint16_t i;

    if (a == NULL || b == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (len == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    dist = 0;
    for (i = 0; i < len; i++) {
        dist += popcount8(a[i] ^ b[i]);
    }

    *result = dist;
    return TIKU_KITS_MATHS_OK;
}
