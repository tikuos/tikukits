/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml.h - Common ML types and definitions
 *
 * Provides shared return codes and helper definitions used across
 * all TikuKits machine learning libraries. Each ML sub-module
 * includes this header for its common types.
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

#ifndef TIKU_KITS_ML_H_
#define TIKU_KITS_ML_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_ML_STATUS ML Status Codes
 * @{ */
#define TIKU_KITS_ML_OK              0   /**< Operation succeeded */
#define TIKU_KITS_ML_ERR_NULL      (-1)  /**< NULL pointer argument */
#define TIKU_KITS_ML_ERR_SIZE      (-2)  /**< Insufficient data */
#define TIKU_KITS_ML_ERR_PARAM     (-3)  /**< Invalid parameter */
#define TIKU_KITS_ML_ERR_SINGULAR  (-4)  /**< Degenerate / singular data */
#define TIKU_KITS_ML_ERR_OVERFLOW  (-5)  /**< Arithmetic overflow */
/** @} */

#endif /* TIKU_KITS_ML_H_ */
