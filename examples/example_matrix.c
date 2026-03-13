/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Matrix Operations
 *
 * Demonstrates the matrix library for embedded linear algebra:
 *   - Matrix initialization and element access
 *   - Addition, subtraction, scalar multiplication
 *   - Matrix multiplication and transpose
 *   - Determinant and trace of square matrices
 *
 * All matrices use static storage (no heap). Maximum dimension is
 * controlled by TIKU_KITS_MATRIX_MAX_SIZE (default 4).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../maths/tiku_kits_maths.h"
#include "../maths/linear_algebra/tiku_kits_matrix.h"

/*---------------------------------------------------------------------------*/
/* Helper: print a matrix                                                    */
/*---------------------------------------------------------------------------*/

static void print_matrix(const char *label, const struct tiku_kits_matrix *m)
{
    uint8_t r, c;

    printf("  %s (%dx%d):\n", label,
           tiku_kits_matrix_rows(m), tiku_kits_matrix_cols(m));
    for (r = 0; r < tiku_kits_matrix_rows(m); r++) {
        printf("    [");
        for (c = 0; c < tiku_kits_matrix_cols(m); c++) {
            printf("%4ld", (long)tiku_kits_matrix_get(m, r, c));
            if (c < tiku_kits_matrix_cols(m) - 1) {
                printf(", ");
            }
        }
        printf("]\n");
    }
}

/*---------------------------------------------------------------------------*/
/* Example 1: Basic creation, identity, and element access                   */
/*---------------------------------------------------------------------------*/

static void example_matrix_basics(void)
{
    struct tiku_kits_matrix m;

    printf("--- Matrix Basics ---\n");

    /* Create a 3x3 identity matrix */
    tiku_kits_matrix_identity(&m, 3);
    print_matrix("3x3 Identity", &m);

    /* Modify elements */
    tiku_kits_matrix_set(&m, 0, 1, 5);
    tiku_kits_matrix_set(&m, 1, 2, -3);
    tiku_kits_matrix_set(&m, 2, 0, 7);
    print_matrix("Modified", &m);

    printf("  Is square: %s\n",
           tiku_kits_matrix_is_square(&m) ? "yes" : "no");
    printf("  Element [2][0]: %ld\n",
           (long)tiku_kits_matrix_get(&m, 2, 0));
}

/*---------------------------------------------------------------------------*/
/* Example 2: Arithmetic -- add, subtract, scale                             */
/*---------------------------------------------------------------------------*/

static void example_matrix_arithmetic(void)
{
    struct tiku_kits_matrix a, b, result;

    printf("--- Matrix Arithmetic ---\n");

    /* Set up two 2x2 matrices */
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

    print_matrix("A", &a);
    print_matrix("B", &b);

    /* Addition: A + B */
    tiku_kits_matrix_add(&result, &a, &b);
    print_matrix("A + B", &result);

    /* Subtraction: A - B */
    tiku_kits_matrix_sub(&result, &a, &b);
    print_matrix("A - B", &result);

    /* Scalar multiply: 3 * A */
    tiku_kits_matrix_scale(&result, &a, 3);
    print_matrix("3 * A", &result);
}

/*---------------------------------------------------------------------------*/
/* Example 3: Matrix multiplication                                          */
/*---------------------------------------------------------------------------*/

static void example_matrix_multiply(void)
{
    struct tiku_kits_matrix a, b, result;

    printf("--- Matrix Multiplication ---\n");

    /* A is 2x3 */
    tiku_kits_matrix_init(&a, 2, 3);
    tiku_kits_matrix_set(&a, 0, 0, 1);
    tiku_kits_matrix_set(&a, 0, 1, 2);
    tiku_kits_matrix_set(&a, 0, 2, 3);
    tiku_kits_matrix_set(&a, 1, 0, 4);
    tiku_kits_matrix_set(&a, 1, 1, 5);
    tiku_kits_matrix_set(&a, 1, 2, 6);

    /* B is 3x2 */
    tiku_kits_matrix_init(&b, 3, 2);
    tiku_kits_matrix_set(&b, 0, 0, 7);
    tiku_kits_matrix_set(&b, 0, 1, 8);
    tiku_kits_matrix_set(&b, 1, 0, 9);
    tiku_kits_matrix_set(&b, 1, 1, 10);
    tiku_kits_matrix_set(&b, 2, 0, 11);
    tiku_kits_matrix_set(&b, 2, 1, 12);

    print_matrix("A (2x3)", &a);
    print_matrix("B (3x2)", &b);

    /* Result is 2x2: must NOT alias a or b */
    tiku_kits_matrix_mul(&result, &a, &b);
    print_matrix("A * B (2x2)", &result);
    /* Expected: [58, 64; 139, 154] */
}

