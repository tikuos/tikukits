/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_bitmap.h - Fixed-size bitmap for boolean flag tracking
 *
 * Platform-independent bitmap library for embedded systems. All storage
 * is statically allocated using a compile-time maximum bit count. Each
 * bit represents a boolean flag, packed into 32-bit words for efficient
 * storage on MSP430 and other 16/32-bit targets.
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

#ifndef TIKU_KITS_DS_BITMAP_H_
#define TIKU_KITS_DS_BITMAP_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of bits the bitmap can hold.
 *
 * This compile-time constant defines the upper bound on bitmap capacity.
 * Each bitmap instance reserves enough 32-bit words to store this many
 * bits in its static storage, so choose a value that balances memory
 * usage against the largest bitmap your application needs.  The actual
 * memory consumed is (MAX_BITS + 31) / 32 * 4 bytes.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_BITMAP_MAX_BITS 512
 *   #include "tiku_kits_ds_bitmap.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_BITMAP_MAX_BITS
#define TIKU_KITS_DS_BITMAP_MAX_BITS 256
#endif

/**
 * @brief Number of 32-bit words needed to store MAX_BITS bits.
 *
 * Computed at compile time using ceiling division.  This determines
 * the size of the static backing array inside every bitmap instance.
 */
#define TIKU_KITS_DS_BITMAP_MAX_WORDS \
    ((TIKU_KITS_DS_BITMAP_MAX_BITS + 31) / 32)

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_bitmap
 * @brief Fixed-capacity bitmap with statically allocated word storage
 *
 * A compact, index-addressable boolean container that packs one flag
 * per bit into 32-bit words.  Because all storage lives inside the
 * struct itself, no heap allocation is needed -- just declare the
 * bitmap as a static or local variable.
 *
 * Bit N is stored in @c words[N/32] at bit position @c (N%32).  Using
 * 32-bit words (rather than bytes) gives efficient bulk operations on
 * both 16-bit MSP430 (two 16-bit ops) and 32-bit targets (single op).
 *
 * The runtime bit count is tracked independently:
 *   - @c n_bits -- the user-requested size passed to init (must be
 *     <= TIKU_KITS_DS_BITMAP_MAX_BITS).  All single-bit operations
 *     bounds-check against this value, so different bitmap instances
 *     can use different logical sizes while sharing the same
 *     compile-time backing buffer.
 *
 * @note The backing word type is uint32_t.  On 16-bit targets the
 *       compiler will split 32-bit operations into two 16-bit
 *       instructions, which is still more efficient than byte-level
 *       packing for bulk set/clear/count operations.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_bitmap bm;
 *   tiku_kits_ds_bitmap_init(&bm, 64);  // use 64 of 256 bits
 *   tiku_kits_ds_bitmap_set(&bm, 0);
 *   tiku_kits_ds_bitmap_set(&bm, 63);
 *   // bm now has bits 0 and 63 set, all others clear
 *   uint16_t n = tiku_kits_ds_bitmap_count_set(&bm);  // n == 2
 * @endcode
 */
