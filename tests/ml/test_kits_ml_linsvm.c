/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ml_linsvm.c - Linear SVM classifier tests
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
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_init(void)
{
    struct tiku_kits_ml_linsvm svm;
    int rc;

    TEST_GROUP_BEGIN("LinSVM Init");

    rc = tiku_kits_ml_linsvm_init(&svm, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "init returns OK");
    TEST_ASSERT(tiku_kits_ml_linsvm_count(&svm) == 0,
                "initial count is 0");

    /* Invalid: 0 features */
    rc = tiku_kits_ml_linsvm_init(&svm, 0, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "0 features rejected");

    /* Invalid: too many features */
    rc = tiku_kits_ml_linsvm_init(
        &svm, TIKU_KITS_ML_LINSVM_MAX_FEATURES + 1, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "too many features rejected");

    /* Invalid: shift = 0 */
    rc = tiku_kits_ml_linsvm_init(&svm, 2, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=0 rejected");

    /* Invalid: shift = 31 */
    rc = tiku_kits_ml_linsvm_init(&svm, 2, 31);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=31 rejected");

    TEST_GROUP_END("LinSVM Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: PRE-TRAINED WEIGHTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_pretrained(void)
{
    struct tiku_kits_ml_linsvm svm;
    int8_t cls;

    TEST_GROUP_BEGIN("LinSVM Pretrained");

    /*
     * Load pre-trained weights for a simple 1-D threshold at x = 0:
     *   f(x) = 0 + 256*x  (bias=0, w1=1.0 in Q8 = 256)
     *   x > 0  -> class +1
     *   x < 0  -> class -1
     */
    tiku_kits_ml_linsvm_init(&svm, 1, 8);
    {
        int32_t w[] = {0, 256};  /* bias=0, w1=1.0 in Q8 */
        tiku_kits_ml_linsvm_set_weights(&svm, w);
    }

    /* Positive side */
    {
        tiku_kits_ml_elem_t x[] = {5};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        TEST_ASSERT(cls == 1, "x=5 -> +1");
    }

    /* Negative side */
    {
        tiku_kits_ml_elem_t x[] = {-5};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        TEST_ASSERT(cls == -1, "x=-5 -> -1");
    }

    /* On boundary: f(0) = 0 >= 0 -> +1 */
    {
        tiku_kits_ml_elem_t x[] = {0};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        TEST_ASSERT(cls == 1, "x=0 -> +1 (boundary)");
    }

    TEST_GROUP_END("LinSVM Pretrained");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: DECISION FUNCTION (RAW MARGIN)                                    */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_decision(void)
{
    struct tiku_kits_ml_linsvm svm;
    int32_t margin;
    int rc;

    TEST_GROUP_BEGIN("LinSVM Decision");

    /*
     * Pre-trained: f(x) = 128 + 256*x  (bias=0.5, w1=1.0 in Q8)
     * f(3) = 128 + 768 = 896
     * f(-3) = 128 - 768 = -640
     */
    tiku_kits_ml_linsvm_init(&svm, 1, 8);
    {
        int32_t w[] = {128, 256};
        tiku_kits_ml_linsvm_set_weights(&svm, w);
    }

    {
        tiku_kits_ml_elem_t x[] = {3};
        rc = tiku_kits_ml_linsvm_decision(&svm, x, &margin);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "decision returns OK");
        TEST_ASSERT(margin == 896,
                    "f(3) = 896 (0.5 + 1.0*3 = 3.5 in Q8)");
    }

    {
        tiku_kits_ml_elem_t x[] = {-3};
        rc = tiku_kits_ml_linsvm_decision(&svm, x, &margin);
        TEST_ASSERT(margin == -640,
                    "f(-3) = -640 (0.5 - 3.0 = -2.5 in Q8)");
    }

    /* Positive margin -> class +1 */
    TEST_ASSERT(margin < 0,
                "negative margin confirms -1 side");

    TEST_GROUP_END("LinSVM Decision");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: TRAINING CONVERGENCE                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_training(void)
{
    struct tiku_kits_ml_linsvm svm;
    int8_t cls;
    uint8_t i;

    TEST_GROUP_BEGIN("LinSVM Training");

    /*
     * Train a 1-D classifier: x > 0 -> +1, x < 0 -> -1
     * Use multiple passes to let SGD converge.
     */
    tiku_kits_ml_linsvm_init(&svm, 1, 8);
    tiku_kits_ml_linsvm_set_lr(&svm, 26);     /* ~0.1 in Q8 */
    tiku_kits_ml_linsvm_set_lambda(&svm, 3);   /* ~0.01 in Q8 */

    for (i = 0; i < 20; i++) {
        tiku_kits_ml_elem_t xp[] = {5};
        tiku_kits_ml_elem_t xn[] = {-5};
        tiku_kits_ml_linsvm_train(&svm, xp, 1);
        tiku_kits_ml_linsvm_train(&svm, xn, -1);
    }

    TEST_ASSERT(tiku_kits_ml_linsvm_count(&svm) == 40,
                "40 samples trained");

    /* Test predictions */
    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        TEST_ASSERT(cls == 1, "x=10 -> +1");
    }

    {
        tiku_kits_ml_elem_t x[] = {-10};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        TEST_ASSERT(cls == -1, "x=-10 -> -1");
    }

    TEST_GROUP_END("LinSVM Training");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: TWO-FEATURE CLASSIFICATION                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_two_features(void)
{
    struct tiku_kits_ml_linsvm svm;
    int8_t cls;
    uint8_t i;

    TEST_GROUP_BEGIN("LinSVM 2-Feature");

    /*
     * Train a 2-D classifier: x1+x2 > 0 -> +1, x1+x2 < 0 -> -1
     */
    tiku_kits_ml_linsvm_init(&svm, 2, 8);
    tiku_kits_ml_linsvm_set_lr(&svm, 26);
    tiku_kits_ml_linsvm_set_lambda(&svm, 3);

    for (i = 0; i < 30; i++) {
        {
            tiku_kits_ml_elem_t x[] = {3, 4};
            tiku_kits_ml_linsvm_train(&svm, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {5, 2};
            tiku_kits_ml_linsvm_train(&svm, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {-3, -4};
            tiku_kits_ml_linsvm_train(&svm, x, -1);
        }
        {
            tiku_kits_ml_elem_t x[] = {-5, -2};
            tiku_kits_ml_linsvm_train(&svm, x, -1);
        }
    }

    /* Test well-separated points */
    {
        tiku_kits_ml_elem_t x[] = {10, 10};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        TEST_ASSERT(cls == 1, "(10,10) -> +1");
    }

    {
        tiku_kits_ml_elem_t x[] = {-10, -10};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        TEST_ASSERT(cls == -1, "(-10,-10) -> -1");
    }

    TEST_GROUP_END("LinSVM 2-Feature");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: LEARNING RATE CONFIGURATION                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_learning_rate(void)
{
    struct tiku_kits_ml_linsvm svm;
    int rc;

    TEST_GROUP_BEGIN("LinSVM Learning Rate");

    tiku_kits_ml_linsvm_init(&svm, 2, 8);

    /* Valid learning rate */
    rc = tiku_kits_ml_linsvm_set_lr(&svm, 50);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                "set_lr 50 OK");

    /* Invalid: zero */
    rc = tiku_kits_ml_linsvm_set_lr(&svm, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "lr=0 rejected");

    /* Invalid: negative */
    rc = tiku_kits_ml_linsvm_set_lr(&svm, -1);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "lr=-1 rejected");

    TEST_GROUP_END("LinSVM Learning Rate");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: LAMBDA (REGULARIZATION) CONFIGURATION                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_lambda(void)
{
    struct tiku_kits_ml_linsvm svm;
    int rc;

    TEST_GROUP_BEGIN("LinSVM Lambda");

    tiku_kits_ml_linsvm_init(&svm, 2, 8);

    /* Valid lambda */
    rc = tiku_kits_ml_linsvm_set_lambda(&svm, 10);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                "set_lambda 10 OK");

    /* Lambda = 0 is valid (no regularization) */
    rc = tiku_kits_ml_linsvm_set_lambda(&svm, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                "lambda=0 OK (no regularization)");

    /* Invalid: negative */
    rc = tiku_kits_ml_linsvm_set_lambda(&svm, -1);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "lambda=-1 rejected");

    TEST_GROUP_END("LinSVM Lambda");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: RESET                                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_reset(void)
{
    struct tiku_kits_ml_linsvm svm;
    int32_t w[3];
    int rc;

    TEST_GROUP_BEGIN("LinSVM Reset");

    tiku_kits_ml_linsvm_init(&svm, 2, 8);

    /* Train a few samples */
    {
        tiku_kits_ml_elem_t x[] = {5, 3};
        tiku_kits_ml_linsvm_train(&svm, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {-5, -3};
        tiku_kits_ml_linsvm_train(&svm, x, -1);
    }
    TEST_ASSERT(tiku_kits_ml_linsvm_count(&svm) == 2,
                "2 samples before reset");

    /* Verify weights are non-zero */
    tiku_kits_ml_linsvm_get_weights(&svm, w);
    TEST_ASSERT(w[1] != 0 || w[2] != 0,
                "weights non-zero after training");

    /* Reset */
    rc = tiku_kits_ml_linsvm_reset(&svm);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_ml_linsvm_count(&svm) == 0,
                "count 0 after reset");

    /* Verify weights are zero */
    tiku_kits_ml_linsvm_get_weights(&svm, w);
    TEST_ASSERT(w[0] == 0 && w[1] == 0 && w[2] == 0,
                "weights zero after reset");

    /* Can retrain after reset */
    {
        tiku_kits_ml_elem_t x[] = {3, 2};
        rc = tiku_kits_ml_linsvm_train(&svm, x, 1);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "train after reset OK");
    }
    TEST_ASSERT(tiku_kits_ml_linsvm_count(&svm) == 1,
                "count 1 after retrain");

    TEST_GROUP_END("LinSVM Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_linsvm_null_inputs(void)
{
    struct tiku_kits_ml_linsvm svm;
    int32_t w[3];
    int32_t margin;
    int8_t cls;
    int rc;

    TEST_GROUP_BEGIN("LinSVM NULL Inputs");

    /* init NULL */
    rc = tiku_kits_ml_linsvm_init(NULL, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "init NULL rejected");

    /* reset NULL */
    rc = tiku_kits_ml_linsvm_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "reset NULL rejected");

    tiku_kits_ml_linsvm_init(&svm, 2, 8);

    /* set_lr NULL */
    rc = tiku_kits_ml_linsvm_set_lr(NULL, 26);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_lr NULL rejected");

    /* set_lambda NULL */
    rc = tiku_kits_ml_linsvm_set_lambda(NULL, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_lambda NULL rejected");

    /* train NULL svm */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_linsvm_train(NULL, x, 1);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "train NULL svm rejected");
    }

    /* train NULL x */
    rc = tiku_kits_ml_linsvm_train(&svm, NULL, 1);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "train NULL x rejected");

    /* train invalid label */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_linsvm_train(&svm, x, 0);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                    "train label=0 rejected");
    }

    /* decision NULL */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_linsvm_decision(NULL, x, &margin);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "decision NULL svm rejected");

        rc = tiku_kits_ml_linsvm_decision(&svm, NULL, &margin);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "decision NULL x rejected");

        rc = tiku_kits_ml_linsvm_decision(&svm, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "decision NULL result rejected");
    }

    /* predict NULL */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_linsvm_predict(NULL, x, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL svm rejected");

        rc = tiku_kits_ml_linsvm_predict(&svm, NULL, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL x rejected");

        rc = tiku_kits_ml_linsvm_predict(&svm, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL result rejected");
    }

    /* get_weights NULL */
    rc = tiku_kits_ml_linsvm_get_weights(NULL, w);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_weights NULL svm rejected");

    rc = tiku_kits_ml_linsvm_get_weights(&svm, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_weights NULL weights rejected");

    /* set_weights NULL */
    rc = tiku_kits_ml_linsvm_set_weights(NULL, w);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_weights NULL svm rejected");

    rc = tiku_kits_ml_linsvm_set_weights(&svm, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_weights NULL weights rejected");

    /* utility NULL */
    TEST_ASSERT(tiku_kits_ml_linsvm_count(NULL) == 0,
                "count NULL returns 0");

    TEST_GROUP_END("LinSVM NULL Inputs");
}
