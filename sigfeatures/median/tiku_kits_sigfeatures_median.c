/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_median.c - Sliding window median filter
 *
 * Circular buffer with insertion-sort-based median computation.
 * Window size is bounded by TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE
 * so the sort is always fast.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_sigfeatures_median.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a median filter with the given window size
 *
 * Validates that window_size is odd, non-zero, and within the
 * maximum.  Zeros the circular buffer, count, and head.
 */
int tiku_kits_sigfeatures_median_init(
    struct tiku_kits_sigfeatures_median *m,
    uint8_t window_size)
{
    if (m == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (window_size == 0
        || window_size > TIKU_KITS_SIGFEATURES_MEDIAN_MAX_SIZE
        || (window_size & 1) == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    m->size = window_size;
    m->count = 0;
    m->head = 0;
    memset(m->window, 0, sizeof(m->window));
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset a median filter, keeping the window size
 *
 * Clears the circular buffer, count, and head.  The configured
 * window size is preserved.
 */
int tiku_kits_sigfeatures_median_reset(
    struct tiku_kits_sigfeatures_median *m)
{
    if (m == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    m->count = 0;
    m->head = 0;
    memset(m->window, 0, sizeof(m->window));
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* SAMPLE INPUT                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the median filter
 *
 * Writes to window[head], advances head mod size.  Increments
 * count up to size.  O(1) per call.
 */
int tiku_kits_sigfeatures_median_push(
    struct tiku_kits_sigfeatures_median *m,
    tiku_kits_sigfeatures_elem_t sample)
{
    if (m == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    m->window[m->head] = sample;
    m->head++;
    if (m->head >= m->size) {
        m->head = 0;
    }

    if (m->count < m->size) {
        m->count++;
    }

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERIES                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Retrieve the current median value
 *
 * Copies the active samples into the sorted scratch array, runs
 * an insertion sort, and returns the middle element.  O(count^2)
 * but count <= MAX_SIZE (typically 7) so this is fast.
 */
int tiku_kits_sigfeatures_median_value(
    struct tiku_kits_sigfeatures_median *m,
    tiku_kits_sigfeatures_elem_t *result)
{
    uint8_t i;
    uint8_t j;
    tiku_kits_sigfeatures_elem_t key;

    if (m == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (m->count == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_NODATA;
    }

    /* Copy active samples to sorted scratch array */
    for (i = 0; i < m->count; i++) {
        m->sorted[i] = m->window[i];
    }

    /* Insertion sort */
    for (i = 1; i < m->count; i++) {
        key = m->sorted[i];
        j = i;
        while (j > 0 && m->sorted[j - 1] > key) {
            m->sorted[j] = m->sorted[j - 1];
            j--;
        }
        m->sorted[j] = key;
    }

    /* Return middle element */
    *result = m->sorted[m->count / 2];
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of samples in the window
 *
 * NULL-safe: returns 0 if m is NULL.
 */
uint8_t tiku_kits_sigfeatures_median_count(
    const struct tiku_kits_sigfeatures_median *m)
{
    if (m == NULL) {
        return 0;
    }
    return m->count;
}
