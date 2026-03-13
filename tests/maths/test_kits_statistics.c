/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_statistics.c - Statistics library tests
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

#include "test_kits_maths.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: WINDOWED TRACKER - BASIC OPERATIONS                               */
/*---------------------------------------------------------------------------*/

void test_kits_stats_windowed(void)
{
    struct tiku_kits_statistics s;
    tiku_kits_statistics_elem_t val;
    int rc;

    TEST_PRINT("\n--- Test: Statistics Windowed ---\n");

    rc = tiku_kits_statistics_init(&s, 4);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "windowed init OK");
    TEST_ASSERT(tiku_kits_statistics_count(&s) == 0, "initial count is 0");
    TEST_ASSERT(tiku_kits_statistics_full(&s) == 0, "not full initially");
    TEST_ASSERT(tiku_kits_statistics_capacity(&s) == 4, "capacity is 4");

    /* Mean on empty tracker returns error */
    rc = tiku_kits_statistics_mean(&s, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "mean on empty rejected");

    /* Push 3 samples: 10, 20, 30 -> mean = 20 */
    tiku_kits_statistics_push(&s, 10);
    tiku_kits_statistics_push(&s, 20);
    tiku_kits_statistics_push(&s, 30);

    TEST_ASSERT(tiku_kits_statistics_count(&s) == 3, "count is 3");
    TEST_ASSERT(tiku_kits_statistics_full(&s) == 0, "not full at 3/4");

    rc = tiku_kits_statistics_mean(&s, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "mean returns OK");
    TEST_ASSERT(val == 20, "mean(10,20,30) == 20");

    /* Min/max */
    tiku_kits_statistics_min(&s, &val);
    TEST_ASSERT(val == 10, "min(10,20,30) == 10");

    tiku_kits_statistics_max(&s, &val);
    TEST_ASSERT(val == 30, "max(10,20,30) == 30");

    /* Variance: mean=20, diffs=-10,0,10, sq=100+0+100=200, var=200/3=66 */
    tiku_kits_statistics_variance(&s, &val);
    TEST_ASSERT(val == 66, "variance(10,20,30) == 66 (integer)");

    /* Fill to capacity */
    tiku_kits_statistics_push(&s, 40);
    TEST_ASSERT(tiku_kits_statistics_count(&s) == 4, "count is 4");
    TEST_ASSERT(tiku_kits_statistics_full(&s) == 1, "full at 4/4");

    /* mean(10,20,30,40) = 100/4 = 25 */
    tiku_kits_statistics_mean(&s, &val);
    TEST_ASSERT(val == 25, "mean(10,20,30,40) == 25");

    /* Reset clears samples but keeps capacity */
    rc = tiku_kits_statistics_reset(&s);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_statistics_count(&s) == 0, "count 0 after reset");
    TEST_ASSERT(tiku_kits_statistics_capacity(&s) == 4,
                "capacity preserved after reset");

    /* Invalid init sizes */
    rc = tiku_kits_statistics_init(&s, 0);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "window=0 rejected");

    rc = tiku_kits_statistics_init(&s,
                                    TIKU_KITS_STATISTICS_MAX_WINDOW + 1);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "oversized window rejected");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: WINDOWED TRACKER - EVICTION                                       */
/*---------------------------------------------------------------------------*/

