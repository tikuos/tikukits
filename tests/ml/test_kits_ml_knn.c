/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ml_knn.c - k-Nearest Neighbors classifier tests
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

void test_kits_ml_knn_init(void)
{
    struct tiku_kits_ml_knn knn;
    int rc;

    TEST_GROUP_BEGIN("KNN Init");

    rc = tiku_kits_ml_knn_init(&knn, 2, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "init returns OK");
    TEST_ASSERT(tiku_kits_ml_knn_count(&knn) == 0,
                "initial sample count is 0");
    TEST_ASSERT(tiku_kits_ml_knn_get_k(&knn) == 3,
                "k is 3");

    /* Invalid: 0 features */
    rc = tiku_kits_ml_knn_init(&knn, 0, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "0 features rejected");

    /* Invalid: too many features */
    rc = tiku_kits_ml_knn_init(
        &knn, TIKU_KITS_ML_KNN_MAX_FEATURES + 1, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "too many features rejected");

    /* Invalid: k = 0 */
    rc = tiku_kits_ml_knn_init(&knn, 2, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "k=0 rejected");

    TEST_GROUP_END("KNN Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: SIMPLE 1-D CLASSIFICATION                                         */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_simple(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;

    TEST_GROUP_BEGIN("KNN Simple 1-D");

    tiku_kits_ml_knn_init(&knn, 1, 3);

    /* Class 0 cluster around x=0 */
    {
        tiku_kits_ml_elem_t x[] = {0};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {1};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {2};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }

    /* Class 1 cluster around x=100 */
    {
        tiku_kits_ml_elem_t x[] = {98};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {100};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {102};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }

    TEST_ASSERT(tiku_kits_ml_knn_count(&knn) == 6,
                "6 samples stored");

    /* Query near class 0 cluster */
    {
        tiku_kits_ml_elem_t q[] = {3};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 0, "x=3 -> class 0");
    }

    /* Query near class 1 cluster */
    {
        tiku_kits_ml_elem_t q[] = {99};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 1, "x=99 -> class 1");
    }

    TEST_GROUP_END("KNN Simple 1-D");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: 2-D MULTI-CLASS                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_two_features(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;

    TEST_GROUP_BEGIN("KNN 2-D Multi-Class");

    tiku_kits_ml_knn_init(&knn, 2, 3);

    /* Class 0: bottom-left cluster */
    {
        tiku_kits_ml_elem_t x[] = {1, 1};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {2, 1};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }

    /* Class 1: top-right cluster */
    {
        tiku_kits_ml_elem_t x[] = {10, 10};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {11, 10};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {10, 11};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }

    /* Class 2: far away cluster */
    {
        tiku_kits_ml_elem_t x[] = {50, 50};
        tiku_kits_ml_knn_add(&knn, x, 2);
    }
    {
        tiku_kits_ml_elem_t x[] = {51, 50};
        tiku_kits_ml_knn_add(&knn, x, 2);
    }
    {
        tiku_kits_ml_elem_t x[] = {50, 51};
        tiku_kits_ml_knn_add(&knn, x, 2);
    }

    /* Query near class 0 */
    {
        tiku_kits_ml_elem_t q[] = {2, 2};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 0, "(2,2) -> class 0");
    }

    /* Query near class 1 */
    {
        tiku_kits_ml_elem_t q[] = {9, 9};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 1, "(9,9) -> class 1");
    }

    /* Query near class 2 */
    {
        tiku_kits_ml_elem_t q[] = {49, 49};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 2, "(49,49) -> class 2");
    }

    TEST_GROUP_END("KNN 2-D Multi-Class");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: k=1 NEAREST NEIGHBOR                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_k1(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;

    TEST_GROUP_BEGIN("KNN k=1");

    tiku_kits_ml_knn_init(&knn, 1, 1);

    {
        tiku_kits_ml_elem_t x[] = {0};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {20};
        tiku_kits_ml_knn_add(&knn, x, 2);
    }

    /* Closest to 0 -> class 0 */
    {
        tiku_kits_ml_elem_t q[] = {1};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 0, "x=1, k=1 -> class 0");
    }

    /* Closest to 10 -> class 1 */
    {
        tiku_kits_ml_elem_t q[] = {9};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 1, "x=9, k=1 -> class 1");
    }

    /* Closest to 20 -> class 2 */
    {
        tiku_kits_ml_elem_t q[] = {19};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 2, "x=19, k=1 -> class 2");
    }

    TEST_GROUP_END("KNN k=1");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: CHANGE K                                                          */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_change_k(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("KNN Change k");

    tiku_kits_ml_knn_init(&knn, 1, 1);

    /* 2 samples of class 0, 1 sample of class 1 (closer to query) */
    {
        tiku_kits_ml_elem_t x[] = {0};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {4};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {3};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }

    /* k=1: nearest to x=3 is (3, class 1) -> class 1 */
    {
        tiku_kits_ml_elem_t q[] = {3};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 1, "k=1, x=3 -> class 1 (nearest)");
    }

    /* Change to k=3: 2 votes for class 0, 1 vote for class 1 -> class 0 */
    rc = tiku_kits_ml_knn_set_k(&knn, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "set_k(3) returns OK");
    TEST_ASSERT(tiku_kits_ml_knn_get_k(&knn) == 3, "k is now 3");

    {
        tiku_kits_ml_elem_t q[] = {3};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 0, "k=3, x=3 -> class 0 (majority)");
    }

    /* Invalid set_k */
    rc = tiku_kits_ml_knn_set_k(&knn, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM, "set_k(0) rejected");

    TEST_GROUP_END("KNN Change k");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: RING BUFFER OVERFLOW                                              */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_overflow(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;
    uint16_t i;

    TEST_GROUP_BEGIN("KNN Ring Buffer");

    tiku_kits_ml_knn_init(&knn, 1, 3);

    /* Fill buffer with class 0 samples */
    for (i = 0; i < TIKU_KITS_ML_KNN_MAX_SAMPLES; i++) {
        tiku_kits_ml_elem_t x[] = {(tiku_kits_ml_elem_t)i};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    TEST_ASSERT(tiku_kits_ml_knn_count(&knn)
                == TIKU_KITS_ML_KNN_MAX_SAMPLES,
                "count capped at MAX_SAMPLES");

    /* Query should find class 0 */
    {
        tiku_kits_ml_elem_t q[] = {5};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 0, "all class 0 -> predict class 0");
    }

    /* Overwrite all with class 1 samples (ring buffer wraps) */
    for (i = 0; i < TIKU_KITS_ML_KNN_MAX_SAMPLES; i++) {
        tiku_kits_ml_elem_t x[] = {(tiku_kits_ml_elem_t)(i + 1000)};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    TEST_ASSERT(tiku_kits_ml_knn_count(&knn)
                == TIKU_KITS_ML_KNN_MAX_SAMPLES,
                "count still MAX_SAMPLES after overwrite");

    /* Query should now find class 1 */
    {
        tiku_kits_ml_elem_t q[] = {1005};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 1,
                    "after overwrite -> predict class 1");
    }

    TEST_GROUP_END("KNN Ring Buffer");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: NEGATIVE VALUES                                                   */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_negative(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;

    TEST_GROUP_BEGIN("KNN Negative Values");

    tiku_kits_ml_knn_init(&knn, 1, 3);

    /* Class 0: negative cluster */
    {
        tiku_kits_ml_elem_t x[] = {-100};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {-99};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {-101};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }

    /* Class 1: positive cluster */
    {
        tiku_kits_ml_elem_t x[] = {100};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {99};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {101};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }

    /* Query negative -> class 0 */
    {
        tiku_kits_ml_elem_t q[] = {-95};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 0, "x=-95 -> class 0");
    }

    /* Query positive -> class 1 */
    {
        tiku_kits_ml_elem_t q[] = {95};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 1, "x=95 -> class 1");
    }

    TEST_GROUP_END("KNN Negative Values");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: RESET                                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_reset(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("KNN Reset");

    tiku_kits_ml_knn_init(&knn, 1, 1);

    {
        tiku_kits_ml_elem_t x[] = {5};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    TEST_ASSERT(tiku_kits_ml_knn_count(&knn) == 1,
                "1 sample before reset");

    /* Predict works before reset */
    {
        tiku_kits_ml_elem_t q[] = {5};
        rc = tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "predict before reset OK");
    }

    /* Reset */
    rc = tiku_kits_ml_knn_reset(&knn);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_ml_knn_count(&knn) == 0,
                "count 0 after reset");
    TEST_ASSERT(tiku_kits_ml_knn_get_k(&knn) == 1,
                "k preserved after reset");

    /* Predict fails after reset (no samples) */
    {
        tiku_kits_ml_elem_t q[] = {5};
        rc = tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_SIZE,
                    "predict after reset rejected");
    }

    /* Can add samples and predict after reset */
    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t q[] = {10};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        TEST_ASSERT(cls == 1, "predict after re-add OK");
    }

    TEST_GROUP_END("KNN Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_knn_null_inputs(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;
    int rc;

    TEST_GROUP_BEGIN("KNN NULL Inputs");

    /* init NULL */
    rc = tiku_kits_ml_knn_init(NULL, 2, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "init NULL rejected");

    /* reset NULL */
    rc = tiku_kits_ml_knn_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "reset NULL rejected");

    /* set_k NULL */
    rc = tiku_kits_ml_knn_set_k(NULL, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_k NULL rejected");

    tiku_kits_ml_knn_init(&knn, 2, 3);

    /* add NULL x */
    rc = tiku_kits_ml_knn_add(&knn, NULL, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "add NULL x rejected");

    /* add NULL knn */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_knn_add(NULL, x, 0);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "add NULL knn rejected");
    }

    /* add invalid label */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_knn_add(
            &knn, x, TIKU_KITS_ML_KNN_MAX_CLASSES);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                    "add label too large rejected");
    }

    /* predict NULL */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_knn_predict(NULL, x, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL knn rejected");

        rc = tiku_kits_ml_knn_predict(&knn, NULL, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL x rejected");

        rc = tiku_kits_ml_knn_predict(&knn, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL result rejected");
    }

    /* utility NULL */
    TEST_ASSERT(tiku_kits_ml_knn_count(NULL) == 0,
                "count NULL returns 0");
    TEST_ASSERT(tiku_kits_ml_knn_get_k(NULL) == 0,
                "get_k NULL returns 0");

    TEST_GROUP_END("KNN NULL Inputs");
}
