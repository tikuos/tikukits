/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_maths.h - Common maths types and definitions
 *
 * Provides shared return codes and helper definitions used across
 * all TikuKits maths libraries. Each maths sub-module includes
 * this header for its common types.
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

#ifndef TIKU_KITS_MATHS_H_
#define TIKU_KITS_MATHS_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_MATHS_STATUS Maths Status Codes
 *
 * Return codes shared by every maths sub-module (matrix, distance,
 * statistics, etc.).  All functions return one of these codes so that
 * the caller can distinguish success from each failure mode without
 * inspecting output data.
 *
 * Convention: zero is success, negative values are errors.  New
 * sub-modules must reuse these codes rather than defining their own.
 *
 * @{
 */

/** @brief Operation completed successfully -- output data is valid. */
#define TIKU_KITS_MATHS_OK              0

/**
 * @brief A required pointer argument was NULL.
 *
 * Returned when any input or output pointer that the function must
 * dereference is NULL.  Check every pointer before calling.
 */
#define TIKU_KITS_MATHS_ERR_NULL      (-1)

/**
 * @brief Matrix or vector dimensions do not match the operation.
 *
 * For addition/subtraction both operands must have identical (rows, cols).
 * For multiplication, the inner dimensions must agree (a.cols == b.rows).
 * For square-matrix operations (determinant, trace) the matrix must be
 * square.  For distance functions this indicates mismatched vector lengths.
 */
#define TIKU_KITS_MATHS_ERR_DIM       (-2)

/**
 * @brief A dimension or count exceeds the compile-time capacity.
 *
 * Returned when a requested size (rows, cols, window, etc.) is zero or
 * larger than the module's static maximum (e.g. TIKU_KITS_MATRIX_MAX_SIZE,
 * TIKU_KITS_STATISTICS_MAX_WINDOW).  Also returned by query functions
 * when no samples have been pushed yet.
 */
#define TIKU_KITS_MATHS_ERR_SIZE      (-3)

/**
 * @brief The input data is singular or degenerate.
 *
 * Returned when a mathematically valid but degenerate condition makes
 * the result undefined -- e.g. a singular matrix for inversion, or a
 * zero-magnitude vector for normalization.
 */
#define TIKU_KITS_MATHS_ERR_SINGULAR  (-4)

/**
 * @brief An intermediate or final result would overflow the element type.
 *
 * Returned when overflow is detected before it corrupts the output.
 * Sub-modules that use wider accumulators (int64_t) may avoid this
 * error at the cost of extra cycles on 16-bit targets.
 */
#define TIKU_KITS_MATHS_ERR_OVERFLOW  (-5)

/** @} */

#endif /* TIKU_KITS_MATHS_H_ */
