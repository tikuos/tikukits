/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_matrix.c - Matrix library tests
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
/* TEST 1: INITIALIZATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_init(void)
{
    struct tiku_kits_matrix m;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Init ---\n");

    rc = tiku_kits_matrix_init(&m, 2, 3);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "init 2x3 returns OK");
    TEST_ASSERT(tiku_kits_matrix_rows(&m) == 2, "rows is 2");
    TEST_ASSERT(tiku_kits_matrix_cols(&m) == 3, "cols is 3");
    TEST_ASSERT(tiku_kits_matrix_is_square(&m) == 0, "2x3 is not square");

    /* All elements should be zero */
    TEST_ASSERT(tiku_kits_matrix_get(&m, 0, 0) == 0, "element [0][0] is 0");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 1, 2) == 0, "element [1][2] is 0");

    /* Zero-size dimensions rejected */
    rc = tiku_kits_matrix_init(&m, 0, 3);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "zero rows rejected");

    rc = tiku_kits_matrix_init(&m, 2, 0);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "zero cols rejected");

    /* Oversized dimensions rejected */
    rc = tiku_kits_matrix_init(&m, TIKU_KITS_MATRIX_MAX_SIZE + 1, 1);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "oversized rows rejected");

    /* Max size accepted */
    rc = tiku_kits_matrix_init(&m, TIKU_KITS_MATRIX_MAX_SIZE,
                                   TIKU_KITS_MATRIX_MAX_SIZE);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "max size init OK");

    /* Zero preserves dimensions */
    tiku_kits_matrix_init(&m, 2, 2);
    tiku_kits_matrix_set(&m, 0, 0, 99);
    rc = tiku_kits_matrix_zero(&m);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "zero returns OK");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 0, 0) == 0,
                "element zeroed after zero()");
    TEST_ASSERT(tiku_kits_matrix_rows(&m) == 2,
                "rows preserved after zero()");
}

/*---------------------------------------------------------------------------*/
/* TEST 2: IDENTITY                                                          */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_identity(void)
{
    struct tiku_kits_matrix m;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Identity ---\n");

    rc = tiku_kits_matrix_identity(&m, 3);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "identity 3x3 returns OK");
    TEST_ASSERT(tiku_kits_matrix_rows(&m) == 3, "identity rows is 3");
    TEST_ASSERT(tiku_kits_matrix_cols(&m) == 3, "identity cols is 3");
    TEST_ASSERT(tiku_kits_matrix_is_square(&m) == 1, "identity is square");

    /* Diagonal elements are 1 */
    TEST_ASSERT(tiku_kits_matrix_get(&m, 0, 0) == 1, "I[0][0] is 1");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 1, 1) == 1, "I[1][1] is 1");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 2, 2) == 1, "I[2][2] is 1");

    /* Off-diagonal elements are 0 */
    TEST_ASSERT(tiku_kits_matrix_get(&m, 0, 1) == 0, "I[0][1] is 0");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 1, 0) == 0, "I[1][0] is 0");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 0, 2) == 0, "I[0][2] is 0");

    /* Invalid sizes */
    rc = tiku_kits_matrix_identity(&m, 0);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "identity n=0 rejected");

    rc = tiku_kits_matrix_identity(&m, TIKU_KITS_MATRIX_MAX_SIZE + 1);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_SIZE, "identity oversized rejected");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: SET / GET ELEMENT ACCESS                                          */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_set_get(void)
{
    struct tiku_kits_matrix m;

    TEST_PRINT("\n--- Test: Matrix Set/Get ---\n");

    tiku_kits_matrix_init(&m, 3, 3);

    tiku_kits_matrix_set(&m, 0, 0, 42);
    tiku_kits_matrix_set(&m, 1, 2, -7);
    tiku_kits_matrix_set(&m, 2, 1, 100);

    TEST_ASSERT(tiku_kits_matrix_get(&m, 0, 0) == 42, "get [0][0] == 42");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 1, 2) == -7, "get [1][2] == -7");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 2, 1) == 100, "get [2][1] == 100");

    /* Out-of-bounds get returns 0 */
    TEST_ASSERT(tiku_kits_matrix_get(&m, 3, 0) == 0,
                "out-of-bounds row get returns 0");
    TEST_ASSERT(tiku_kits_matrix_get(&m, 0, 3) == 0,
                "out-of-bounds col get returns 0");

    /* Out-of-bounds set is silently ignored */
    tiku_kits_matrix_set(&m, 3, 0, 999);
    TEST_ASSERT(tiku_kits_matrix_get(&m, 2, 0) == 0,
                "out-of-bounds set did not corrupt data");

    /* NULL get returns 0 */
    TEST_ASSERT(tiku_kits_matrix_get(NULL, 0, 0) == 0,
                "NULL get returns 0");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: COPY AND EQUALITY                                                 */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_copy_equal(void)
{
    struct tiku_kits_matrix a, b;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Copy/Equal ---\n");

    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_set(&a, 0, 0, 1);
    tiku_kits_matrix_set(&a, 0, 1, 2);
    tiku_kits_matrix_set(&a, 1, 0, 3);
    tiku_kits_matrix_set(&a, 1, 1, 4);

    rc = tiku_kits_matrix_copy(&b, &a);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "copy returns OK");
    TEST_ASSERT(tiku_kits_matrix_equal(&a, &b) == 1,
                "copy is equal to original");

    /* Modify copy, verify no longer equal */
    tiku_kits_matrix_set(&b, 0, 0, 999);
    TEST_ASSERT(tiku_kits_matrix_equal(&a, &b) == 0,
                "modified copy is not equal");

    /* Different dimensions are not equal */
    tiku_kits_matrix_init(&b, 3, 2);
    TEST_ASSERT(tiku_kits_matrix_equal(&a, &b) == 0,
                "different dimensions not equal");

    /* NULL equality returns 0 */
    TEST_ASSERT(tiku_kits_matrix_equal(NULL, &b) == 0,
                "NULL a returns not equal");
    TEST_ASSERT(tiku_kits_matrix_equal(&a, NULL) == 0,
                "NULL b returns not equal");
}

