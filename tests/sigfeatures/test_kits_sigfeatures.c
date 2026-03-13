/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_sigfeatures.c - Signal features library tests
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

#include "test_kits_sigfeatures.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: PEAK DETECTOR                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_peak(void)
{
    struct tiku_kits_sigfeatures_peak p;
    tiku_kits_sigfeatures_elem_t val;
    uint16_t idx;
    int rc;

    TEST_GROUP_BEGIN("SigFeatures Peak");

    rc = tiku_kits_sigfeatures_peak_init(&p, 10);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "peak init OK");
    TEST_ASSERT(tiku_kits_sigfeatures_peak_count(&p) == 0,
                "peak initial count is 0");
    TEST_ASSERT(tiku_kits_sigfeatures_peak_detected(&p) == 0,
                "no peak detected initially");

    /* No data yet */
    rc = tiku_kits_sigfeatures_peak_last_value(&p, &val);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NODATA,
                "peak last_value ERR_NODATA before any peak");

    /* Rising sequence: 0, 10, 20, 30 (no peak yet - still rising) */
    tiku_kits_sigfeatures_peak_push(&p, 0);
    tiku_kits_sigfeatures_peak_push(&p, 10);
    tiku_kits_sigfeatures_peak_push(&p, 20);
    tiku_kits_sigfeatures_peak_push(&p, 30);
    TEST_ASSERT(tiku_kits_sigfeatures_peak_detected(&p) == 0,
                "no peak while still rising");

    /* Drop by more than hysteresis: 30 -> 19 (drop of 11 > 10) */
    tiku_kits_sigfeatures_peak_push(&p, 19);
    TEST_ASSERT(tiku_kits_sigfeatures_peak_detected(&p) == 1,
                "peak detected after hysteresis drop");
    TEST_ASSERT(tiku_kits_sigfeatures_peak_count(&p) == 1,
                "peak count is 1");

    rc = tiku_kits_sigfeatures_peak_last_value(&p, &val);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "peak last_value OK");
    TEST_ASSERT(val == 30, "peak value is 30");

    rc = tiku_kits_sigfeatures_peak_last_index(&p, &idx);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "peak last_index OK");
    TEST_ASSERT(idx == 3, "peak index is 3 (4th sample, 0-indexed)");

    /* Continue falling - no new peak */
    tiku_kits_sigfeatures_peak_push(&p, 10);
    TEST_ASSERT(tiku_kits_sigfeatures_peak_detected(&p) == 0,
                "no new peak while falling");

    /* Reset */
    tiku_kits_sigfeatures_peak_reset(&p);
    TEST_ASSERT(tiku_kits_sigfeatures_peak_count(&p) == 0,
                "peak count 0 after reset");

    /* Hysteresis = 0 rejected */
    rc = tiku_kits_sigfeatures_peak_init(&p, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_PARAM,
                "peak hysteresis=0 rejected");
    TEST_GROUP_END("SigFeatures Peak");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: ZERO-CROSSING RATE                                                */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_zcr(void)
{
    struct tiku_kits_sigfeatures_zcr z;
    int rc;

    TEST_GROUP_BEGIN("SigFeatures ZCR");

    rc = tiku_kits_sigfeatures_zcr_init(&z, 8);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "zcr init OK");
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_count(&z) == 0,
                "zcr initial count is 0");
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_full(&z) == 0,
                "zcr not full initially");

    /* Push: 5, -3, 4, -2 -> crossings at transitions (5→-3), (-3→4), (4→-2) */
    tiku_kits_sigfeatures_zcr_push(&z, 5);
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_crossings(&z) == 0,
                "no crossings after 1 sample");

    tiku_kits_sigfeatures_zcr_push(&z, -3);
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_crossings(&z) == 1,
                "1 crossing: 5 -> -3");

    tiku_kits_sigfeatures_zcr_push(&z, 4);
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_crossings(&z) == 2,
                "2 crossings: -3 -> 4");

    tiku_kits_sigfeatures_zcr_push(&z, -2);
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_crossings(&z) == 3,
                "3 crossings: 4 -> -2");

    TEST_ASSERT(tiku_kits_sigfeatures_zcr_count(&z) == 4,
                "zcr count is 4");

    /* No crossing for same-sign: push 10, 20 (both positive) */
    tiku_kits_sigfeatures_zcr_push(&z, -5);
    tiku_kits_sigfeatures_zcr_push(&z, -8);
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_crossings(&z) == 4,
                "no crossing between same-sign samples");

    /* Reset */
    tiku_kits_sigfeatures_zcr_reset(&z);
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_count(&z) == 0,
                "zcr count 0 after reset");
    TEST_ASSERT(tiku_kits_sigfeatures_zcr_crossings(&z) == 0,
                "zcr crossings 0 after reset");

    /* Window too small rejected */
    rc = tiku_kits_sigfeatures_zcr_init(&z, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_SIZE,
                "zcr window=0 rejected");

    rc = tiku_kits_sigfeatures_zcr_init(&z, 1);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_SIZE,
                "zcr window=1 rejected");
    TEST_GROUP_END("SigFeatures ZCR");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: HISTOGRAM                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_histogram(void)
{
    struct tiku_kits_sigfeatures_histogram h;
    uint8_t mode;
    int rc;

    TEST_GROUP_BEGIN("SigFeatures Histogram");

    /* 4 bins: [0,10), [10,20), [20,30), [30,40) */
    rc = tiku_kits_sigfeatures_histogram_init(&h, 4, 0, 10);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "histogram init OK");
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_total(&h) == 0,
                "histogram initial total is 0");

    /* Push values into different bins */
    tiku_kits_sigfeatures_histogram_push(&h, 5);   /* bin 0 */
    tiku_kits_sigfeatures_histogram_push(&h, 15);  /* bin 1 */
    tiku_kits_sigfeatures_histogram_push(&h, 15);  /* bin 1 */
    tiku_kits_sigfeatures_histogram_push(&h, 25);  /* bin 2 */
    tiku_kits_sigfeatures_histogram_push(&h, 35);  /* bin 3 */

    TEST_ASSERT(tiku_kits_sigfeatures_histogram_total(&h) == 5,
                "histogram total is 5");
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_get_bin(&h, 0) == 1,
                "bin 0 count == 1");
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_get_bin(&h, 1) == 2,
                "bin 1 count == 2");
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_get_bin(&h, 2) == 1,
                "bin 2 count == 1");
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_get_bin(&h, 3) == 1,
                "bin 3 count == 1");

    /* Mode is bin 1 (highest count) */
    rc = tiku_kits_sigfeatures_histogram_mode_bin(&h, &mode);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "mode_bin OK");
    TEST_ASSERT(mode == 1, "mode is bin 1");

    /* Underflow and overflow */
    tiku_kits_sigfeatures_histogram_push(&h, -5);  /* underflow */
    tiku_kits_sigfeatures_histogram_push(&h, 45);  /* overflow */
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_underflow(&h) == 1,
                "underflow count == 1");
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_overflow(&h) == 1,
                "overflow count == 1");
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_total(&h) == 7,
                "total includes under/overflow");

    /* Out-of-range bin index returns 0 */
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_get_bin(&h, 10) == 0,
                "out-of-range bin returns 0");

    /* Reset clears counts but preserves config */
    tiku_kits_sigfeatures_histogram_reset(&h);
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_total(&h) == 0,
                "total 0 after reset");
    TEST_ASSERT(tiku_kits_sigfeatures_histogram_underflow(&h) == 0,
                "underflow 0 after reset");

    /* Invalid init */
    rc = tiku_kits_sigfeatures_histogram_init(&h, 0, 0, 10);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_PARAM,
                "histogram num_bins=0 rejected");

    rc = tiku_kits_sigfeatures_histogram_init(&h, 4, 0, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_PARAM,
                "histogram bin_width=0 rejected");

    /* Mode on empty returns error */
    tiku_kits_sigfeatures_histogram_init(&h, 4, 0, 10);
    rc = tiku_kits_sigfeatures_histogram_mode_bin(&h, &mode);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NODATA,
                "mode_bin on empty rejected");
    TEST_GROUP_END("SigFeatures Histogram");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: FIRST-ORDER DELTA                                                 */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_delta(void)
{
    struct tiku_kits_sigfeatures_delta d;
    tiku_kits_sigfeatures_elem_t val;
    int rc;

    TEST_GROUP_BEGIN("SigFeatures Delta");

    rc = tiku_kits_sigfeatures_delta_init(&d);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "delta init OK");

    /* No delta available yet */
    rc = tiku_kits_sigfeatures_delta_get(&d, &val);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NODATA,
                "delta get before 2 samples rejected");

    /* First push - still no delta */
    tiku_kits_sigfeatures_delta_push(&d, 100);
    rc = tiku_kits_sigfeatures_delta_get(&d, &val);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NODATA,
                "delta get after 1 sample rejected");

    /* Second push: delta = 150 - 100 = 50 */
    tiku_kits_sigfeatures_delta_push(&d, 150);
    rc = tiku_kits_sigfeatures_delta_get(&d, &val);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "delta get OK");
    TEST_ASSERT(val == 50, "delta(100,150) == 50");

    /* Third push: delta = 120 - 150 = -30 */
    tiku_kits_sigfeatures_delta_push(&d, 120);
    tiku_kits_sigfeatures_delta_get(&d, &val);
    TEST_ASSERT(val == -30, "delta(150,120) == -30");

    /* Batch compute */
    {
        tiku_kits_sigfeatures_elem_t src[] = {10, 30, 25, 40};
        tiku_kits_sigfeatures_elem_t dst[3];

        rc = tiku_kits_sigfeatures_delta_compute(src, 4, dst);
        TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "delta compute OK");
        TEST_ASSERT(dst[0] == 20, "batch delta[0] == 20");
        TEST_ASSERT(dst[1] == -5, "batch delta[1] == -5");
        TEST_ASSERT(dst[2] == 15, "batch delta[2] == 15");
    }

    /* Reset */
    tiku_kits_sigfeatures_delta_reset(&d);
    rc = tiku_kits_sigfeatures_delta_get(&d, &val);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NODATA,
                "delta get after reset rejected");
    TEST_GROUP_END("SigFeatures Delta");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: GOERTZEL SINGLE-FREQUENCY DFT                                     */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_goertzel(void)
{
    struct tiku_kits_sigfeatures_goertzel g;
    int64_t mag;
    int rc;

    TEST_GROUP_BEGIN("SigFeatures Goertzel");

    /*
     * N=4, k=1 -> coeff = 2*cos(2*pi*1/4) = 2*cos(pi/2) = 0
     * coeff_q14 = 0
     */
    rc = tiku_kits_sigfeatures_goertzel_init(&g, 0, 4);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "goertzel init OK");
    TEST_ASSERT(tiku_kits_sigfeatures_goertzel_complete(&g) == 0,
                "not complete initially");

    /* Magnitude before complete rejected */
    rc = tiku_kits_sigfeatures_goertzel_magnitude_sq(&g, &mag);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NODATA,
                "magnitude_sq before complete rejected");

    /* Pure tone at bin 1: [1000, 0, -1000, 0] should have energy */
    tiku_kits_sigfeatures_goertzel_push(&g, 1000);
    tiku_kits_sigfeatures_goertzel_push(&g, 0);
    tiku_kits_sigfeatures_goertzel_push(&g, -1000);
    tiku_kits_sigfeatures_goertzel_push(&g, 0);

    TEST_ASSERT(tiku_kits_sigfeatures_goertzel_complete(&g) == 1,
                "complete after N samples");

    rc = tiku_kits_sigfeatures_goertzel_magnitude_sq(&g, &mag);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "magnitude_sq OK");
    TEST_ASSERT(mag > 0, "tone at bin 1 has nonzero magnitude");

    /* DC signal [100, 100, 100, 100] should have zero energy at bin 1 */
    tiku_kits_sigfeatures_goertzel_reset(&g);
    tiku_kits_sigfeatures_goertzel_push(&g, 100);
    tiku_kits_sigfeatures_goertzel_push(&g, 100);
    tiku_kits_sigfeatures_goertzel_push(&g, 100);
    tiku_kits_sigfeatures_goertzel_push(&g, 100);

    tiku_kits_sigfeatures_goertzel_magnitude_sq(&g, &mag);
    TEST_ASSERT(mag == 0, "DC signal has zero magnitude at bin 1");

    /* Block convenience function */
    {
        tiku_kits_sigfeatures_elem_t tone[] = {1000, 0, -1000, 0};

        rc = tiku_kits_sigfeatures_goertzel_block(&g, tone, 4, &mag);
        TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "block returns OK");
        TEST_ASSERT(mag > 0, "block tone magnitude > 0");
    }

    /* Invalid init */
    rc = tiku_kits_sigfeatures_goertzel_init(&g, 0, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_PARAM,
                "goertzel block_size=0 rejected");
    TEST_GROUP_END("SigFeatures Goertzel");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: Z-SCORE NORMALIZATION                                             */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_zscore(void)
{
    struct tiku_kits_sigfeatures_zscore z;
    int32_t result;
    int rc;

    TEST_GROUP_BEGIN("SigFeatures Z-Score");

    /* mean=100, stddev=10, shift=16 -> inv = 65536/10 = 6553 */
    rc = tiku_kits_sigfeatures_zscore_init(&z, 100, 10, 16);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "zscore init OK");

    /* At the mean: z-score = 0 */
    tiku_kits_sigfeatures_zscore_normalize(&z, 100, &result);
    TEST_ASSERT(result == 0, "zscore(mean) == 0");

    /* One stddev above: z = (110-100) * 6553 = 65530 (~1.0 in Q16) */
    tiku_kits_sigfeatures_zscore_normalize(&z, 110, &result);
    TEST_ASSERT(result == 65530, "zscore(mean+stddev) ~= 1.0 in Q16");

    /* One stddev below: z = (90-100) * 6553 = -65530 */
    tiku_kits_sigfeatures_zscore_normalize(&z, 90, &result);
    TEST_ASSERT(result == -65530, "zscore(mean-stddev) ~= -1.0 in Q16");

    /* Batch */
    {
        tiku_kits_sigfeatures_elem_t src[] = {100, 110, 90};
        int32_t dst[3];

        rc = tiku_kits_sigfeatures_zscore_normalize_batch(&z, src, dst, 3);
        TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "zscore batch OK");
        TEST_ASSERT(dst[0] == 0, "batch zscore[0] == 0");
        TEST_ASSERT(dst[1] == 65530, "batch zscore[1] == 65530");
    }

    /* Update statistics */
    rc = tiku_kits_sigfeatures_zscore_update(&z, 50, 5);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "zscore update OK");

    tiku_kits_sigfeatures_zscore_normalize(&z, 50, &result);
    TEST_ASSERT(result == 0, "zscore(new_mean) == 0 after update");

    /* Invalid: stddev = 0 */
    rc = tiku_kits_sigfeatures_zscore_init(&z, 0, 0, 16);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_PARAM,
                "zscore stddev=0 rejected");

    /* Invalid: shift out of range */
    rc = tiku_kits_sigfeatures_zscore_init(&z, 0, 10, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_PARAM,
                "zscore shift=0 rejected");
    TEST_GROUP_END("SigFeatures Z-Score");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: MIN-MAX SCALING                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_scale(void)
{
    struct tiku_kits_sigfeatures_scale s;
    int32_t result;
    int rc;

    TEST_GROUP_BEGIN("SigFeatures Scale");

    /* Scale [0, 100] -> [0, 255] */
    rc = tiku_kits_sigfeatures_scale_init(&s, 0, 100, 255, 16);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "scale init OK");

    /* At boundaries */
    tiku_kits_sigfeatures_scale_normalize(&s, 0, &result);
    TEST_ASSERT(result == 0, "scale(0) == 0");

    tiku_kits_sigfeatures_scale_normalize(&s, 100, &result);
    TEST_ASSERT(result == 255, "scale(100) == 255");

    /* Midpoint */
    tiku_kits_sigfeatures_scale_normalize(&s, 50, &result);
    TEST_ASSERT(result == 127, "scale(50) == 127");

    /* Clamping: below min -> 0 */
    tiku_kits_sigfeatures_scale_normalize(&s, -10, &result);
    TEST_ASSERT(result == 0, "scale(-10) clamped to 0");

    /* Clamping: above max -> out_max */
    tiku_kits_sigfeatures_scale_normalize(&s, 200, &result);
    TEST_ASSERT(result == 255, "scale(200) clamped to 255");

    /* Batch */
    {
        tiku_kits_sigfeatures_elem_t src[] = {0, 50, 100};
        int32_t dst[3];

        rc = tiku_kits_sigfeatures_scale_normalize_batch(&s, src, dst, 3);
        TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "scale batch OK");
        TEST_ASSERT(dst[0] == 0, "batch scale[0] == 0");
        TEST_ASSERT(dst[2] == 255, "batch scale[2] == 255");
    }

    /* Update input range */
    rc = tiku_kits_sigfeatures_scale_update(&s, 0, 200);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_OK, "scale update OK");

    tiku_kits_sigfeatures_scale_normalize(&s, 100, &result);
    TEST_ASSERT(result == 127, "scale(100) == 127 after update to [0,200]");

    /* Invalid: in_max <= in_min */
    rc = tiku_kits_sigfeatures_scale_init(&s, 100, 100, 255, 16);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_PARAM,
                "scale in_max==in_min rejected");

    rc = tiku_kits_sigfeatures_scale_init(&s, 100, 50, 255, 16);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_PARAM,
                "scale in_max<in_min rejected");
    TEST_GROUP_END("SigFeatures Scale");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_sigfeatures_null(void)
{
    tiku_kits_sigfeatures_elem_t val;
    int32_t result32;
    int64_t result64;
    uint16_t idx;
    uint8_t bin;
    int rc;

    TEST_GROUP_BEGIN("SigFeatures NULL Inputs");

    /* Peak */
    rc = tiku_kits_sigfeatures_peak_init(NULL, 10);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "peak init NULL rejected");
    rc = tiku_kits_sigfeatures_peak_push(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "peak push NULL rejected");
    TEST_ASSERT(tiku_kits_sigfeatures_peak_count(NULL) == 0,
                "peak count NULL returns 0");

    /* ZCR */
    rc = tiku_kits_sigfeatures_zcr_init(NULL, 8);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "zcr init NULL rejected");
    rc = tiku_kits_sigfeatures_zcr_push(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "zcr push NULL rejected");

    /* Histogram */
    rc = tiku_kits_sigfeatures_histogram_init(NULL, 4, 0, 10);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "histogram init NULL rejected");
    rc = tiku_kits_sigfeatures_histogram_push(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "histogram push NULL rejected");

    /* Delta */
    rc = tiku_kits_sigfeatures_delta_init(NULL);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "delta init NULL rejected");
    rc = tiku_kits_sigfeatures_delta_push(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "delta push NULL rejected");
    rc = tiku_kits_sigfeatures_delta_compute(NULL, 4, &val);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "delta compute NULL src rejected");

    /* Goertzel */
    rc = tiku_kits_sigfeatures_goertzel_init(NULL, 0, 4);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "goertzel init NULL rejected");
    rc = tiku_kits_sigfeatures_goertzel_push(NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "goertzel push NULL rejected");

    /* Z-Score */
    rc = tiku_kits_sigfeatures_zscore_init(NULL, 0, 10, 16);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "zscore init NULL rejected");

    /* Scale */
    rc = tiku_kits_sigfeatures_scale_init(NULL, 0, 100, 255, 16);
    TEST_ASSERT(rc == TIKU_KITS_SIGFEATURES_ERR_NULL,
                "scale init NULL rejected");
    TEST_GROUP_END("SigFeatures NULL Inputs");
}
