/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Hash Table
 *
 * Demonstrates the open-addressed hash table for common embedded
 * use cases:
 *   1. Configuration store -- key-value settings with update
 *   2. Sensor ID lookup    -- fast node-ID to reading mapping
 *   3. String-to-enum map  -- pre-hashed command dispatch
 *
 * All storage is static (no heap). Maximum entry count is controlled
 * by TIKU_KITS_DS_HTABLE_MAX_SIZE (default 16).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/htable/tiku_kits_ds_htable.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Configuration key-value store                                  */
/*---------------------------------------------------------------------------*/

/**
 * Stores device configuration parameters as key-value pairs.
 * Keys are 32-bit config IDs; values are the settings. Supports
 * lookup and in-place update.
 */
static void example_config_store(void)
{
    struct tiku_kits_ds_htable cfg;
    tiku_kits_ds_elem_t val;

    /* Configuration key IDs */
    enum {
        CFG_BAUD_RATE   = 0x0001,
        CFG_SAMPLE_RATE = 0x0002,
        CFG_TX_POWER    = 0x0003,
        CFG_SLEEP_MS    = 0x0004
    };

    printf("--- Configuration Key-Value Store ---\n");

    tiku_kits_ds_htable_init(&cfg, 8);

    /* Set initial configuration */
    tiku_kits_ds_htable_put(&cfg, CFG_BAUD_RATE,   9600);
    tiku_kits_ds_htable_put(&cfg, CFG_SAMPLE_RATE, 100);
    tiku_kits_ds_htable_put(&cfg, CFG_TX_POWER,    10);
    tiku_kits_ds_htable_put(&cfg, CFG_SLEEP_MS,    5000);

    printf("  Config entries: %u\n",
           tiku_kits_ds_htable_count(&cfg));

    /* Read a setting */
    if (tiku_kits_ds_htable_get(&cfg, CFG_BAUD_RATE, &val) ==
        TIKU_KITS_DS_OK) {
        printf("  Baud rate: %ld\n", (long)val);
    }

    /* Update TX power at runtime */
    tiku_kits_ds_htable_put(&cfg, CFG_TX_POWER, 20);
    tiku_kits_ds_htable_get(&cfg, CFG_TX_POWER, &val);
    printf("  TX power (updated): %ld dBm\n", (long)val);

    /* Remove a setting */
    tiku_kits_ds_htable_remove(&cfg, CFG_SLEEP_MS);
    printf("  After removing SLEEP_MS: %u entries\n",
           tiku_kits_ds_htable_count(&cfg));
}

/*---------------------------------------------------------------------------*/
/* Example 2: Sensor ID lookup table                                         */
/*---------------------------------------------------------------------------*/

/**
 * Maps sensor node IDs (uint32_t) to their most recent reading.
 * Demonstrates fast O(1) average-case lookup for incoming sensor
 * data packets.
 */
static void example_sensor_lookup(void)
{
    struct tiku_kits_ds_htable sensors;
    tiku_kits_ds_elem_t reading;
    uint32_t query_id;

    printf("--- Sensor ID Lookup ---\n");

    tiku_kits_ds_htable_init(&sensors, 8);

    /* Register sensor readings: node_id -> temperature (0.1 deg) */
    tiku_kits_ds_htable_put(&sensors, 0x1001, 235);  /* 23.5 C */
    tiku_kits_ds_htable_put(&sensors, 0x1002, 218);  /* 21.8 C */
    tiku_kits_ds_htable_put(&sensors, 0x1003, 241);  /* 24.1 C */
    tiku_kits_ds_htable_put(&sensors, 0x1004, 199);  /* 19.9 C */

    printf("  Registered sensors: %u\n",
           tiku_kits_ds_htable_count(&sensors));

    /* Look up a specific sensor */
    query_id = 0x1003;
    if (tiku_kits_ds_htable_get(&sensors, query_id, &reading) ==
        TIKU_KITS_DS_OK) {
        printf("  Sensor 0x%04lX: %ld.%ld C\n",
               (unsigned long)query_id,
               (long)(reading / 10), (long)(reading % 10));
    }

    /* Update a reading */
    tiku_kits_ds_htable_put(&sensors, 0x1001, 237);
    tiku_kits_ds_htable_get(&sensors, 0x1001, &reading);
    printf("  Sensor 0x1001 updated: %ld.%ld C\n",
           (long)(reading / 10), (long)(reading % 10));

    /* Check if a sensor exists */
    printf("  Sensor 0x1005 present: %s\n",
           tiku_kits_ds_htable_contains(&sensors, 0x1005)
               ? "yes" : "no");

    /* Node leaves: remove sensor 0x1002 */
    tiku_kits_ds_htable_remove(&sensors, 0x1002);
    printf("  After 0x1002 left: %u sensors\n",
           tiku_kits_ds_htable_count(&sensors));
}

/*---------------------------------------------------------------------------*/
/* Example 3: String-to-enum mapping with pre-hashed keys                    */
/*---------------------------------------------------------------------------*/

/**
 * Maps command strings to enum values using pre-computed FNV-1a
 * hashes. On an embedded target, strings are hashed at compile time
 * (or in a host tool), so the MCU only stores/compares uint32_t
 * keys. This avoids storing or comparing raw strings at runtime.
 *
 * Pre-computed hashes (FNV-1a of ASCII bytes):
 *   "START" -> 0xBC2DF tried: use distinct uint32_t constants
 */
static void example_command_dispatch(void)
{
    struct tiku_kits_ds_htable cmds;
    tiku_kits_ds_elem_t cmd_id;

    /* Pre-hashed command keys (representative constants) */
    enum {
        HASH_START = 0xA001,
        HASH_STOP  = 0xA002,
        HASH_RESET = 0xA003,
        HASH_STATUS = 0xA004
    };

    /* Command enum values */
    enum {
        CMD_START  = 1,
        CMD_STOP   = 2,
        CMD_RESET  = 3,
        CMD_STATUS = 4
    };

    printf("--- String-to-Enum Map (Pre-Hashed Keys) ---\n");

    tiku_kits_ds_htable_init(&cmds, 8);

    /* Register command mappings */
    tiku_kits_ds_htable_put(&cmds, HASH_START,  CMD_START);
    tiku_kits_ds_htable_put(&cmds, HASH_STOP,   CMD_STOP);
    tiku_kits_ds_htable_put(&cmds, HASH_RESET,  CMD_RESET);
    tiku_kits_ds_htable_put(&cmds, HASH_STATUS, CMD_STATUS);

    printf("  Registered commands: %u\n",
           tiku_kits_ds_htable_count(&cmds));

    /* Dispatch: simulate receiving "STOP" (pre-hashed) */
    if (tiku_kits_ds_htable_get(&cmds, HASH_STOP, &cmd_id) ==
        TIKU_KITS_DS_OK) {
        printf("  Received STOP -> enum %ld\n", (long)cmd_id);
    }

    /* Dispatch: simulate receiving "RESET" */
    if (tiku_kits_ds_htable_get(&cmds, HASH_RESET, &cmd_id) ==
        TIKU_KITS_DS_OK) {
        printf("  Received RESET -> enum %ld\n", (long)cmd_id);
    }

    /* Unknown command */
    if (tiku_kits_ds_htable_get(&cmds, 0xFFFF, &cmd_id) !=
        TIKU_KITS_DS_OK) {
        printf("  Unknown command 0xFFFF: not found\n");
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_htable_run(void)
{
    printf("=== TikuKits Hash Table Examples ===\n\n");

    example_config_store();
    printf("\n");

    example_sensor_lookup();
    printf("\n");

    example_command_dispatch();
    printf("\n");
}
