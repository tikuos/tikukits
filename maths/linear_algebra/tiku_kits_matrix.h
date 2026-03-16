/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_matrix.h - Basic matrix operations library
 *
 * Platform-independent matrix library for embedded systems. All storage
 * is statically allocated using a compile-time maximum dimension. The
 * element type is configurable via TIKU_KITS_MATRIX_ELEM_TYPE.
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

#ifndef TIKU_KITS_MATRIX_H_
#define TIKU_KITS_MATRIX_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_maths.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum matrix dimension (rows or columns).
 *
 * This compile-time constant defines the upper bound on both the row
 * and column count.  Each matrix instance reserves a full
 * MAX_SIZE x MAX_SIZE element grid in its static storage, so choose
 * a value that balances memory usage against the largest matrix your
 * application needs.  Typical embedded use: 4 (for 4x4 transforms)
 * or 8 (for small feature vectors).
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_MATRIX_MAX_SIZE 8
 *   #include "tiku_kits_matrix.h"
 * @endcode
 */
#ifndef TIKU_KITS_MATRIX_MAX_SIZE
#define TIKU_KITS_MATRIX_MAX_SIZE 4
#endif

/**
 * @brief Matrix element type.
 *
 * Defaults to int32_t, which is safe for integer-only targets with
 * no FPU (e.g. MSP430).  Change to int16_t to halve memory on very
 * small matrices, or to float if your target has hardware FP support.
 *
 * Override before including this header to change the type:
 * @code
 *   #define TIKU_KITS_MATRIX_ELEM_TYPE int16_t
 *   #include "tiku_kits_matrix.h"
 * @endcode
 */
#ifndef TIKU_KITS_MATRIX_ELEM_TYPE
#define TIKU_KITS_MATRIX_ELEM_TYPE int32_t
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_matrix_elem_t
 *  @brief Element type used in all matrix operations
 */
typedef TIKU_KITS_MATRIX_ELEM_TYPE tiku_kits_matrix_elem_t;

/**
 * @struct tiku_kits_matrix
 * @brief Fixed-capacity matrix with contiguous static storage
 *
 * A general-purpose, two-dimensional container that stores elements in
 * a row-major, statically allocated 2-D array.  Because all storage
 * lives inside the struct itself, no heap allocation is needed --
 * just declare the matrix as a static or local variable.
 *
 * Two dimension fields are tracked independently:
 *   - @c rows -- the number of active rows set by init (must be
 *     <= TIKU_KITS_MATRIX_MAX_SIZE).
 *   - @c cols -- the number of active columns set by init (must be
 *     <= TIKU_KITS_MATRIX_MAX_SIZE).
 *
 * Only elements within [0..rows) x [0..cols) are logically valid;
 * the remaining slots in the backing array are zeroed at init time
 * but should not be relied upon.
 *
 * @note Element type is controlled by tiku_kits_matrix_elem_t
 *       (default int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_MATRIX_ELEM_TYPE=int16_t to change it
 *       globally for all matrix operations.
 *
 * Example:
 * @code
 *   struct tiku_kits_matrix a, b, c;
 *   tiku_kits_matrix_init(&a, 3, 3);
 *   tiku_kits_matrix_init(&b, 3, 3);
 *   tiku_kits_matrix_set(&a, 0, 0, 42);
 *   tiku_kits_matrix_add(&c, &a, &b);
 *   // c[0][0] == 42, all other elements == 0
 * @endcode
 */
