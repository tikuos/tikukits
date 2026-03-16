/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_textcompression.h - Common text compression types and definitions
 *
 * Provides shared return codes used across all TikuKits text compression
 * libraries. Each compression sub-module includes this header for its
 * common definitions.
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

#ifndef TIKU_KITS_TEXTCOMPRESSION_H_
#define TIKU_KITS_TEXTCOMPRESSION_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_TEXTCOMPRESSION_STATUS Text Compression Status Codes
 * @brief Shared return codes for every text-compression sub-module
 *
 * All compression and decompression functions in the textcompression kit
 * return one of these codes.  Sub-modules (RLE, BPE, Heatshrink) do NOT
 * define their own return codes -- they rely exclusively on this common
 * set.  Callers can therefore use a single switch/if-chain regardless of
 * which algorithm was invoked.
 *
 * Conventions:
 *   - Zero means success; negative values indicate errors.
 *   - ERR_NULL is returned when any required pointer argument is NULL.
 *   - ERR_SIZE is returned when the caller-supplied output buffer is
 *     too small to hold the result.
 *   - ERR_PARAM covers non-pointer argument validation failures.
 *   - ERR_CORRUPT signals that compressed input data is structurally
 *     invalid (e.g. truncated header, impossible field values).
 *   - ERR_OVERFLOW means an internal working buffer (bounded by a
 *     compile-time constant) would be exceeded.
 *
 * @{
 */
#define TIKU_KITS_TEXTCOMPRESSION_OK            0   /**< Operation succeeded */
#define TIKU_KITS_TEXTCOMPRESSION_ERR_NULL    (-1)  /**< NULL pointer argument */
#define TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE    (-2)  /**< Output buffer too small */
#define TIKU_KITS_TEXTCOMPRESSION_ERR_PARAM   (-3)  /**< Invalid parameter */
#define TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT (-4)  /**< Corrupt compressed data */
#define TIKU_KITS_TEXTCOMPRESSION_ERR_OVERFLOW (-5) /**< Internal buffer overflow */
/** @} */

#endif /* TIKU_KITS_TEXTCOMPRESSION_H_ */
