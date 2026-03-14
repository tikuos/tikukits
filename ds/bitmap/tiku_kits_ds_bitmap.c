/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_bitmap.c - Fixed-size bitmap implementation
 *
 * Platform-independent implementation of a fixed-capacity bitmap.
 * All storage is statically allocated; no heap usage. Bits are
 * packed into 32-bit words for efficient boolean flag tracking.
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

#include "tiku_kits_ds_bitmap.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Count set bits in a 32-bit word using Kernighan's method
 *
 * Each iteration clears the lowest set bit: n &= (n - 1).
 * The loop runs once per set bit, making it efficient for
 * sparse words common in embedded flag registers.
 */
static uint16_t popcount32(uint32_t n)
{
    uint16_t count = 0;

    while (n) {
        n &= (n - 1);
        count++;
    }
    return count;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_init(struct tiku_kits_ds_bitmap *bm,
                             uint16_t n_bits)
{
    if (bm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (n_bits == 0 || n_bits > TIKU_KITS_DS_BITMAP_MAX_BITS) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    bm->n_bits = n_bits;
    memset(bm->words, 0, sizeof(bm->words));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* SINGLE-BIT OPERATIONS                                                     */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_set(struct tiku_kits_ds_bitmap *bm,
                            uint16_t bit)
{
    uint16_t w;
    uint16_t pos;

    if (bm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (bit >= bm->n_bits) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    w   = bit / 32;
    pos = bit % 32;
    bm->words[w] |= ((uint32_t)1u << pos);
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_clear_bit(struct tiku_kits_ds_bitmap *bm,
                                  uint16_t bit)
{
    uint16_t w;
    uint16_t pos;

    if (bm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (bit >= bm->n_bits) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    w   = bit / 32;
    pos = bit % 32;
    bm->words[w] &= ~((uint32_t)1u << pos);
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_toggle(struct tiku_kits_ds_bitmap *bm,
                               uint16_t bit)
{
    uint16_t w;
    uint16_t pos;

    if (bm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (bit >= bm->n_bits) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    w   = bit / 32;
    pos = bit % 32;
    bm->words[w] ^= ((uint32_t)1u << pos);
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_test(const struct tiku_kits_ds_bitmap *bm,
                             uint16_t bit, uint8_t *result)
{
    uint16_t w;
    uint16_t pos;

    if (bm == NULL || result == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (bit >= bm->n_bits) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    w   = bit / 32;
    pos = bit % 32;
    *result = (uint8_t)((bm->words[w] >> pos) & 1u);
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_set_all(struct tiku_kits_ds_bitmap *bm)
{
    uint16_t full_words;
    uint16_t remainder;
    uint16_t i;

    if (bm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    full_words = bm->n_bits / 32;
    remainder  = bm->n_bits % 32;

    /* Set all full words to all-ones */
    for (i = 0; i < full_words; i++) {
        bm->words[i] = 0xFFFFFFFFu;
    }

    /* Mask the partial last word to only set valid bits */
    if (remainder > 0) {
        bm->words[full_words] = ((uint32_t)1u << remainder) - 1u;
    }

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_clear_all(struct tiku_kits_ds_bitmap *bm)
{
    uint16_t n_words;

    if (bm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    n_words = (bm->n_bits + 31) / 32;
    memset(bm->words, 0, (size_t)n_words * sizeof(uint32_t));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* COUNTING                                                                  */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_bitmap_count_set(
    const struct tiku_kits_ds_bitmap *bm)
{
    uint16_t n_words;
    uint16_t count;
    uint16_t i;

    if (bm == NULL) {
        return 0;
    }

    n_words = (bm->n_bits + 31) / 32;
    count = 0;
    for (i = 0; i < n_words; i++) {
        count += popcount32(bm->words[i]);
    }
    return count;
}

/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_bitmap_count_clear(
    const struct tiku_kits_ds_bitmap *bm)
{
    if (bm == NULL) {
        return 0;
    }
    return bm->n_bits - tiku_kits_ds_bitmap_count_set(bm);
}

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_find_first_set(
    const struct tiku_kits_ds_bitmap *bm, uint16_t *bit)
{
    uint16_t n_words;
    uint16_t i;
    uint32_t w;
    uint16_t pos;
    uint16_t idx;

    if (bm == NULL || bit == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    n_words = (bm->n_bits + 31) / 32;

    for (i = 0; i < n_words; i++) {
        w = bm->words[i];
        if (w == 0) {
            continue;
        }
        /* Find lowest set bit position in this word */
        pos = 0;
        while ((w & 1u) == 0) {
            w >>= 1;
            pos++;
        }
        idx = (uint16_t)(i * 32u + pos);
        if (idx < bm->n_bits) {
            *bit = idx;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_NOTFOUND;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_bitmap_find_first_clear(
    const struct tiku_kits_ds_bitmap *bm, uint16_t *bit)
{
    uint16_t n_words;
    uint16_t i;
    uint32_t w;
    uint16_t pos;
    uint16_t idx;

    if (bm == NULL || bit == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    n_words = (bm->n_bits + 31) / 32;

    for (i = 0; i < n_words; i++) {
        /* Invert the word so clear bits become set bits */
        w = ~bm->words[i];
        if (w == 0) {
            continue;
        }
        /* Find lowest set bit position in the inverted word */
        pos = 0;
        while ((w & 1u) == 0) {
            w >>= 1;
            pos++;
        }
        idx = (uint16_t)(i * 32u + pos);
        if (idx < bm->n_bits) {
            *bit = idx;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_NOTFOUND;
}
