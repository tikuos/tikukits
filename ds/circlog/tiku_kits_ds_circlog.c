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
    memset(log->entries, 0, sizeof(log->entries));
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* APPEND / READ                                                             */
/*---------------------------------------------------------------------------*/

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
    if (payload_len > 0 && payload == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (payload_len > TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    if (log->count < log->capacity) {
        /* Not full: write at (head + count) % capacity */
        write_pos = (log->head + log->count) % log->capacity;
        log->count++;
    } else {
        /* Full: overwrite oldest entry at head, advance head */
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
    /* Zero remaining payload bytes for clean FRAM state */
    if (payload_len < TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE) {
        memset(&entry->payload[payload_len], 0,
               TIKU_KITS_DS_CIRCLOG_PAYLOAD_SIZE - payload_len);
    }

    log->seq++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_circlog_read_latest(
    const struct tiku_kits_ds_circlog *log,
    struct tiku_kits_ds_circlog_entry *entry_out)
{
    return tiku_kits_ds_circlog_read_at(log, 0, entry_out);
}

/*---------------------------------------------------------------------------*/

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

    /*
     * Index 0 = newest, index (count-1) = oldest.
     * Newest sits at (head + count - 1) % capacity.
     * For age index i, the position is:
     *   (head + count - 1 - i) % capacity
     */
    actual_pos = (log->head + log->count - 1 - index)
                 % log->capacity;
    *entry_out = log->entries[actual_pos];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

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

uint16_t tiku_kits_ds_circlog_count(
    const struct tiku_kits_ds_circlog *log)
{
    if (log == NULL) {
        return 0;
    }
    return log->count;
}

/*---------------------------------------------------------------------------*/

uint32_t tiku_kits_ds_circlog_sequence(
    const struct tiku_kits_ds_circlog *log)
{
    if (log == NULL) {
        return 0;
    }
    return log->seq;
}
