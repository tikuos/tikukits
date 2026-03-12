/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_matrix.h - Basic matrix operations library
 *
 * Platform-independent matrix library for embedded systems. All storage
 * is statically allocated using a compile-time maximum dimension. The
 * element type is configurable via TIKU_MATRIX_ELEM_TYPE.
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

#ifndef TIKU_MATRIX_H_
#define TIKU_MATRIX_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum matrix dimension (rows or columns).
 * Override before including this header to change the limit.
 */
#ifndef TIKU_MATRIX_MAX_SIZE
#define TIKU_MATRIX_MAX_SIZE 4
#endif

/**
 * Matrix element type.
 * Defaults to int32_t for integer-only targets (no FPU).
 * Define as float or int16_t before including if needed.
 */
#ifndef TIKU_MATRIX_ELEM_TYPE
#define TIKU_MATRIX_ELEM_TYPE int32_t
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_matrix_elem_t
 *  @brief Element type used in all matrix operations
 */
typedef TIKU_MATRIX_ELEM_TYPE tiku_matrix_elem_t;

/**
 * @struct tiku_matrix
 * @brief Fixed-capacity matrix with static storage
 *
 * Data is stored in row-major order. Actual dimensions are tracked
 * by rows and cols; only elements within those bounds are valid.
 *
 * Declare matrices as static or local variables -- no heap required.
 *
 * Example:
 * @code
 *   struct tiku_matrix a;
 *   tiku_matrix_init(&a, 3, 3);
 *   tiku_matrix_set(&a, 0, 0, 42);
 * @endcode
 */
struct tiku_matrix {
    uint8_t rows; /**< Number of active rows */
    uint8_t cols; /**< Number of active columns */
    /** Element storage (row-major) */
    tiku_matrix_elem_t data[TIKU_MATRIX_MAX_SIZE][TIKU_MATRIX_MAX_SIZE];
};

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

#define TIKU_MATRIX_OK          0  /**< Operation succeeded */
#define TIKU_MATRIX_ERR_DIM    -1  /**< Dimension mismatch */
#define TIKU_MATRIX_ERR_SIZE   -2  /**< Exceeds TIKU_MATRIX_MAX_SIZE */
#define TIKU_MATRIX_ERR_SINGULAR -3 /**< Matrix is singular */
#define TIKU_MATRIX_ERR_NULL   -4  /**< NULL pointer argument */

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a matrix with given dimensions, all elements zero
 * @param m    Matrix to initialize
 * @param rows Number of rows (1..TIKU_MATRIX_MAX_SIZE)
 * @param cols Number of columns (1..TIKU_MATRIX_MAX_SIZE)
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_SIZE
 */
int tiku_matrix_init(struct tiku_matrix *m, uint8_t rows, uint8_t cols);

/**
 * @brief Set all elements to zero, preserving dimensions
 * @param m Matrix to zero
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_NULL
 */
int tiku_matrix_zero(struct tiku_matrix *m);

/**
 * @brief Set matrix to the identity matrix
 * @param m Matrix (must be square)
 * @param n Dimension (n x n)
 * @return TIKU_MATRIX_OK, TIKU_MATRIX_ERR_SIZE, or TIKU_MATRIX_ERR_NULL
 */
int tiku_matrix_identity(struct tiku_matrix *m, uint8_t n);

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set a single element
 * @param m   Matrix
 * @param row Row index (0-based)
 * @param col Column index (0-based)
 * @param val Value to set
 */
void tiku_matrix_set(struct tiku_matrix *m, uint8_t row, uint8_t col,
                     tiku_matrix_elem_t val);

/**
 * @brief Get a single element
 * @param m   Matrix
 * @param row Row index (0-based)
 * @param col Column index (0-based)
 * @return Element value, or 0 if out of bounds
 */
tiku_matrix_elem_t tiku_matrix_get(const struct tiku_matrix *m,
                                   uint8_t row, uint8_t col);

