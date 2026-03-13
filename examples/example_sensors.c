/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Temperature Sensors
 *
 * Demonstrates how to read temperature from three different sensors:
 *   - MCP9808 (I2C, Microchip)
 *   - ADT7410 (I2C, Analog Devices)
 *   - DS18B20 (1-Wire, Dallas/Maxim)
 *
 * All sensors produce the same tiku_kits_sensor_temp_t structure,
 * making it easy to swap sensors without changing application logic.
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../sensors/tiku_kits_sensor.h"
#include "../sensors/temperature/tiku_kits_sensor_mcp9808.h"
#include "../sensors/temperature/tiku_kits_sensor_adt7410.h"
#include "../sensors/temperature/tiku_kits_sensor_ds18b20.h"

/*---------------------------------------------------------------------------*/
/* Helper: print a temperature reading                                       */
/*---------------------------------------------------------------------------*/

static void print_temp(const char *name, const tiku_kits_sensor_temp_t *t)
{
    int frac_dec = TIKU_KITS_SENSOR_FRAC_TO_DEC(t->frac);
    printf("  %s: %s%d.%02d C\n",
           name,
           t->negative ? "-" : "",
           (int)t->integer,
           frac_dec);
}

/*---------------------------------------------------------------------------*/
/* Example 1: Read from a single I2C sensor (MCP9808)                        */
/*---------------------------------------------------------------------------*/

static void example_mcp9808_single_read(void)
{
    tiku_kits_sensor_temp_t temp;
    int rc;

    printf("--- MCP9808 Single Read ---\n");

    /* Initialize the sensor at the default I2C address.
     * The I2C bus must already be configured by the platform. */
    rc = tiku_kits_sensor_mcp9808_init(
        TIKU_KITS_SENSOR_MCP9808_ADDR_DEFAULT);
    if (rc != TIKU_KITS_SENSOR_OK) {
        printf("  MCP9808 init failed (error %d)\n", rc);
        return;
    }

    /* Read temperature */
    rc = tiku_kits_sensor_mcp9808_read(&temp);
    if (rc == TIKU_KITS_SENSOR_OK) {
        print_temp(tiku_kits_sensor_mcp9808_name(), &temp);
    } else {
        printf("  MCP9808 read failed (error %d)\n", rc);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 2: Read from ADT7410                                              */
/*---------------------------------------------------------------------------*/

static void example_adt7410_single_read(void)
{
    tiku_kits_sensor_temp_t temp;
    int rc;

    printf("--- ADT7410 Single Read ---\n");

    rc = tiku_kits_sensor_adt7410_init(
        TIKU_KITS_SENSOR_ADT7410_ADDR_DEFAULT);
    if (rc != TIKU_KITS_SENSOR_OK) {
        printf("  ADT7410 init failed (error %d)\n", rc);
        return;
    }

    rc = tiku_kits_sensor_adt7410_read(&temp);
    if (rc == TIKU_KITS_SENSOR_OK) {
        print_temp(tiku_kits_sensor_adt7410_name(), &temp);
    } else {
        printf("  ADT7410 read failed (error %d)\n", rc);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 3: Read from DS18B20 (1-Wire, two-phase read)                     */
/*---------------------------------------------------------------------------*/

static void example_ds18b20_read(void)
{
    tiku_kits_sensor_temp_t temp;
    int rc;

    printf("--- DS18B20 Two-Phase Read ---\n");

    rc = tiku_kits_sensor_ds18b20_init();
    if (rc != TIKU_KITS_SENSOR_OK) {
        printf("  DS18B20 not detected (error %d)\n", rc);
        return;
    }

    /* Start conversion -- the sensor needs up to 750 ms */
    rc = tiku_kits_sensor_ds18b20_start_conversion();
    if (rc != TIKU_KITS_SENSOR_OK) {
        printf("  DS18B20 conversion start failed (error %d)\n", rc);
        return;
    }

    /*
     * In a real application, use a timer or protothread yield here
     * instead of busy-waiting:
     *
     *   PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
     *
     * For this example we assume the wait has elapsed.
     */
    printf("  (waiting 750 ms for conversion...)\n");

    rc = tiku_kits_sensor_ds18b20_read(&temp);
    if (rc == TIKU_KITS_SENSOR_OK) {
        print_temp(tiku_kits_sensor_ds18b20_name(), &temp);
    } else {
        printf("  DS18B20 read failed (error %d)\n", rc);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 4: Polymorphic sensor reading using function pointers              */
/*---------------------------------------------------------------------------*/

typedef int (*sensor_read_fn)(tiku_kits_sensor_temp_t *);
typedef const char *(*sensor_name_fn)(void);

struct sensor_driver {
    sensor_read_fn  read;
    sensor_name_fn  name;
};

static const struct sensor_driver drivers[] = {
    { tiku_kits_sensor_mcp9808_read, tiku_kits_sensor_mcp9808_name },
    { tiku_kits_sensor_adt7410_read, tiku_kits_sensor_adt7410_name },
    { tiku_kits_sensor_ds18b20_read, tiku_kits_sensor_ds18b20_name },
};

#define NUM_DRIVERS (sizeof(drivers) / sizeof(drivers[0]))

static void example_multi_sensor_poll(void)
{
    tiku_kits_sensor_temp_t temp;
    uint8_t i;
    int rc;

    printf("--- Multi-Sensor Poll ---\n");

    /*
     * Assumes all sensors are already initialized.
     * In production, check init return codes first.
     */
    for (i = 0; i < NUM_DRIVERS; i++) {
        rc = drivers[i].read(&temp);
        if (rc == TIKU_KITS_SENSOR_OK) {
            print_temp(drivers[i].name(), &temp);
        } else {
            printf("  %s: error %d\n", drivers[i].name(), rc);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_sensors_run(void)
{
    printf("=== TikuKits Sensor Examples ===\n\n");

    example_mcp9808_single_read();
    printf("\n");

    example_adt7410_single_read();
    printf("\n");

    example_ds18b20_read();
    printf("\n");

    example_multi_sensor_poll();
    printf("\n");
}