/*---------------------------------------------------------------------------*/
/* TEST 5: ADDITION AND SUBTRACTION                                          */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_add_sub(void)
{
    struct tiku_kits_matrix a, b, result;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Add/Sub ---\n");

    /* a = [1 2; 3 4], b = [10 20; 30 40] */
    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_set(&a, 0, 0, 1);
    tiku_kits_matrix_set(&a, 0, 1, 2);
    tiku_kits_matrix_set(&a, 1, 0, 3);
    tiku_kits_matrix_set(&a, 1, 1, 4);

    tiku_kits_matrix_init(&b, 2, 2);
    tiku_kits_matrix_set(&b, 0, 0, 10);
    tiku_kits_matrix_set(&b, 0, 1, 20);
    tiku_kits_matrix_set(&b, 1, 0, 30);
    tiku_kits_matrix_set(&b, 1, 1, 40);

    /* Addition: result = [11 22; 33 44] */
    rc = tiku_kits_matrix_add(&result, &a, &b);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "add returns OK");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 0) == 11, "add [0][0] == 11");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 1) == 22, "add [0][1] == 22");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 0) == 33, "add [1][0] == 33");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 1) == 44, "add [1][1] == 44");

    /* Subtraction: result = [-9 -18; -27 -36] */
    rc = tiku_kits_matrix_sub(&result, &a, &b);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "sub returns OK");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 0) == -9, "sub [0][0] == -9");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 1) == -36,
                "sub [1][1] == -36");

    /* In-place aliasing: a = a + b */
    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_set(&a, 0, 0, 1);
    tiku_kits_matrix_set(&a, 0, 1, 2);
    tiku_kits_matrix_set(&a, 1, 0, 3);
    tiku_kits_matrix_set(&a, 1, 1, 4);

    rc = tiku_kits_matrix_add(&a, &a, &b);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "in-place add returns OK");
    TEST_ASSERT(tiku_kits_matrix_get(&a, 0, 0) == 11,
                "in-place add [0][0] == 11");
}

