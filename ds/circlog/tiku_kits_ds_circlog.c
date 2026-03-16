/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_circlog.c - Fixed-size circular log implementation
 *
 * Platform-independent implementation of a structured circular log
 * designed for FRAM logging on embedded systems. All storage is
 * statically allocated; no heap usage.
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_ds_circlog.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a circular log with the given capacity
 *
 * Zeros the entire entry array so that all slots (including those
 * beyond the runtime capacity) start in a clean, deterministic state.
 * This is important on FRAM targets where power loss may leave
 * arbitrary values in memory.
 */
int tiku_kits_ds_circlog_init(struct tiku_kits_ds_circlog *log,
                              uint16_t capacity)
{
    if (log == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0
        || capacity > TIKU_KITS_DS_CIRCLOG_MAX_ENTRIES) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    log->head = 0;
    log->count = 0;
    log->capacity = capacity;
    log->seq = 0;
    /* Zero the full static buffer, not just `capacity` slots, so
     * the struct is in a clean state regardless of prior contents. */
    memset(log->entries, 0, sizeof(log->entries));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* APPEND / READ                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append a new entry to the circular log
 *
 * O(1) operation.  When the log is not yet full, the entry is written
 * at the next free slot and count is incremented.  When full, the
 * oldest entry (at head) is overwritten and head advances by one,
 * keeping the ring semantics.  Unused payload bytes are explicitly
 * zeroed to maintain clean FRAM state across power cycles.
 */
int tiku_kits_ds_circlog_append(
    struct tiku_kits_ds_circlog *log,
    uint8_t level,
    uint8_t tag,
    uint32_t timestamp,
    const uint8_t *payload,
    uint8_t payload_len)
{
    uint16_t write_pos;
    struct tiku_kits_ds_circlog_entry *entry;

    if (log == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    /* Allow NULL payload only when no bytes are to be copied */
    if (payload_len > 0 && payload == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (payload_len > TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    if (log->count < log->capacity) {
        /* Not full: write at the next free slot after the last
         * valid entry. */
        write_pos = (log->head + log->count) % log->capacity;
        log->count++;
    } else {
        /* Full: overwrite the oldest entry (at head) and advance
         * head so the second-oldest becomes the new oldest. */
        write_pos = log->head;
        log->head = (log->head + 1) % log->capacity;
    }

    entry = &log->entries[write_pos];
    entry->timestamp = timestamp;
    entry->level = level;
    entry->tag = tag;
    entry->payload_len = payload_len;

    if (payload_len > 0) {
        memcpy(entry->payload, payload, payload_len);
    }
    /* Zero remaining payload bytes so that partial writes leave
     * clean FRAM state -- important for power-loss resilience. */
    if (payload_len < TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE) {
        memset(&entry->payload[payload_len], 0,
               TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE - payload_len);
    }

    log->seq++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read the newest (most recent) log entry
 *
 * Convenience wrapper that delegates to read_at() with age index 0.
 * All validation is performed by the callee.
 */
int tiku_kits_ds_circlog_read_latest(
    const struct tiku_kits_ds_circlog *log,
    struct tiku_kits_ds_circlog_entry *entry_out)
{
    return tiku_kits_ds_circlog_read_at(log, 0, entry_out);
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read a log entry by age index
 *
 * Converts the caller's age index (0 = newest) to a physical ring
 * position and copies the entry into the output struct.  The mapping
 * formula is: actual_pos = (head + count - 1 - index) % capacity,
 * which counts backwards from the most recent write position.
 */
int tiku_kits_ds_circlog_read_at(
    const struct tiku_kits_ds_circlog *log,
    uint16_t index,
    struct tiku_kits_ds_circlog_entry *entry_out)
{
    uint16_t actual_pos;

    if (log == NULL || entry_out == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (log->count == 0) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }
    if (index >= log->count) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    /* Map age index to physical position.  Index 0 is the newest
     * entry at (head + count - 1); each increment of index steps
     * one entry further into the past. */
    actual_pos = (log->head + log->count - 1 - index)
                 % log->capacity;
    *entry_out = log->entries[actual_pos];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the circular log by resetting head, count, and seq
 *
 * Logically removes all entries without zeroing the backing array.
 * Old entries remain in memory but are inaccessible through the
 * public API because all read functions bounds-check against count.
 */
int tiku_kits_ds_circlog_clear(struct tiku_kits_ds_circlog *log)
{
    if (log == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    log->head = 0;
    log->count = 0;
    log->seq = 0;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of entries in the log
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the logical count, not the capacity.
 */
uint16_t tiku_kits_ds_circlog_count(
    const struct tiku_kits_ds_circlog *log)
{
    if (log == NULL) {
        return 0;
    }
    return log->count;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current monotonic sequence number
 *
 * Returns the sequence counter which increments on every append().
 * Safe to call with a NULL pointer -- returns 0.
 */
uint32_t tiku_kits_ds_circlog_sequence(
    const struct tiku_kits_ds_circlog *log)
{
    if (log == NULL) {
        return 0;
    }
    return log->seq;
}
