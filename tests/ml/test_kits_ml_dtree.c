/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_ml_dtree.c - Decision tree classifier tests
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

void test_kits_ml_dtree_init(void)
{
    struct tiku_kits_ml_dtree dt;
    int rc;

    TEST_GROUP_BEGIN("DTree Init");

    rc = tiku_kits_ml_dtree_init(&dt, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "init returns OK");
    TEST_ASSERT(tiku_kits_ml_dtree_node_count(&dt) == 0,
                "initial node count is 0");

    /* Invalid: 0 features */
    rc = tiku_kits_ml_dtree_init(&dt, 0, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "0 features rejected");

    /* Invalid: too many features */
    rc = tiku_kits_ml_dtree_init(
        &dt, TIKU_KITS_ML_DTREE_MAX_FEATURES + 1, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "too many features rejected");

    /* Invalid: shift = 0 */
    rc = tiku_kits_ml_dtree_init(&dt, 2, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=0 rejected");

    /* Invalid: shift = 31 */
    rc = tiku_kits_ml_dtree_init(&dt, 2, 31);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "shift=31 rejected");

    TEST_GROUP_END("DTree Init");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: SIMPLE 3-NODE TREE                                                */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_simple_tree(void)
{
    struct tiku_kits_ml_dtree dt;
    uint8_t cls;
    int rc;

    /*
     * Tree: x[0] <= 5 -> class 0, else class 1
     *
     *        [0] x[0]<=5
     *       /          \
     *     [1] c=0     [2] c=1
     */
    struct tiku_kits_ml_dtree_node nodes[] = {
        {5,  0, 1, 2, 0},                    /* root: split feature 0 at 5 */
        {0,  0, 0xFF, 0xFF, 0},              /* leaf: class 0 */
        {0,  0, 0xFF, 0xFF, 1},              /* leaf: class 1 */
    };

    TEST_GROUP_BEGIN("DTree Simple Tree");

    tiku_kits_ml_dtree_init(&dt, 1, 8);

    rc = tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "set_tree returns OK");
    TEST_ASSERT(tiku_kits_ml_dtree_node_count(&dt) == 3,
                "node count is 3");

    /* x=3 <= 5 -> class 0 */
    {
        tiku_kits_ml_elem_t x[] = {3};
        tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(cls == 0, "x=3 -> class 0");
    }

    /* x=5 <= 5 -> class 0 (boundary: <=) */
    {
        tiku_kits_ml_elem_t x[] = {5};
        tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(cls == 0, "x=5 -> class 0 (boundary)");
    }

    /* x=6 > 5 -> class 1 */
    {
        tiku_kits_ml_elem_t x[] = {6};
        tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(cls == 1, "x=6 -> class 1");
    }

    /* x=100 > 5 -> class 1 */
    {
        tiku_kits_ml_elem_t x[] = {100};
        tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(cls == 1, "x=100 -> class 1");
    }

    TEST_GROUP_END("DTree Simple Tree");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: MULTI-FEATURE TREE                                                */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_multi_feature(void)
{
    struct tiku_kits_ml_dtree dt;
    uint8_t cls;

    /*
     * 2-feature tree with 3 classes:
     *
     *            [0] x[0]<=10
     *           /            \
     *     [1] x[1]<=5      [2] c=2
     *     /         \
     *   [3] c=0   [4] c=1
     */
    struct tiku_kits_ml_dtree_node nodes[] = {
        {10, 0, 1, 2, 0},                   /* root: feature 0 <= 10 */
        {5,  1, 3, 4, 0},                   /* node 1: feature 1 <= 5 */
        {0,  0, 0xFF, 0xFF, 2},             /* leaf: class 2 */
        {0,  0, 0xFF, 0xFF, 0},             /* leaf: class 0 */
        {0,  0, 0xFF, 0xFF, 1},             /* leaf: class 1 */
    };

    TEST_GROUP_BEGIN("DTree Multi-Feature");

    tiku_kits_ml_dtree_init(&dt, 2, 8);
    tiku_kits_ml_dtree_set_tree(&dt, nodes, 5);

    /* x[0]=5, x[1]=3 -> left, left -> class 0 */
    {
        tiku_kits_ml_elem_t x[] = {5, 3};
        tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(cls == 0,
                    "x=(5,3) -> class 0");
    }

    /* x[0]=8, x[1]=7 -> left, right -> class 1 */
    {
        tiku_kits_ml_elem_t x[] = {8, 7};
        tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(cls == 1,
                    "x=(8,7) -> class 1");
    }

    /* x[0]=15, x[1]=3 -> right -> class 2 */
    {
        tiku_kits_ml_elem_t x[] = {15, 3};
        tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(cls == 2,
                    "x=(15,3) -> class 2");
    }

    TEST_GROUP_END("DTree Multi-Feature");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: TREE DEPTH                                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_depth(void)
{
    struct tiku_kits_ml_dtree dt;

    TEST_GROUP_BEGIN("DTree Depth");

    tiku_kits_ml_dtree_init(&dt, 1, 8);

    /* Single leaf -> depth 1 */
    {
        struct tiku_kits_ml_dtree_node nodes[] = {
            {0, 0, 0xFF, 0xFF, 0},
        };
        tiku_kits_ml_dtree_set_tree(&dt, nodes, 1);
        TEST_ASSERT(tiku_kits_ml_dtree_depth(&dt) == 1,
                    "single leaf depth == 1");
    }

    /* 3-node tree -> depth 2 */
    {
        struct tiku_kits_ml_dtree_node nodes[] = {
            {5, 0, 1, 2, 0},
            {0, 0, 0xFF, 0xFF, 0},
            {0, 0, 0xFF, 0xFF, 1},
        };
        tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);
        TEST_ASSERT(tiku_kits_ml_dtree_depth(&dt) == 2,
                    "3-node tree depth == 2");
    }

    /* Unbalanced: right-leaning depth 3 */
    {
        /*
         *   [0] -> left=1(leaf), right=2
         *   [2] -> left=3(leaf), right=4(leaf)
         */
        struct tiku_kits_ml_dtree_node nodes[] = {
            {5,  0, 1, 2, 0},
            {0,  0, 0xFF, 0xFF, 0},
            {10, 0, 3, 4, 0},
            {0,  0, 0xFF, 0xFF, 1},
            {0,  0, 0xFF, 0xFF, 2},
        };
        tiku_kits_ml_dtree_set_tree(&dt, nodes, 5);
        TEST_ASSERT(tiku_kits_ml_dtree_depth(&dt) == 3,
                    "unbalanced tree depth == 3");
    }

    /* Empty tree -> depth 0 */
    tiku_kits_ml_dtree_reset(&dt);
    TEST_ASSERT(tiku_kits_ml_dtree_depth(&dt) == 0,
                "empty tree depth == 0");

    TEST_GROUP_END("DTree Depth");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: PREDICT PROBA (CONFIDENCE)                                        */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_predict_proba(void)
{
    struct tiku_kits_ml_dtree dt;
    int32_t conf;

    /*
     * Tree with confidence values stored in leaf thresholds:
     *   [0] x[0] <= 5 -> left=1, right=2
     *   [1] leaf: class 0, confidence = 230 (~0.90 in Q8)
     *   [2] leaf: class 1, confidence = 200 (~0.78 in Q8)
     */
    struct tiku_kits_ml_dtree_node nodes[] = {
        {5,   0, 1, 2, 0},
        {230, 0, 0xFF, 0xFF, 0},
        {200, 0, 0xFF, 0xFF, 1},
    };

    TEST_GROUP_BEGIN("DTree Predict Proba");

    tiku_kits_ml_dtree_init(&dt, 1, 8);
    tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);

    /* x=3 -> leaf 1 -> confidence = 230 */
    {
        tiku_kits_ml_elem_t x[] = {3};
        tiku_kits_ml_dtree_predict_proba(&dt, x, &conf);
        TEST_ASSERT(conf == 230,
                    "x=3 confidence == 230 (~0.90 Q8)");
    }

    /* x=7 -> leaf 2 -> confidence = 200 */
    {
        tiku_kits_ml_elem_t x[] = {7};
        tiku_kits_ml_dtree_predict_proba(&dt, x, &conf);
        TEST_ASSERT(conf == 200,
                    "x=7 confidence == 200 (~0.78 Q8)");
    }

    /* Leaf with threshold=0 -> default full confidence (256) */
    {
        struct tiku_kits_ml_dtree_node nodes2[] = {
            {0, 0, 0xFF, 0xFF, 0},
        };
        tiku_kits_ml_dtree_set_tree(&dt, nodes2, 1);

        {
            tiku_kits_ml_elem_t x[] = {0};
            tiku_kits_ml_dtree_predict_proba(&dt, x, &conf);
            TEST_ASSERT(conf == 256,
                        "no confidence stored -> 256 (1.0 Q8)");
        }
    }

    TEST_GROUP_END("DTree Predict Proba");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: BOUNDARY CONDITIONS                                               */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_boundary(void)
{
    struct tiku_kits_ml_dtree dt;
    uint8_t cls;

    TEST_GROUP_BEGIN("DTree Boundary");

    tiku_kits_ml_dtree_init(&dt, 1, 8);

    /* Threshold at 0: test negative/zero/positive */
    {
        struct tiku_kits_ml_dtree_node nodes[] = {
            {0,  0, 1, 2, 0},
            {0,  0, 0xFF, 0xFF, 0},
            {0,  0, 0xFF, 0xFF, 1},
        };
        tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);

        {
            tiku_kits_ml_elem_t x[] = {-5};
            tiku_kits_ml_dtree_predict(&dt, x, &cls);
            TEST_ASSERT(cls == 0, "x=-5 <= 0 -> class 0");
        }
        {
            tiku_kits_ml_elem_t x[] = {0};
            tiku_kits_ml_dtree_predict(&dt, x, &cls);
            TEST_ASSERT(cls == 0, "x=0 <= 0 -> class 0");
        }
        {
            tiku_kits_ml_elem_t x[] = {1};
            tiku_kits_ml_dtree_predict(&dt, x, &cls);
            TEST_ASSERT(cls == 1, "x=1 > 0 -> class 1");
        }
    }

    /* Negative threshold */
    {
        struct tiku_kits_ml_dtree_node nodes[] = {
            {-10, 0, 1, 2, 0},
            {0,   0, 0xFF, 0xFF, 0},
            {0,   0, 0xFF, 0xFF, 1},
        };
        tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);

        {
            tiku_kits_ml_elem_t x[] = {-10};
            tiku_kits_ml_dtree_predict(&dt, x, &cls);
            TEST_ASSERT(cls == 0,
                        "x=-10 <= -10 -> class 0");
        }
        {
            tiku_kits_ml_elem_t x[] = {-9};
            tiku_kits_ml_dtree_predict(&dt, x, &cls);
            TEST_ASSERT(cls == 1,
                        "x=-9 > -10 -> class 1");
        }
    }

    TEST_GROUP_END("DTree Boundary");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: GET TREE                                                          */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_get_tree(void)
{
    struct tiku_kits_ml_dtree dt;
    struct tiku_kits_ml_dtree_node out[TIKU_KITS_ML_DTREE_MAX_NODES];
    uint8_t n_out;
    int rc;

    struct tiku_kits_ml_dtree_node nodes[] = {
        {5,  0, 1, 2, 0},
        {0,  0, 0xFF, 0xFF, 0},
        {0,  0, 0xFF, 0xFF, 1},
    };

    TEST_GROUP_BEGIN("DTree Get Tree");

    tiku_kits_ml_dtree_init(&dt, 1, 8);
    tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);

    rc = tiku_kits_ml_dtree_get_tree(&dt, out, &n_out);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "get_tree returns OK");
    TEST_ASSERT(n_out == 3, "get_tree n_nodes == 3");

    /* Verify root node */
    TEST_ASSERT(out[0].threshold == 5, "root threshold == 5");
    TEST_ASSERT(out[0].feature_index == 0,
                "root feature_index == 0");
    TEST_ASSERT(out[0].left == 1, "root left == 1");
    TEST_ASSERT(out[0].right == 2, "root right == 2");

    /* Verify leaf nodes */
    TEST_ASSERT(out[1].class_label == 0,
                "leaf 1 class_label == 0");
    TEST_ASSERT(out[1].left == 0xFF, "leaf 1 left == LEAF");
    TEST_ASSERT(out[2].class_label == 1,
                "leaf 2 class_label == 1");

    TEST_GROUP_END("DTree Get Tree");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: RESET                                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_reset(void)
{
    struct tiku_kits_ml_dtree dt;
    uint8_t cls;
    int rc;

    struct tiku_kits_ml_dtree_node nodes[] = {
        {5,  0, 1, 2, 0},
        {0,  0, 0xFF, 0xFF, 0},
        {0,  0, 0xFF, 0xFF, 1},
    };

    TEST_GROUP_BEGIN("DTree Reset");

    tiku_kits_ml_dtree_init(&dt, 1, 8);
    tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);

    /* Predict works before reset */
    {
        tiku_kits_ml_elem_t x[] = {3};
        rc = tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                    "predict before reset OK");
    }

    /* Reset */
    rc = tiku_kits_ml_dtree_reset(&dt);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK, "reset returns OK");
    TEST_ASSERT(tiku_kits_ml_dtree_node_count(&dt) == 0,
                "node count 0 after reset");

    /* Predict fails after reset */
    {
        tiku_kits_ml_elem_t x[] = {3};
        rc = tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_SIZE,
                    "predict after reset rejected");
    }

    /* Can reload tree after reset */
    rc = tiku_kits_ml_dtree_set_tree(&dt, nodes, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_OK,
                "set_tree after reset OK");

    {
        tiku_kits_ml_elem_t x[] = {3};
        tiku_kits_ml_dtree_predict(&dt, x, &cls);
        TEST_ASSERT(cls == 0, "predict after reload OK");
    }

    TEST_GROUP_END("DTree Reset");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_ml_dtree_null_inputs(void)
{
    struct tiku_kits_ml_dtree dt;
    struct tiku_kits_ml_dtree_node nodes[3];
    struct tiku_kits_ml_dtree_node out[TIKU_KITS_ML_DTREE_MAX_NODES];
    uint8_t cls;
    uint8_t n_out;
    int32_t conf;
    int rc;

    TEST_GROUP_BEGIN("DTree NULL Inputs");

    /* init NULL */
    rc = tiku_kits_ml_dtree_init(NULL, 2, 8);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "init NULL rejected");

    /* reset NULL */
    rc = tiku_kits_ml_dtree_reset(NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "reset NULL rejected");

    /* set_tree NULL */
    rc = tiku_kits_ml_dtree_set_tree(NULL, nodes, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_tree NULL dt rejected");

    tiku_kits_ml_dtree_init(&dt, 2, 8);

    rc = tiku_kits_ml_dtree_set_tree(&dt, NULL, 3);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "set_tree NULL nodes rejected");

    /* set_tree invalid n_nodes */
    rc = tiku_kits_ml_dtree_set_tree(&dt, nodes, 0);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_PARAM,
                "set_tree n_nodes=0 rejected");

    /* get_tree NULL */
    rc = tiku_kits_ml_dtree_get_tree(NULL, out, &n_out);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_tree NULL dt rejected");

    rc = tiku_kits_ml_dtree_get_tree(&dt, NULL, &n_out);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_tree NULL out rejected");

    rc = tiku_kits_ml_dtree_get_tree(&dt, out, NULL);
    TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                "get_tree NULL n_out rejected");

    /* predict NULL */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_dtree_predict(NULL, x, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL dt rejected");

        rc = tiku_kits_ml_dtree_predict(&dt, NULL, &cls);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL x rejected");

        rc = tiku_kits_ml_dtree_predict(&dt, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict NULL result rejected");
    }

    /* predict_proba NULL */
    {
        tiku_kits_ml_elem_t x[] = {1, 2};
        rc = tiku_kits_ml_dtree_predict_proba(NULL, x, &conf);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_proba NULL dt rejected");

        rc = tiku_kits_ml_dtree_predict_proba(&dt, NULL, &conf);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_proba NULL x rejected");

        rc = tiku_kits_ml_dtree_predict_proba(&dt, x, NULL);
        TEST_ASSERT(rc == TIKU_KITS_ML_ERR_NULL,
                    "predict_proba NULL result rejected");
    }

    /* utility NULL */
    TEST_ASSERT(tiku_kits_ml_dtree_node_count(NULL) == 0,
                "node_count NULL returns 0");
    TEST_ASSERT(tiku_kits_ml_dtree_depth(NULL) == 0,
                "depth NULL returns 0");

    TEST_GROUP_END("DTree NULL Inputs");
}