/*---------------------------------------------------------------------------*/
/* TEST 6: MULTIPLICATION                                                    */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_mul(void)
{
    struct tiku_kits_matrix a, b, result;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Mul ---\n");

    /*
     * a = [1 2; 3 4]  (2x2)
     * b = [5 6; 7 8]  (2x2)
     * result = [1*5+2*7  1*6+2*8;  3*5+4*7  3*6+4*8]
     *        = [19 22; 43 50]
     */
    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_set(&a, 0, 0, 1);
    tiku_kits_matrix_set(&a, 0, 1, 2);
    tiku_kits_matrix_set(&a, 1, 0, 3);
    tiku_kits_matrix_set(&a, 1, 1, 4);

    tiku_kits_matrix_init(&b, 2, 2);
    tiku_kits_matrix_set(&b, 0, 0, 5);
    tiku_kits_matrix_set(&b, 0, 1, 6);
    tiku_kits_matrix_set(&b, 1, 0, 7);
    tiku_kits_matrix_set(&b, 1, 1, 8);

    rc = tiku_kits_matrix_mul(&result, &a, &b);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "mul 2x2 * 2x2 returns OK");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 0) == 19,
                "mul [0][0] == 19");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 1) == 22,
                "mul [0][1] == 22");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 0) == 43,
                "mul [1][0] == 43");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 1) == 50,
                "mul [1][1] == 50");

    /*
     * Non-square: a (2x3) * b (3x1)
     * a = [1 0 2; 0 1 3]
     * b = [4; 5; 6]
     * result = [1*4+0*5+2*6; 0*4+1*5+3*6] = [16; 23]
     */
    tiku_kits_matrix_init(&a, 2, 3);
    tiku_kits_matrix_set(&a, 0, 0, 1);
    tiku_kits_matrix_set(&a, 0, 2, 2);
    tiku_kits_matrix_set(&a, 1, 1, 1);
    tiku_kits_matrix_set(&a, 1, 2, 3);

    tiku_kits_matrix_init(&b, 3, 1);
    tiku_kits_matrix_set(&b, 0, 0, 4);
    tiku_kits_matrix_set(&b, 1, 0, 5);
    tiku_kits_matrix_set(&b, 2, 0, 6);

    rc = tiku_kits_matrix_mul(&result, &a, &b);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "mul 2x3 * 3x1 returns OK");
    TEST_ASSERT(tiku_kits_matrix_rows(&result) == 2,
                "result rows == 2");
    TEST_ASSERT(tiku_kits_matrix_cols(&result) == 1,
                "result cols == 1");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 0) == 16,
                "mul [0][0] == 16");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 0) == 23,
                "mul [1][0] == 23");

    /* A * I = A */
    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_set(&a, 0, 0, 7);
    tiku_kits_matrix_set(&a, 0, 1, 8);
    tiku_kits_matrix_set(&a, 1, 0, 9);
    tiku_kits_matrix_set(&a, 1, 1, 10);

    tiku_kits_matrix_identity(&b, 2);
    tiku_kits_matrix_mul(&result, &a, &b);
    TEST_ASSERT(tiku_kits_matrix_equal(&result, &a) == 1,
                "A * I == A");
}

/*---------------------------------------------------------------------------*/
/* TEST 7: SCALAR MULTIPLICATION                                             */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_scale(void)
{
    struct tiku_kits_matrix a, result;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Scale ---\n");

    /* a = [2 -3; 4 0], scalar = 3 -> [6 -9; 12 0] */
    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_set(&a, 0, 0, 2);
    tiku_kits_matrix_set(&a, 0, 1, -3);
    tiku_kits_matrix_set(&a, 1, 0, 4);
    tiku_kits_matrix_set(&a, 1, 1, 0);

    rc = tiku_kits_matrix_scale(&result, &a, 3);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "scale returns OK");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 0) == 6,
                "scale [0][0] == 6");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 1) == -9,
                "scale [0][1] == -9");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 0) == 12,
                "scale [1][0] == 12");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 1) == 0,
                "scale [1][1] == 0");

    /* Scale by 0 gives zero matrix */
    rc = tiku_kits_matrix_scale(&result, &a, 0);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "scale by 0 returns OK");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 0) == 0,
                "scale by 0 gives zero");

    /* In-place aliasing */
    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_set(&a, 0, 0, 5);
    tiku_kits_matrix_set(&a, 1, 1, 10);

    rc = tiku_kits_matrix_scale(&a, &a, 2);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "in-place scale OK");
    TEST_ASSERT(tiku_kits_matrix_get(&a, 0, 0) == 10,
                "in-place scale [0][0] == 10");
    TEST_ASSERT(tiku_kits_matrix_get(&a, 1, 1) == 20,
                "in-place scale [1][1] == 20");
}

