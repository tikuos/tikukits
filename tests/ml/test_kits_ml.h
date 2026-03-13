/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ml.h - TikuKits ML test interface
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

#ifndef TEST_KITS_ML_H_
#define TEST_KITS_ML_H_

#include <stdint.h>
#include <string.h>
#include "tests/tiku_test_harness.h"
#include "tikukits/ml/tiku_kits_ml.h"
#include "tikukits/ml/regression/tiku_kits_ml_linreg.h"

/*---------------------------------------------------------------------------*/
/* LINEAR REGRESSION TESTS                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_perfect_fit(void);
void test_kits_ml_linreg_intercept(void);
void test_kits_ml_linreg_fractional_slope(void);
void test_kits_ml_linreg_predict(void);
void test_kits_ml_linreg_r2(void);
void test_kits_ml_linreg_negative_values(void);
void test_kits_ml_linreg_reset(void);
void test_kits_ml_linreg_edge_cases(void);
void test_kits_ml_linreg_null_inputs(void);

#endif /* TEST_KITS_ML_H_ */
