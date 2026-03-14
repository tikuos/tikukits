/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds.h - TikuKits Data Structures common header
 *
 * Shared return codes and element type for all DS sub-modules.
 * Every sub-module includes this header.
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

#ifndef TIKU_KITS_DS_H_
#define TIKU_KITS_DS_H_

#include <stdint.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/** @brief Operation succeeded */
#define TIKU_KITS_DS_OK           0

/** @brief NULL pointer passed */
#define TIKU_KITS_DS_ERR_NULL    (-1)

/** @brief Container is full */
#define TIKU_KITS_DS_ERR_FULL    (-2)

/** @brief Container is empty */
#define TIKU_KITS_DS_ERR_EMPTY   (-3)

/** @brief Invalid parameter */
#define TIKU_KITS_DS_ERR_PARAM   (-4)

/** @brief Key or value not found */
#define TIKU_KITS_DS_ERR_NOTFOUND (-5)

/** @brief Index out of bounds */
#define TIKU_KITS_DS_ERR_BOUNDS  (-6)

/*---------------------------------------------------------------------------*/
/* ELEMENT TYPE                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Configurable element type for all DS sub-modules.
 *
 * Default is int32_t. Override at compile time:
 *   -DTIKU_KITS_DS_ELEM_TYPE=int16_t
 */
#ifndef TIKU_KITS_DS_ELEM_TYPE
#define TIKU_KITS_DS_ELEM_TYPE int32_t
#endif

typedef TIKU_KITS_DS_ELEM_TYPE tiku_kits_ds_elem_t;

#endif /* TIKU_KITS_DS_H_ */
