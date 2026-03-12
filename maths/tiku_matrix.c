/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_matrix.c - Basic matrix operations library
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

#include "tiku_matrix.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute determinant of a submatrix via cofactor expansion
 * @param m    Source matrix
 * @param skip_row Row indices to include (size n)
 * @param skip_col Column indices to include (size n)
 * @param n    Dimension of the submatrix
 * @return Determinant value
 *
 * Recursive cofactor expansion along the first row.
 * Stack depth is bounded by TIKU_MATRIX_MAX_SIZE.
 */
static tiku_matrix_elem_t det_recursive(
    const struct tiku_matrix *m,
    const uint8_t *row_idx,
    const uint8_t *col_idx,
    uint8_t n)
{
    tiku_matrix_elem_t result;
    tiku_matrix_elem_t cofactor;
    uint8_t sub_cols[TIKU_MATRIX_MAX_SIZE];
    uint8_t j;
    uint8_t k;
    int sign;

    if (n == 1) {
        return m->data[row_idx[0]][col_idx[0]];
    }

    if (n == 2) {
        return m->data[row_idx[0]][col_idx[0]]
             * m->data[row_idx[1]][col_idx[1]]
             - m->data[row_idx[0]][col_idx[1]]
             * m->data[row_idx[1]][col_idx[0]];
    }

    result = 0;
    sign = 1;

    for (j = 0; j < n; j++) {
        /* Build column index list excluding column j */
        k = 0;
        for (uint8_t c = 0; c < n; c++) {
            if (c != j) {
                sub_cols[k++] = col_idx[c];
            }
        }

        cofactor = det_recursive(m, row_idx + 1, sub_cols, n - 1);
        result += sign * m->data[row_idx[0]][col_idx[j]] * cofactor;
        sign = -sign;
    }

    return result;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_matrix_init(struct tiku_matrix *m, uint8_t rows, uint8_t cols)
{
    if (m == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }
    if (rows == 0 || cols == 0
        || rows > TIKU_MATRIX_MAX_SIZE
        || cols > TIKU_MATRIX_MAX_SIZE) {
        return TIKU_MATRIX_ERR_SIZE;
    }

    m->rows = rows;
    m->cols = cols;
    memset(m->data, 0, sizeof(m->data));
    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_matrix_zero(struct tiku_matrix *m)
{
    if (m == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }

    memset(m->data, 0, sizeof(m->data));
    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_matrix_identity(struct tiku_matrix *m, uint8_t n)
{
    uint8_t i;

    if (m == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }
    if (n == 0 || n > TIKU_MATRIX_MAX_SIZE) {
        return TIKU_MATRIX_ERR_SIZE;
    }

    m->rows = n;
    m->cols = n;
    memset(m->data, 0, sizeof(m->data));

    for (i = 0; i < n; i++) {
        m->data[i][i] = 1;
    }

    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/
/* ELEMENT ACCESS                                                            */
/*---------------------------------------------------------------------------*/

void tiku_matrix_set(struct tiku_matrix *m, uint8_t row, uint8_t col,
                     tiku_matrix_elem_t val)
{
    if (m != NULL && row < m->rows && col < m->cols) {
        m->data[row][col] = val;
    }
}

/*---------------------------------------------------------------------------*/

tiku_matrix_elem_t tiku_matrix_get(const struct tiku_matrix *m,
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

int tiku_matrix_copy(struct tiku_matrix *dst,
                     const struct tiku_matrix *src)
{
    if (dst == NULL || src == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }

    dst->rows = src->rows;
    dst->cols = src->cols;
    memcpy(dst->data, src->data, sizeof(dst->data));
    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_matrix_equal(const struct tiku_matrix *a,
                      const struct tiku_matrix *b)
{
    uint8_t r;
    uint8_t c;

    if (a == NULL || b == NULL) {
        return 0;
    }
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

int tiku_matrix_add(struct tiku_matrix *result,
                    const struct tiku_matrix *a,
                    const struct tiku_matrix *b)
{
    uint8_t r;
    uint8_t c;

    if (result == NULL || a == NULL || b == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }
    if (a->rows != b->rows || a->cols != b->cols) {
        return TIKU_MATRIX_ERR_DIM;
    }

    result->rows = a->rows;
    result->cols = a->cols;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            result->data[r][c] = a->data[r][c] + b->data[r][c];
        }
    }

    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_matrix_sub(struct tiku_matrix *result,
                    const struct tiku_matrix *a,
                    const struct tiku_matrix *b)
{
    uint8_t r;
    uint8_t c;

    if (result == NULL || a == NULL || b == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }
    if (a->rows != b->rows || a->cols != b->cols) {
        return TIKU_MATRIX_ERR_DIM;
    }

    result->rows = a->rows;
    result->cols = a->cols;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            result->data[r][c] = a->data[r][c] - b->data[r][c];
        }
    }

    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_matrix_mul(struct tiku_matrix *result,
                    const struct tiku_matrix *a,
                    const struct tiku_matrix *b)
{
    uint8_t r;
    uint8_t c;
    uint8_t k;
    tiku_matrix_elem_t sum;

    if (result == NULL || a == NULL || b == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }
    if (a->cols != b->rows) {
        return TIKU_MATRIX_ERR_DIM;
    }
    if (a->rows > TIKU_MATRIX_MAX_SIZE
        || b->cols > TIKU_MATRIX_MAX_SIZE) {
        return TIKU_MATRIX_ERR_SIZE;
    }

    result->rows = a->rows;
    result->cols = b->cols;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < b->cols; c++) {
            sum = 0;
            for (k = 0; k < a->cols; k++) {
                sum += a->data[r][k] * b->data[k][c];
            }
            result->data[r][c] = sum;
        }
    }

    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_matrix_scale(struct tiku_matrix *result,
                      const struct tiku_matrix *a,
                      tiku_matrix_elem_t scalar)
{
    uint8_t r;
    uint8_t c;

    if (result == NULL || a == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }

    result->rows = a->rows;
    result->cols = a->cols;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            result->data[r][c] = a->data[r][c] * scalar;
        }
    }

    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/
/* MATRIX TRANSFORMS                                                         */
/*---------------------------------------------------------------------------*/

int tiku_matrix_transpose(struct tiku_matrix *result,
                          const struct tiku_matrix *a)
{
    uint8_t r;
    uint8_t c;

    if (result == NULL || a == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }

    result->rows = a->cols;
    result->cols = a->rows;

    for (r = 0; r < a->rows; r++) {
        for (c = 0; c < a->cols; c++) {
            result->data[c][r] = a->data[r][c];
        }
    }

    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/
/* SQUARE MATRIX OPERATIONS                                                  */
/*---------------------------------------------------------------------------*/

int tiku_matrix_det(const struct tiku_matrix *m,
                    tiku_matrix_elem_t *det)
{
    uint8_t idx[TIKU_MATRIX_MAX_SIZE];
    uint8_t i;

    if (m == NULL || det == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }
    if (m->rows != m->cols) {
        return TIKU_MATRIX_ERR_DIM;
    }

    /* Build identity index arrays */
    for (i = 0; i < m->rows; i++) {
        idx[i] = i;
    }

    *det = det_recursive(m, idx, idx, m->rows);
    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_matrix_trace(const struct tiku_matrix *m,
                      tiku_matrix_elem_t *trace)
{
    uint8_t i;
    tiku_matrix_elem_t sum;

    if (m == NULL || trace == NULL) {
        return TIKU_MATRIX_ERR_NULL;
    }
    if (m->rows != m->cols) {
        return TIKU_MATRIX_ERR_DIM;
    }

    sum = 0;
    for (i = 0; i < m->rows; i++) {
        sum += m->data[i][i];
    }

    *trace = sum;
    return TIKU_MATRIX_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

uint8_t tiku_matrix_rows(const struct tiku_matrix *m)
{
    if (m == NULL) {
        return 0;
    }
    return m->rows;
}

/*---------------------------------------------------------------------------*/

uint8_t tiku_matrix_cols(const struct tiku_matrix *m)
{
    if (m == NULL) {
        return 0;
    }
    return m->cols;
}

/*---------------------------------------------------------------------------*/

int tiku_matrix_is_square(const struct tiku_matrix *m)
{
    if (m == NULL) {
        return 0;
    }
    return (m->rows == m->cols) ? 1 : 0;
}
