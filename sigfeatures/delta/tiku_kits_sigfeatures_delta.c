/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_delta.c - First-order difference (delta)
 *
 * Streaming and batch x[n] - x[n-1] computation.
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

#include "tiku_kits_sigfeatures_delta.h"

/*---------------------------------------------------------------------------*/
/* STREAMING -- INITIALIZATION                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a delta tracker to its empty state
 *
 * Zeros prev, last_delta, and the ready flag so the tracker
 * is prepared to accept its first sample via push().
 */
int tiku_kits_sigfeatures_delta_init(
    struct tiku_kits_sigfeatures_delta *d)
{
    if (d == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    d->prev = 0;
    d->last_delta = 0;
    d->ready = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset a delta tracker, returning it to the pre-first-push state
 *
 * Functionally identical to init().  Clears prev, last_delta, and
 * the ready flag so the next push() will seed a fresh sequence.
 */
int tiku_kits_sigfeatures_delta_reset(
    struct tiku_kits_sigfeatures_delta *d)
{
    if (d == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    d->prev = 0;
    d->last_delta = 0;
    d->ready = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* STREAMING -- SAMPLE INPUT                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample and compute the first-order difference
 *
 * On the first call, the sample is stored in prev and ready is set
 * to 1 but no delta is computed.  On every subsequent call,
 * last_delta = value - prev.  O(1) per call.
 */
int tiku_kits_sigfeatures_delta_push(
    struct tiku_kits_sigfeatures_delta *d,
    tiku_kits_sigfeatures_elem_t value)
{
    if (d == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    if (d->ready) {
        /* Compute the discrete derivative: x[n] - x[n-1] */
        d->last_delta = value - d->prev;
    } else {
        /* First sample: seed prev only -- no predecessor exists
         * so no meaningful delta can be produced yet. */
        d->last_delta = 0;
        d->ready = 1;
    }

    d->prev = value;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* STREAMING -- QUERIES                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Retrieve the most recently computed delta value
 *
 * Copies last_delta into *result.  Returns ERR_NODATA if fewer
 * than two samples have been pushed (the ready flag is still 0).
 */
int tiku_kits_sigfeatures_delta_get(
    const struct tiku_kits_sigfeatures_delta *d,
    tiku_kits_sigfeatures_elem_t *result)
{
    if (d == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (!d->ready) {
        return TIKU_KITS_SIGFEATURES_ERR_NODATA;
    }

    *result = d->last_delta;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* BATCH OPERATION                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute first-order differences for an entire buffer
 *
 * Iterates from index 0 to src_len-2 computing dst[i] = src[i+1] -
 * src[i].  O(n) where n is src_len.  The loop runs forward through
 * both buffers so cache / FRAM prefetch is sequential.
 */
int tiku_kits_sigfeatures_delta_compute(
    const tiku_kits_sigfeatures_elem_t *src, uint16_t src_len,
    tiku_kits_sigfeatures_elem_t *dst)
{
    uint16_t i;

    if (src == NULL || dst == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (src_len < 2) {
        return TIKU_KITS_SIGFEATURES_ERR_SIZE;
    }

    for (i = 0; i < src_len - 1; i++) {
        dst[i] = src[i + 1] - src[i];
    }

    return TIKU_KITS_SIGFEATURES_OK;
}