struct tiku_kits_ds_bitmap {
    uint32_t words[TIKU_KITS_DS_BITMAP_MAX_WORDS];
    uint16_t n_bits;    /**< Runtime bit count set by init (<= MAX_BITS) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a bitmap with the given number of bits
 *
 * Resets the bitmap to an all-clear state and sets the runtime bit
 * count limit.  The backing word array is zeroed so that every bit
 * starts in a deterministic (clear) state regardless of prior memory
 * contents.
 *
 * @param bm     Bitmap to initialize (must not be NULL)
 * @param n_bits Number of bits to use (1..BITMAP_MAX_BITS)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bm is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if n_bits is 0 or exceeds MAX_BITS
 */
int tiku_kits_ds_bitmap_init(struct tiku_kits_ds_bitmap *bm,
                             uint16_t n_bits);

/*---------------------------------------------------------------------------*/
/* SINGLE-BIT OPERATIONS                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set a single bit to 1
 *
 * Locates the 32-bit word containing the target bit and applies a
 * bitwise OR with the appropriate mask.  O(1) -- direct indexed
 * access with no looping.
 *
 * @param bm  Bitmap (must not be NULL)
 * @param bit Bit index to set (must be < n_bits)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bm is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if bit >= n_bits
 */
int tiku_kits_ds_bitmap_set(struct tiku_kits_ds_bitmap *bm,
                            uint16_t bit);

/**
 * @brief Clear a single bit to 0
 *
 * Locates the 32-bit word containing the target bit and applies a
 * bitwise AND with the inverted mask.  O(1) -- direct indexed access
 * with no looping.
 *
 * @param bm  Bitmap (must not be NULL)
 * @param bit Bit index to clear (must be < n_bits)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bm is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if bit >= n_bits
 */
int tiku_kits_ds_bitmap_clear_bit(struct tiku_kits_ds_bitmap *bm,
                                  uint16_t bit);

/**
 * @brief Toggle a single bit (0 becomes 1, 1 becomes 0)
 *
 * Locates the 32-bit word containing the target bit and applies a
 * bitwise XOR with the appropriate mask.  O(1) -- direct indexed
 * access with no looping.
 *
 * @param bm  Bitmap (must not be NULL)
 * @param bit Bit index to toggle (must be < n_bits)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bm is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if bit >= n_bits
 */
int tiku_kits_ds_bitmap_toggle(struct tiku_kits_ds_bitmap *bm,
                               uint16_t bit);

/**
 * @brief Test whether a single bit is set
 *
 * Reads the 32-bit word containing the target bit, shifts the
 * relevant bit into position 0, and masks it to produce a clean
 * 0 or 1 result.  O(1) -- direct indexed access with no looping.
 *
 * @param bm     Bitmap (must not be NULL)
 * @param bit    Bit index to test (must be < n_bits)
 * @param result Output pointer: written to 1 if the bit is set,
 *               0 if the bit is clear (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bm or result is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if bit >= n_bits
 */
int tiku_kits_ds_bitmap_test(const struct tiku_kits_ds_bitmap *bm,
                             uint16_t bit, uint8_t *result);

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set all valid bits to 1
 *
 * Fills every fully-populated 32-bit word with 0xFFFFFFFF and masks
 * the final partial word so that only the bits within [0, n_bits)
 * are set.  Bits beyond n_bits in the last word remain clear to
 * avoid corrupting count and search operations.
 *
 * @param bm Bitmap (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bm is NULL
 */
int tiku_kits_ds_bitmap_set_all(struct tiku_kits_ds_bitmap *bm);

/**
 * @brief Clear all valid bits to 0
 *
 * Zeros all words that contain valid bits using memset for
 * efficiency.  Only the words that cover [0, n_bits) are zeroed,
 * not the full MAX_WORDS backing buffer.
 *
 * @param bm Bitmap (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bm is NULL
 */
int tiku_kits_ds_bitmap_clear_all(struct tiku_kits_ds_bitmap *bm);

/*---------------------------------------------------------------------------*/
/* COUNTING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Count the number of bits that are set (1)
 *
 * Iterates over all words that contain valid bits and sums the
 * population count of each word using Kernighan's bit-clearing
 * algorithm.  O(W) where W is the number of words, with each
 * word's inner loop proportional to its set-bit count.  Safe to
 * call with a NULL pointer -- returns 0.
 *
 * @param bm Bitmap, or NULL
 * @return Number of set bits, or 0 if bm is NULL
 */
uint16_t tiku_kits_ds_bitmap_count_set(
    const struct tiku_kits_ds_bitmap *bm);

/**
 * @brief Count the number of bits that are clear (0)
 *
 * Computed as @c n_bits minus the set-bit count, avoiding a second
 * full scan of the word array.  Safe to call with a NULL pointer --
 * returns 0.
 *
 * @param bm Bitmap, or NULL
 * @return Number of clear bits, or 0 if bm is NULL
 */
uint16_t tiku_kits_ds_bitmap_count_clear(
    const struct tiku_kits_ds_bitmap *bm);

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Find the lowest-index bit that is set (1)
 *
 * Scans words from index 0 upward, skipping any word that is
 * entirely zero.  Within a non-zero word, the lowest set bit is
 * found by shifting right until bit 0 is set.  O(W) worst case
 * where W is the number of words; early exit on the first non-zero
 * word makes the common case fast.
 *
 * @param bm  Bitmap (must not be NULL)
 * @param bit Output pointer where the found index is written (must
 *            not be NULL)
 * @return TIKU_KITS_DS_OK if a set bit was found,
 *         TIKU_KITS_DS_ERR_NULL if bm or bit is NULL,
 *         TIKU_KITS_DS_ERR_NOTFOUND if all bits are clear
 */
int tiku_kits_ds_bitmap_find_first_set(
    const struct tiku_kits_ds_bitmap *bm, uint16_t *bit);

/**
 * @brief Find the lowest-index bit that is clear (0)
 *
 * Scans words from index 0 upward.  Each word is bitwise inverted
 * so that clear bits become set bits, then the same lowest-set-bit
 * scan is applied.  Words that are all-ones (0xFFFFFFFF) are skipped
 * since their inversion is zero.  O(W) worst case.
 *
 * @param bm  Bitmap (must not be NULL)
 * @param bit Output pointer where the found index is written (must
 *            not be NULL)
 * @return TIKU_KITS_DS_OK if a clear bit was found,
 *         TIKU_KITS_DS_ERR_NULL if bm or bit is NULL,
 *         TIKU_KITS_DS_ERR_NOTFOUND if all bits are set
 */
int tiku_kits_ds_bitmap_find_first_clear(
    const struct tiku_kits_ds_bitmap *bm, uint16_t *bit);

#endif /* TIKU_KITS_DS_BITMAP_H_ */