/*---------------------------------------------------------------------------*/
/* COPY AND COMPARISON                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Copy matrix src to dst
 * @param dst Destination matrix
 * @param src Source matrix
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_NULL
 */
int tiku_matrix_copy(struct tiku_matrix *dst,
                     const struct tiku_matrix *src);

/**
 * @brief Check if two matrices are equal (same dimensions and elements)
 * @param a First matrix
 * @param b Second matrix
 * @return 1 if equal, 0 otherwise
 */
int tiku_matrix_equal(const struct tiku_matrix *a,
                      const struct tiku_matrix *b);

/*---------------------------------------------------------------------------*/
/* ARITHMETIC OPERATIONS                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Matrix addition: result = a + b
 * @param result Output matrix (may alias a or b)
 * @param a      Left operand
 * @param b      Right operand
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_DIM
 *
 * Matrices a and b must have the same dimensions.
 */
int tiku_matrix_add(struct tiku_matrix *result,
                    const struct tiku_matrix *a,
                    const struct tiku_matrix *b);

/**
 * @brief Matrix subtraction: result = a - b
 * @param result Output matrix (may alias a or b)
 * @param a      Left operand
 * @param b      Right operand
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_DIM
 *
 * Matrices a and b must have the same dimensions.
 */
int tiku_matrix_sub(struct tiku_matrix *result,
                    const struct tiku_matrix *a,
                    const struct tiku_matrix *b);

/**
 * @brief Matrix multiplication: result = a * b
 * @param result Output matrix (must NOT alias a or b)
 * @param a      Left operand (m x n)
 * @param b      Right operand (n x p)
 * @return TIKU_MATRIX_OK, TIKU_MATRIX_ERR_DIM, or TIKU_MATRIX_ERR_SIZE
 *
 * Requires a->cols == b->rows. Result dimensions: a->rows x b->cols.
 */
int tiku_matrix_mul(struct tiku_matrix *result,
                    const struct tiku_matrix *a,
                    const struct tiku_matrix *b);

/**
 * @brief Scalar multiplication: result = scalar * a
 * @param result Output matrix (may alias a)
 * @param a      Input matrix
 * @param scalar Scalar value
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_NULL
 */
int tiku_matrix_scale(struct tiku_matrix *result,
                      const struct tiku_matrix *a,
                      tiku_matrix_elem_t scalar);

/*---------------------------------------------------------------------------*/
/* MATRIX TRANSFORMS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Transpose: result = a^T
 * @param result Output matrix (must NOT alias a)
 * @param a      Input matrix
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_NULL
 */
int tiku_matrix_transpose(struct tiku_matrix *result,
                          const struct tiku_matrix *a);

/*---------------------------------------------------------------------------*/
/* SQUARE MATRIX OPERATIONS                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the determinant of a square matrix
 * @param m   Square matrix (up to TIKU_MATRIX_MAX_SIZE x TIKU_MATRIX_MAX_SIZE)
 * @param det Output: determinant value
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_DIM (if not square)
 *
 * Uses cofactor expansion. Practical for small matrices (up to 4x4).
 */
int tiku_matrix_det(const struct tiku_matrix *m,
                    tiku_matrix_elem_t *det);

/**
 * @brief Compute the trace (sum of diagonal elements)
 * @param m     Square matrix
 * @param trace Output: trace value
 * @return TIKU_MATRIX_OK or TIKU_MATRIX_ERR_DIM (if not square)
 */
int tiku_matrix_trace(const struct tiku_matrix *m,
                      tiku_matrix_elem_t *trace);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the number of rows
 * @param m Matrix
 * @return Number of rows
 */
uint8_t tiku_matrix_rows(const struct tiku_matrix *m);

/**
 * @brief Get the number of columns
 * @param m Matrix
 * @return Number of columns
 */
uint8_t tiku_matrix_cols(const struct tiku_matrix *m);

/**
 * @brief Check if a matrix is square
 * @param m Matrix
 * @return 1 if square, 0 otherwise
 */
int tiku_matrix_is_square(const struct tiku_matrix *m);

#endif /* TIKU_MATRIX_H_ */