void test_kits_stats_windowed_eviction(void)
{
    struct tiku_kits_statistics s;
    tiku_kits_statistics_elem_t val;

    TEST_PRINT("\n--- Test: Statistics Windowed Eviction ---\n");

    tiku_kits_statistics_init(&s, 3);

    /* Fill: {100, 200, 300} */
    tiku_kits_statistics_push(&s, 100);
    tiku_kits_statistics_push(&s, 200);
    tiku_kits_statistics_push(&s, 300);

    /* mean(100,200,300) = 200 */
    tiku_kits_statistics_mean(&s, &val);
    TEST_ASSERT(val == 200, "mean before eviction == 200");

    /* Push 400 -> evicts 100, window = {200, 300, 400} */
    tiku_kits_statistics_push(&s, 400);
    TEST_ASSERT(tiku_kits_statistics_count(&s) == 3,
                "count stays at capacity");

    tiku_kits_statistics_mean(&s, &val);
    TEST_ASSERT(val == 300, "mean after eviction == 300");

    tiku_kits_statistics_min(&s, &val);
    TEST_ASSERT(val == 200, "min after eviction == 200");

    tiku_kits_statistics_max(&s, &val);
    TEST_ASSERT(val == 400, "max after eviction == 400");

    /* Push 500 -> evicts 200, window = {300, 400, 500} */
    tiku_kits_statistics_push(&s, 500);
    tiku_kits_statistics_mean(&s, &val);
    TEST_ASSERT(val == 400, "mean after second eviction == 400");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: WELFORD RUNNING MEAN/VARIANCE                                     */
/*---------------------------------------------------------------------------*/

void test_kits_stats_welford(void)
{
    struct tiku_kits_statistics_welford w;
    tiku_kits_statistics_elem_t val;
    int rc;

    TEST_PRINT("\n--- Test: Statistics Welford ---\n");

    rc = tiku_kits_statistics_welford_init(&w);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "welford init OK");
    TEST_ASSERT(tiku_kits_statistics_welford_count(&w) == 0,
                "welford initial count is 0");

    /* Mean on empty rejected */
    rc = tiku_kits_statistics_welford_mean(&w, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "welford mean empty rejected");

    /* Push samples: 10, 20, 30, 40, 50 -> mean = 30 */
    tiku_kits_statistics_welford_push(&w, 10);
    tiku_kits_statistics_welford_push(&w, 20);
    tiku_kits_statistics_welford_push(&w, 30);
    tiku_kits_statistics_welford_push(&w, 40);
    tiku_kits_statistics_welford_push(&w, 50);

    TEST_ASSERT(tiku_kits_statistics_welford_count(&w) == 5,
                "welford count is 5");

    tiku_kits_statistics_welford_mean(&w, &val);
    TEST_ASSERT(val == 30, "welford mean(10..50) == 30");

    /*
     * Variance = sum((x-30)^2)/5 = (400+100+0+100+400)/5 = 200
     * Note: integer division in welford may introduce ~1 LSB error
     */
    tiku_kits_statistics_welford_variance(&w, &val);
    TEST_ASSERT(val == 200, "welford variance == 200");

    /* stddev = isqrt(200) = 14 */
    tiku_kits_statistics_welford_stddev(&w, &val);
    TEST_ASSERT(val == 14, "welford stddev == 14");

    /* Reset and verify empty */
    tiku_kits_statistics_welford_reset(&w);
    TEST_ASSERT(tiku_kits_statistics_welford_count(&w) == 0,
                "welford count 0 after reset");

    /* Single sample: mean = value, variance = 0 */
    tiku_kits_statistics_welford_push(&w, 42);
    tiku_kits_statistics_welford_mean(&w, &val);
    TEST_ASSERT(val == 42, "welford single sample mean == 42");

    tiku_kits_statistics_welford_variance(&w, &val);
    TEST_ASSERT(val == 0, "welford single sample variance == 0");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: SLIDING-WINDOW MIN/MAX                                            */
/*---------------------------------------------------------------------------*/

void test_kits_stats_minmax(void)
{
    struct tiku_kits_statistics_minmax mm;
    tiku_kits_statistics_elem_t val;
    int rc;

    TEST_PRINT("\n--- Test: Statistics MinMax ---\n");

    rc = tiku_kits_statistics_minmax_init(&mm, 3);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "minmax init OK");
    TEST_ASSERT(tiku_kits_statistics_minmax_count(&mm) == 0,
                "minmax initial count is 0");

    /* Min/max on empty rejected */
    rc = tiku_kits_statistics_minmax_min(&mm, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "minmax min empty rejected");

    /* Push 50, 20, 80 -> min=20, max=80 */
    tiku_kits_statistics_minmax_push(&mm, 50);
    tiku_kits_statistics_minmax_push(&mm, 20);
    tiku_kits_statistics_minmax_push(&mm, 80);

    TEST_ASSERT(tiku_kits_statistics_minmax_count(&mm) == 3,
                "minmax count is 3");
    TEST_ASSERT(tiku_kits_statistics_minmax_full(&mm) == 1,
                "minmax full at capacity");

    tiku_kits_statistics_minmax_min(&mm, &val);
    TEST_ASSERT(val == 20, "minmax min(50,20,80) == 20");

    tiku_kits_statistics_minmax_max(&mm, &val);
    TEST_ASSERT(val == 80, "minmax max(50,20,80) == 80");

    /* Push 10 -> evicts 50, window={20,80,10} -> min=10, max=80 */
    tiku_kits_statistics_minmax_push(&mm, 10);

    tiku_kits_statistics_minmax_min(&mm, &val);
    TEST_ASSERT(val == 10, "minmax min after eviction == 10");

    tiku_kits_statistics_minmax_max(&mm, &val);
    TEST_ASSERT(val == 80, "minmax max after eviction == 80");

    /* Push 5 -> evicts 20, window={80,10,5} -> min=5, max=80 */
    tiku_kits_statistics_minmax_push(&mm, 5);

    tiku_kits_statistics_minmax_min(&mm, &val);
    TEST_ASSERT(val == 5, "minmax min == 5");

    tiku_kits_statistics_minmax_max(&mm, &val);
    TEST_ASSERT(val == 80, "minmax max == 80");

    /* Push 90 -> evicts 80, window={10,5,90} -> min=5, max=90 */
    tiku_kits_statistics_minmax_push(&mm, 90);

    tiku_kits_statistics_minmax_min(&mm, &val);
    TEST_ASSERT(val == 5, "minmax min after max evicted == 5");

    tiku_kits_statistics_minmax_max(&mm, &val);
    TEST_ASSERT(val == 90, "minmax max after max evicted == 90");

    /* Reset */
    tiku_kits_statistics_minmax_reset(&mm);
    TEST_ASSERT(tiku_kits_statistics_minmax_count(&mm) == 0,
                "minmax count 0 after reset");

    /* Invalid init */
    rc = tiku_kits_statistics_minmax_init(&mm, 0);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "minmax window=0 rejected");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: EWMA                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_stats_ewma(void)
{
    struct tiku_kits_statistics_ewma e;
    tiku_kits_statistics_elem_t val;
    int rc;

    TEST_PRINT("\n--- Test: Statistics EWMA ---\n");

    /* alpha=128, shift=8 -> 50% smoothing */
    rc = tiku_kits_statistics_ewma_init(&e, 128, 8);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "ewma init OK");

    /* Get before push rejected */
    rc = tiku_kits_statistics_ewma_get(&e, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "ewma get empty rejected");

    /* First push initializes directly */
    tiku_kits_statistics_ewma_push(&e, 1000);
    tiku_kits_statistics_ewma_get(&e, &val);
    TEST_ASSERT(val == 1000, "ewma first push == 1000");

    /*
     * Push 0 with alpha=128, shift=8:
     * ewma += 128 * (0 - 1000) >> 8 = -128000 >> 8 = -500
     * ewma = 1000 + (-500) = 500
     */
    tiku_kits_statistics_ewma_push(&e, 0);
    tiku_kits_statistics_ewma_get(&e, &val);
    TEST_ASSERT(val == 500, "ewma after push(0) == 500");

    /* Reset clears state */
    tiku_kits_statistics_ewma_reset(&e);
    rc = tiku_kits_statistics_ewma_get(&e, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "ewma empty after reset");

    /* Shift too large rejected */
    rc = tiku_kits_statistics_ewma_init(&e, 128, 17);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "ewma shift=17 rejected");

    /* alpha=256, shift=8 -> full tracking (ewma = last sample) */
    tiku_kits_statistics_ewma_init(&e, 256, 8);
    tiku_kits_statistics_ewma_push(&e, 100);
    tiku_kits_statistics_ewma_push(&e, 200);
    tiku_kits_statistics_ewma_get(&e, &val);
    TEST_ASSERT(val == 200, "ewma full tracking == last sample");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: ENERGY / RMS                                                      */
