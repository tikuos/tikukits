/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_circlog.h - Fixed-size circular log for structured entries
 *
 * Platform-independent circular log designed for structured FRAM logging
 * on embedded systems. Each entry carries a timestamp, severity level,
 * application-defined tag, and a small payload. The log overwrites the
 * oldest entry when full. All storage is statically allocated.
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

#ifndef TIKU_KITS_DS_CIRCLOG_H_
#define TIKU_KITS_DS_CIRCLOG_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of entries the circular log can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES
#define TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES 16
#endif

/**
 * Size of the payload field in each log entry, in bytes.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE
#define TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE 8
#endif

/*---------------------------------------------------------------------------*/
/* LOG LEVEL CONSTANTS                                                       */
/*---------------------------------------------------------------------------*/

/** @brief Debug-level log entry */
#define TIKU_KITS_DS_CIRCLOG_LEVEL_DEBUG  0

/** @brief Informational log entry */
#define TIKU_KITS_DS_CIRCLOG_LEVEL_INFO   1

/** @brief Warning-level log entry */
#define TIKU_KITS_DS_CIRCLOG_LEVEL_WARN   2

/** @brief Error-level log entry */
#define TIKU_KITS_DS_CIRCLOG_LEVEL_ERROR  3

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_circlog_entry
 * @brief Single log entry with timestamp, level, tag, and payload
 *
 * Each entry stores a 32-bit timestamp, an 8-bit severity level
 * (DEBUG/INFO/WARN/ERROR), an application-defined tag byte, and
 * a variable-length payload up to CIRCLOG_PAYLOAD_SIZE bytes.
 */
struct tiku_kits_ds_circlog_entry {
    uint32_t timestamp;     /**< Application-provided timestamp */
    uint8_t  level;         /**< Severity: 0=DEBUG .. 3=ERROR */
    uint8_t  tag;           /**< Application-defined category */
    uint8_t  payload[TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE];
                            /**< Entry payload data */
    uint8_t  payload_len;   /**< Valid bytes in payload */
};

/**
 * @struct tiku_kits_ds_circlog
 * @brief Fixed-capacity circular log with static storage
 *
 * Head points to the oldest entry. New entries are appended at
 * (head + count) % capacity. When the log is full, head advances
 * and the oldest entry is overwritten. A monotonic sequence number
 * increments on every append.
 *
 * Declare circular logs as static or local variables -- no heap
 * required.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_circlog log;
 *   tiku_kits_ds_circlog_init(&log, 16);
 *   uint8_t data[] = {0xAB, 0xCD};
 *   tiku_kits_ds_circlog_append(&log, 1, 0x10, 12345, data, 2);
 * @endcode
 */
struct tiku_kits_ds_circlog {
    struct tiku_kits_ds_circlog_entry
        entries[TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES];
                            /**< Static entry storage */
    uint16_t head;          /**< Index of oldest entry */
    uint16_t count;         /**< Number of valid entries */
    uint16_t capacity;      /**< User-requested capacity */
    uint32_t seq;           /**< Monotonic sequence number */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a circular log with the given capacity
 * @param log      Circular log to initialize
 * @param capacity Maximum entries (1..CIRCLOG_MAX_ENTRIES)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_circlog_init(struct tiku_kits_ds_circlog *log,
                              uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* APPEND / READ                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append a new entry to the circular log
 *
 * Writes at (head + count) % capacity. When the log is full the
 * oldest entry is overwritten and head advances. Each append
 * increments the monotonic sequence number.
 *
 * @param log         Circular log
 * @param level       Severity level (0..3)
 * @param tag         Application-defined tag byte
 * @param timestamp   Timestamp value
 * @param payload     Pointer to payload data (may be NULL if len 0)
 * @param payload_len Number of payload bytes (0..PAYLOAD_SIZE)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_circlog_append(
    struct tiku_kits_ds_circlog *log,
    uint8_t level,
    uint8_t tag,
    uint32_t timestamp,
    const uint8_t *payload,
    uint8_t payload_len);

/**
 * @brief Read the newest (most recent) log entry
 * @param log       Circular log
 * @param entry_out Output: copy of the newest entry
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_circlog_read_latest(
    const struct tiku_kits_ds_circlog *log,
    struct tiku_kits_ds_circlog_entry *entry_out);

/**
 * @brief Read a log entry by age index
 *
 * Index 0 returns the newest entry, index (count - 1) returns the
 * oldest entry.
 *
 * @param log       Circular log
 * @param index     Age index (0 = newest)
 * @param entry_out Output: copy of the entry
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_circlog_read_at(
    const struct tiku_kits_ds_circlog *log,
    uint16_t index,
    struct tiku_kits_ds_circlog_entry *entry_out);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the circular log (reset head, count, seq)
 * @param log Circular log
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_circlog_clear(struct tiku_kits_ds_circlog *log);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of entries
 * @param log Circular log
 * @return Current count, or 0 if log is NULL
 */
uint16_t tiku_kits_ds_circlog_count(
    const struct tiku_kits_ds_circlog *log);

/**
 * @brief Get the current monotonic sequence number
 * @param log Circular log
 * @return Current sequence number, or 0 if log is NULL
 */
uint32_t tiku_kits_ds_circlog_sequence(
    const struct tiku_kits_ds_circlog *log);

#endif /* TIKU_KITS_DS_CIRCLOG_H_ */
