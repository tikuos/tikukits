/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures.h - Common signal feature extraction types
 *
 * Provides shared return codes, element type, and helper definitions
 * used across all TikuKits signal feature extraction libraries:
 *
 *  - Zero-crossing rate  : sign-change counting per window
 *  - Peak detector       : local maxima with configurable hysteresis
 *  - Histogram / binning : fixed-width bin accumulation
 *  - First-order delta   : x[n] - x[n-1] rate-of-change
 *  - Goertzel            : single-frequency energy via DFT bin
 *  - Z-score             : fixed-point (x - mean) / stddev normalization
 *  - Min-max scale       : map [min, max] to [0, out_max] with clamping
 *
 * All storage is statically allocated; no heap required.
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

#ifndef TIKU_KITS_SIGFEATURES_H_
#define TIKU_KITS_SIGFEATURES_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Element type for sample values.
 * Defaults to int32_t for integer-only targets (no FPU).
 * Define as int16_t before including if memory is very tight.
 */
#ifndef TIKU_KITS_SIGFEATURES_ELEM_TYPE
#define TIKU_KITS_SIGFEATURES_ELEM_TYPE int32_t
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/** @typedef tiku_kits_sigfeatures_elem_t
 *  @brief Element type used for all sample values
 */
typedef TIKU_KITS_SIGFEATURES_ELEM_TYPE tiku_kits_sigfeatures_elem_t;

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_SIGFEATURES_STATUS Signal Features Status Codes
 * @{ */
#define TIKU_KITS_SIGFEATURES_OK            0   /**< Operation succeeded */
#define TIKU_KITS_SIGFEATURES_ERR_NULL    (-1)  /**< NULL pointer argument */
#define TIKU_KITS_SIGFEATURES_ERR_SIZE    (-2)  /**< Buffer/window size error */
#define TIKU_KITS_SIGFEATURES_ERR_PARAM   (-3)  /**< Invalid parameter */
#define TIKU_KITS_SIGFEATURES_ERR_NODATA  (-4)  /**< No data available yet */
/** @} */

#endif /* TIKU_KITS_SIGFEATURES_H_ */
