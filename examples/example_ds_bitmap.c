/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Bitmap Data Structure
 *
 * Demonstrates the bitmap library for embedded boolean flag tracking:
 *   - Sensor active flags (32 sensors packed into one uint32_t word)
 *   - Memory block allocator (first-fit allocation via find_first_clear)
 *   - Peripheral status register (toggle, count, and search)
 *
 * All bitmaps use static storage (no heap). Maximum bit count is
 * controlled by TIKU_KITS_DS_BITMAP_MAX_BITS (default 256).
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ds/tiku_kits_ds.h"
#include "../ds/bitmap/tiku_kits_ds_bitmap.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Sensor active flags                                            */
/*---------------------------------------------------------------------------*/

/**
 * Track which sensors are active in a wireless sensor network node.
 * Each bit corresponds to one sensor channel. This packs 32 boolean
 * flags into a single uint32_t word -- ideal for resource-constrained
 * MCUs where every byte of RAM matters.
 */
static void example_bitmap_sensor_flags(void)
{
    struct tiku_kits_ds_bitmap sensors;
    uint8_t active;
    uint16_t count;

    printf("--- Sensor Active Flags (32 channels) ---\n");

    tiku_kits_ds_bitmap_init(&sensors, 32);

    /* Activate sensors: temperature (0), humidity (3), light (7) */
    tiku_kits_ds_bitmap_set(&sensors, 0);
    tiku_kits_ds_bitmap_set(&sensors, 3);
    tiku_kits_ds_bitmap_set(&sensors, 7);

    count = tiku_kits_ds_bitmap_count_set(&sensors);
    printf("  Active sensors: %u / 32\n", count);

    /* Check if temperature sensor (channel 0) is active */
    tiku_kits_ds_bitmap_test(&sensors, 0, &active);
    printf("  Temperature (ch 0): %s\n",
           active ? "ACTIVE" : "inactive");

    /* Check if pressure sensor (channel 4) is active */
    tiku_kits_ds_bitmap_test(&sensors, 4, &active);
    printf("  Pressure    (ch 4): %s\n",
           active ? "ACTIVE" : "inactive");

    /* Deactivate humidity sensor for power saving */
    tiku_kits_ds_bitmap_clear_bit(&sensors, 3);
    count = tiku_kits_ds_bitmap_count_set(&sensors);
    printf("  After deactivating humidity: %u active\n", count);

    /* Enter deep sleep -- deactivate all sensors */
    tiku_kits_ds_bitmap_clear_all(&sensors);
    count = tiku_kits_ds_bitmap_count_set(&sensors);
    printf("  After deep sleep: %u active\n", count);
}

/*---------------------------------------------------------------------------*/
/* Example 2: Memory block allocator                                         */
/*---------------------------------------------------------------------------*/

/**
 * Simple first-fit memory block allocator using a bitmap. Each bit
 * represents a fixed-size memory block (e.g. 64 bytes). Allocation
 * finds the first clear bit, marks it as used, and returns the block
 * index. Deallocation clears the bit.
 *
 * This pattern is common in RTOS memory pools where malloc is not
 * available or too unpredictable for real-time constraints.
 */
static void example_bitmap_block_allocator(void)
{
    struct tiku_kits_ds_bitmap pool;
    uint16_t block;
    int rc;

    printf("--- Memory Block Allocator (16 blocks) ---\n");

    tiku_kits_ds_bitmap_init(&pool, 16);

    /* Allocate 3 blocks */
    rc = tiku_kits_ds_bitmap_find_first_clear(&pool, &block);
    if (rc == TIKU_KITS_DS_OK) {
        tiku_kits_ds_bitmap_set(&pool, block);
        printf("  Allocated block %u\n", block);
    }

    rc = tiku_kits_ds_bitmap_find_first_clear(&pool, &block);
    if (rc == TIKU_KITS_DS_OK) {
        tiku_kits_ds_bitmap_set(&pool, block);
        printf("  Allocated block %u\n", block);
    }

    rc = tiku_kits_ds_bitmap_find_first_clear(&pool, &block);
    if (rc == TIKU_KITS_DS_OK) {
        tiku_kits_ds_bitmap_set(&pool, block);
        printf("  Allocated block %u\n", block);
    }

    printf("  Used: %u / 16 blocks\n",
           tiku_kits_ds_bitmap_count_set(&pool));

    /* Free block 1 (middle block) */
    tiku_kits_ds_bitmap_clear_bit(&pool, 1);
    printf("  Freed block 1\n");

    /* Next allocation reuses block 1 (first-fit) */
    rc = tiku_kits_ds_bitmap_find_first_clear(&pool, &block);
    if (rc == TIKU_KITS_DS_OK) {
        tiku_kits_ds_bitmap_set(&pool, block);
        printf("  Re-allocated block %u (first-fit reuse)\n",
               block);
    }

    printf("  Free blocks remaining: %u\n",
           tiku_kits_ds_bitmap_count_clear(&pool));
}

/*---------------------------------------------------------------------------*/
/* Example 3: Peripheral status register                                     */
/*---------------------------------------------------------------------------*/

/**
 * Model a peripheral status register where each bit represents the
 * operational status of a hardware peripheral. Demonstrates toggle,
 * count, and search operations useful for system diagnostics.
 */
static void example_bitmap_peripheral_status(void)
{
    struct tiku_kits_ds_bitmap status;
    uint16_t first_active;
    uint16_t first_idle;
    int rc;

    printf("--- Peripheral Status Register (8 peripherals) ---\n");

    tiku_kits_ds_bitmap_init(&status, 8);

    /* Peripherals: 0=UART, 1=SPI, 2=I2C, 3=ADC,
     *              4=DMA, 5=PWM, 6=RTC, 7=WDT */

    /* Boot: activate UART, RTC, and WDT */
    tiku_kits_ds_bitmap_set(&status, 0);   /* UART */
    tiku_kits_ds_bitmap_set(&status, 6);   /* RTC  */
    tiku_kits_ds_bitmap_set(&status, 7);   /* WDT  */

    printf("  After boot: %u peripherals active\n",
           tiku_kits_ds_bitmap_count_set(&status));

    /* Toggle SPI for a transfer, then toggle back */
    tiku_kits_ds_bitmap_toggle(&status, 1);
    printf("  SPI toggled ON for transfer\n");
    tiku_kits_ds_bitmap_toggle(&status, 1);
    printf("  SPI toggled OFF after transfer\n");

    /* Find first active peripheral */
    rc = tiku_kits_ds_bitmap_find_first_set(&status, &first_active);
    if (rc == TIKU_KITS_DS_OK) {
        printf("  First active peripheral: index %u (UART)\n",
               first_active);
    }

    /* Find first idle peripheral */
    rc = tiku_kits_ds_bitmap_find_first_clear(&status, &first_idle);
    if (rc == TIKU_KITS_DS_OK) {
        printf("  First idle peripheral:   index %u (SPI)\n",
               first_idle);
    }

    /* Activate all peripherals for self-test */
    tiku_kits_ds_bitmap_set_all(&status);
    printf("  Self-test: all %u peripherals active\n",
           tiku_kits_ds_bitmap_count_set(&status));
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ds_bitmap_run(void)
{
    printf("=== TikuKits Bitmap Examples ===\n\n");

    example_bitmap_sensor_flags();
    printf("\n");

    example_bitmap_block_allocator();
    printf("\n");

    example_bitmap_peripheral_status();
    printf("\n");
}
