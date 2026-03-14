/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ml_nbayes.c - Categorical Naive Bayes classifier tests
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

void test_kits_ml_nbayes_init(void)
{
    struct tiku_kits_ml_nbayes nb;
    int rc;

    TEST_GROUP_BEGIN("NBayes Init");

    rc = tiku_kits_ml_nbayes_init(&nb, 2, 4, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "init returns OK");
    TEST_ASSERT(tiku_kits_ml_nbayes_count(&nb) == 0,
                "initial count is 0");

    /* Invalid: 0 features */
    rc = tiku_kits_ml_nbayes_init(&nb, 0, 4, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "0 features rejected");

    /* Invalid: too many features */
    rc = tiku_kits_ml_nbayes_init(
        &nb, TIKU_KITS_ML_NBAYES_MAX_FEATURES + 1, 4, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "too many features rejected");

    /* Invalid: 1 bin (need at least 2) */
    rc = tiku_kits_ml_nbayes_init(&nb, 2, 1, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "1 bin rejected");

    /* Invalid: 1 class (need at least 2) */
    rc = tiku_kits_ml_nbayes_init(&nb, 2, 4, 1, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "1 class rejected");

    /* Invalid: shift = 0 */
    rc = tiku_kits_ml_nbayes_init(&nb, 2, 4, 2, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=0 rejected");

    TEST_GROUP_END("NBayes Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: SIMPLE BINARY CLASSIFICATION                                      */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_simple(void)
{
    struct tiku_kits_ml_nbayes nb;
    uint8_t cls;

    TEST_GROUP_BEGIN("NBayes Simple Binary");

    /*
     * 1 feature, 4 bins, 2 classes.
     * Class 0: feature in bin 0 or 1
     * Class 1: feature in bin 2 or 3
     */
    tiku_kits_ml_nbayes_init(&nb, 1, 4, 2, 8);

    /* Train class 0 with low bins */
    {
        uint8_t x[] = {0};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    {
        uint8_t x[] = {0};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    {
        uint8_t x[] = {1};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }

    /* Train class 1 with high bins */
    {
        uint8_t x[] = {2};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t x[] = {3};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t x[] = {3};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }

    TEST_ASSERT(tiku_kits_ml_nbayes_count(&nb) == 6,
                "6 samples trained");
    TEST_ASSERT(tiku_kits_ml_nbayes_class_count(&nb, 0) == 3,
                "class 0 has 3 samples");
    TEST_ASSERT(tiku_kits_ml_nbayes_class_count(&nb, 1) == 3,
                "class 1 has 3 samples");

    /* Query bin 0 -> class 0 */
    {
        uint8_t q[] = {0};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 0, "bin=0 -> class 0");
    }

    /* Query bin 3 -> class 1 */
    {
        uint8_t q[] = {3};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 1, "bin=3 -> class 1");
    }

    TEST_GROUP_END("NBayes Simple Binary");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: TWO-FEATURE CLASSIFICATION                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_two_features(void)
{
    struct tiku_kits_ml_nbayes nb;
    uint8_t cls;

    TEST_GROUP_BEGIN("NBayes 2-Feature");

    /*
     * 2 features, 4 bins, 2 classes.
     * Class 0: both features in low bins (0,1)
     * Class 1: both features in high bins (2,3)
     */
    tiku_kits_ml_nbayes_init(&nb, 2, 4, 2, 8);

    /* Class 0: low-low patterns */
    {
        uint8_t x[] = {0, 0};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    {
        uint8_t x[] = {0, 1};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    {
        uint8_t x[] = {1, 0};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    {
        uint8_t x[] = {1, 1};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }

    /* Class 1: high-high patterns */
    {
        uint8_t x[] = {2, 2};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t x[] = {2, 3};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t x[] = {3, 2};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t x[] = {3, 3};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }

    /* Query low-low -> class 0 */
    {
        uint8_t q[] = {0, 0};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 0, "(0,0) -> class 0");
    }

    /* Query high-high -> class 1 */
    {
        uint8_t q[] = {3, 3};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 1, "(3,3) -> class 1");
    }

    /* Query low-low variant -> class 0 */
    {
        uint8_t q[] = {1, 0};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 0, "(1,0) -> class 0");
    }

    TEST_GROUP_END("NBayes 2-Feature");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: THREE-CLASS CLASSIFICATION                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_three_class(void)
{
    struct tiku_kits_ml_nbayes nb;
    uint8_t cls;

    TEST_GROUP_BEGIN("NBayes 3-Class");

    /*
     * 1 feature, 6 bins, 3 classes.
     * Class 0: bins 0-1
     * Class 1: bins 2-3
     * Class 2: bins 4-5
     */
    tiku_kits_ml_nbayes_init(&nb, 1, 6, 3, 8);

    /* Class 0 */
    {
        uint8_t x[] = {0};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    {
        uint8_t x[] = {1};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }

    /* Class 1 */
    {
        uint8_t x[] = {2};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t x[] = {3};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }

    /* Class 2 */
    {
        uint8_t x[] = {4};
        tiku_kits_ml_nbayes_train(&nb, x, 2);
        tiku_kits_ml_nbayes_train(&nb, x, 2);
    }
    {
        uint8_t x[] = {5};
        tiku_kits_ml_nbayes_train(&nb, x, 2);
    }

    /* Query each class's region */
    {
        uint8_t q[] = {0};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 0, "bin=0 -> class 0");
    }
    {
        uint8_t q[] = {2};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 1, "bin=2 -> class 1");
    }
    {
        uint8_t q[] = {4};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 2, "bin=4 -> class 2");
    }

    TEST_GROUP_END("NBayes 3-Class");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: LOG LIKELIHOOD SCORES                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_log_proba(void)
{
    struct tiku_kits_ml_nbayes nb;
    int32_t scores[TIKU_KITS_ML_NBAYES_MAX_CLASSES];
    int rc;

    TEST_GROUP_BEGIN("NBayes Log Proba");

    tiku_kits_ml_nbayes_init(&nb, 1, 4, 2, 8);

    /* Train class 0 heavily on bin 0 */
    {
        uint8_t x[] = {0};
        uint8_t i;
        for (i = 0; i < 10; i++) {
            tiku_kits_ml_nbayes_train(&nb, x, 0);
        }
    }

    /* Train class 1 on bin 3 */
    {
        uint8_t x[] = {3};
        uint8_t i;
        for (i = 0; i < 10; i++) {
            tiku_kits_ml_nbayes_train(&nb, x, 1);
        }
    }

    /* Query bin 0: class 0 should have higher score */
    {
        uint8_t q[] = {0};
        rc = tiku_kits_ml_nbayes_predict_log_proba(&nb, q, scores);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "predict_log_proba returns OK");
        TEST_ASSERT(scores[0] > scores[1],
                    "bin=0: class 0 score > class 1 score");
    }

    /* Query bin 3: class 1 should have higher score */
    {
        uint8_t q[] = {3};
        tiku_kits_ml_nbayes_predict_log_proba(&nb, q, scores);
        TEST_ASSERT(scores[1] > scores[0],
                    "bin=3: class 1 score > class 0 score");
    }

    TEST_GROUP_END("NBayes Log Proba");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: LAPLACE SMOOTHING                                                 */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_smoothing(void)
{
    struct tiku_kits_ml_nbayes nb;
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("NBayes Laplace Smoothing");

    /*
     * Test that unseen feature values don't cause zero-probability
     * collapse due to Laplace smoothing (+1 in numerator).
     *
     * Train only on bin 0 for class 0 and bin 1 for class 1.
     * Query with bin 2 (unseen by both classes): should still
     * produce a valid prediction based on prior probabilities.
     */
    tiku_kits_ml_nbayes_init(&nb, 1, 4, 2, 8);

    /* Class 0: 5 samples, all bin 0 */
    {
        uint8_t x[] = {0};
        uint8_t i;
        for (i = 0; i < 5; i++) {
            tiku_kits_ml_nbayes_train(&nb, x, 0);
        }
    }

    /* Class 1: 3 samples, all bin 1 */
    {
        uint8_t x[] = {1};
        uint8_t i;
        for (i = 0; i < 3; i++) {
            tiku_kits_ml_nbayes_train(&nb, x, 1);
        }
    }

    /* Query unseen bin 2: should predict class 0 (higher prior) */
    {
        uint8_t q[] = {2};
        rc = tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "predict on unseen bin OK");
        TEST_ASSERT(cls == 0,
                    "unseen bin -> class 0 (higher prior)");
    }

    TEST_GROUP_END("NBayes Laplace Smoothing");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: IMBALANCED CLASSES                                                */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_imbalanced(void)
{
    struct tiku_kits_ml_nbayes nb;
    uint8_t cls;

    TEST_GROUP_BEGIN("NBayes Imbalanced");

    /*
     * Class 0 has many more samples than class 1.
     * Both classes have the same feature distribution for the
     * queried bin. The prior should dominate.
     */
    tiku_kits_ml_nbayes_init(&nb, 1, 4, 2, 8);

    /* Class 0: 20 samples spread across bins */
    {
        uint8_t i;
        for (i = 0; i < 20; i++) {
            uint8_t x[] = {(uint8_t)(i % 4)};
            tiku_kits_ml_nbayes_train(&nb, x, 0);
        }
    }

    /* Class 1: 2 samples, also spread */
    {
        uint8_t x0[] = {0};
        uint8_t x1[] = {1};
        tiku_kits_ml_nbayes_train(&nb, x0, 1);
        tiku_kits_ml_nbayes_train(&nb, x1, 1);
    }

    TEST_ASSERT(tiku_kits_ml_nbayes_class_count(&nb, 0) == 20,
                "class 0 has 20 samples");
    TEST_ASSERT(tiku_kits_ml_nbayes_class_count(&nb, 1) == 2,
                "class 1 has 2 samples");

    /* Query bin 0: class 0 should win due to prior */
    {
        uint8_t q[] = {0};
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(cls == 0,
                    "imbalanced: class 0 wins on prior");
    }

    TEST_GROUP_END("NBayes Imbalanced");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: RESET                                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_reset(void)
{
    struct tiku_kits_ml_nbayes nb;
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("NBayes Reset");

    tiku_kits_ml_nbayes_init(&nb, 1, 4, 2, 8);

    {
        uint8_t x[] = {0};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    TEST_ASSERT(tiku_kits_ml_nbayes_count(&nb) == 1,
                "1 sample before reset");

    /* Predict works before reset */
    {
        uint8_t q[] = {0};
        rc = tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "predict before reset OK");
    }

    /* Reset */
    rc = tiku_kits_ml_nbayes_reset(&nb);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_ml_nbayes_count(&nb) == 0,
                "count 0 after reset");
    TEST_ASSERT(tiku_kits_ml_nbayes_class_count(&nb, 0) == 0,
                "class 0 count 0 after reset");

    /* Predict fails after reset (no data) */
    {
        uint8_t q[] = {0};
        rc = tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_SIZE,
                    "predict after reset rejected");
    }

    /* Can retrain after reset */
    {
        uint8_t x[] = {1};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t q[] = {1};
        rc = tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "predict after retrain OK");
    }

    TEST_GROUP_END("NBayes Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_nbayes_null_inputs(void)
{
    struct tiku_kits_ml_nbayes nb;
    int32_t scores[TIKU_KITS_ML_NBAYES_MAX_CLASSES];
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("NBayes NULL Inputs");

    /* init NULL */
    rc = tiku_kits_ml_nbayes_init(NULL, 2, 4, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "init NULL rejected");

    /* reset NULL */
    rc = tiku_kits_ml_nbayes_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "reset NULL rejected");

    tiku_kits_ml_nbayes_init(&nb, 2, 4, 2, 8);

    /* train NULL x */
    rc = tiku_kits_ml_nbayes_train(&nb, NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "train NULL x rejected");

    /* train NULL nb */
    {
        uint8_t x[] = {0, 0};
        rc = tiku_kits_ml_nbayes_train(NULL, x, 0);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "train NULL nb rejected");
    }

    /* train invalid label */
    {
        uint8_t x[] = {0, 0};
        rc = tiku_kits_ml_nbayes_train(&nb, x, 2);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                    "train label >= n_classes rejected");
    }

    /* train invalid bin */
    {
        uint8_t x[] = {0, 4};
        rc = tiku_kits_ml_nbayes_train(&nb, x, 0);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                    "train bin >= n_bins rejected");
    }

    /* predict NULL */
    {
        uint8_t x[] = {0, 0};
        rc = tiku_kits_ml_nbayes_predict(NULL, x, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL nb rejected");

        rc = tiku_kits_ml_nbayes_predict(&nb, NULL, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL x rejected");

        rc = tiku_kits_ml_nbayes_predict(&nb, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL result rejected");
    }

    /* predict_log_proba NULL */
    {
        uint8_t x[] = {0, 0};
        rc = tiku_kits_ml_nbayes_predict_log_proba(
            NULL, x, scores);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_log_proba NULL nb rejected");

        rc = tiku_kits_ml_nbayes_predict_log_proba(
            &nb, NULL, scores);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_log_proba NULL x rejected");

        rc = tiku_kits_ml_nbayes_predict_log_proba(
            &nb, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_log_proba NULL scores rejected");
    }

    /* utility NULL */
    TEST_ASSERT(tiku_kits_ml_nbayes_count(NULL) == 0,
                "count NULL returns 0");
    TEST_ASSERT(tiku_kits_ml_nbayes_class_count(NULL, 0) == 0,
                "class_count NULL returns 0");

    TEST_GROUP_END("NBayes NULL Inputs");
}
