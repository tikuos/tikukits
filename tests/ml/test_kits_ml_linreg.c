/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ml_linreg.c - Linear regression library tests
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

#include "test_kits_ml.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: PERFECT FIT (y = 2x, intercept = 0)                              */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_perfect_fit(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t slope_q;
    int rc;

    TEST_GROUP_BEGIN("LinReg Perfect Fit");

    rc = tiku_kits_ml_linreg_init(&lr, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "init returns OK");
    TEST_ASSERT(tiku_kits_ml_linreg_count(&lr) == 0,
                "initial count is 0");

    /* y = 2x: (1,2), (2,4), (3,6) */
    tiku_kits_ml_linreg_push(&lr, 1, 2);
    tiku_kits_ml_linreg_push(&lr, 2, 4);
    tiku_kits_ml_linreg_push(&lr, 3, 6);

    TEST_ASSERT(tiku_kits_ml_linreg_count(&lr) == 3,
                "count is 3 after 3 pushes");

    rc = tiku_kits_ml_linreg_slope(&lr, &slope_q);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "slope returns OK");
    /* slope = 2.0, in Q8 = 512 */
    TEST_ASSERT(slope_q == 512, "slope(y=2x) == 512 (2.0 Q8)");

    TEST_GROUP_END("LinReg Perfect Fit");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: NON-ZERO INTERCEPT (y = 2x + 1)                                  */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_intercept(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t slope_q, intercept_q;

    TEST_GROUP_BEGIN("LinReg Intercept");

    tiku_kits_ml_linreg_init(&lr, 8);

    /* y = 2x + 1: (0,1), (1,3), (2,5) */
    tiku_kits_ml_linreg_push(&lr, 0, 1);
    tiku_kits_ml_linreg_push(&lr, 1, 3);
    tiku_kits_ml_linreg_push(&lr, 2, 5);

    tiku_kits_ml_linreg_slope(&lr, &slope_q);
    TEST_ASSERT(slope_q == 512, "slope(y=2x+1) == 512 (2.0 Q8)");

    tiku_kits_ml_linreg_intercept(&lr, &intercept_q);
    TEST_ASSERT(intercept_q == 256,
                "intercept(y=2x+1) == 256 (1.0 Q8)");

    TEST_GROUP_END("LinReg Intercept");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: FRACTIONAL SLOPE (y ~= 1.5x + 10)                                */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_fractional_slope(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t slope_q, intercept_q;

    TEST_GROUP_BEGIN("LinReg Fractional Slope");

    tiku_kits_ml_linreg_init(&lr, 8);

    /* y = 1.5x + 10: (0,10), (2,13), (4,16), (6,19) */
    tiku_kits_ml_linreg_push(&lr, 0, 10);
    tiku_kits_ml_linreg_push(&lr, 2, 13);
    tiku_kits_ml_linreg_push(&lr, 4, 16);
    tiku_kits_ml_linreg_push(&lr, 6, 19);

    tiku_kits_ml_linreg_slope(&lr, &slope_q);
    /* slope = 1.5, in Q8 = 384 */
    TEST_ASSERT(slope_q == 384,
                "slope(y=1.5x+10) == 384 (1.5 Q8)");

    tiku_kits_ml_linreg_intercept(&lr, &intercept_q);
    /* intercept = 10, in Q8 = 2560 */
    TEST_ASSERT(intercept_q == 2560,
                "intercept(y=1.5x+10) == 2560 (10.0 Q8)");

    TEST_GROUP_END("LinReg Fractional Slope");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: PREDICTION                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_predict(void)
{
    struct tiku_kits_ml_linreg lr;
    tiku_kits_ml_elem_t y_pred;

    TEST_GROUP_BEGIN("LinReg Predict");

    tiku_kits_ml_linreg_init(&lr, 8);

    /* y = 3x + 5: (0,5), (10,35), (20,65) */
    tiku_kits_ml_linreg_push(&lr, 0, 5);
    tiku_kits_ml_linreg_push(&lr, 10, 35);
    tiku_kits_ml_linreg_push(&lr, 20, 65);

    /* Predict at x = 15: y = 3*15 + 5 = 50 */
    tiku_kits_ml_linreg_predict(&lr, 15, &y_pred);
    TEST_ASSERT(y_pred == 50, "predict(15) for y=3x+5 == 50");

    /* Predict at training point: x = 10 -> y = 35 */
    tiku_kits_ml_linreg_predict(&lr, 10, &y_pred);
    TEST_ASSERT(y_pred == 35, "predict(10) for y=3x+5 == 35");

    /* Extrapolate: x = 30 -> y = 3*30 + 5 = 95 */
    tiku_kits_ml_linreg_predict(&lr, 30, &y_pred);
    TEST_ASSERT(y_pred == 95, "predict(30) for y=3x+5 == 95");

    /* Predict at x = 0 -> y = 5 */
    tiku_kits_ml_linreg_predict(&lr, 0, &y_pred);
    TEST_ASSERT(y_pred == 5, "predict(0) for y=3x+5 == 5");

    TEST_GROUP_END("LinReg Predict");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: R-SQUARED                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_r2(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t r2_q;

    TEST_GROUP_BEGIN("LinReg R-squared");

    /* Perfect fit: R^2 should be 1.0 = 256 in Q8 */
    tiku_kits_ml_linreg_init(&lr, 8);
    tiku_kits_ml_linreg_push(&lr, 1, 2);
    tiku_kits_ml_linreg_push(&lr, 2, 4);
    tiku_kits_ml_linreg_push(&lr, 3, 6);

    tiku_kits_ml_linreg_r2(&lr, &r2_q);
    TEST_ASSERT(r2_q == 256, "R2 for perfect fit == 256 (1.0 Q8)");

    /* Imperfect fit: R^2 < 1 */
    tiku_kits_ml_linreg_reset(&lr);
    tiku_kits_ml_linreg_push(&lr, 1, 2);
    tiku_kits_ml_linreg_push(&lr, 2, 5);
    tiku_kits_ml_linreg_push(&lr, 3, 4);
    tiku_kits_ml_linreg_push(&lr, 4, 8);

    tiku_kits_ml_linreg_r2(&lr, &r2_q);
    TEST_ASSERT(r2_q > 0 && r2_q < 256,
                "R2 for noisy data in (0, 256)");

    TEST_GROUP_END("LinReg R-squared");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: NEGATIVE VALUES                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_negative_values(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t slope_q, intercept_q;
    tiku_kits_ml_elem_t y_pred;

    TEST_GROUP_BEGIN("LinReg Negative Values");

    tiku_kits_ml_linreg_init(&lr, 8);

    /* y = -2x + 100: (0,100), (10,80), (20,60), (50,0) */
    tiku_kits_ml_linreg_push(&lr, 0, 100);
    tiku_kits_ml_linreg_push(&lr, 10, 80);
    tiku_kits_ml_linreg_push(&lr, 20, 60);
    tiku_kits_ml_linreg_push(&lr, 50, 0);

    tiku_kits_ml_linreg_slope(&lr, &slope_q);
    /* slope = -2.0, in Q8 = -512 */
    TEST_ASSERT(slope_q == -512,
                "slope(y=-2x+100) == -512 (-2.0 Q8)");

    tiku_kits_ml_linreg_intercept(&lr, &intercept_q);
    /* intercept = 100, in Q8 = 25600 */
    TEST_ASSERT(intercept_q == 25600,
                "intercept(y=-2x+100) == 25600 (100.0 Q8)");

    /* Predict at x = 25 -> y = -50 + 100 = 50 */
    tiku_kits_ml_linreg_predict(&lr, 25, &y_pred);
    TEST_ASSERT(y_pred == 50,
                "predict(25) for y=-2x+100 == 50");

    TEST_GROUP_END("LinReg Negative Values");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: RESET                                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_reset(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t slope_q;
    tiku_kits_ml_elem_t y_pred;
    int rc;

    TEST_GROUP_BEGIN("LinReg Reset");

    tiku_kits_ml_linreg_init(&lr, 8);

    /* First model: y = 2x */
    tiku_kits_ml_linreg_push(&lr, 1, 2);
    tiku_kits_ml_linreg_push(&lr, 2, 4);
    tiku_kits_ml_linreg_push(&lr, 3, 6);

    tiku_kits_ml_linreg_slope(&lr, &slope_q);
    TEST_ASSERT(slope_q == 512, "slope before reset == 512");

    /* Reset and verify empty */
    rc = tiku_kits_ml_linreg_reset(&lr);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_ml_linreg_count(&lr) == 0,
                "count 0 after reset");

    /* Querying empty model returns ERR_SIZE */
    rc = tiku_kits_ml_linreg_slope(&lr, &slope_q);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_SIZE,
                "slope on empty model rejected");

    rc = tiku_kits_ml_linreg_predict(&lr, 5, &y_pred);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_SIZE,
                "predict on empty model rejected");

    /* Refit: y = 5x + 10 */
    tiku_kits_ml_linreg_push(&lr, 0, 10);
    tiku_kits_ml_linreg_push(&lr, 2, 20);
    tiku_kits_ml_linreg_push(&lr, 4, 30);

    tiku_kits_ml_linreg_slope(&lr, &slope_q);
    /* slope = 5.0, in Q8 = 1280 */
    TEST_ASSERT(slope_q == 1280,
                "slope after refit == 1280 (5.0 Q8)");

    TEST_GROUP_END("LinReg Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: EDGE CASES                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_edge_cases(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t slope_q, r2_q;
    int rc;

    TEST_GROUP_BEGIN("LinReg Edge Cases");

    tiku_kits_ml_linreg_init(&lr, 8);

    /* Single point: should fail */
    tiku_kits_ml_linreg_push(&lr, 5, 10);
    rc = tiku_kits_ml_linreg_slope(&lr, &slope_q);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_SIZE,
                "slope with 1 point rejected");

    /* Two identical x values: singular */
    tiku_kits_ml_linreg_reset(&lr);
    tiku_kits_ml_linreg_push(&lr, 5, 10);
    tiku_kits_ml_linreg_push(&lr, 5, 20);
    rc = tiku_kits_ml_linreg_slope(&lr, &slope_q);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_SINGULAR,
                "slope with identical x values rejected");

    /* All y values identical: R^2 should be SINGULAR */
    tiku_kits_ml_linreg_reset(&lr);
    tiku_kits_ml_linreg_push(&lr, 1, 10);
    tiku_kits_ml_linreg_push(&lr, 2, 10);
    tiku_kits_ml_linreg_push(&lr, 3, 10);
    rc = tiku_kits_ml_linreg_r2(&lr, &r2_q);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_SINGULAR,
                "R2 with constant y rejected");

    /* Slope should be 0 when y is constant */
    rc = tiku_kits_ml_linreg_slope(&lr, &slope_q);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "slope with constant y OK");
    TEST_ASSERT(slope_q == 0, "slope with constant y == 0");

    /* Invalid shift */
    rc = tiku_kits_ml_linreg_init(&lr, 31);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=31 rejected");

    TEST_GROUP_END("LinReg Edge Cases");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linreg_null_inputs(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t val;
    tiku_kits_ml_elem_t y_pred;
    int rc;

    TEST_GROUP_BEGIN("LinReg NULL Inputs");

    rc = tiku_kits_ml_linreg_init(NULL, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "init NULL rejected");

    rc = tiku_kits_ml_linreg_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "reset NULL rejected");

    rc = tiku_kits_ml_linreg_push(NULL, 1, 2);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "push NULL rejected");

    /* Valid model, NULL output pointers */
    tiku_kits_ml_linreg_init(&lr, 8);
    tiku_kits_ml_linreg_push(&lr, 1, 2);
    tiku_kits_ml_linreg_push(&lr, 2, 4);

    rc = tiku_kits_ml_linreg_slope(&lr, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "slope NULL out rejected");

    rc = tiku_kits_ml_linreg_slope(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "slope NULL lr rejected");

    rc = tiku_kits_ml_linreg_intercept(&lr, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "intercept NULL out rejected");

    rc = tiku_kits_ml_linreg_predict(&lr, 5, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "predict NULL out rejected");

    rc = tiku_kits_ml_linreg_predict(NULL, 5, &y_pred);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "predict NULL lr rejected");

    rc = tiku_kits_ml_linreg_r2(&lr, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "r2 NULL out rejected");

    TEST_ASSERT(tiku_kits_ml_linreg_count(NULL) == 0,
                "count NULL returns 0");

    TEST_GROUP_END("LinReg NULL Inputs");
}