/*---------------------------------------------------------------------------*/
/* TEST 8: TRANSPOSE                                                         */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_transpose(void)
{
    struct tiku_kits_matrix a, result;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Transpose ---\n");

    /* a = [1 2 3; 4 5 6]  (2x3) -> result = [1 4; 2 5; 3 6]  (3x2) */
    tiku_kits_matrix_init(&a, 2, 3);
    tiku_kits_matrix_set(&a, 0, 0, 1);
    tiku_kits_matrix_set(&a, 0, 1, 2);
    tiku_kits_matrix_set(&a, 0, 2, 3);
    tiku_kits_matrix_set(&a, 1, 0, 4);
    tiku_kits_matrix_set(&a, 1, 1, 5);
    tiku_kits_matrix_set(&a, 1, 2, 6);

    rc = tiku_kits_matrix_transpose(&result, &a);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "transpose returns OK");
    TEST_ASSERT(tiku_kits_matrix_rows(&result) == 3,
                "transpose rows == 3");
    TEST_ASSERT(tiku_kits_matrix_cols(&result) == 2,
                "transpose cols == 2");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 0) == 1,
                "T[0][0] == 1");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 0, 1) == 4,
                "T[0][1] == 4");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 1, 0) == 2,
                "T[1][0] == 2");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 2, 0) == 3,
                "T[2][0] == 3");
    TEST_ASSERT(tiku_kits_matrix_get(&result, 2, 1) == 6,
                "T[2][1] == 6");
}

/*---------------------------------------------------------------------------*/
/* TEST 9: DETERMINANT                                                       */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_det(void)
{
    struct tiku_kits_matrix m;
    tiku_kits_matrix_elem_t det;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Determinant ---\n");

    /* 1x1: det([5]) = 5 */
    tiku_kits_matrix_init(&m, 1, 1);
    tiku_kits_matrix_set(&m, 0, 0, 5);
    rc = tiku_kits_matrix_det(&m, &det);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "det 1x1 returns OK");
    TEST_ASSERT(det == 5, "det([5]) == 5");

    /* 2x2: det([1 2; 3 4]) = 1*4 - 2*3 = -2 */
    tiku_kits_matrix_init(&m, 2, 2);
    tiku_kits_matrix_set(&m, 0, 0, 1);
    tiku_kits_matrix_set(&m, 0, 1, 2);
    tiku_kits_matrix_set(&m, 1, 0, 3);
    tiku_kits_matrix_set(&m, 1, 1, 4);
    rc = tiku_kits_matrix_det(&m, &det);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "det 2x2 returns OK");
    TEST_ASSERT(det == -2, "det([1 2; 3 4]) == -2");

    /*
     * 3x3: det([1 2 3; 0 4 5; 1 0 6])
     * = 1*(4*6-5*0) - 2*(0*6-5*1) + 3*(0*0-4*1)
     * = 1*24 - 2*(-5) + 3*(-4)
     * = 24 + 10 - 12 = 22
     */
    tiku_kits_matrix_init(&m, 3, 3);
    tiku_kits_matrix_set(&m, 0, 0, 1);
    tiku_kits_matrix_set(&m, 0, 1, 2);
    tiku_kits_matrix_set(&m, 0, 2, 3);
    tiku_kits_matrix_set(&m, 1, 0, 0);
    tiku_kits_matrix_set(&m, 1, 1, 4);
    tiku_kits_matrix_set(&m, 1, 2, 5);
    tiku_kits_matrix_set(&m, 2, 0, 1);
    tiku_kits_matrix_set(&m, 2, 1, 0);
    tiku_kits_matrix_set(&m, 2, 2, 6);
    rc = tiku_kits_matrix_det(&m, &det);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "det 3x3 returns OK");
    TEST_ASSERT(det == 22, "det 3x3 == 22");

    /* Singular matrix: det = 0 */
    tiku_kits_matrix_init(&m, 2, 2);
    tiku_kits_matrix_set(&m, 0, 0, 1);
    tiku_kits_matrix_set(&m, 0, 1, 2);
    tiku_kits_matrix_set(&m, 1, 0, 2);
    tiku_kits_matrix_set(&m, 1, 1, 4);
    tiku_kits_matrix_det(&m, &det);
    TEST_ASSERT(det == 0, "singular matrix det == 0");

    /* Identity matrix: det = 1 */
    tiku_kits_matrix_identity(&m, 3);
    tiku_kits_matrix_det(&m, &det);
    TEST_ASSERT(det == 1, "identity 3x3 det == 1");

    /* Non-square matrix rejected */
    tiku_kits_matrix_init(&m, 2, 3);
    rc = tiku_kits_matrix_det(&m, &det);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_DIM, "det non-square rejected");
}

