/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_zcr.c - Zero-crossing rate feature
 *
 * Incremental zero-crossing count over a sliding window.
 * O(1) per push, zero extra RAM beyond the circular buffer.
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

#include "tiku_kits_sigfeatures_zcr.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether two values have different signs
 * @return 1 if signs differ (zero crossing), 0 otherwise
 *
 * Zero is treated as non-negative.
 */
static int is_crossing(tiku_kits_sigfeatures_elem_t a,
                       tiku_kits_sigfeatures_elem_t b)
{
    return ((a < 0) != (b < 0)) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_zcr_init(
    struct tiku_kits_sigfeatures_zcr *z,
    uint16_t window)
{
    if (z == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (window < 2
        || window > TIKU_KITS_SIGFEATURES_ZCR_MAX_WINDOW) {
        return TIKU_KITS_SIGFEATURES_ERR_SIZE;
    }

    z->capacity = window;
    z->head = 0;
    z->count = 0;
    z->crossings = 0;
    memset(z->buf, 0, sizeof(z->buf));
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_zcr_reset(
    struct tiku_kits_sigfeatures_zcr *z)
{
    if (z == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    z->head = 0;
    z->count = 0;
    z->crossings = 0;
    memset(z->buf, 0, sizeof(z->buf));
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* SAMPLE INPUT                                                              */
/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_zcr_push(
    struct tiku_kits_sigfeatures_zcr *z,
    tiku_kits_sigfeatures_elem_t value)
{
    uint16_t successor;
    uint16_t predecessor;

    if (z == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    if (z->count >= z->capacity) {
        /*
         * Window is full -- evict the oldest sample at head.
         * Remove the crossing between oldest and its successor
         * (the sample that will become the new oldest).
         */
        successor = (z->head + 1) % z->capacity;
        if (is_crossing(z->buf[z->head], z->buf[successor])) {
            z->crossings--;
        }
    }

    /*
     * Check crossing between the current newest sample and
     * the new value being pushed. Skip for the very first sample.
     */
    if (z->count >= 1) {
        predecessor = (z->head + z->capacity - 1) % z->capacity;
        if (is_crossing(z->buf[predecessor], value)) {
            z->crossings++;
        }
    }

    /* Write new sample and advance head */
    z->buf[z->head] = value;
    z->head = (z->head + 1) % z->capacity;

    if (z->count < z->capacity) {
        z->count++;
    }

    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERIES                                                                   */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_sigfeatures_zcr_crossings(
    const struct tiku_kits_sigfeatures_zcr *z)
{
    if (z == NULL) {
        return 0;
    }
    return z->crossings;
}

/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_sigfeatures_zcr_count(
    const struct tiku_kits_sigfeatures_zcr *z)
{
    if (z == NULL) {
        return 0;
    }
    return z->count;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_zcr_full(
    const struct tiku_kits_sigfeatures_zcr *z)
{
    if (z == NULL) {
        return 0;
    }
    return (z->count >= z->capacity) ? 1 : 0;
}
