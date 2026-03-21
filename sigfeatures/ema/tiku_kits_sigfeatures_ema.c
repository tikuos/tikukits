/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures_ema.c - Exponential Moving Average filter
 *
 * Integer-only EMA with power-of-two alpha.  Internal accumulator
 * is scaled by 2^16 for fixed-point precision.  First sample seeds
 * the accumulator; subsequent samples apply the recurrence.
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

#include "tiku_kits_sigfeatures_ema.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize an EMA filter with the given smoothing factor
 *
 * Validates alpha_shift, zeros the accumulator and count.
 */
int tiku_kits_sigfeatures_ema_init(
    struct tiku_kits_sigfeatures_ema *ema,
    uint8_t alpha_shift)
{
    if (ema == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (alpha_shift == 0
        || alpha_shift > TIKU_KITS_SIGFEATURES_EMA_SHIFT_MAX) {
        return TIKU_KITS_SIGFEATURES_ERR_PARAM;
    }

    ema->alpha_shift = alpha_shift;
    ema->value = 0;
    ema->count = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Reset an EMA filter, keeping the current alpha_shift
 *
 * Clears the accumulator and count.  The alpha_shift is preserved.
 */
int tiku_kits_sigfeatures_ema_reset(
    struct tiku_kits_sigfeatures_ema *ema)
{
    if (ema == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    ema->value = 0;
    ema->count = 0;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* SAMPLE INPUT                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a new sample into the EMA filter
 *
 * First sample: seeds the accumulator as value = sample << 16.
 * Subsequent samples: value += ((sample << 16) - value) >> alpha_shift.
 * O(1) per call.
 */
int tiku_kits_sigfeatures_ema_push(
    struct tiku_kits_sigfeatures_ema *ema,
    tiku_kits_sigfeatures_elem_t sample)
{
    int32_t scaled;

    if (ema == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }

    scaled = (int32_t)sample << 16;

    if (ema->count == 0) {
        /* First sample: seed directly, no smoothing */
        ema->value = scaled;
    } else {
        /* EMA recurrence: value += (scaled - value) >> alpha_shift */
        ema->value += (scaled - ema->value) >> ema->alpha_shift;
    }

    ema->count++;
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERIES                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Retrieve the current filtered value (descaled)
 *
 * Returns value >> 16 to remove the fixed-point scaling.
 */
int tiku_kits_sigfeatures_ema_value(
    const struct tiku_kits_sigfeatures_ema *ema,
    tiku_kits_sigfeatures_elem_t *result)
{
    if (ema == NULL || result == NULL) {
        return TIKU_KITS_SIGFEATURES_ERR_NULL;
    }
    if (ema->count == 0) {
        return TIKU_KITS_SIGFEATURES_ERR_NODATA;
    }

    *result = (tiku_kits_sigfeatures_elem_t)(ema->value >> 16);
    return TIKU_KITS_SIGFEATURES_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of samples processed
 *
 * NULL-safe: returns 0 if ema is NULL.
 */
uint16_t tiku_kits_sigfeatures_ema_count(
    const struct tiku_kits_sigfeatures_ema *ema)
{
    if (ema == NULL) {
        return 0;
    }
    return ema->count;
}