struct tiku_kits_matrix {
    uint8_t rows; /**< Number of active rows (set by init) */
    uint8_t cols; /**< Number of active columns (set by init) */
    /** Element storage in row-major layout (row index first) */
    tiku_kits_matrix_elem_t data[TIKU_KITS_MATRIX_MAX_SIZE][TIKU_KITS_MATRIX_MAX_SIZE];
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a matrix with given dimensions, all elements zero
 *
 * Resets the matrix to an empty (all-zero) state and sets the runtime
 * dimensions.  The entire backing 2-D buffer is zeroed so that reads
 * of any in-bounds element return a deterministic value.
 *
 * @param m    Matrix to initialize (must not be NULL)
 * @param rows Number of rows (1..TIKU_KITS_MATRIX_MAX_SIZE)
 * @param cols Number of columns (1..TIKU_KITS_MATRIX_MAX_SIZE)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if rows or cols is 0 or exceeds
 *         MAX_SIZE
 */
int tiku_kits_matrix_init(struct tiku_kits_matrix *m, uint8_t rows, uint8_t cols);

/**
 * @brief Set all elements to zero, preserving dimensions
 *
 * Zeros the entire backing buffer without changing the rows/cols
 * dimensions.  Useful for reusing a matrix struct between operations
 * without the overhead of a full re-init.
 *
 * @param m Matrix to zero (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m is NULL
 */
int tiku_kits_matrix_zero(struct tiku_kits_matrix *m);

/**
 * @brief Set matrix to the n x n identity matrix
 *
 * Initializes the matrix as a square n x n identity (1 on the
 * diagonal, 0 elsewhere).  The dimensions are overwritten to n x n.
 *
 * @param m Matrix (must not be NULL)
 * @param n Dimension (1..TIKU_KITS_MATRIX_MAX_SIZE)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m is NULL,
 *         TIKU_KITS_MATHS_ERR_SIZE if n is 0 or exceeds MAX_SIZE
 */
int tiku_kits_matrix_identity(struct tiku_kits_matrix *m, uint8_t n);

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set a single element at (row, col)
 *
 * Writes @p val into the matrix at the given position.  If @p m is
 * NULL or the indices are out of bounds the call is silently ignored
 * (void return -- no error code).  This keeps element-level access
 * lightweight; use init/add/mul for validated operations.
 *
 * @param m   Matrix (NULL-safe -- call is a no-op)
 * @param row Row index (0-based, must be < m->rows)
 * @param col Column index (0-based, must be < m->cols)
 * @param val Value to store
 */
void tiku_kits_matrix_set(struct tiku_kits_matrix *m, uint8_t row, uint8_t col,
                     tiku_kits_matrix_elem_t val);

/**
 * @brief Read a single element at (row, col)
 *
 * Returns the value stored at the given position.  If @p m is NULL
 * or the indices are out of bounds, returns 0 as a safe sentinel
 * rather than triggering undefined behaviour.
 *
 * @param m   Matrix (NULL-safe -- returns 0)
 * @param row Row index (0-based, must be < m->rows)
 * @param col Column index (0-based, must be < m->cols)
 * @return Element value at (row, col), or 0 if out of bounds or
 *         m is NULL
 */
tiku_kits_matrix_elem_t tiku_kits_matrix_get(const struct tiku_kits_matrix *m,
                                   uint8_t row, uint8_t col);

/*---------------------------------------------------------------------------*/
/* COPY AND COMPARISON                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Deep-copy a matrix from src to dst
 *
 * Copies the dimensions and the entire backing buffer from @p src
 * into @p dst.  After the call, dst is an independent clone --
 * modifying one does not affect the other.  Uses memcpy on the
 * full backing array for a single, fast bulk copy.
 *
 * @param dst Destination matrix (must not be NULL)
 * @param src Source matrix (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if dst or src is NULL
 */
int tiku_kits_matrix_copy(struct tiku_kits_matrix *dst,
                     const struct tiku_kits_matrix *src);

/**
 * @brief Check if two matrices are element-wise equal
 *
 * Compares dimensions first; if they differ the matrices are unequal.
 * Then scans all elements within [0..rows) x [0..cols) and returns
 * false at the first mismatch.  O(rows * cols) worst case.
 *
 * @param a First matrix (NULL-safe -- returns 0)
 * @param b Second matrix (NULL-safe -- returns 0)
 * @return 1 if dimensions and all elements match, 0 otherwise
 *         (including when either pointer is NULL)
 */
int tiku_kits_matrix_equal(const struct tiku_kits_matrix *a,
                      const struct tiku_kits_matrix *b);

/*---------------------------------------------------------------------------*/
/* ARITHMETIC OPERATIONS                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Element-wise matrix addition: result = a + b
 *
 * Adds corresponding elements of @p a and @p b and writes the sums
 * into @p result.  Both operands must have identical dimensions.
 * O(rows * cols).  @p result may safely alias @p a or @p b for
 * in-place addition.
 *
 * @param result Output matrix (must not be NULL; may alias a or b)
 * @param a      Left operand (must not be NULL)
 * @param b      Right operand (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_MATHS_ERR_DIM if a and b have different dimensions
 */
int tiku_kits_matrix_add(struct tiku_kits_matrix *result,
                    const struct tiku_kits_matrix *a,
                    const struct tiku_kits_matrix *b);

/**
 * @brief Element-wise matrix subtraction: result = a - b
 *
 * Subtracts corresponding elements of @p b from @p a and writes the
 * differences into @p result.  Both operands must have identical
 * dimensions.  O(rows * cols).  @p result may safely alias @p a or
 * @p b for in-place subtraction.
 *
 * @param result Output matrix (must not be NULL; may alias a or b)
 * @param a      Left operand (must not be NULL)
 * @param b      Right operand (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_MATHS_ERR_DIM if a and b have different dimensions
 */
int tiku_kits_matrix_sub(struct tiku_kits_matrix *result,
                    const struct tiku_kits_matrix *a,
                    const struct tiku_kits_matrix *b);

/**
 * @brief Matrix multiplication: result = a * b
 *
 * Performs the standard O(m * n * p) matrix product.  The inner
 * dimensions must agree: a->cols == b->rows.  The output matrix
 * will have dimensions a->rows x b->cols.
 *
 * @warning @p result must NOT alias @p a or @p b.  The result is
 *          built element-by-element from input data, so aliasing
 *          would corrupt the computation.
 *
 * @param result Output matrix (must not be NULL; must not alias a or b)
 * @param a      Left operand, m x n (must not be NULL)
 * @param b      Right operand, n x p (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if any pointer is NULL,
 *         TIKU_KITS_MATHS_ERR_DIM if a->cols != b->rows,
 *         TIKU_KITS_MATHS_ERR_SIZE if result dimensions exceed MAX_SIZE
 */
int tiku_kits_matrix_mul(struct tiku_kits_matrix *result,
                    const struct tiku_kits_matrix *a,
                    const struct tiku_kits_matrix *b);

/**
 * @brief Scalar multiplication: result = scalar * a
 *
 * Multiplies every element of @p a by @p scalar and writes the
 * products into @p result.  O(rows * cols).  @p result may safely
 * alias @p a for in-place scaling.
 *
 * @param result Output matrix (must not be NULL; may alias a)
 * @param a      Input matrix (must not be NULL)
 * @param scalar Scalar multiplier
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if result or a is NULL
 */
int tiku_kits_matrix_scale(struct tiku_kits_matrix *result,
                      const struct tiku_kits_matrix *a,
                      tiku_kits_matrix_elem_t scalar);

/*---------------------------------------------------------------------------*/
/* MATRIX TRANSFORMS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Transpose: result = a^T
 *
 * Writes the transpose of @p a into @p result.  The result will have
 * dimensions (a->cols x a->rows).  O(rows * cols).
 *
 * @warning @p result must NOT alias @p a.  Reading and writing the
 *          same matrix would corrupt off-diagonal elements.
 *
 * @param result Output matrix (must not be NULL; must not alias a)
 * @param a      Input matrix (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if result or a is NULL
 */
int tiku_kits_matrix_transpose(struct tiku_kits_matrix *result,
                          const struct tiku_kits_matrix *a);

/*---------------------------------------------------------------------------*/
/* SQUARE MATRIX OPERATIONS                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the determinant of a square matrix
 *
 * Uses recursive cofactor expansion along the first row.  Complexity
 * is O(n!) in the general case, which is acceptable for the small
 * matrices targeted here (n <= TIKU_KITS_MATRIX_MAX_SIZE, typically
 * 4).  Stack depth is bounded by MAX_SIZE due to the recursion.
 *
 * Specialized base cases for 1x1 and 2x2 avoid unnecessary recursion.
 *
 * @param m   Square matrix (must not be NULL; rows must equal cols)
 * @param det Output pointer for the determinant value (must not be
 *            NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m or det is NULL,
 *         TIKU_KITS_MATHS_ERR_DIM if the matrix is not square
 */
int tiku_kits_matrix_det(const struct tiku_kits_matrix *m,
                    tiku_kits_matrix_elem_t *det);

/**
 * @brief Compute the trace (sum of diagonal elements)
 *
 * Sums elements m[i][i] for i in [0..n).  O(n) where n is the
 * matrix dimension.  The matrix must be square.
 *
 * @param m     Square matrix (must not be NULL; rows must equal cols)
 * @param trace Output pointer for the trace value (must not be NULL)
 * @return TIKU_KITS_MATHS_OK on success,
 *         TIKU_KITS_MATHS_ERR_NULL if m or trace is NULL,
 *         TIKU_KITS_MATHS_ERR_DIM if the matrix is not square
 */
int tiku_kits_matrix_trace(const struct tiku_kits_matrix *m,
                      tiku_kits_matrix_elem_t *trace);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of active rows
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 *
 * @param m Matrix, or NULL
 * @return Number of rows, or 0 if m is NULL
 */
uint8_t tiku_kits_matrix_rows(const struct tiku_kits_matrix *m);

/**
 * @brief Return the number of active columns
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 *
 * @param m Matrix, or NULL
 * @return Number of columns, or 0 if m is NULL
 */
uint8_t tiku_kits_matrix_cols(const struct tiku_kits_matrix *m);

/**
 * @brief Check if a matrix is square (rows == cols)
 *
 * Safe to call with a NULL pointer -- returns 0 (not square).
 *
 * @param m Matrix, or NULL
 * @return 1 if rows == cols and m is not NULL, 0 otherwise
 */
int tiku_kits_matrix_is_square(const struct tiku_kits_matrix *m);

#endif /* TIKU_KITS_MATRIX_H_ */
