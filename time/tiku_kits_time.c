/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_time.c - Common time utility functions
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

#include "tiku_kits_time.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Days in each month (non-leap year).
 */
static const uint8_t days_in_month[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/**
 * @brief Check if a year is a leap year.
 */
static int is_leap_year(uint16_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @brief Days in a given year (365 or 366).
 */
static uint16_t days_in_year(uint16_t year)
{
    return is_leap_year(year) ? 366 : 365;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_time_to_tm(tiku_kits_time_unix_t ts,
                          tiku_kits_time_tm_t *out)
{
    uint32_t rem;
    uint16_t year;
    uint8_t  month;
    uint16_t yday;
    uint8_t  mdays;

    if (out == (void *)0) {
        return TIKU_KITS_TIME_ERR_NULL;
    }

    /* Extract time-of-day */
    out->second = (uint8_t)(ts % 60);
    rem = ts / 60;
    out->minute = (uint8_t)(rem % 60);
    rem /= 60;
    out->hour = (uint8_t)(rem % 24);

    /* Day count since epoch (1970-01-01 was a Thursday = weekday 4) */
    {
        uint32_t days = ts / 86400UL;
        out->weekday = (uint8_t)((days + 4) % 7);

        /* Find year */
        year = 1970;
        while (days >= days_in_year(year)) {
            days -= days_in_year(year);
            year++;
        }
        out->year = year;
        yday = (uint16_t)days;
    }

    /* Find month and day */
    for (month = 0; month < 12; month++) {
        mdays = days_in_month[month];
        if (month == 1 && is_leap_year(year)) {
            mdays = 29;
        }
        if (yday < mdays) {
            break;
        }
        yday -= mdays;
    }
    out->month = month + 1;
    out->day = (uint8_t)(yday + 1);

    return TIKU_KITS_TIME_OK;
}
