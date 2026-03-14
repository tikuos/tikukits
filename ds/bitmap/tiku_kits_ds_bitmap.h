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
 * Maximum number of bits the bitmap can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_BITMAP_MAX_BITS
#define TIKU_KITS_DS_BITMAP_MAX_BITS 256
#endif

/** Number of 32-bit words needed to store MAX_BITS bits */
#define TIKU_KITS_DS_BITMAP_MAX_WORDS \
    ((TIKU_KITS_DS_BITMAP_MAX_BITS + 31) / 32)

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_bitmap
 * @brief Fixed-capacity bitmap with static storage
 *
 * Bits are packed into 32-bit words. Bit N is stored in
 * words[N / 32] at position (N % 32). The n_bits field tracks
 * the user-requested size (<= MAX_BITS).
 *
 * Declare bitmaps as static or local variables -- no heap required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_bitmap bm;
 *   tiku_kits_ds_bitmap_init(&bm, 64);
 *   tiku_kits_ds_bitmap_set(&bm, 0);
 *   tiku_kits_ds_bitmap_set(&bm, 63);
 * @endcode
 */
struct tiku_kits_ds_bitmap {
    uint32_t words[TIKU_KITS_DS_BITMAP_MAX_WORDS];
    uint16_t n_bits;    /**< Number of valid bits */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a bitmap with the given number of bits
 * @param bm     Bitmap to initialize
 * @param n_bits Number of bits (1..BITMAP_MAX_BITS)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bitmap_init(struct tiku_kits_ds_bitmap *bm,
                             uint16_t n_bits);

/*---------------------------------------------------------------------------*/
/* SINGLE-BIT OPERATIONS                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set a bit to 1
 * @param bm  Bitmap
 * @param bit Bit index (must be < n_bits)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bitmap_set(struct tiku_kits_ds_bitmap *bm,
                            uint16_t bit);

/**
 * @brief Clear a bit to 0
 * @param bm  Bitmap
 * @param bit Bit index (must be < n_bits)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bitmap_clear_bit(struct tiku_kits_ds_bitmap *bm,
                                  uint16_t bit);

/**
 * @brief Toggle a bit (0 -> 1, 1 -> 0)
 * @param bm  Bitmap
 * @param bit Bit index (must be < n_bits)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bitmap_toggle(struct tiku_kits_ds_bitmap *bm,
                               uint16_t bit);

/**
 * @brief Test whether a bit is set
 * @param bm     Bitmap
 * @param bit    Bit index (must be < n_bits)
 * @param result Output: 1 if set, 0 if clear
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bitmap_test(const struct tiku_kits_ds_bitmap *bm,
                             uint16_t bit, uint8_t *result);

/*---------------------------------------------------------------------------*/
/* BULK OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set all bits to 1
 * @param bm Bitmap
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bitmap_set_all(struct tiku_kits_ds_bitmap *bm);

/**
 * @brief Clear all bits to 0
 * @param bm Bitmap
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_bitmap_clear_all(struct tiku_kits_ds_bitmap *bm);

/*---------------------------------------------------------------------------*/
/* COUNTING                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Count the number of bits that are set (1)
 * @param bm Bitmap
 * @return Number of set bits, or 0 if bm is NULL
 */
uint16_t tiku_kits_ds_bitmap_count_set(
    const struct tiku_kits_ds_bitmap *bm);

/**
 * @brief Count the number of bits that are clear (0)
 * @param bm Bitmap
 * @return Number of clear bits, or 0 if bm is NULL
 */
uint16_t tiku_kits_ds_bitmap_count_clear(
    const struct tiku_kits_ds_bitmap *bm);

/*---------------------------------------------------------------------------*/
/* SEARCH                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Find the lowest-index bit that is set (1)
 * @param bm  Bitmap
 * @param bit Output: index of the first set bit
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND if all clear
 */
int tiku_kits_ds_bitmap_find_first_set(
    const struct tiku_kits_ds_bitmap *bm, uint16_t *bit);

/**
 * @brief Find the lowest-index bit that is clear (0)
 * @param bm  Bitmap
 * @param bit Output: index of the first clear bit
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND if all set
 */
int tiku_kits_ds_bitmap_find_first_clear(
    const struct tiku_kits_ds_bitmap *bm, uint16_t *bit);

#endif /* TIKU_KITS_DS_BITMAP_H_ */
