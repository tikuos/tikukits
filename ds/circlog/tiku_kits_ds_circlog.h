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
 * @brief Maximum number of entries the circular log can hold.
 *
 * This compile-time constant defines the upper bound on log capacity.
 * Each log instance reserves this many entry slots in its static
 * storage, so choose a value that balances memory usage against the
 * longest history your application needs to retain.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES 64
 *   #include "tiku_kits_ds_circlog.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES
#define TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES 16
#endif

/**
 * @brief Size of the payload field in each log entry, in bytes.
 *
 * Each entry carries a fixed-size payload buffer of this many bytes.
 * Larger values allow richer per-entry data but increase the memory
 * footprint of every slot (used or not) in the entry array.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE 16
 *   #include "tiku_kits_ds_circlog.h"
 * @endcode
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
 * Each entry is a self-contained record carrying:
 *   - @c timestamp -- a 32-bit application-provided value (e.g.
 *     ticks from the system clock).  The log itself does not
 *     interpret this field; it is stored and returned as-is.
 *   - @c level -- severity from 0 (DEBUG) to 3 (ERROR), matching
 *     the TIKU_KITS_DS_CIRCLOG_LEVEL_* constants.
 *   - @c tag -- an application-defined category byte that allows
 *     filtering entries by subsystem at read time.
 *   - @c payload -- a fixed-size buffer of CIRCLOG_PAYLOAD_SIZE
 *     bytes.  Only the first @c payload_len bytes are meaningful;
 *     remaining bytes are zeroed for clean FRAM state.
 *
 * @note The struct layout is chosen so that the 32-bit timestamp is
 *       naturally aligned, minimizing padding on both 16-bit and
 *       32-bit targets.
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
 * A ring-buffer of structured log entries designed for persistent
 * FRAM logging on embedded systems.  All storage lives inside the
 * struct itself, so no heap allocation is needed -- just declare the
 * log as a static or local variable.
 *
 * Two indices manage the ring:
 *   - @c head -- points to the oldest valid entry.
 *   - Write position is derived as (head + count) % capacity.
 *
 * When the log is full, append() overwrites the oldest entry and
 * advances @c head, giving constant-time O(1) append regardless of
 * log size.  A monotonic @c seq counter increments on every append
 * and never wraps during normal operation (uint32_t gives over 4
 * billion appends).
 *
 * The runtime @c capacity may be less than CIRCLOG_MAX_ENTRIES,
 * allowing different log instances to use different logical sizes
 * while sharing the same compile-time backing buffer.
 *
 * @note Unlike the array and B-Tree modules, the circular log does
 *       not use tiku_kits_ds_elem_t for its entries.  Each entry is
 *       a structured record (timestamp + level + tag + payload).
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_circlog log;
 *   tiku_kits_ds_circlog_init(&log, 16);
 *   uint8_t data[] = {0xAB, 0xCD};
 *   tiku_kits_ds_circlog_append(&log, TIKU_KITS_DS_CIRCLOG_LEVEL_INFO,
 *                                0x10, 12345, data, 2);
 *   struct tiku_kits_ds_circlog_entry out;
 *   tiku_kits_ds_circlog_read_latest(&log, &out);
 *   // out.timestamp == 12345, out.tag == 0x10
 * @endcode
 */
struct tiku_kits_ds_circlog {
    struct tiku_kits_ds_circlog_entry
        entries[TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES];
                            /**< Static entry storage */
    uint16_t head;          /**< Index of oldest entry */
    uint16_t count;         /**< Number of valid entries */
    uint16_t capacity;      /**< Runtime capacity set by init (<= MAX_ENTRIES) */
    uint32_t seq;           /**< Monotonic sequence number (increments on each append) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a circular log with the given capacity
 *
 * Resets the log to an empty state, sets the runtime capacity limit,
 * and zeros the sequence counter.  The backing entry array is zeroed
 * so that unwritten slots hold deterministic values.
 *
 * @param log      Circular log to initialize (must not be NULL)
 * @param capacity Maximum number of entries (1..CIRCLOG_MAX_ENTRIES)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if log is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds
 *         MAX_ENTRIES
 */
int tiku_kits_ds_circlog_init(struct tiku_kits_ds_circlog *log,
                              uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* APPEND / READ                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append a new entry to the circular log
 *
 * Writes at position (head + count) % capacity.  When the log is
 * full the oldest entry is silently overwritten and head advances,
 * keeping the operation O(1).  Each successful append increments the
 * monotonic sequence number.  Unused payload bytes are zeroed so
 * that FRAM contents remain clean.
 *
 * @param log         Circular log (must not be NULL)
 * @param level       Severity level (0..3, matching
 *                    TIKU_KITS_DS_CIRCLOG_LEVEL_* constants)
 * @param tag         Application-defined category byte
 * @param timestamp   Caller-provided timestamp value
 * @param payload     Pointer to payload data (may be NULL only when
 *                    payload_len is 0)
 * @param payload_len Number of payload bytes to copy
 *                    (0..CIRCLOG_PAYLOAD_SIZE)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if log is NULL, or if payload is
 *         NULL while payload_len > 0,
 *         TIKU_KITS_DS_ERR_PARAM if payload_len exceeds
 *         CIRCLOG_PAYLOAD_SIZE
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
 *
 * Convenience wrapper around read_at() with index 0.  Copies the
 * full entry (timestamp, level, tag, payload) into the caller-owned
 * output struct.
 *
 * @param log       Circular log (must not be NULL)
 * @param entry_out Output pointer where the newest entry is written
 *                  (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if log or entry_out is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if the log contains no entries
 */
int tiku_kits_ds_circlog_read_latest(
    const struct tiku_kits_ds_circlog *log,
    struct tiku_kits_ds_circlog_entry *entry_out);

/**
 * @brief Read a log entry by age index
 *
 * Index 0 returns the newest entry, index (count - 1) returns the
 * oldest.  Internally the age index is converted to a physical ring
 * position as (head + count - 1 - index) % capacity.  The entry is
 * copied into the caller-owned output struct.
 *
 * @param log       Circular log (must not be NULL)
 * @param index     Age index (0 = newest, count-1 = oldest)
 * @param entry_out Output pointer where the entry is written (must
 *                  not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if log or entry_out is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if the log contains no entries,
 *         TIKU_KITS_DS_ERR_BOUNDS if index >= count
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
 *
 * Logically removes all entries by resetting head, count, and the
 * sequence counter to zero.  The backing entry array is not zeroed
 * for efficiency -- old entries remain in memory but are inaccessible
 * through the public API since all read functions bounds-check
 * against count.
 *
 * @param log Circular log (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if log is NULL
 */
int tiku_kits_ds_circlog_clear(struct tiku_kits_ds_circlog *log);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of entries in the log
 *
 * Returns the logical entry count (number of valid entries), not the
 * capacity.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param log Circular log, or NULL
 * @return Number of entries currently stored, or 0 if log is NULL
 */
uint16_t tiku_kits_ds_circlog_count(
    const struct tiku_kits_ds_circlog *log);

/**
 * @brief Get the current monotonic sequence number
 *
 * The sequence number increments by one on every successful append()
 * and is never reset except by clear() or init().  Useful for
 * detecting whether new entries have been written since the last
 * check.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param log Circular log, or NULL
 * @return Current sequence number, or 0 if log is NULL
 */
uint32_t tiku_kits_ds_circlog_sequence(
    const struct tiku_kits_ds_circlog *log);

#endif /* TIKU_KITS_DS_CIRCLOG_H_ */
