/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec.h - Common codec types and definitions
 *
 * Provides shared return codes used across all TikuKits codec
 * sub-modules (CBOR, future MessagePack, etc.).  Each codec includes
 * this header for its common definitions.
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

#ifndef TIKU_KITS_CODEC_H_
#define TIKU_KITS_CODEC_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_CODEC_STATUS Codec Status Codes
 * @brief Return codes shared by all TikuKits codec sub-modules.
 *
 * Every public codec function returns one of these codes.  Zero
 * indicates success; negative values indicate distinct error classes.
 * Sub-modules must never define their own return codes -- all codes
 * live here so that application code can handle errors uniformly.
 *
 * @{
 */
#define TIKU_KITS_CODEC_OK            0   /**< Operation succeeded */
#define TIKU_KITS_CODEC_ERR_NULL    (-1)  /**< NULL pointer argument */
#define TIKU_KITS_CODEC_ERR_PARAM   (-2)  /**< Invalid parameter */
#define TIKU_KITS_CODEC_ERR_OVERFLOW (-3) /**< Output buffer too small */
#define TIKU_KITS_CODEC_ERR_UNDERFLOW (-4) /**< Input truncated / not enough data */
#define TIKU_KITS_CODEC_ERR_TYPE    (-5)  /**< Unexpected CBOR type encountered */
#define TIKU_KITS_CODEC_ERR_CORRUPT (-6)  /**< Structurally invalid encoded data */
/** @} */

#endif /* TIKU_KITS_CODEC_H_ */