/*---------------------------------------------------------------------------*/
/* Example 4: Transpose                                                      */
/*---------------------------------------------------------------------------*/

static void example_matrix_transpose(void)
{
    struct tiku_kits_matrix a, at;

    printf("--- Transpose ---\n");

    tiku_kits_matrix_init(&a, 2, 3);
    tiku_kits_matrix_set(&a, 0, 0, 1);
    tiku_kits_matrix_set(&a, 0, 1, 2);
    tiku_kits_matrix_set(&a, 0, 2, 3);
    tiku_kits_matrix_set(&a, 1, 0, 4);
    tiku_kits_matrix_set(&a, 1, 1, 5);
    tiku_kits_matrix_set(&a, 1, 2, 6);

    print_matrix("A", &a);

    tiku_kits_matrix_transpose(&at, &a);
    print_matrix("A^T", &at);
}

/*---------------------------------------------------------------------------*/
/* Example 5: Determinant and trace                                          */
/*---------------------------------------------------------------------------*/

static void example_matrix_det_trace(void)
{
    struct tiku_kits_matrix m;
    tiku_kits_matrix_elem_t det, trace;

    printf("--- Determinant & Trace ---\n");

    /* 3x3 matrix */
    tiku_kits_matrix_init(&m, 3, 3);
    tiku_kits_matrix_set(&m, 0, 0, 6);
    tiku_kits_matrix_set(&m, 0, 1, 1);
    tiku_kits_matrix_set(&m, 0, 2, 1);
    tiku_kits_matrix_set(&m, 1, 0, 4);
    tiku_kits_matrix_set(&m, 1, 1, -2);
    tiku_kits_matrix_set(&m, 1, 2, 5);
    tiku_kits_matrix_set(&m, 2, 0, 2);
    tiku_kits_matrix_set(&m, 2, 1, 8);
    tiku_kits_matrix_set(&m, 2, 2, 7);

    print_matrix("M", &m);

    tiku_kits_matrix_det(&m, &det);
    printf("  det(M) = %ld\n", (long)det);
    /* Expected: 6*(-2*7 - 5*8) - 1*(4*7 - 5*2) + 1*(4*8 - (-2)*2)
     *         = 6*(-54) - 1*(18) + 1*(36)
     *         = -324 - 18 + 36 = -306 */

    tiku_kits_matrix_trace(&m, &trace);
    printf("  trace(M) = %ld\n", (long)trace);
    /* Expected: 6 + (-2) + 7 = 11 */
}

/*---------------------------------------------------------------------------*/
/* Example 6: Copy and equality check                                        */
/*---------------------------------------------------------------------------*/

static void example_matrix_copy_equal(void)
{
    struct tiku_kits_matrix a, b;

    printf("--- Copy & Equality ---\n");

    tiku_kits_matrix_init(&a, 2, 2);
    tiku_kits_matrix_set(&a, 0, 0, 10);
    tiku_kits_matrix_set(&a, 0, 1, 20);
    tiku_kits_matrix_set(&a, 1, 0, 30);
    tiku_kits_matrix_set(&a, 1, 1, 40);

    tiku_kits_matrix_copy(&b, &a);

    printf("  A == B after copy: %s\n",
           tiku_kits_matrix_equal(&a, &b) ? "yes" : "no");

    tiku_kits_matrix_set(&b, 1, 1, 99);
    printf("  A == B after modify: %s\n",
           tiku_kits_matrix_equal(&a, &b) ? "yes" : "no");
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_matrix_run(void)
{
    printf("=== TikuKits Matrix Examples ===\n\n");

    example_matrix_basics();
    printf("\n");

    example_matrix_arithmetic();
    printf("\n");

    example_matrix_multiply();
    printf("\n");

    example_matrix_transpose();
    printf("\n");

    example_matrix_det_trace();
    printf("\n");

    example_matrix_copy_equal();
    printf("\n");
}