/*---------------------------------------------------------------------------*/

void test_kits_stats_energy(void)
{
    struct tiku_kits_statistics_energy e;
    tiku_kits_statistics_elem_t val;
    int rc;

    TEST_PRINT("\n--- Test: Statistics Energy/RMS ---\n");

    rc = tiku_kits_statistics_energy_init(&e);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "energy init OK");
    TEST_ASSERT(tiku_kits_statistics_energy_count(&e) == 0,
                "energy initial count is 0");

    /* mean_sq on empty rejected */
    rc = tiku_kits_statistics_energy_mean_sq(&e, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "energy empty rejected");

    /*
     * Push 3, 4 -> mean(x^2) = (9+16)/2 = 12 (integer)
     * rms = isqrt(12) = 3
     */
    tiku_kits_statistics_energy_push(&e, 3);
    tiku_kits_statistics_energy_push(&e, 4);

    TEST_ASSERT(tiku_kits_statistics_energy_count(&e) == 2,
                "energy count is 2");

    tiku_kits_statistics_energy_mean_sq(&e, &val);
    TEST_ASSERT(val == 12, "mean_sq(3,4) == 12");

    tiku_kits_statistics_energy_rms(&e, &val);
    TEST_ASSERT(val == 3, "rms(3,4) == 3");

    /* Negative values: push -5 -> mean(x^2) = (9+16+25)/3 = 16 */
    tiku_kits_statistics_energy_push(&e, -5);
    tiku_kits_statistics_energy_mean_sq(&e, &val);
    TEST_ASSERT(val == 16, "mean_sq(3,4,-5) == 16");

    tiku_kits_statistics_energy_rms(&e, &val);
    TEST_ASSERT(val == 4, "rms(3,4,-5) == 4");

    /* Reset */
    tiku_kits_statistics_energy_reset(&e);
    TEST_ASSERT(tiku_kits_statistics_energy_count(&e) == 0,
                "energy count 0 after reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: INTEGER SQUARE ROOT                                               */
/*---------------------------------------------------------------------------*/

void test_kits_stats_isqrt(void)
{
    TEST_PRINT("\n--- Test: Statistics isqrt ---\n");

    TEST_ASSERT(tiku_kits_statistics_isqrt(0) == 0, "isqrt(0) == 0");
    TEST_ASSERT(tiku_kits_statistics_isqrt(1) == 1, "isqrt(1) == 1");
    TEST_ASSERT(tiku_kits_statistics_isqrt(4) == 2, "isqrt(4) == 2");
    TEST_ASSERT(tiku_kits_statistics_isqrt(9) == 3, "isqrt(9) == 3");
    TEST_ASSERT(tiku_kits_statistics_isqrt(16) == 4, "isqrt(16) == 4");
    TEST_ASSERT(tiku_kits_statistics_isqrt(100) == 10, "isqrt(100) == 10");

    /* Non-perfect squares: floor */
    TEST_ASSERT(tiku_kits_statistics_isqrt(2) == 1, "isqrt(2) == 1");
    TEST_ASSERT(tiku_kits_statistics_isqrt(3) == 1, "isqrt(3) == 1");
    TEST_ASSERT(tiku_kits_statistics_isqrt(5) == 2, "isqrt(5) == 2");
    TEST_ASSERT(tiku_kits_statistics_isqrt(8) == 2, "isqrt(8) == 2");
    TEST_ASSERT(tiku_kits_statistics_isqrt(99) == 9, "isqrt(99) == 9");
    TEST_ASSERT(tiku_kits_statistics_isqrt(10000) == 100,
                "isqrt(10000) == 100");

    /* Negative input returns 0 */
    TEST_ASSERT(tiku_kits_statistics_isqrt(-1) == 0, "isqrt(-1) == 0");
    TEST_ASSERT(tiku_kits_statistics_isqrt(-100) == 0, "isqrt(-100) == 0");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_stats_null_inputs(void)
{
    struct tiku_kits_statistics s;
    struct tiku_kits_statistics_welford w;
    struct tiku_kits_statistics_minmax mm;
    struct tiku_kits_statistics_ewma e;
    struct tiku_kits_statistics_energy en;
    tiku_kits_statistics_elem_t val;
    int rc;

    TEST_PRINT("\n--- Test: Statistics NULL Inputs ---\n");

    /* Windowed */
    rc = tiku_kits_statistics_init(NULL, 4);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "windowed init NULL rejected");

    rc = tiku_kits_statistics_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "windowed reset NULL rejected");

    rc = tiku_kits_statistics_push(NULL, 10);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "windowed push NULL rejected");

    tiku_kits_statistics_init(&s, 4);
    tiku_kits_statistics_push(&s, 10);

    rc = tiku_kits_statistics_mean(&s, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL,
                "windowed mean NULL out rejected");

    rc = tiku_kits_statistics_mean(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL,
                "windowed mean NULL s rejected");

    TEST_ASSERT(tiku_kits_statistics_count(NULL) == 0,
                "windowed count NULL returns 0");

    /* Welford */
    rc = tiku_kits_statistics_welford_init(NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "welford init NULL rejected");

    rc = tiku_kits_statistics_welford_push(NULL, 10);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "welford push NULL rejected");

    TEST_ASSERT(tiku_kits_statistics_welford_count(NULL) == 0,
                "welford count NULL returns 0");

    /* Minmax */
    rc = tiku_kits_statistics_minmax_init(NULL, 4);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "minmax init NULL rejected");

    rc = tiku_kits_statistics_minmax_push(NULL, 10);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "minmax push NULL rejected");

    TEST_ASSERT(tiku_kits_statistics_minmax_count(NULL) == 0,
                "minmax count NULL returns 0");

    /* EWMA */
    rc = tiku_kits_statistics_ewma_init(NULL, 128, 8);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "ewma init NULL rejected");

    rc = tiku_kits_statistics_ewma_push(NULL, 10);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "ewma push NULL rejected");

    rc = tiku_kits_statistics_ewma_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "ewma reset NULL rejected");

    /* Energy */
    rc = tiku_kits_statistics_energy_init(NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "energy init NULL rejected");

    rc = tiku_kits_statistics_energy_push(NULL, 10);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "energy push NULL rejected");

    TEST_ASSERT(tiku_kits_statistics_energy_count(NULL) == 0,
                "energy count NULL returns 0");
}
