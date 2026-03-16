/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_matrix.c - Basic matrix operations library
 *
 * Platform-independent implementation of common matrix operations.
 * All storage is statically allocated; no heap usage.
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_matrix.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute determinant of a submatrix via cofactor expansion
 *
 * Recursive helper that expands along the first row of the logical
 * submatrix defined by @p row_idx and @p col_idx.  Base cases for
 * n==1 and n==2 avoid further recursion.  The maximum recursion
 * depth is TIKU_KITS_MATRIX_MAX_SIZE, so stack usage is bounded
 * and predictable for embedded targets.
 *
 * @param m       Source matrix (full backing storage)
 * @param row_idx Array of row indices defining the submatrix (size n)
 * @param col_idx Array of column indices defining the submatrix (size n)
 * @param n       Dimension of the submatrix
 * @return Determinant of the submatrix
 */
static tiku_kits_matrix_elem_t det_recursive(
    const struct tiku_kits_matrix *m,
    const uint8_t *row_idx,
    const uint8_t *col_idx,
    uint8_t n)
{
    tiku_kits_matrix_elem_t result;
    tiku_kits_matrix_elem_t cofactor;
    uint8_t sub_cols[TIKU_KITS_MATRIX_MAX_SIZE];
    uint8_t j;
    uint8_t k;
    int sign;

    /* Base case: 1x1 determinant is the element itself */
    if (n == 1) {
        return m->data[row_idx[0]][col_idx[0]];
    }

    /* Base case: 2x2 determinant = ad - bc, avoids one recursion level */
    if (n == 2) {
        return m->data[row_idx[0]][col_idx[0]]
             * m->data[row_idx[1]][col_idx[1]]
             - m->data[row_idx[0]][col_idx[1]]
             * m->data[row_idx[1]][col_idx[0]];
    }

    result = 0;
    sign = 1;

    for (j = 0; j < n; j++) {
        /* Build column index list excluding column j for the minor */
        k = 0;
        for (uint8_t c = 0; c < n; c++) {
            if (c != j) {
                sub_cols[k++] = col_idx[c];
            }
        }

        /* Recurse on the (n-1) x (n-1) minor, skipping the first row */
        cofactor = det_recursive(m, row_idx + 1, sub_cols, n - 1);
        result += sign * m->data[row_idx[0]][col_idx[j]] * cofactor;
        /* Alternating sign for cofactor expansion */
        sign = -sign;
    }

    return result;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a matrix with given dimensions, all elements zero
 *
 * Zeros the entire backing 2-D buffer so that any subsequent reads
 * of in-bounds elements return a deterministic value.  The runtime
 * dimensions are clamped to TIKU_KITS_MATRIX_MAX_SIZE at compile time
 * so that the static buffer is never overrun.
 */
int tiku_kits_matrix_init(struct tiku_kits_matrix *m, uint8_t rows, uint8_t cols)
{
    if (m == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (rows == 0 || cols == 0
        || rows > TIKU_KITS_MATRIX_MAX_SIZE
        || cols > TIKU_KITS_MATRIX_MAX_SIZE) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    m->rows = rows;
    m->cols = cols;
    /* Zero the full static buffer, not just the active region, so that
     * the struct is in a clean state regardless of prior contents. */
    memset(m->data, 0, sizeof(m->data));
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Set all elements to zero, preserving dimensions
 *
 * Uses memset on the full backing buffer for a single fast bulk
 * clear.  Dimensions (rows, cols) are left unchanged.
 */
int tiku_kits_matrix_zero(struct tiku_kits_matrix *m)
{
    if (m == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    memset(m->data, 0, sizeof(m->data));
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Set matrix to the n x n identity matrix
 *
 * Zeros the entire buffer first, then writes 1 on the diagonal.
 * Overwrites the existing dimensions to n x n.
 */
int tiku_kits_matrix_identity(struct tiku_kits_matrix *m, uint8_t n)
{
    uint8_t i;

    if (m == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (n == 0 || n > TIKU_KITS_MATRIX_MAX_SIZE) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    m->rows = n;
    m->cols = n;
    memset(m->data, 0, sizeof(m->data));

    /* Place 1 on each diagonal element */
    for (i = 0; i < n; i++) {
        m->data[i][i] = 1;
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set a single element at (row, col)
 *
 * Silently ignored when m is NULL or indices are out of bounds.
 */
void tiku_kits_matrix_set(struct tiku_kits_matrix *m, uint8_t row, uint8_t col,
                     tiku_kits_matrix_elem_t val)
{
    if (m != NULL && row < m->rows && col < m->cols) {
        m->data[row][col] = val;
    }
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read a single element at (row, col)
 *
 * Returns 0 as a safe sentinel when m is NULL or indices are out of
 * bounds, avoiding undefined behaviour.
 */
tiku_kits_matrix_elem_t tiku_kits_matrix_get(const struct tiku_kits_matrix *m,
                                   uint8_t row, uint8_t col)
{
    if (m != NULL && row < m->rows && col < m->cols) {
        return m->data[row][col];
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
/* COPY AND COMPARISON                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Deep-copy a matrix from src to dst
 *
 * Copies dimensions and the entire backing buffer via memcpy for a
 * single, fast bulk copy.  After the call dst is independent of src.
 */
int tiku_kits_matrix_copy(struct tiku_kits_matrix *dst,
                     const struct tiku_kits_matrix *src)
{
    if (dst == NULL || src == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    dst->rows = src->rows;
    dst->cols = src->cols;
    memcpy(dst->data, src->data, sizeof(dst->data));
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check if two matrices are element-wise equal
 *
 * Short-circuits on the first dimension or element mismatch.
 * O(rows * cols) worst case when both matrices are identical.
 */
int tiku_kits_matrix_equal(const struct tiku_kits_matrix *a,
                      const struct tiku_kits_matrix *b)
{
    uint8_t r;
    uint8_t c;

    if (a == NULL || b == NULL) {
        return 0;
    }
    /* Dimension check first -- different shapes are never equal */
    if (a->rows != b->rows || a->cols != b->cols) {
        return 0;
    }

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            if (a->data[r][c] != b->data[r][c]) {
                return 0;
            }
        }
    }

    return 1;
}

/*---------------------------------------------------------------------------*/
/* ARITHMETIC OPERATIONS                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Element-wise matrix addition: result = a + b
 *
 * O(rows * cols).  Safe for in-place use (result may alias a or b).
 */
int tiku_kits_matrix_add(struct tiku_kits_matrix *result,
                    const struct tiku_kits_matrix *a,
                    const struct tiku_kits_matrix *b)
{
    uint8_t r;
    uint8_t c;

    if (result == NULL || a == NULL || b == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (a->rows != b->rows || a->cols != b->cols) {
        return TIKU_KITS_MATHS_ERR_DIM;
    }

    result->rows = a->rows;
    result->cols = a->cols;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            result->data[r][c] = a->data[r][c] + b->data[r][c];
        }
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Element-wise matrix subtraction: result = a - b
 *
 * O(rows * cols).  Safe for in-place use (result may alias a or b).
 */
int tiku_kits_matrix_sub(struct tiku_kits_matrix *result,
                    const struct tiku_kits_matrix *a,
                    const struct tiku_kits_matrix *b)
{
    uint8_t r;
    uint8_t c;

    if (result == NULL || a == NULL || b == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (a->rows != b->rows || a->cols != b->cols) {
        return TIKU_KITS_MATHS_ERR_DIM;
    }

    result->rows = a->rows;
    result->cols = a->cols;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            result->data[r][c] = a->data[r][c] - b->data[r][c];
        }
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Matrix multiplication: result = a * b
 *
 * Standard triple-loop multiplication, O(m * n * p).  Result must
 * NOT alias either operand because each output element depends on
 * an entire row of a and column of b.
 */
int tiku_kits_matrix_mul(struct tiku_kits_matrix *result,
                    const struct tiku_kits_matrix *a,
                    const struct tiku_kits_matrix *b)
{
    uint8_t r;
    uint8_t c;
    uint8_t k;
    tiku_kits_matrix_elem_t sum;

    if (result == NULL || a == NULL || b == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (a->cols != b->rows) {
        return TIKU_KITS_MATHS_ERR_DIM;
    }
    /* Guard against result dimensions exceeding the static buffer */
    if (a->rows > TIKU_KITS_MATRIX_MAX_SIZE
        || b->cols > TIKU_KITS_MATRIX_MAX_SIZE) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    result->rows = a->rows;
    result->cols = b->cols;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            /* Dot product of row r of a with column c of b */
            sum = 0;
            for (k = 0; k < a->cols; k++) {
                sum += a->data[r][k] * b->data[k][c];
            }
            result->data[r][c] = sum;
        }
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Scalar multiplication: result = scalar * a
 *
 * O(rows * cols).  Safe for in-place use (result may alias a).
 */
int tiku_kits_matrix_scale(struct tiku_kits_matrix *result,
                      const struct tiku_kits_matrix *a,
                      tiku_kits_matrix_elem_t scalar)
{
    uint8_t r;
    uint8_t c;

    if (result == NULL || a == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    result->rows = a->rows;
    result->cols = a->cols;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            result->data[r][c] = a->data[r][c] * scalar;
        }
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/
/* MATRIX TRANSFORMS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Transpose: result = a^T
 *
 * Swaps rows and columns.  Result must NOT alias a because reading
 * and writing the same matrix would corrupt off-diagonal elements.
 */
int tiku_kits_matrix_transpose(struct tiku_kits_matrix *result,
                          const struct tiku_kits_matrix *a)
{
    uint8_t r;
    uint8_t c;

    if (result == NULL || a == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    /* Output dimensions are the reverse of input dimensions */
    result->rows = a->cols;
    result->cols = a->rows;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            result->data[c][r] = a->data[r][c];
        }
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/
/* SQUARE MATRIX OPERATIONS                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the determinant of a square matrix
 *
 * Delegates to det_recursive() which performs cofactor expansion.
 * The identity index array [0, 1, ..., n-1] selects the full matrix
 * as the initial submatrix.
 */
int tiku_kits_matrix_det(const struct tiku_kits_matrix *m,
                    tiku_kits_matrix_elem_t *det)
{
    uint8_t idx[TIKU_KITS_MATRIX_MAX_SIZE];
    uint8_t i;

    if (m == NULL || det == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (m->rows != m->cols) {
        return TIKU_KITS_MATHS_ERR_DIM;
    }

    /* Build identity index array so the recursive helper operates
     * on the full matrix as its initial submatrix. */
    for (i = 0; i < m->rows; i++) {
        idx[i] = i;
    }

    *det = det_recursive(m, idx, idx, m->rows);
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the trace (sum of diagonal elements)
 *
 * O(n) -- single pass over the diagonal.  Requires a square matrix.
 */
int tiku_kits_matrix_trace(const struct tiku_kits_matrix *m,
                      tiku_kits_matrix_elem_t *trace)
{
    uint8_t i;
    tiku_kits_matrix_elem_t sum;

    if (m == NULL || trace == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (m->rows != m->cols) {
        return TIKU_KITS_MATHS_ERR_DIM;
    }

    sum = 0;
    for (i = 0; i < m->rows; i++) {
        sum += m->data[i][i];
    }

    *trace = sum;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of active rows
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint8_t tiku_kits_matrix_rows(const struct tiku_kits_matrix *m)
{
    if (m == NULL) {
        return 0;
    }
    return m->rows;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of active columns
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.
 */
uint8_t tiku_kits_matrix_cols(const struct tiku_kits_matrix *m)
{
    if (m == NULL) {
        return 0;
    }
    return m->cols;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check if a matrix is square (rows == cols)
 *
 * Safe to call with a NULL pointer -- returns 0 (not square).
 */
int tiku_kits_matrix_is_square(const struct tiku_kits_matrix *m)
{
    if (m == NULL) {
        return 0;
    }
    return (m->rows == m->cols) ? 1 : 0;
}
