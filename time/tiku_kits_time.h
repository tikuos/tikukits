/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_time.h - Common types and return codes for TikuKits time
 *
 * All time sub-modules (NTP, etc.) share the return codes and types
 * defined here, following the same convention as tiku_kits_maths.h
 * and tiku_kits_net.h.
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

#ifndef TIKU_KITS_TIME_H_
#define TIKU_KITS_TIME_H_

#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_TIME_STATUS Time Status Codes
 * @brief Return codes shared by all TikuKits time sub-modules.
 * @{
 */
#define TIKU_KITS_TIME_OK             0   /**< Operation succeeded */
#define TIKU_KITS_TIME_ERR_NULL     (-1)  /**< NULL pointer argument */
#define TIKU_KITS_TIME_ERR_PARAM    (-2)  /**< Invalid parameter */
#define TIKU_KITS_TIME_ERR_NODATA   (-3)  /**< No time data available */
#define TIKU_KITS_TIME_ERR_TIMEOUT  (-4)  /**< Request timed out */
#define TIKU_KITS_TIME_ERR_NET      (-5)  /**< Network send/recv error */
/** @} */

/*---------------------------------------------------------------------------*/
/* COMMON TYPES                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Unix timestamp (seconds since 1970-01-01 00:00:00 UTC).
 *
 * 32-bit unsigned gives a range up to 2106-02-07.  Sufficient for
 * embedded systems that need wall-clock time for logging or scheduling.
 */
typedef uint32_t tiku_kits_time_unix_t;

/**
 * @brief Broken-down time structure.
 *
 * Compact representation of calendar date and time for display
 * or logging.  All fields are in UTC.
 */
typedef struct tiku_kits_time_tm {
    uint16_t year;    /**< Full year (e.g. 2026) */
    uint8_t  month;   /**< 1..12 */
    uint8_t  day;     /**< 1..31 */
    uint8_t  hour;    /**< 0..23 */
    uint8_t  minute;  /**< 0..59 */
    uint8_t  second;  /**< 0..59 */
    uint8_t  weekday; /**< 0=Sunday .. 6=Saturday */
} tiku_kits_time_tm_t;

/*---------------------------------------------------------------------------*/
/* UTILITY FUNCTIONS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Convert Unix timestamp to broken-down UTC time.
 *
 * @param ts   Unix timestamp (seconds since epoch)
 * @param out  Broken-down time output (must not be NULL)
 * @return TIKU_KITS_TIME_OK or TIKU_KITS_TIME_ERR_NULL
 */
int tiku_kits_time_to_tm(tiku_kits_time_unix_t ts,
                          tiku_kits_time_tm_t *out);

#endif /* TIKU_KITS_TIME_H_ */
