/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Linear Regression
 *
 * Demonstrates simple linear regression for common embedded use cases:
 *   1. Sensor calibration  -- map raw ADC readings to physical units
 *   2. Trend estimation    -- detect linear drift in sensor readings
 *   3. Prediction          -- interpolate / extrapolate from training data
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ml/tiku_kits_ml.h"
#include "../ml/regression/tiku_kits_ml_linreg.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Sensor calibration (raw ADC -> temperature in 0.1 deg C)       */
/*---------------------------------------------------------------------------*/

static void example_calibration(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t slope_q, intercept_q, r2_q;
    tiku_kits_ml_elem_t y_pred;

    printf("--- Sensor Calibration ---\n");

    /*
     * Calibration data: (raw_adc, temperature_deci_C)
     * Measured at known reference temperatures.
     */
    tiku_kits_ml_linreg_init(&lr, 8);

    tiku_kits_ml_linreg_push(&lr, 100, 0);     /* 0.0 C */
    tiku_kits_ml_linreg_push(&lr, 200, 100);   /* 10.0 C */
    tiku_kits_ml_linreg_push(&lr, 300, 200);   /* 20.0 C */
    tiku_kits_ml_linreg_push(&lr, 400, 300);   /* 30.0 C */
    tiku_kits_ml_linreg_push(&lr, 500, 400);   /* 40.0 C */

    printf("  Data points: %u\n", tiku_kits_ml_linreg_count(&lr));

    tiku_kits_ml_linreg_slope(&lr, &slope_q);
    tiku_kits_ml_linreg_intercept(&lr, &intercept_q);
    tiku_kits_ml_linreg_r2(&lr, &r2_q);

    printf("  Slope (Q8):     %ld  (real: %ld.%03ld)\n",
           (long)slope_q,
           (long)(slope_q / 256),
           (long)((slope_q % 256) * 1000 / 256));
    printf("  Intercept (Q8): %ld  (real: %ld.%03ld)\n",
           (long)intercept_q,
           (long)(intercept_q / 256),
           (long)((intercept_q % 256) * 1000 / 256));
    printf("  R-squared (Q8): %ld / 256\n", (long)r2_q);

    /* Convert a new ADC reading to temperature */
    tiku_kits_ml_linreg_predict(&lr, 350, &y_pred);
    printf("  ADC=350 -> temp=%ld.%ld C\n",
           (long)(y_pred / 10), (long)(y_pred % 10));
}

/*---------------------------------------------------------------------------*/
/* Example 2: Trend estimation (detect drift over time)                      */
/*---------------------------------------------------------------------------*/

static void example_trend(void)
{
    struct tiku_kits_ml_linreg lr;
    int32_t slope_q;
    uint16_t i;

    /* Simulated sensor readings over time (slight upward drift) */
    static const tiku_kits_ml_elem_t readings[] = {
        500, 502, 498, 505, 501, 507, 503, 510,
        506, 512, 508, 515, 511, 518, 514, 521
    };
    const uint16_t num = sizeof(readings) / sizeof(readings[0]);

    printf("--- Trend Estimation ---\n");

    tiku_kits_ml_linreg_init(&lr, 8);

    for (i = 0; i < num; i++) {
        tiku_kits_ml_linreg_push(&lr, (tiku_kits_ml_elem_t)i,
                                  readings[i]);
    }

    tiku_kits_ml_linreg_slope(&lr, &slope_q);

    printf("  Samples:    %u\n", tiku_kits_ml_linreg_count(&lr));
    printf("  Slope (Q8): %ld  (drift per sample: %ld.%03ld)\n",
           (long)slope_q,
           (long)(slope_q / 256),
           (long)((slope_q % 256) * 1000 / 256));

    if (slope_q > 25) {         /* > ~0.1 per sample */
        printf("  -> Upward drift detected\n");
    } else if (slope_q < -25) {
        printf("  -> Downward drift detected\n");
    } else {
        printf("  -> Stable (no significant drift)\n");
    }
}

/*---------------------------------------------------------------------------*/
/* Example 3: Prediction / extrapolation                                     */
/*---------------------------------------------------------------------------*/

static void example_prediction(void)
{
    struct tiku_kits_ml_linreg lr;
    tiku_kits_ml_elem_t y_pred;
    tiku_kits_ml_elem_t x_test;

    printf("--- Prediction ---\n");

    tiku_kits_ml_linreg_init(&lr, 8);

    /* Battery voltage (mV) vs. charge level (percent * 10) */
    tiku_kits_ml_linreg_push(&lr, 3000, 0);     /* 0% */
    tiku_kits_ml_linreg_push(&lr, 3300, 250);   /* 25% */
    tiku_kits_ml_linreg_push(&lr, 3600, 500);   /* 50% */
    tiku_kits_ml_linreg_push(&lr, 3900, 750);   /* 75% */
    tiku_kits_ml_linreg_push(&lr, 4200, 1000);  /* 100% */

    printf("  Model fitted with %u calibration points\n",
           tiku_kits_ml_linreg_count(&lr));

    /* Predict charge level for measured voltages */
    x_test = 3450;
    tiku_kits_ml_linreg_predict(&lr, x_test, &y_pred);
    printf("  V=%ld mV -> charge=%ld.%ld%%\n",
           (long)x_test,
           (long)(y_pred / 10), (long)(y_pred % 10));

    x_test = 3750;
    tiku_kits_ml_linreg_predict(&lr, x_test, &y_pred);
    printf("  V=%ld mV -> charge=%ld.%ld%%\n",
           (long)x_test,
           (long)(y_pred / 10), (long)(y_pred % 10));
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ml_linreg_run(void)
{
    printf("=== TikuKits Linear Regression Examples ===\n\n");

    example_calibration();
    printf("\n");

    example_trend();
    printf("\n");

    example_prediction();
    printf("\n");
}
