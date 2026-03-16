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

/**
 * @brief Initialize a bitmap with the given number of bits
 *
 * Zeros the entire backing word array so that every bit starts in a
 * deterministic (clear) state regardless of prior memory contents.
 * The runtime bit count is stored for subsequent bounds checking.
 */
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

    /* Zero the full static buffer, not just the words covering n_bits,
     * so that the struct is in a clean state regardless of prior contents. */
    memset(bm->words, 0, sizeof(bm->words));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* SINGLE-BIT OPERATIONS                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set a single bit to 1
 *
 * Computes the word index and intra-word position, then applies a
 * bitwise OR with a single-bit mask.  O(1).
 */
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

/**
 * @brief Clear a single bit to 0
 *
 * Computes the word index and intra-word position, then applies a
 * bitwise AND with the inverted single-bit mask to clear only the
 * target bit while preserving all others.  O(1).
 */
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

/**
 * @brief Toggle a single bit (0 becomes 1, 1 becomes 0)
 *
 * Computes the word index and intra-word position, then applies a
 * bitwise XOR with a single-bit mask to flip only the target bit.
 * O(1).
 */
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

/**
 * @brief Test whether a single bit is set
 *
 * Shifts the target word right so the bit of interest lands in
 * position 0, then masks with 1 to produce a clean 0/1 value.
 * The result is written through the caller-provided output pointer.
 * O(1).
 */
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

/**
 * @brief Set all valid bits to 1
 *
 * Fully-populated words are written as 0xFFFFFFFF.  The final
 * partial word (if n_bits is not a multiple of 32) is masked so
 * that only the bits within [0, n_bits) are set -- leaving the
 * unused upper bits clear prevents count/search from returning
 * incorrect results.
 */
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

    /* Mask the partial last word to only set valid bits --
     * leaving unused upper bits clear is essential for correct
     * count_set / find_first_clear behaviour. */
    if (remainder > 0) {
        bm->words[full_words] = ((uint32_t)1u << remainder) - 1u;
    }

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all valid bits to 0
 *
 * Zeros only the words that cover [0, n_bits) using memset for
 * efficiency, rather than clearing the full MAX_WORDS buffer.
 */
int tiku_kits_ds_bitmap_clear_all(struct tiku_kits_ds_bitmap *bm)
{
    uint16_t n_words;

    if (bm == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Only zero the words that actually hold valid bits, not the
     * full MAX_WORDS backing buffer. */
    n_words = (bm->n_bits + 31) / 32;
    memset(bm->words, 0, (size_t)n_words * sizeof(uint32_t));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* COUNTING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Count the number of bits that are set (1)
 *
 * Sums the population count of each word that holds valid bits.
 * Uses popcount32() (Kernighan's method) which loops once per set
 * bit, making it efficient for sparse bitmaps typical in embedded
 * flag-tracking use cases.
 */
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

/**
 * @brief Count the number of bits that are clear (0)
 *
 * Computed as n_bits minus the set-bit count, reusing count_set()
 * to avoid duplicating the word-scan logic.
 */
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

/**
 * @brief Find the lowest-index bit that is set (1)
 *
 * Scans words from index 0 upward, skipping any word that is
 * entirely zero.  Within a non-zero word, the lowest set bit is
 * located by shifting right until bit 0 is set.  The final index
 * is bounds-checked against n_bits to avoid reporting phantom
 * set bits in the unused upper portion of the last word.
 */
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
            continue;  /* Skip entirely-clear words */
        }
        /* Find lowest set bit position in this word by shifting
         * right until bit 0 is set. */
        pos = 0;
        while ((w & 1u) == 0) {
            w >>= 1;
            pos++;
        }
        idx = (uint16_t)(i * 32u + pos);
        /* Guard against phantom bits beyond n_bits in the last word */
        if (idx < bm->n_bits) {
            *bit = idx;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_NOTFOUND;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Find the lowest-index bit that is clear (0)
 *
 * Uses the same word-scan strategy as find_first_set, but inverts
 * each word before scanning so that clear bits become set bits.
 * Words that are all-ones (0xFFFFFFFF) invert to zero and are
 * skipped.  The final index is bounds-checked against n_bits to
 * avoid reporting unused upper bits in the last word as clear.
 */
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
        /* Invert the word so clear bits become set bits, allowing
         * the same lowest-set-bit scan to locate them. */
        w = ~bm->words[i];
        if (w == 0) {
            continue;  /* Skip entirely-set words */
        }
        /* Find lowest set bit position in the inverted word */
        pos = 0;
        while ((w & 1u) == 0) {
            w >>= 1;
            pos++;
        }
        idx = (uint16_t)(i * 32u + pos);
        /* Guard against unused bits beyond n_bits in the last word */
        if (idx < bm->n_bits) {
            *bit = idx;
            return TIKU_KITS_DS_OK;
        }
    }

    return TIKU_KITS_DS_ERR_NOTFOUND;
}
