/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_maths.h - TikuKits maths test interface
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

#ifndef TEST_KITS_MATHS_H_
#define TEST_KITS_MATHS_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tikukits/maths/tiku_kits_maths.h"
#include "tikukits/maths/linear_algebra/tiku_kits_matrix.h"
#include "tikukits/maths/statistics/tiku_kits_statistics.h"
#include "tikukits/maths/distance/tiku_kits_distance.h"

/*---------------------------------------------------------------------------*/
/* TEST HARNESS                                                              */
/*---------------------------------------------------------------------------*/

#ifdef PLATFORM_MSP430
#include "tiku.h"
#define TEST_PRINT(...) TIKU_PRINTF(__VA_ARGS__)
#else
#define TEST_PRINT(...) printf(__VA_ARGS__)
#endif

extern int tests_run;
extern int tests_passed;
extern int tests_failed;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        tests_run++;                                                        \
        if (cond) {                                                         \
            tests_passed++;                                                 \
            TEST_PRINT("  PASS: %s\n", msg);                                \
        } else {                                                            \
            tests_failed++;                                                 \
            TEST_PRINT("  FAIL: %s\n", msg);                                \
        }                                                                   \
    } while (0)

/*---------------------------------------------------------------------------*/
/* MATRIX TESTS                                                              */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_init(void);
void test_kits_matrix_identity(void);
void test_kits_matrix_set_get(void);
void test_kits_matrix_copy_equal(void);
void test_kits_matrix_add_sub(void);
void test_kits_matrix_mul(void);
void test_kits_matrix_scale(void);
void test_kits_matrix_transpose(void);
void test_kits_matrix_det(void);
void test_kits_matrix_trace(void);
void test_kits_matrix_dim_mismatch(void);
void test_kits_matrix_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* STATISTICS TESTS                                                          */
/*---------------------------------------------------------------------------*/

void test_kits_stats_windowed(void);
void test_kits_stats_windowed_eviction(void);
void test_kits_stats_welford(void);
void test_kits_stats_minmax(void);
void test_kits_stats_ewma(void);
void test_kits_stats_energy(void);
void test_kits_stats_isqrt(void);
void test_kits_stats_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* DISTANCE TESTS                                                            */
/*---------------------------------------------------------------------------*/

void test_kits_distance_manhattan(void);
void test_kits_distance_euclidean_sq(void);
void test_kits_distance_dot(void);
void test_kits_distance_cosine_sq(void);
void test_kits_distance_hamming(void);
void test_kits_distance_null_inputs(void);

#endif /* TEST_KITS_MATHS_H_ */
