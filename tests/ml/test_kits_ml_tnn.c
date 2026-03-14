/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ml_tnn.c - Tiny Neural Network classifier tests
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

void test_kits_ml_tnn_init(void)
{
    struct tiku_kits_ml_tnn net;
    int rc;

    TEST_GROUP_BEGIN("TNN Init");

    rc = tiku_kits_ml_tnn_init(&net, 2, 4, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "init returns OK");
    TEST_ASSERT(tiku_kits_ml_tnn_count(&net) == 0,
                "initial count is 0");

    /* Invalid: 0 inputs */
    rc = tiku_kits_ml_tnn_init(&net, 0, 4, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "0 inputs rejected");

    /* Invalid: too many inputs */
    rc = tiku_kits_ml_tnn_init(
        &net, TIKU_KITS_ML_TNN_MAX_INPUT + 1, 4, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "too many inputs rejected");

    /* Invalid: 0 hidden */
    rc = tiku_kits_ml_tnn_init(&net, 2, 0, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "0 hidden rejected");

    /* Invalid: 1 output (need >= 2 classes) */
    rc = tiku_kits_ml_tnn_init(&net, 2, 4, 1, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "1 output rejected");

    /* Invalid: shift = 0 */
    rc = tiku_kits_ml_tnn_init(&net, 2, 4, 2, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=0 rejected");

    TEST_GROUP_END("TNN Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: PRE-TRAINED WEIGHTS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_pretrained(void)
{
    struct tiku_kits_ml_tnn net;
    uint8_t cls;

    TEST_GROUP_BEGIN("TNN Pretrained");

    /*
     * 1 input, 2 hidden, 2 outputs.
     * Hidden layer: neuron 0 = ReLU(x), neuron 1 = ReLU(-x)
     * Output: class 0 = h0 - h1, class 1 = h1 - h0
     * x > 0 -> h0 active -> class 0
     * x < 0 -> h1 active -> class 1
     */
    tiku_kits_ml_tnn_init(&net, 1, 2, 2, 8);

    /* Hidden weights: [bias, w1] for each neuron */
    {
        int32_t wh[] = {
            0, 256,     /* neuron 0: ReLU(x), w=1.0 in Q8 */
            0, -256     /* neuron 1: ReLU(-x), w=-1.0 in Q8 */
        };
        tiku_kits_ml_tnn_set_weights(&net, 0, wh);
    }

    /* Output weights: [bias, w_h0, w_h1] for each class */
    {
        int32_t wo[] = {
            0, 256, -256,   /* class 0: h0 - h1 */
            0, -256, 256    /* class 1: h1 - h0 */
        };
        tiku_kits_ml_tnn_set_weights(&net, 1, wo);
    }

    /* x = 5 -> class 0 */
    {
        tiku_kits_ml_elem_t x[] = {5};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        TEST_ASSERT(cls == 0, "x=5 -> class 0");
    }

    /* x = -5 -> class 1 */
    {
        tiku_kits_ml_elem_t x[] = {-5};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        TEST_ASSERT(cls == 1, "x=-5 -> class 1");
    }

    TEST_GROUP_END("TNN Pretrained");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: FORWARD PASS (RAW SCORES)                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_forward_pass(void)
{
    struct tiku_kits_ml_tnn net;
    int32_t scores[2];
    int rc;

    TEST_GROUP_BEGIN("TNN Forward");

    /* Same pretrained network as test 2 */
    tiku_kits_ml_tnn_init(&net, 1, 2, 2, 8);
    {
        int32_t wh[] = {0, 256, 0, -256};
        tiku_kits_ml_tnn_set_weights(&net, 0, wh);
    }
    {
        int32_t wo[] = {0, 256, -256, 0, -256, 256};
        tiku_kits_ml_tnn_set_weights(&net, 1, wo);
    }

    /* x = 3: h0 = ReLU(768) = 768, h1 = ReLU(-768) = 0
     * o0 = (256*768)>>8 = 768, o1 = (-256*768)>>8 = -768 */
    {
        tiku_kits_ml_elem_t x[] = {3};
        rc = tiku_kits_ml_tnn_forward(&net, x, scores);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "forward returns OK");
        TEST_ASSERT(scores[0] > 0,
                    "class 0 score positive for x=3");
        TEST_ASSERT(scores[1] < 0,
                    "class 1 score negative for x=3");
        TEST_ASSERT(scores[0] > scores[1],
                    "class 0 score > class 1 for x=3");
    }

    TEST_GROUP_END("TNN Forward");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: TRAINING CONVERGENCE                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_training(void)
{
    struct tiku_kits_ml_tnn net;
    uint8_t cls;
    uint8_t i;

    TEST_GROUP_BEGIN("TNN Training");

    /*
     * Train 2-input, 4-hidden, 2-output network.
     * Class 0: both inputs positive (5,5), (3,4)
     * Class 1: both inputs negative (-5,-5), (-3,-4)
     */
    tiku_kits_ml_tnn_init(&net, 2, 4, 2, 8);
    tiku_kits_ml_tnn_set_lr(&net, 13); /* ~0.05 in Q8 */

    for (i = 0; i < 50; i++) {
        {
            tiku_kits_ml_elem_t x[] = {5, 5};
            tiku_kits_ml_tnn_train(&net, x, 0);
        }
        {
            tiku_kits_ml_elem_t x[] = {3, 4};
            tiku_kits_ml_tnn_train(&net, x, 0);
        }
        {
            tiku_kits_ml_elem_t x[] = {-5, -5};
            tiku_kits_ml_tnn_train(&net, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {-3, -4};
            tiku_kits_ml_tnn_train(&net, x, 1);
        }
    }

    TEST_ASSERT(tiku_kits_ml_tnn_count(&net) == 200,
                "200 samples trained");

    /* Test well-separated points */
    {
        tiku_kits_ml_elem_t x[] = {10, 10};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        TEST_ASSERT(cls == 0, "(10,10) -> class 0");
    }

    {
        tiku_kits_ml_elem_t x[] = {-10, -10};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        TEST_ASSERT(cls == 1, "(-10,-10) -> class 1");
    }

    TEST_GROUP_END("TNN Training");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: THREE-CLASS CLASSIFICATION                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_three_class(void)
{
    struct tiku_kits_ml_tnn net;
    uint8_t cls;
    uint8_t i;

    TEST_GROUP_BEGIN("TNN 3-Class");

    /*
     * 1-input, 4-hidden, 3-output network.
     * Class 0: x around -10
     * Class 1: x around 0
     * Class 2: x around +10
     */
    tiku_kits_ml_tnn_init(&net, 1, 4, 3, 8);
    tiku_kits_ml_tnn_set_lr(&net, 6); /* ~0.025 in Q8 */

    for (i = 0; i < 80; i++) {
        {
            tiku_kits_ml_elem_t x[] = {-10};
            tiku_kits_ml_tnn_train(&net, x, 0);
        }
        {
            tiku_kits_ml_elem_t x[] = {-8};
            tiku_kits_ml_tnn_train(&net, x, 0);
        }
        {
            tiku_kits_ml_elem_t x[] = {0};
            tiku_kits_ml_tnn_train(&net, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {1};
            tiku_kits_ml_tnn_train(&net, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {10};
            tiku_kits_ml_tnn_train(&net, x, 2);
        }
        {
            tiku_kits_ml_elem_t x[] = {8};
            tiku_kits_ml_tnn_train(&net, x, 2);
        }
    }

    /* Test class 0 */
    {
        tiku_kits_ml_elem_t x[] = {-12};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        TEST_ASSERT(cls == 0, "x=-12 -> class 0");
    }

    /* Test class 2 */
    {
        tiku_kits_ml_elem_t x[] = {12};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        TEST_ASSERT(cls == 2, "x=12 -> class 2");
    }

    TEST_GROUP_END("TNN 3-Class");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: LEARNING RATE CONFIGURATION                                       */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_learning_rate(void)
{
    struct tiku_kits_ml_tnn net;
    int rc;

    TEST_GROUP_BEGIN("TNN Learning Rate");

    tiku_kits_ml_tnn_init(&net, 2, 4, 2, 8);

    /* Valid learning rate */
    rc = tiku_kits_ml_tnn_set_lr(&net, 50);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                "set_lr 50 OK");

    /* Invalid: zero */
    rc = tiku_kits_ml_tnn_set_lr(&net, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "lr=0 rejected");

    /* Invalid: negative */
    rc = tiku_kits_ml_tnn_set_lr(&net, -1);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "lr=-1 rejected");

    TEST_GROUP_END("TNN Learning Rate");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: WEIGHT ACCESS                                                     */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_weight_access(void)
{
    struct tiku_kits_ml_tnn net;
    int32_t wh_in[6];   /* 2 hidden * (1 input + 1 bias) = 4, but 6 for 2*(2+1) */
    int32_t wh_out[6];
    int32_t wo_in[6];   /* 2 output * (2 hidden + 1 bias) = 6 */
    int32_t wo_out[6];
    int rc;

    TEST_GROUP_BEGIN("TNN Weight Access");

    tiku_kits_ml_tnn_init(&net, 2, 2, 2, 8);

    /* Set hidden weights: 2 neurons * (2 inputs + 1 bias) = 6 */
    wh_in[0] = 10;   wh_in[1] = 20;   wh_in[2] = 30;
    wh_in[3] = 40;   wh_in[4] = 50;   wh_in[5] = 60;
    rc = tiku_kits_ml_tnn_set_weights(&net, 0, wh_in);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "set hidden OK");

    /* Get hidden weights and verify round-trip */
    rc = tiku_kits_ml_tnn_get_weights(&net, 0, wh_out);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "get hidden OK");
    TEST_ASSERT(wh_out[0] == 10 && wh_out[1] == 20
                && wh_out[2] == 30,
                "hidden neuron 0 round-trip");
    TEST_ASSERT(wh_out[3] == 40 && wh_out[4] == 50
                && wh_out[5] == 60,
                "hidden neuron 1 round-trip");

    /* Set output weights: 2 outputs * (2 hidden + 1 bias) = 6 */
    wo_in[0] = 100;  wo_in[1] = 200;  wo_in[2] = 300;
    wo_in[3] = 400;  wo_in[4] = 500;  wo_in[5] = 600;
    rc = tiku_kits_ml_tnn_set_weights(&net, 1, wo_in);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "set output OK");

    rc = tiku_kits_ml_tnn_get_weights(&net, 1, wo_out);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "get output OK");
    TEST_ASSERT(wo_out[0] == 100 && wo_out[3] == 400,
                "output round-trip");

    /* Invalid layer */
    rc = tiku_kits_ml_tnn_get_weights(&net, 2, wh_out);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "layer=2 rejected");

    TEST_GROUP_END("TNN Weight Access");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: RESET                                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_reset(void)
{
    struct tiku_kits_ml_tnn net;
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("TNN Reset");

    tiku_kits_ml_tnn_init(&net, 2, 4, 2, 8);

    /* Train a few samples */
    {
        tiku_kits_ml_elem_t x[] = {5, 5};
        tiku_kits_ml_tnn_train(&net, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {-5, -5};
        tiku_kits_ml_tnn_train(&net, x, 1);
    }
    TEST_ASSERT(tiku_kits_ml_tnn_count(&net) == 2,
                "2 samples before reset");

    /* Reset */
    rc = tiku_kits_ml_tnn_reset(&net);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_ml_tnn_count(&net) == 0,
                "count 0 after reset");

    /* Can retrain after reset */
    {
        tiku_kits_ml_elem_t x[] = {3, 2};
        rc = tiku_kits_ml_tnn_train(&net, x, 0);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "train after reset OK");
    }
    TEST_ASSERT(tiku_kits_ml_tnn_count(&net) == 1,
                "count 1 after retrain");

    TEST_GROUP_END("TNN Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_tnn_null_inputs(void)
{
    struct tiku_kits_ml_tnn net;
    int32_t scores[2];
    int32_t w[6];
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("TNN NULL Inputs");

    /* init NULL */
    rc = tiku_kits_ml_tnn_init(NULL, 2, 4, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "init NULL rejected");

    /* reset NULL */
    rc = tiku_kits_ml_tnn_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "reset NULL rejected");

    tiku_kits_ml_tnn_init(&net, 2, 2, 2, 8);

    /* set_lr NULL */
    rc = tiku_kits_ml_tnn_set_lr(NULL, 13);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_lr NULL rejected");

    /* train NULL */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_tnn_train(NULL, x, 0);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "train NULL net rejected");
    }
    rc = tiku_kits_ml_tnn_train(&net, NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "train NULL x rejected");

    /* train invalid label */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_tnn_train(&net, x, 2);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                    "train label >= n_output rejected");
    }

    /* forward NULL */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_tnn_forward(NULL, x, scores);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "forward NULL net rejected");

        rc = tiku_kits_ml_tnn_forward(&net, NULL, scores);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "forward NULL x rejected");

        rc = tiku_kits_ml_tnn_forward(&net, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "forward NULL scores rejected");
    }

    /* predict NULL */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_tnn_predict(NULL, x, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL net rejected");

        rc = tiku_kits_ml_tnn_predict(&net, NULL, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL x rejected");

        rc = tiku_kits_ml_tnn_predict(&net, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL result rejected");
    }

    /* get_weights NULL */
    rc = tiku_kits_ml_tnn_get_weights(NULL, 0, w);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_weights NULL net rejected");

    rc = tiku_kits_ml_tnn_get_weights(&net, 0, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_weights NULL weights rejected");

    /* set_weights NULL */
    rc = tiku_kits_ml_tnn_set_weights(NULL, 0, w);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_weights NULL net rejected");

    rc = tiku_kits_ml_tnn_set_weights(&net, 0, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_weights NULL weights rejected");

    /* utility NULL */
    TEST_ASSERT(tiku_kits_ml_tnn_count(NULL) == 0,
                "count NULL returns 0");

    TEST_GROUP_END("TNN NULL Inputs");
}
