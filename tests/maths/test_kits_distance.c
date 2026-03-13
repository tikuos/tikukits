/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_distance.c - Distance metrics tests
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

#include "test_kits_maths.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: MANHATTAN DISTANCE (L1)                                           */
/*---------------------------------------------------------------------------*/

void test_kits_distance_manhattan(void)
{
    tiku_kits_distance_elem_t a[] = {1, 2, 3};
    tiku_kits_distance_elem_t b[] = {4, 0, 5};
    tiku_kits_distance_elem_t c[] = {1, 2, 3};
    int64_t result;
    int rc;

    TEST_GROUP_BEGIN("Distance Manhattan");

    /* |1-4| + |2-0| + |3-5| = 3 + 2 + 2 = 7 */
    rc = tiku_kits_distance_manhattan(a, b, 3, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "manhattan returns OK");
    TEST_ASSERT(result == 7, "manhattan({1,2,3},{4,0,5}) == 7");

    /* Same vectors -> distance 0 */
    rc = tiku_kits_distance_manhattan(a, c, 3, &result);
    TEST_ASSERT(result == 0, "manhattan same vectors == 0");

    /* Negative values */
    {
        tiku_kits_distance_elem_t x[] = {-10, 5};
        tiku_kits_distance_elem_t y[] = {10, -5};

        /* |-10-10| + |5-(-5)| = 20 + 10 = 30 */
        tiku_kits_distance_manhattan(x, y, 2, &result);
        TEST_ASSERT(result == 30, "manhattan with negatives == 30");
    }

    /* Single element */
    {
        tiku_kits_distance_elem_t x[] = {100};
        tiku_kits_distance_elem_t y[] = {42};

        tiku_kits_distance_manhattan(x, y, 1, &result);
        TEST_ASSERT(result == 58, "manhattan single element == 58");
    }

    /* len=0 rejected */
    rc = tiku_kits_distance_manhattan(a, b, 0, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "manhattan len=0 rejected");
    TEST_GROUP_END("Distance Manhattan");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: SQUARED EUCLIDEAN DISTANCE                                        */
/*---------------------------------------------------------------------------*/

void test_kits_distance_euclidean_sq(void)
{
    tiku_kits_distance_elem_t a[] = {1, 2, 3};
    tiku_kits_distance_elem_t b[] = {4, 0, 5};
    int64_t result;
    int rc;

    TEST_GROUP_BEGIN("Distance Euclidean Squared");

    /* (1-4)^2 + (2-0)^2 + (3-5)^2 = 9 + 4 + 4 = 17 */
    rc = tiku_kits_distance_euclidean_sq(a, b, 3, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "euclidean_sq returns OK");
    TEST_ASSERT(result == 17, "euclidean_sq({1,2,3},{4,0,5}) == 17");

    /* Same vectors -> distance 0 */
    rc = tiku_kits_distance_euclidean_sq(a, a, 3, &result);
    TEST_ASSERT(result == 0, "euclidean_sq same vectors == 0");

    /* Known Pythagorean triple: (3,4) -> dist^2 = 25 */
    {
        tiku_kits_distance_elem_t x[] = {0, 0};
        tiku_kits_distance_elem_t y[] = {3, 4};

        tiku_kits_distance_euclidean_sq(x, y, 2, &result);
        TEST_ASSERT(result == 25, "euclidean_sq (3,4) == 25");
    }

    /* len=0 rejected */
    rc = tiku_kits_distance_euclidean_sq(a, b, 0, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE,
                "euclidean_sq len=0 rejected");
    TEST_GROUP_END("Distance Euclidean Squared");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: DOT PRODUCT                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_distance_dot(void)
{
    tiku_kits_distance_elem_t a[] = {1, 2, 3};
    tiku_kits_distance_elem_t b[] = {4, 5, 6};
    int64_t result;
    int rc;

    TEST_GROUP_BEGIN("Distance Dot Product");

    /* 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32 */
    rc = tiku_kits_distance_dot(a, b, 3, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "dot returns OK");
    TEST_ASSERT(result == 32, "dot({1,2,3},{4,5,6}) == 32");

    /* Orthogonal vectors -> dot = 0 */
    {
        tiku_kits_distance_elem_t x[] = {1, 0};
        tiku_kits_distance_elem_t y[] = {0, 1};

        tiku_kits_distance_dot(x, y, 2, &result);
        TEST_ASSERT(result == 0, "dot orthogonal == 0");
    }

    /* Negative values */
    {
        tiku_kits_distance_elem_t x[] = {-1, 2, -3};
        tiku_kits_distance_elem_t y[] = {4, -5, 6};

        /* -4 + -10 + -18 = -32 */
        tiku_kits_distance_dot(x, y, 3, &result);
        TEST_ASSERT(result == -32, "dot with negatives == -32");
    }

    /* len=0 rejected */
    rc = tiku_kits_distance_dot(a, b, 0, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "dot len=0 rejected");
    TEST_GROUP_END("Distance Dot Product");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: COSINE SIMILARITY COMPONENTS                                      */
/*---------------------------------------------------------------------------*/

void test_kits_distance_cosine_sq(void)
{
    tiku_kits_distance_elem_t a[] = {3, 4};
    tiku_kits_distance_elem_t b[] = {3, 4};
    int64_t dot_ab, dot_aa, dot_bb;
    int rc;

    TEST_GROUP_BEGIN("Distance Cosine Components");

    /* Identical vectors: cos^2 = 1 */
    rc = tiku_kits_distance_cosine_sq(a, b, 2, &dot_ab, &dot_aa, &dot_bb);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "cosine_sq returns OK");
    TEST_ASSERT(dot_ab == 25, "dot(a,b) == 25");
    TEST_ASSERT(dot_aa == 25, "dot(a,a) == 25");
    TEST_ASSERT(dot_bb == 25, "dot(b,b) == 25");
    /* cos^2 = 25^2 / (25*25) = 1 */
    TEST_ASSERT(dot_ab * dot_ab == dot_aa * dot_bb,
                "identical vectors: cos^2 == 1");

    /* Orthogonal vectors: cos = 0 */
    {
        tiku_kits_distance_elem_t x[] = {1, 0};
        tiku_kits_distance_elem_t y[] = {0, 1};

        tiku_kits_distance_cosine_sq(x, y, 2, &dot_ab, &dot_aa, &dot_bb);
        TEST_ASSERT(dot_ab == 0, "orthogonal dot_ab == 0");
        TEST_ASSERT(dot_aa == 1, "orthogonal dot_aa == 1");
        TEST_ASSERT(dot_bb == 1, "orthogonal dot_bb == 1");
    }

    /* Anti-parallel vectors: dot_ab negative */
    {
        tiku_kits_distance_elem_t x[] = {1, 0};
        tiku_kits_distance_elem_t y[] = {-1, 0};

        tiku_kits_distance_cosine_sq(x, y, 2, &dot_ab, &dot_aa, &dot_bb);
        TEST_ASSERT(dot_ab == -1, "anti-parallel dot_ab == -1");
        /* cos^2 = (-1)^2 / (1*1) = 1 */
        TEST_ASSERT(dot_ab * dot_ab == dot_aa * dot_bb,
                    "anti-parallel: cos^2 == 1");
    }

    /* len=0 rejected */
    rc = tiku_kits_distance_cosine_sq(a, b, 0, &dot_ab, &dot_aa, &dot_bb);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "cosine_sq len=0 rejected");
    TEST_GROUP_END("Distance Cosine Components");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: HAMMING DISTANCE                                                  */
/*---------------------------------------------------------------------------*/

void test_kits_distance_hamming(void)
{
    uint32_t result;
    int rc;

    TEST_GROUP_BEGIN("Distance Hamming");

    /* 0xFF ^ 0x0F = 0xF0 -> 4 bits, 0x00 ^ 0x00 = 0, 0xAA ^ 0x55 = 0xFF -> 8 */
    {
        uint8_t a[] = {0xFF, 0x00, 0xAA};
        uint8_t b[] = {0x0F, 0x00, 0x55};

        rc = tiku_kits_distance_hamming(a, b, 3, &result);
        TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "hamming returns OK");
        TEST_ASSERT(result == 12, "hamming(FF00AA, 0F0055) == 12");
    }

    /* Identical -> distance 0 */
    {
        uint8_t a[] = {0xAB, 0xCD};
        uint8_t b[] = {0xAB, 0xCD};

        tiku_kits_distance_hamming(a, b, 2, &result);
        TEST_ASSERT(result == 0, "hamming identical == 0");
    }

    /* All bits differ */
    {
        uint8_t a[] = {0xFF};
        uint8_t b[] = {0x00};

        tiku_kits_distance_hamming(a, b, 1, &result);
        TEST_ASSERT(result == 8, "hamming all-ones vs all-zeros == 8");
    }

    /* Single bit difference */
    {
        uint8_t a[] = {0x01};
        uint8_t b[] = {0x00};

        tiku_kits_distance_hamming(a, b, 1, &result);
        TEST_ASSERT(result == 1, "hamming single bit == 1");
    }

    /* len=0 rejected */
    {
        uint8_t a[] = {0xFF};
        uint8_t b[] = {0x00};

        rc = tiku_kits_distance_hamming(a, b, 0, &result);
        TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE,
                    "hamming len=0 rejected");
    }
    TEST_GROUP_END("Distance Hamming");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_distance_null_inputs(void)
{
    tiku_kits_distance_elem_t a[] = {1, 2};
    tiku_kits_distance_elem_t b[] = {3, 4};
    int64_t result;
    int64_t dot_ab, dot_aa, dot_bb;
    uint32_t hamming_result;
    uint8_t ha[] = {0xFF};
    uint8_t hb[] = {0x00};
    int rc;

    TEST_GROUP_BEGIN("Distance NULL Inputs");

    /* Manhattan */
    rc = tiku_kits_distance_manhattan(NULL, b, 2, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "manhattan NULL a rejected");

    rc = tiku_kits_distance_manhattan(a, NULL, 2, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "manhattan NULL b rejected");

    rc = tiku_kits_distance_manhattan(a, b, 2, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL,
                "manhattan NULL result rejected");

    /* Euclidean squared */
    rc = tiku_kits_distance_euclidean_sq(NULL, b, 2, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL,
                "euclidean_sq NULL a rejected");

    rc = tiku_kits_distance_euclidean_sq(a, b, 2, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL,
                "euclidean_sq NULL result rejected");

    /* Dot */
    rc = tiku_kits_distance_dot(NULL, b, 2, &result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "dot NULL a rejected");

    rc = tiku_kits_distance_dot(a, b, 2, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "dot NULL result rejected");

    /* Cosine */
    rc = tiku_kits_distance_cosine_sq(NULL, b, 2,
                                       &dot_ab, &dot_aa, &dot_bb);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "cosine_sq NULL a rejected");

    rc = tiku_kits_distance_cosine_sq(a, b, 2, NULL, &dot_aa, &dot_bb);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL,
                "cosine_sq NULL dot_ab rejected");

    /* Hamming */
    rc = tiku_kits_distance_hamming(NULL, hb, 1, &hamming_result);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "hamming NULL a rejected");

    rc = tiku_kits_distance_hamming(ha, hb, 1, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL,
                "hamming NULL result rejected");
    TEST_GROUP_END("Distance NULL Inputs");
}