/*---------------------------------------------------------------------------*/
/* TEST 10: TRACE                                                            */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_trace(void)
{
    struct tiku_kits_matrix m;
    tiku_kits_matrix_elem_t tr;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Trace ---\n");

    /* trace([1 2; 3 4]) = 1 + 4 = 5 */
    tiku_kits_matrix_init(&m, 2, 2);
    tiku_kits_matrix_set(&m, 0, 0, 1);
    tiku_kits_matrix_set(&m, 0, 1, 2);
    tiku_kits_matrix_set(&m, 1, 0, 3);
    tiku_kits_matrix_set(&m, 1, 1, 4);

    rc = tiku_kits_matrix_trace(&m, &tr);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_OK, "trace 2x2 returns OK");
    TEST_ASSERT(tr == 5, "trace([1 2; 3 4]) == 5");

    /* Identity trace = n */
    tiku_kits_matrix_identity(&m, 4);
    tiku_kits_matrix_trace(&m, &tr);
    TEST_ASSERT(tr == 4, "trace(I_4) == 4");

    /* Non-square rejected */
    tiku_kits_matrix_init(&m, 2, 3);
    rc = tiku_kits_matrix_trace(&m, &tr);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_DIM, "trace non-square rejected");
}

/*---------------------------------------------------------------------------*/
/* TEST 11: DIMENSION MISMATCH ERRORS                                        */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_dim_mismatch(void)
{
    struct tiku_kits_matrix a, b, result;
    int rc;

    TEST_PRINT("\n--- Test: Matrix Dimension Mismatch ---\n");

    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_init(&b, 3, 3);

    rc = tiku_kits_matrix_add(&result, &a, &b);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_DIM, "add dim mismatch rejected");

    rc = tiku_kits_matrix_sub(&result, &a, &b);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_DIM, "sub dim mismatch rejected");

    /* a(2x2) * b(3x3): a->cols(2) != b->rows(3) */
    rc = tiku_kits_matrix_mul(&result, &a, &b);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_DIM, "mul dim mismatch rejected");
}

/*---------------------------------------------------------------------------*/
/* TEST 12: NULL POINTER INPUTS                                              */
/*---------------------------------------------------------------------------*/

void test_kits_matrix_null_inputs(void)
{
    struct tiku_kits_matrix m, result;
    tiku_kits_matrix_elem_t val;
    int rc;

    TEST_PRINT("\n--- Test: Matrix NULL Inputs ---\n");

    tiku_kits_matrix_init(&m, 2, 2);

    rc = tiku_kits_matrix_init(NULL, 2, 2);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "init NULL rejected");

    rc = tiku_kits_matrix_zero(NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "zero NULL rejected");

    rc = tiku_kits_matrix_identity(NULL, 2);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "identity NULL rejected");

    rc = tiku_kits_matrix_copy(NULL, &m);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "copy NULL dst rejected");

    rc = tiku_kits_matrix_copy(&result, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "copy NULL src rejected");

    rc = tiku_kits_matrix_add(NULL, &m, &m);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "add NULL result rejected");

    rc = tiku_kits_matrix_sub(&result, NULL, &m);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "sub NULL a rejected");

    rc = tiku_kits_matrix_mul(&result, &m, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "mul NULL b rejected");

    rc = tiku_kits_matrix_scale(NULL, &m, 2);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "scale NULL rejected");

    rc = tiku_kits_matrix_transpose(NULL, &m);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "transpose NULL rejected");

    rc = tiku_kits_matrix_det(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "det NULL m rejected");

    rc = tiku_kits_matrix_det(&m, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "det NULL out rejected");

    rc = tiku_kits_matrix_trace(NULL, &val);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "trace NULL m rejected");

    rc = tiku_kits_matrix_trace(&m, NULL);
    TEST_ASSERT(rc == TIKU_KITS_MATHS_ERR_NULL, "trace NULL out rejected");

    TEST_ASSERT(tiku_kits_matrix_rows(NULL) == 0, "rows NULL returns 0");
    TEST_ASSERT(tiku_kits_matrix_cols(NULL) == 0, "cols NULL returns 0");
    TEST_ASSERT(tiku_kits_matrix_is_square(NULL) == 0,
                "is_square NULL returns 0");
}
