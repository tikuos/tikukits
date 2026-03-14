/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ml_logreg.c - Logistic regression library tests
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

void test_kits_ml_logreg_init(void)
{
    struct tiku_kits_ml_logreg lg;
    int rc;

    TEST_GROUP_BEGIN("LogReg Init");

    rc = tiku_kits_ml_logreg_init(&lg, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "init returns OK");
    TEST_ASSERT(tiku_kits_ml_logreg_count(&lg) == 0,
                "initial count is 0");

    /* Invalid: 0 features */
    rc = tiku_kits_ml_logreg_init(&lg, 0, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "0 features rejected");

    /* Invalid: too many features */
    rc = tiku_kits_ml_logreg_init(
        &lg, TIKU_KITS_ML_LOGREG_MAX_FEATURES + 1, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "too many features rejected");

    /* Invalid: shift = 0 */
    rc = tiku_kits_ml_logreg_init(&lg, 2, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=0 rejected");

    /* Invalid: shift = 31 */
    rc = tiku_kits_ml_logreg_init(&lg, 2, 31);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=31 rejected");

    TEST_GROUP_END("LogReg Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: PRE-TRAINED WEIGHTS (INFERENCE ONLY)                              */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_pretrained(void)
{
    struct tiku_kits_ml_logreg lg;
    int32_t weights[3]; /* bias + 2 features */
    int32_t prob_q;
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("LogReg Pre-trained Weights");

    tiku_kits_ml_logreg_init(&lg, 2, 8);

    /*
     * Set weights for a known decision boundary.
     * Model: sigmoid(128 + 256*x1 - 256*x2)
     *   bias = 128 (0.5 in Q8)
     *   w1 = 256  (1.0 in Q8)
     *   w2 = -256 (-1.0 in Q8)
     *
     * Decision: class 1 when 0.5 + x1 - x2 > 0, i.e. x1 > x2 - 0.5
     */
    weights[0] = 128;   /* bias */
    weights[1] = 256;   /* w1 */
    weights[2] = -256;  /* w2 */
    rc = tiku_kits_ml_logreg_set_weights(&lg, weights);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "set_weights OK");

    /* Verify weights can be read back */
    {
        int32_t w_out[3];
        tiku_kits_ml_logreg_get_weights(&lg, w_out);
        TEST_ASSERT(w_out[0] == 128, "bias read back correctly");
        TEST_ASSERT(w_out[1] == 256, "w1 read back correctly");
        TEST_ASSERT(w_out[2] == -256, "w2 read back correctly");
    }

    /* x1=5, x2=0 -> z = 128 + 256*5 - 256*0 = 1408 -> sigmoid >> 0.5 */
    {
        tiku_kits_ml_elem_t x[] = {5, 0};
        tiku_kits_ml_logreg_predict(&lg, x, &cls);
        TEST_ASSERT(cls == 1, "x1=5,x2=0 -> class 1");
        tiku_kits_ml_logreg_predict_proba(&lg, x, &prob_q);
        TEST_ASSERT(prob_q > (1 << 7), "prob > 0.5 for class 1");
    }

    /* x1=0, x2=5 -> z = 128 + 0 - 1280 = -1152 -> sigmoid << 0.5 */
    {
        tiku_kits_ml_elem_t x[] = {0, 5};
        tiku_kits_ml_logreg_predict(&lg, x, &cls);
        TEST_ASSERT(cls == 0, "x1=0,x2=5 -> class 0");
        tiku_kits_ml_logreg_predict_proba(&lg, x, &prob_q);
        TEST_ASSERT(prob_q < (1 << 7), "prob < 0.5 for class 0");
    }

    TEST_GROUP_END("LogReg Pre-trained Weights");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: SIGMOID SATURATION                                                */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_sigmoid_saturation(void)
{
    struct tiku_kits_ml_logreg lg;
    int32_t weights[2]; /* bias + 1 feature */
    int32_t prob_q;

    TEST_GROUP_BEGIN("LogReg Sigmoid Saturation");

    tiku_kits_ml_logreg_init(&lg, 1, 8);

    /* Large positive weight -> saturate at 1.0 */
    weights[0] = 0;
    weights[1] = 2560; /* 10.0 in Q8 */
    tiku_kits_ml_logreg_set_weights(&lg, weights);

    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_logreg_predict_proba(&lg, x, &prob_q);
        /* z = 25600, well past saturation at 4<<8 = 1024 */
        TEST_ASSERT(prob_q == 256,
                    "large positive z saturates at 1.0 (256 Q8)");
    }

    /* Large negative weight -> saturate at 0.0 */
    weights[1] = -2560;
    tiku_kits_ml_logreg_set_weights(&lg, weights);

    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_logreg_predict_proba(&lg, x, &prob_q);
        TEST_ASSERT(prob_q == 0,
                    "large negative z saturates at 0.0");
    }

    TEST_GROUP_END("LogReg Sigmoid Saturation");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: SIGMOID MIDPOINT                                                  */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_sigmoid_midpoint(void)
{
    struct tiku_kits_ml_logreg lg;
    int32_t weights[2]; /* bias + 1 feature */
    int32_t prob_q;

    TEST_GROUP_BEGIN("LogReg Sigmoid Midpoint");

    tiku_kits_ml_logreg_init(&lg, 1, 8);

    /* z = 0 -> sigmoid = 0.5 */
    weights[0] = 0;
    weights[1] = 0;
    tiku_kits_ml_logreg_set_weights(&lg, weights);

    {
        tiku_kits_ml_elem_t x[] = {100};
        tiku_kits_ml_logreg_predict_proba(&lg, x, &prob_q);
        /* z = 0, so sigmoid = 0.5 = 128 in Q8 */
        TEST_ASSERT(prob_q == 128,
                    "z=0 gives sigmoid=0.5 (128 Q8)");
    }

    TEST_GROUP_END("LogReg Sigmoid Midpoint");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: ONLINE TRAINING (LINEARLY SEPARABLE DATA)                         */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_training(void)
{
    struct tiku_kits_ml_logreg lg;
    uint8_t cls;
    uint16_t epoch, i;
    uint8_t correct;

    /*
     * Linearly separable 1-D data:
     *   class 0: x < 0
     *   class 1: x > 0
     *
     * Train with several epochs of SGD. After training, the model
     * should correctly classify points well away from the boundary.
     */
    static const tiku_kits_ml_elem_t x_train[][1] = {
        {-10}, {-8}, {-6}, {-4}, {-2},
        {2}, {4}, {6}, {8}, {10}
    };
    static const uint8_t y_train[] = {
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 1
    };
    const uint16_t n_samples = 10;

    TEST_GROUP_BEGIN("LogReg Training");

    tiku_kits_ml_logreg_init(&lg, 1, 8);
    tiku_kits_ml_logreg_set_lr(&lg, 13); /* ~0.05 in Q8 */

    /* Train for multiple epochs */
    for (epoch = 0; epoch < 20; epoch++) {
        for (i = 0; i < n_samples; i++) {
            tiku_kits_ml_logreg_train(&lg, x_train[i], y_train[i]);
        }
    }

    TEST_ASSERT(tiku_kits_ml_logreg_count(&lg) == 200,
                "count is 200 after 20 epochs of 10 samples");

    /* Test: classify points far from boundary */
    correct = 0;

    {
        tiku_kits_ml_elem_t xt[] = {-7};
        tiku_kits_ml_logreg_predict(&lg, xt, &cls);
        if (cls == 0) {
            correct++;
        }
    }
    {
        tiku_kits_ml_elem_t xt[] = {-3};
        tiku_kits_ml_logreg_predict(&lg, xt, &cls);
        if (cls == 0) {
            correct++;
        }
    }
    {
        tiku_kits_ml_elem_t xt[] = {3};
        tiku_kits_ml_logreg_predict(&lg, xt, &cls);
        if (cls == 1) {
            correct++;
        }
    }
    {
        tiku_kits_ml_elem_t xt[] = {7};
        tiku_kits_ml_logreg_predict(&lg, xt, &cls);
        if (cls == 1) {
            correct++;
        }
    }

    TEST_ASSERT(correct >= 3,
                "at least 3/4 test points classified correctly");

    TEST_GROUP_END("LogReg Training");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: TWO-FEATURE TRAINING                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_two_features(void)
{
    struct tiku_kits_ml_logreg lg;
    uint8_t cls;
    uint16_t epoch, i;
    uint8_t correct;

    /*
     * 2-D linearly separable data:
     *   class 1 when x1 + x2 > 0
     *   class 0 when x1 + x2 < 0
     */
    static const tiku_kits_ml_elem_t x_train[][2] = {
        {-5, -5}, {-3, -4}, {-6, -2}, {-4, -3}, {-7, -1},
        {5, 5}, {3, 4}, {6, 2}, {4, 3}, {7, 1}
    };
    static const uint8_t y_train[] = {
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 1
    };
    const uint16_t n_samples = 10;

    TEST_GROUP_BEGIN("LogReg Two Features");

    tiku_kits_ml_logreg_init(&lg, 2, 8);
    tiku_kits_ml_logreg_set_lr(&lg, 13); /* ~0.05 in Q8 */

    for (epoch = 0; epoch < 30; epoch++) {
        for (i = 0; i < n_samples; i++) {
            tiku_kits_ml_logreg_train(&lg, x_train[i], y_train[i]);
        }
    }

    correct = 0;

    {
        tiku_kits_ml_elem_t xt[] = {-4, -4};
        tiku_kits_ml_logreg_predict(&lg, xt, &cls);
        if (cls == 0) {
            correct++;
        }
    }
    {
        tiku_kits_ml_elem_t xt[] = {4, 4};
        tiku_kits_ml_logreg_predict(&lg, xt, &cls);
        if (cls == 1) {
            correct++;
        }
    }
    {
        tiku_kits_ml_elem_t xt[] = {-6, -1};
        tiku_kits_ml_logreg_predict(&lg, xt, &cls);
        if (cls == 0) {
            correct++;
        }
    }
    {
        tiku_kits_ml_elem_t xt[] = {6, 1};
        tiku_kits_ml_logreg_predict(&lg, xt, &cls);
        if (cls == 1) {
            correct++;
        }
    }

    TEST_ASSERT(correct >= 3,
                "at least 3/4 2-D test points correct");

    TEST_GROUP_END("LogReg Two Features");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: RESET                                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_reset(void)
{
    struct tiku_kits_ml_logreg lg;
    int32_t weights[2];
    int rc;

    TEST_GROUP_BEGIN("LogReg Reset");

    tiku_kits_ml_logreg_init(&lg, 1, 8);

    /* Train a bit so weights are non-zero */
    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_logreg_train(&lg, x, 1);
        tiku_kits_ml_logreg_train(&lg, x, 1);
    }
    TEST_ASSERT(tiku_kits_ml_logreg_count(&lg) == 2,
                "count is 2 before reset");

    /* Reset */
    rc = tiku_kits_ml_logreg_reset(&lg);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_ml_logreg_count(&lg) == 0,
                "count is 0 after reset");

    /* Verify weights are zeroed */
    tiku_kits_ml_logreg_get_weights(&lg, weights);
    TEST_ASSERT(weights[0] == 0, "bias is 0 after reset");
    TEST_ASSERT(weights[1] == 0, "w1 is 0 after reset");

    TEST_GROUP_END("LogReg Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: LEARNING RATE CONFIGURATION                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_learning_rate(void)
{
    struct tiku_kits_ml_logreg lg;
    int rc;

    TEST_GROUP_BEGIN("LogReg Learning Rate");

    tiku_kits_ml_logreg_init(&lg, 1, 8);

    /* Valid learning rate */
    rc = tiku_kits_ml_logreg_set_lr(&lg, 26);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "set_lr(26) OK");

    /* Invalid: learning rate = 0 */
    rc = tiku_kits_ml_logreg_set_lr(&lg, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "lr=0 rejected");

    /* Invalid: negative learning rate */
    rc = tiku_kits_ml_logreg_set_lr(&lg, -5);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "lr<0 rejected");

    TEST_GROUP_END("LogReg Learning Rate");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_logreg_null_inputs(void)
{
    struct tiku_kits_ml_logreg lg;
    int32_t prob_q;
    int32_t weights[3];
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("LogReg NULL Inputs");

    /* Init with NULL */
    rc = tiku_kits_ml_logreg_init(NULL, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "init NULL rejected");

    /* Reset with NULL */
    rc = tiku_kits_ml_logreg_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "reset NULL rejected");

    /* set_lr with NULL */
    rc = tiku_kits_ml_logreg_set_lr(NULL, 26);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_lr NULL rejected");

    tiku_kits_ml_logreg_init(&lg, 2, 8);

    /* Train with NULL x */
    rc = tiku_kits_ml_logreg_train(&lg, NULL, 1);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "train NULL x rejected");

    /* Train with NULL model */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_logreg_train(NULL, x, 1);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "train NULL lg rejected");
    }

    /* Train with invalid y */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_logreg_train(&lg, x, 2);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                    "train y=2 rejected");
    }

    /* predict_proba NULL checks */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_logreg_predict_proba(NULL, x, &prob_q);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_proba NULL lg rejected");

        rc = tiku_kits_ml_logreg_predict_proba(&lg, NULL, &prob_q);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_proba NULL x rejected");

        rc = tiku_kits_ml_logreg_predict_proba(&lg, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_proba NULL result rejected");
    }

    /* predict NULL checks */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_logreg_predict(NULL, x, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL lg rejected");

        rc = tiku_kits_ml_logreg_predict(&lg, NULL, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL x rejected");

        rc = tiku_kits_ml_logreg_predict(&lg, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL result rejected");
    }

    /* Weight access NULL checks */
    rc = tiku_kits_ml_logreg_get_weights(NULL, weights);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_weights NULL lg rejected");

    rc = tiku_kits_ml_logreg_get_weights(&lg, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_weights NULL out rejected");

    rc = tiku_kits_ml_logreg_set_weights(NULL, weights);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_weights NULL lg rejected");

    rc = tiku_kits_ml_logreg_set_weights(&lg, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_weights NULL in rejected");

    /* Count with NULL */
    TEST_ASSERT(tiku_kits_ml_logreg_count(NULL) == 0,
                "count NULL returns 0");

    TEST_GROUP_END("LogReg NULL Inputs");
}
