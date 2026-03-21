/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_skip.c - Skip (decimation) filter
 *
 * Outputs every Nth sample by counting pushes and latching the
 * sample when the counter reaches the configured interval.
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

#include "tiku_kits_sigfeatures_skip.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a skip filter with the given decimation interval
 *
 * Validates that interval >= 1, then zeros all state fields.
 */
int tiku_kits_sigfeatures_skip_init(
    struct tiku_kits_sigfeatures_skip *s,
    uint16_t interval)
{
    if (s == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (interval == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    s->interval = interval;
    s->counter = 0;
    s->last = 0;
    s->ready = 0;
    s->count = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset a skip filter, keeping the interval
 *
 * Clears the counter, output, ready flag, and output count.
 * The interval is preserved.
 */
int tiku_kits_sigfeatures_skip_reset(
    struct tiku_kits_sigfeatures_skip *s)
{
    if (s == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    s->counter = 0;
    s->last = 0;
    s->ready = 0;
    s->count = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* SAMPLE INPUT                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the skip filter
 *
 * Increments counter.  When counter == interval, latches the sample
 * as output, resets counter to 0, sets ready = 1, and increments
 * the output count.  Otherwise clears ready.  O(1) per call.
 */
int tiku_kits_sigfeatures_skip_push(
    struct tiku_kits_sigfeatures_skip *s,
    tiku_kits_sigfeatures_elem_t sample)
{
    if (s == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    s->counter++;

    if (s->counter >= s->interval) {
        s->last = sample;
        s->ready = 1;
        s->counter = 0;
        s->count++;
    } else {
        s->ready = 0;
    }

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERIES                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether the last push produced an output
 *
 * NULL-safe: returns 0 if s is NULL.
 */
uint8_t tiku_kits_sigfeatures_skip_ready(
    const struct tiku_kits_sigfeatures_skip *s)
{
    if (s == NULL) {
        return 0;
    }
    return s->ready;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Retrieve the last output value
 *
 * Returns ERR_NODATA if no output has been produced yet.
 */
int tiku_kits_sigfeatures_skip_value(
    const struct tiku_kits_sigfeatures_skip *s,
    tiku_kits_sigfeatures_elem_t *result)
{
    if (s == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (s->count == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_NODATA;
    }

    *result = s->last;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the total number of outputs produced
 *
 * NULL-safe: returns 0 if s is NULL.
 */
uint16_t tiku_kits_sigfeatures_skip_count(
    const struct tiku_kits_sigfeatures_skip *s)
{
    if (s == NULL) {
        return 0;
    }
    return s->count;
}
