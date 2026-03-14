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
#include "tikukits/ml/classification/tiku_kits_ml_logreg.h"
#include "tikukits/ml/classification/tiku_kits_ml_dtree.h"
#include "tikukits/ml/classification/tiku_kits_ml_knn.h"
#include "tikukits/ml/classification/tiku_kits_ml_nbayes.h"
#include "tikukits/ml/classification/tiku_kits_ml_linsvm.h"
#include "tikukits/ml/classification/tiku_kits_ml_tnn.h"

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

/*---------------------------------------------------------------------------*/
/* LOGISTIC REGRESSION TESTS                                                 */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_init(void);
void test_kits_ml_logreg_pretrained(void);
void test_kits_ml_logreg_sigmoid_saturation(void);
void test_kits_ml_logreg_sigmoid_midpoint(void);
void test_kits_ml_logreg_training(void);
void test_kits_ml_logreg_two_features(void);
void test_kits_ml_logreg_reset(void);
void test_kits_ml_logreg_learning_rate(void);
void test_kits_ml_logreg_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* DECISION TREE TESTS                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_init(void);
void test_kits_ml_dtree_simple_tree(void);
void test_kits_ml_dtree_multi_feature(void);
void test_kits_ml_dtree_depth(void);
void test_kits_ml_dtree_predict_proba(void);
void test_kits_ml_dtree_boundary(void);
void test_kits_ml_dtree_get_tree(void);
void test_kits_ml_dtree_reset(void);
void test_kits_ml_dtree_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* K-NEAREST NEIGHBORS TESTS                                                 */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_init(void);
void test_kits_ml_knn_simple(void);
void test_kits_ml_knn_two_features(void);
void test_kits_ml_knn_k1(void);
void test_kits_ml_knn_change_k(void);
void test_kits_ml_knn_overflow(void);
void test_kits_ml_knn_negative(void);
void test_kits_ml_knn_reset(void);
void test_kits_ml_knn_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* NAIVE BAYES TESTS                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_init(void);
void test_kits_ml_nbayes_simple(void);
void test_kits_ml_nbayes_two_features(void);
void test_kits_ml_nbayes_three_class(void);
void test_kits_ml_nbayes_log_proba(void);
void test_kits_ml_nbayes_smoothing(void);
void test_kits_ml_nbayes_imbalanced(void);
void test_kits_ml_nbayes_reset(void);
void test_kits_ml_nbayes_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* LINEAR SVM TESTS                                                          */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_init(void);
void test_kits_ml_linsvm_pretrained(void);
void test_kits_ml_linsvm_decision(void);
void test_kits_ml_linsvm_training(void);
void test_kits_ml_linsvm_two_features(void);
void test_kits_ml_linsvm_learning_rate(void);
void test_kits_ml_linsvm_lambda(void);
void test_kits_ml_linsvm_reset(void);
void test_kits_ml_linsvm_null_inputs(void);

/*---------------------------------------------------------------------------*/
/* TINY NEURAL NETWORK TESTS                                                 */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_init(void);
void test_kits_ml_tnn_pretrained(void);
void test_kits_ml_tnn_forward_pass(void);
void test_kits_ml_tnn_training(void);
void test_kits_ml_tnn_three_class(void);
void test_kits_ml_tnn_learning_rate(void);
void test_kits_ml_tnn_weight_access(void);
void test_kits_ml_tnn_reset(void);
void test_kits_ml_tnn_null_inputs(void);

#endif /* TEST_KITS_ML_H_ */
