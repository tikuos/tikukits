/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_peak.c - Peak detector with hysteresis
 *
 * Two-state (RISING/FALLING) streaming peak detector. A peak is
 * confirmed when the signal drops by more than the hysteresis
 * deadband below the running maximum.
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

#include "tiku_kits_sigfeatures_peak.h"

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_peak_init(
    struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t hysteresis)
{
    if (p == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (hysteresis <= 0) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    p->hysteresis = hysteresis;
    p->extreme = 0;
    p->last_peak = 0;
    p->extreme_idx = 0;
    p->last_peak_idx = 0;
    p->sample_idx = 0;
    p->peak_count = 0;
    p->state = TIKU_KITS_SIGFEATURES_PEAK_RISING;
    p->detected = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_peak_reset(
    struct tiku_kits_sigfeatures_peak *p)
{
    if (p == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    p->extreme = 0;
    p->last_peak = 0;
    p->extreme_idx = 0;
    p->last_peak_idx = 0;
    p->sample_idx = 0;
    p->peak_count = 0;
    p->state = TIKU_KITS_SIGFEATURES_PEAK_RISING;
    p->detected = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* SAMPLE INPUT                                                              */
/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_peak_push(
    struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t value)
{
    if (p == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    p->detected = 0;

    if (p->sample_idx == 0) {
        /* First sample: seed the extreme */
        p->extreme = value;
        p->extreme_idx = 0;
        p->sample_idx = 1;
        return TIKU_KITS_SIGFEATURES_OK;
    }

    if (p->state == TIKU_KITS_SIGFEATURES_PEAK_RISING) {
        if (value > p->extreme) {
            /* Still rising -- update the running maximum */
            p->extreme = value;
            p->extreme_idx = p->sample_idx;
        } else if (value < p->extreme - p->hysteresis) {
            /*
             * Signal dropped below max - hysteresis.
             * Confirm peak at the running maximum.
             */
            p->last_peak = p->extreme;
            p->last_peak_idx = p->extreme_idx;
            p->peak_count++;
            p->detected = 1;

            /* Switch to FALLING, start tracking minimum */
            p->state = TIKU_KITS_SIGFEATURES_PEAK_FALLING;
            p->extreme = value;
            p->extreme_idx = p->sample_idx;
        }
    } else {
        /* FALLING */
        if (value < p->extreme) {
            /* Still falling -- update the running minimum */
            p->extreme = value;
            p->extreme_idx = p->sample_idx;
        } else if (value > p->extreme + p->hysteresis) {
            /*
             * Signal rose above min + hysteresis.
             * Switch back to RISING, start tracking maximum.
             */
            p->state = TIKU_KITS_SIGFEATURES_PEAK_RISING;
            p->extreme = value;
            p->extreme_idx = p->sample_idx;
        }
    }

    p->sample_idx++;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERIES                                                                   */
/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_peak_detected(
    const struct tiku_kits_sigfeatures_peak *p)
{
    if (p == NULL) {
        return 0;
    }
    return p->detected ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_peak_last_value(
    const struct tiku_kits_sigfeatures_peak *p,
    tiku_kits_sigfeatures_elem_t *result)
{
    if (p == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (p->peak_count == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_NODATA;
    }

    *result = p->last_peak;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_sigfeatures_peak_last_index(
    const struct tiku_kits_sigfeatures_peak *p,
    uint16_t *result)
{
    if (p == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (p->peak_count == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_NODATA;
    }

    *result = p->last_peak_idx;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_sigfeatures_peak_count(
    const struct tiku_kits_sigfeatures_peak *p)
{
    if (p == NULL) {
        return 0;
    }
    return p->peak_count;
}
