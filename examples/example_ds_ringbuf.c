/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Ring Buffer Data Structure
 *
 * Demonstrates the fixed-size circular buffer for embedded applications:
 *   - UART receive buffer (producer/consumer)
 *   - Sensor sample history (sliding window)
 *   - Event logging (bounded log)
 *
 * All ring buffers use static storage (no heap). Maximum capacity is
 * controlled by TIKU_KITS_DS_RINGBUF_MAX_SIZE (default 32).
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

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/ringbuf/tiku_kits_ds_ringbuf.h"

/*---------------------------------------------------------------------------*/
/* Example 1: UART receive buffer                                            */
/*---------------------------------------------------------------------------*/

/**
 * Simulates a UART ISR pushing received bytes into a ring buffer,
 * and a main-loop consumer processing them in order.
 */
static void example_uart_buffer(void)
{
    struct tiku_kits_ds_ringbuf rx_buf;
    tiku_kits_ds_elem_t byte_val;
    uint16_t i;

    /* Simulated incoming UART bytes (ASCII chars) */
    static const tiku_kits_ds_elem_t uart_data[] = {
        'H', 'e', 'l', 'l', 'o', '!', '\r', '\n'
    };

    printf("--- UART Receive Buffer ---\n");

    tiku_kits_ds_ringbuf_init(&rx_buf, 16);

    /* ISR pushes bytes as they arrive */
    printf("  ISR receives: ");
    for (i = 0; i < 8; i++) {
        tiku_kits_ds_ringbuf_push(&rx_buf, uart_data[i]);
        if (uart_data[i] >= 0x20) {
            printf("%c", (char)uart_data[i]);
        } else {
            printf("\\x%02x", (unsigned)uart_data[i]);
        }
    }
    printf("\n");

    printf("  Buffer count: %u\n",
           tiku_kits_ds_ringbuf_count(&rx_buf));

    /* Main loop consumes bytes */
    printf("  Consumer reads: ");
    while (tiku_kits_ds_ringbuf_empty(&rx_buf) == 0) {
        tiku_kits_ds_ringbuf_pop(&rx_buf, &byte_val);
        if (byte_val >= 0x20) {
            printf("%c", (char)byte_val);
        } else {
            printf("\\x%02x", (unsigned)byte_val);
        }
    }
    printf("\n");
    printf("  Buffer empty: %s\n",
           tiku_kits_ds_ringbuf_empty(&rx_buf) ? "yes" : "no");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Sensor sample history                                          */
/*---------------------------------------------------------------------------*/

/**
 * Maintains a sliding window of the last N sensor samples.
 * New samples overwrite the oldest when the buffer is full
 * (pop first, then push).
 */
static void example_sensor_history(void)
{
    struct tiku_kits_ds_ringbuf history;
    tiku_kits_ds_elem_t val;
    uint16_t i;
    int32_t sum;

    /* Simulated ADC readings over time */
    static const tiku_kits_ds_elem_t samples[] = {
        512, 520, 508, 530, 515, 525, 510, 535
    };

    printf("--- Sensor Sample History (window=4) ---\n");

    tiku_kits_ds_ringbuf_init(&history, 4);

    for (i = 0; i < 8; i++) {
        /* If full, discard oldest to make room */
        if (tiku_kits_ds_ringbuf_full(&history)) {
            tiku_kits_ds_ringbuf_pop(&history, &val);
        }
        tiku_kits_ds_ringbuf_push(&history, samples[i]);

        /* Peek at the oldest sample in the window */
        tiku_kits_ds_ringbuf_peek(&history, &val);

        printf("  Sample %u = %3ld  |  count = %u  "
               "|  oldest = %ld\n",
               i, (long)samples[i],
               tiku_kits_ds_ringbuf_count(&history),
               (long)val);
    }

    /* Drain and compute average of final window */
    sum = 0;
    {
        uint16_t n = tiku_kits_ds_ringbuf_count(&history);
        uint16_t j;
        struct tiku_kits_ds_ringbuf copy = history;
        for (j = 0; j < n; j++) {
            tiku_kits_ds_ringbuf_pop(&copy, &val);
            sum += val;
        }
        printf("  Final window average: %ld\n",
               (long)(sum / n));
    }
}

/*---------------------------------------------------------------------------*/
/* Example 3: Event logging                                                  */
/*---------------------------------------------------------------------------*/

/**
 * Bounded event log: each event is an encoded 32-bit value.
 * Format: upper 16 bits = event type, lower 16 bits = timestamp.
 * When the log is full, new events are dropped (or you can drain
 * old ones first).
 */

/* Event type codes */
#define EVT_BOOT    0x0001
#define EVT_SENSOR  0x0002
#define EVT_TX      0x0003
#define EVT_ERROR   0x00FF

static tiku_kits_ds_elem_t encode_event(uint16_t type,
                                         uint16_t timestamp)
{
    return ((tiku_kits_ds_elem_t)type << 16)
           | (tiku_kits_ds_elem_t)timestamp;
}

static void decode_event(tiku_kits_ds_elem_t event,
                         uint16_t *type,
                         uint16_t *timestamp)
{
    *type = (uint16_t)((event >> 16) & 0xFFFF);
    *timestamp = (uint16_t)(event & 0xFFFF);
}

static const char *event_name(uint16_t type)
{
    switch (type) {
    case EVT_BOOT:   return "BOOT";
    case EVT_SENSOR: return "SENSOR";
    case EVT_TX:     return "TX";
    case EVT_ERROR:  return "ERROR";
    default:         return "UNKNOWN";
    }
}

static void example_event_log(void)
{
    struct tiku_kits_ds_ringbuf log;
    tiku_kits_ds_elem_t event;
    uint16_t type, ts;

    printf("--- Event Log ---\n");

    tiku_kits_ds_ringbuf_init(&log, 8);

    /* Log a sequence of system events */
    tiku_kits_ds_ringbuf_push(&log, encode_event(EVT_BOOT, 0));
    tiku_kits_ds_ringbuf_push(&log, encode_event(EVT_SENSOR, 100));
    tiku_kits_ds_ringbuf_push(&log, encode_event(EVT_SENSOR, 200));
    tiku_kits_ds_ringbuf_push(&log, encode_event(EVT_TX, 250));
    tiku_kits_ds_ringbuf_push(&log, encode_event(EVT_ERROR, 300));

    printf("  Log entries: %u\n",
           tiku_kits_ds_ringbuf_count(&log));

    /* Drain and print all events */
    printf("  Replaying log:\n");
    while (tiku_kits_ds_ringbuf_empty(&log) == 0) {
        tiku_kits_ds_ringbuf_pop(&log, &event);
        decode_event(event, &type, &ts);
        printf("    t=%3u  %-7s  (raw=0x%08lx)\n",
               ts, event_name(type), (unsigned long)event);
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_ringbuf_run(void)
{
    printf("=== TikuKits Ring Buffer Examples ===\n\n");

    example_uart_buffer();
    printf("\n");

    example_sensor_history();
    printf("\n");

    example_event_log();
    printf("\n");
}
