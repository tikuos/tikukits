/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ds_bitmap.c - Bitmap data structure tests
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

#include "tests/tiku_test_harness.h"
#include "tikukits/ds/bitmap/tiku_kits_ds_bitmap.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_init(void)
{
    struct tiku_kits_ds_bitmap bm;
    uint8_t val;
    int rc;

    TEST_GROUP_BEGIN("Bitmap Init");

    rc = tiku_kits_ds_bitmap_init(&bm, 64);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "init 64 bits returns OK");
    TEST_ASSERT(bm.n_bits == 64, "n_bits is 64");

    /* All bits should be zero after init */
    tiku_kits_ds_bitmap_test(&bm, 0, &val);
    TEST_ASSERT(val == 0, "bit 0 is clear after init");
    tiku_kits_ds_bitmap_test(&bm, 63, &val);
    TEST_ASSERT(val == 0, "bit 63 is clear after init");

    /* Zero n_bits rejected */
    rc = tiku_kits_ds_bitmap_init(&bm, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM, "zero n_bits rejected");

    /* Oversized n_bits rejected */
    rc = tiku_kits_ds_bitmap_init(&bm,
        TIKU_KITS_DS_BITMAP_MAX_BITS + 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_PARAM,
                "oversized n_bits rejected");

    /* Max size accepted */
    rc = tiku_kits_ds_bitmap_init(&bm,
        TIKU_KITS_DS_BITMAP_MAX_BITS);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "max n_bits init OK");

    /* Single bit bitmap */
    rc = tiku_kits_ds_bitmap_init(&bm, 1);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "single bit init OK");
    TEST_ASSERT(bm.n_bits == 1, "n_bits is 1");

    TEST_GROUP_END("Bitmap Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: SET AND CLEAR BIT, TEST BIT                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_set_clear_test(void)
{
    struct tiku_kits_ds_bitmap bm;
    uint8_t val;
    int rc;

    TEST_GROUP_BEGIN("Bitmap Set/Clear/Test");

    tiku_kits_ds_bitmap_init(&bm, 64);

    /* Set bit 0 */
    rc = tiku_kits_ds_bitmap_set(&bm, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "set bit 0 returns OK");
    tiku_kits_ds_bitmap_test(&bm, 0, &val);
    TEST_ASSERT(val == 1, "bit 0 is set");

    /* Set bit 31 (last bit of first word) */
    tiku_kits_ds_bitmap_set(&bm, 31);
    tiku_kits_ds_bitmap_test(&bm, 31, &val);
    TEST_ASSERT(val == 1, "bit 31 is set");

    /* Set bit 32 (first bit of second word) */
    tiku_kits_ds_bitmap_set(&bm, 32);
    tiku_kits_ds_bitmap_test(&bm, 32, &val);
    TEST_ASSERT(val == 1, "bit 32 is set");

    /* Clear bit 0 */
    rc = tiku_kits_ds_bitmap_clear_bit(&bm, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear bit 0 returns OK");
    tiku_kits_ds_bitmap_test(&bm, 0, &val);
    TEST_ASSERT(val == 0, "bit 0 is clear after clear_bit");

    /* Bit 31 still set */
    tiku_kits_ds_bitmap_test(&bm, 31, &val);
    TEST_ASSERT(val == 1, "bit 31 still set after clearing bit 0");

    /* Out-of-bounds set rejected */
    rc = tiku_kits_ds_bitmap_set(&bm, 64);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "set out-of-bounds rejected");

    /* Out-of-bounds clear rejected */
    rc = tiku_kits_ds_bitmap_clear_bit(&bm, 64);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "clear out-of-bounds rejected");

    /* Out-of-bounds test rejected */
    rc = tiku_kits_ds_bitmap_test(&bm, 64, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "test out-of-bounds rejected");

    TEST_GROUP_END("Bitmap Set/Clear/Test");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: TOGGLE                                                            */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_toggle(void)
{
    struct tiku_kits_ds_bitmap bm;
    uint8_t val;
    int rc;

    TEST_GROUP_BEGIN("Bitmap Toggle");

    tiku_kits_ds_bitmap_init(&bm, 32);

    /* Toggle bit 5: 0 -> 1 */
    rc = tiku_kits_ds_bitmap_toggle(&bm, 5);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "toggle bit 5 returns OK");
    tiku_kits_ds_bitmap_test(&bm, 5, &val);
    TEST_ASSERT(val == 1, "bit 5 is 1 after first toggle");

    /* Toggle bit 5 again: 1 -> 0 */
    tiku_kits_ds_bitmap_toggle(&bm, 5);
    tiku_kits_ds_bitmap_test(&bm, 5, &val);
    TEST_ASSERT(val == 0, "bit 5 is 0 after second toggle");

    /* Toggle out-of-bounds */
    rc = tiku_kits_ds_bitmap_toggle(&bm, 32);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_BOUNDS,
                "toggle out-of-bounds rejected");

    /* Toggle does not affect adjacent bits */
    tiku_kits_ds_bitmap_set(&bm, 10);
    tiku_kits_ds_bitmap_toggle(&bm, 11);
    tiku_kits_ds_bitmap_test(&bm, 10, &val);
    TEST_ASSERT(val == 1, "adjacent bit 10 unaffected by toggle 11");
    tiku_kits_ds_bitmap_test(&bm, 11, &val);
    TEST_ASSERT(val == 1, "bit 11 toggled to 1");

    TEST_GROUP_END("Bitmap Toggle");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: SET ALL AND CLEAR ALL                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_set_all_clear_all(void)
{
    struct tiku_kits_ds_bitmap bm;
    uint8_t val;
    int rc;

    TEST_GROUP_BEGIN("Bitmap Set All / Clear All");

    /* Test with non-word-aligned bit count (50 bits) */
    tiku_kits_ds_bitmap_init(&bm, 50);

    rc = tiku_kits_ds_bitmap_set_all(&bm);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "set_all returns OK");

    tiku_kits_ds_bitmap_test(&bm, 0, &val);
    TEST_ASSERT(val == 1, "bit 0 set after set_all");
    tiku_kits_ds_bitmap_test(&bm, 31, &val);
    TEST_ASSERT(val == 1, "bit 31 set after set_all");
    tiku_kits_ds_bitmap_test(&bm, 49, &val);
    TEST_ASSERT(val == 1, "bit 49 (last) set after set_all");

    /* Verify count matches n_bits exactly */
    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 50,
                "count_set is 50 after set_all");

    /* Clear all */
    rc = tiku_kits_ds_bitmap_clear_all(&bm);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK, "clear_all returns OK");
    tiku_kits_ds_bitmap_test(&bm, 0, &val);
    TEST_ASSERT(val == 0, "bit 0 clear after clear_all");
    tiku_kits_ds_bitmap_test(&bm, 49, &val);
    TEST_ASSERT(val == 0, "bit 49 clear after clear_all");

    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 0,
                "count_set is 0 after clear_all");

    /* Test with word-aligned bit count (32 bits) */
    tiku_kits_ds_bitmap_init(&bm, 32);
    tiku_kits_ds_bitmap_set_all(&bm);
    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 32,
                "count_set is 32 after set_all on 32-bit bitmap");

    TEST_GROUP_END("Bitmap Set All / Clear All");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: COUNT SET AND COUNT CLEAR                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_count_set_clear(void)
{
    struct tiku_kits_ds_bitmap bm;

    TEST_GROUP_BEGIN("Bitmap Count Set/Clear");

    tiku_kits_ds_bitmap_init(&bm, 64);

    /* Empty bitmap */
    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 0,
                "count_set 0 on empty bitmap");
    TEST_ASSERT(tiku_kits_ds_bitmap_count_clear(&bm) == 64,
                "count_clear 64 on empty bitmap");

    /* Set 3 bits */
    tiku_kits_ds_bitmap_set(&bm, 0);
    tiku_kits_ds_bitmap_set(&bm, 15);
    tiku_kits_ds_bitmap_set(&bm, 63);
    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 3,
                "count_set is 3 after setting 3 bits");
    TEST_ASSERT(tiku_kits_ds_bitmap_count_clear(&bm) == 61,
                "count_clear is 61");

    /* Set same bit again -- no change */
    tiku_kits_ds_bitmap_set(&bm, 0);
    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 3,
                "count_set still 3 after re-setting bit 0");

    /* Clear one bit */
    tiku_kits_ds_bitmap_clear_bit(&bm, 15);
    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 2,
                "count_set is 2 after clearing bit 15");
    TEST_ASSERT(tiku_kits_ds_bitmap_count_clear(&bm) == 62,
                "count_clear is 62");

    TEST_GROUP_END("Bitmap Count Set/Clear");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: FIND FIRST SET                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_find_first_set(void)
{
    struct tiku_kits_ds_bitmap bm;
    uint16_t bit;
    int rc;

    TEST_GROUP_BEGIN("Bitmap Find First Set");

    tiku_kits_ds_bitmap_init(&bm, 96);

    /* All clear -- not found */
    rc = tiku_kits_ds_bitmap_find_first_set(&bm, &bit);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "find_first_set on all-clear returns NOTFOUND");

    /* Set bit 50, find it */
    tiku_kits_ds_bitmap_set(&bm, 50);
    rc = tiku_kits_ds_bitmap_find_first_set(&bm, &bit);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "find_first_set returns OK");
    TEST_ASSERT(bit == 50, "first set bit is 50");

    /* Set bit 10, first should now be 10 */
    tiku_kits_ds_bitmap_set(&bm, 10);
    tiku_kits_ds_bitmap_find_first_set(&bm, &bit);
    TEST_ASSERT(bit == 10, "first set bit is 10 after adding");

    /* Set bit 0, first should now be 0 */
    tiku_kits_ds_bitmap_set(&bm, 0);
    tiku_kits_ds_bitmap_find_first_set(&bm, &bit);
    TEST_ASSERT(bit == 0, "first set bit is 0");

    TEST_GROUP_END("Bitmap Find First Set");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: FIND FIRST CLEAR                                                  */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_find_first_clear(void)
{
    struct tiku_kits_ds_bitmap bm;
    uint16_t bit;
    int rc;

    TEST_GROUP_BEGIN("Bitmap Find First Clear");

    tiku_kits_ds_bitmap_init(&bm, 64);

    /* All clear -- first clear is 0 */
    rc = tiku_kits_ds_bitmap_find_first_clear(&bm, &bit);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "find_first_clear returns OK");
    TEST_ASSERT(bit == 0, "first clear bit is 0 on empty bitmap");

    /* Set all, then no clear bits */
    tiku_kits_ds_bitmap_set_all(&bm);
    rc = tiku_kits_ds_bitmap_find_first_clear(&bm, &bit);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NOTFOUND,
                "find_first_clear on all-set returns NOTFOUND");

    /* Clear bit 33, find it */
    tiku_kits_ds_bitmap_clear_bit(&bm, 33);
    rc = tiku_kits_ds_bitmap_find_first_clear(&bm, &bit);
    TEST_ASSERT(rc == TIKU_KITS_DS_OK,
                "find_first_clear returns OK after clearing 33");
    TEST_ASSERT(bit == 33, "first clear bit is 33");

    /* Clear bit 5, first should now be 5 */
    tiku_kits_ds_bitmap_clear_bit(&bm, 5);
    tiku_kits_ds_bitmap_find_first_clear(&bm, &bit);
    TEST_ASSERT(bit == 5, "first clear bit is 5");

    TEST_GROUP_END("Bitmap Find First Clear");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: BOUNDARY BITS                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_boundary_bits(void)
{
    struct tiku_kits_ds_bitmap bm;
    uint8_t val;

    TEST_GROUP_BEGIN("Bitmap Boundary Bits");

    /* Non-word-aligned size: 33 bits (1 full word + 1 bit) */
    tiku_kits_ds_bitmap_init(&bm, 33);

    /* Set and test the last valid bit (bit 32) */
    tiku_kits_ds_bitmap_set(&bm, 32);
    tiku_kits_ds_bitmap_test(&bm, 32, &val);
    TEST_ASSERT(val == 1, "bit 32 set in 33-bit bitmap");

    /* Out-of-bounds at bit 33 */
    TEST_ASSERT(tiku_kits_ds_bitmap_set(&bm, 33) ==
                TIKU_KITS_DS_ERR_BOUNDS,
                "bit 33 out-of-bounds in 33-bit bitmap");

    /* set_all should set exactly 33 bits */
    tiku_kits_ds_bitmap_set_all(&bm);
    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 33,
                "set_all sets exactly 33 bits");

    /* Single bit bitmap */
    tiku_kits_ds_bitmap_init(&bm, 1);
    tiku_kits_ds_bitmap_set(&bm, 0);
    tiku_kits_ds_bitmap_test(&bm, 0, &val);
    TEST_ASSERT(val == 1, "single bit bitmap: bit 0 set");

    TEST_ASSERT(tiku_kits_ds_bitmap_set(&bm, 1) ==
                TIKU_KITS_DS_ERR_BOUNDS,
                "single bit bitmap: bit 1 out-of-bounds");

    tiku_kits_ds_bitmap_set_all(&bm);
    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(&bm) == 1,
                "single bit bitmap: set_all sets exactly 1 bit");

    /* Word-boundary: exactly 32 bits */
    tiku_kits_ds_bitmap_init(&bm, 32);
    tiku_kits_ds_bitmap_set(&bm, 0);
    tiku_kits_ds_bitmap_set(&bm, 31);
    tiku_kits_ds_bitmap_test(&bm, 0, &val);
    TEST_ASSERT(val == 1, "32-bit bitmap: bit 0 set");
    tiku_kits_ds_bitmap_test(&bm, 31, &val);
    TEST_ASSERT(val == 1, "32-bit bitmap: bit 31 set");
    TEST_ASSERT(tiku_kits_ds_bitmap_set(&bm, 32) ==
                TIKU_KITS_DS_ERR_BOUNDS,
                "32-bit bitmap: bit 32 out-of-bounds");

    TEST_GROUP_END("Bitmap Boundary Bits");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER INPUTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ds_bitmap_null_inputs(void)
{
    struct tiku_kits_ds_bitmap bm;
    uint8_t val;
    uint16_t bit;
    int rc;

    TEST_GROUP_BEGIN("Bitmap NULL Inputs");

    rc = tiku_kits_ds_bitmap_init(NULL, 32);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "init NULL rejected");

    rc = tiku_kits_ds_bitmap_set(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "set NULL rejected");

    rc = tiku_kits_ds_bitmap_clear_bit(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear_bit NULL rejected");

    rc = tiku_kits_ds_bitmap_toggle(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "toggle NULL rejected");

    rc = tiku_kits_ds_bitmap_test(NULL, 0, &val);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "test NULL bm rejected");

    tiku_kits_ds_bitmap_init(&bm, 32);
    rc = tiku_kits_ds_bitmap_test(&bm, 0, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "test NULL result rejected");

    rc = tiku_kits_ds_bitmap_set_all(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "set_all NULL rejected");

    rc = tiku_kits_ds_bitmap_clear_all(NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "clear_all NULL rejected");

    TEST_ASSERT(tiku_kits_ds_bitmap_count_set(NULL) == 0,
                "count_set NULL returns 0");
    TEST_ASSERT(tiku_kits_ds_bitmap_count_clear(NULL) == 0,
                "count_clear NULL returns 0");

    rc = tiku_kits_ds_bitmap_find_first_set(NULL, &bit);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find_first_set NULL bm rejected");

    tiku_kits_ds_bitmap_init(&bm, 32);
    rc = tiku_kits_ds_bitmap_find_first_set(&bm, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find_first_set NULL bit rejected");

    rc = tiku_kits_ds_bitmap_find_first_clear(NULL, &bit);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find_first_clear NULL bm rejected");

    rc = tiku_kits_ds_bitmap_find_first_clear(&bm, NULL);
    TEST_ASSERT(rc == TIKU_KITS_DS_ERR_NULL,
                "find_first_clear NULL bit rejected");

    TEST_GROUP_END("Bitmap NULL Inputs");
}
