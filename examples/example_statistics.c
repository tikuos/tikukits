/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Statistics Trackers
 *
 * Demonstrates all five statistics trackers:
 *   1. Windowed statistics  -- mean, variance, min, max over a sliding window
 *   2. Welford tracker      -- streaming mean/variance with no window
 *   3. Sliding min/max      -- O(1) amortized min and max via monotonic deques
 *   4. EWMA                 -- exponentially weighted moving average filter
 *   5. Energy/RMS           -- running signal power and root-mean-square
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../maths/tiku_kits_maths.h"
#include "../maths/statistics/tiku_kits_statistics.h"

/*---------------------------------------------------------------------------*/
/* Sample data: simulated ADC readings (e.g. from an accelerometer)          */
/*---------------------------------------------------------------------------*/

static const tiku_kits_statistics_elem_t samples[] = {
    512, 520, 495, 530, 510, 540, 480, 550,
    505, 515, 490, 535, 500, 525, 485, 545
};

#define NUM_SAMPLES (sizeof(samples) / sizeof(samples[0]))

/*---------------------------------------------------------------------------*/
/* Example 1: Windowed statistics (mean, variance, min, max)                 */
/*---------------------------------------------------------------------------*/

static void example_windowed(void)
{
    struct tiku_kits_statistics stat;
    tiku_kits_statistics_elem_t mean, var, lo, hi;
    uint16_t i;

    printf("--- Windowed Statistics (window=8) ---\n");

    tiku_kits_statistics_init(&stat, 8);

    for (i = 0; i < NUM_SAMPLES; i++) {
        tiku_kits_statistics_push(&stat, samples[i]);
    }

    tiku_kits_statistics_mean(&stat, &mean);
    tiku_kits_statistics_variance(&stat, &var);
    tiku_kits_statistics_min(&stat, &lo);
    tiku_kits_statistics_max(&stat, &hi);

    printf("  Samples in window: %u / %u\n",
           tiku_kits_statistics_count(&stat),
           tiku_kits_statistics_capacity(&stat));
    printf("  Mean:     %ld\n", (long)mean);
    printf("  Variance: %ld\n", (long)var);
    printf("  Min:      %ld\n", (long)lo);
    printf("  Max:      %ld\n", (long)hi);
}

/*---------------------------------------------------------------------------*/
/* Example 2: Welford running mean/variance                                  */
/*---------------------------------------------------------------------------*/

static void example_welford(void)
{
    struct tiku_kits_statistics_welford w;
    tiku_kits_statistics_elem_t mean, var, sd;
    uint16_t i;

    printf("--- Welford Running Statistics ---\n");

    tiku_kits_statistics_welford_init(&w);

    for (i = 0; i < NUM_SAMPLES; i++) {
        tiku_kits_statistics_welford_push(&w, samples[i]);
    }

    tiku_kits_statistics_welford_mean(&w, &mean);
    tiku_kits_statistics_welford_variance(&w, &var);
    tiku_kits_statistics_welford_stddev(&w, &sd);

    printf("  Total samples: %lu\n",
           (unsigned long)tiku_kits_statistics_welford_count(&w));
    printf("  Mean:     %ld\n", (long)mean);
    printf("  Variance: %ld\n", (long)var);
    printf("  Stddev:   %ld\n", (long)sd);
}

/*---------------------------------------------------------------------------*/
/* Example 3: Sliding-window min/max                                         */
/*---------------------------------------------------------------------------*/

static void example_minmax(void)
{
    struct tiku_kits_statistics_minmax mm;
    tiku_kits_statistics_elem_t lo, hi;
    uint16_t i;

    printf("--- Sliding Min/Max (window=4) ---\n");

    tiku_kits_statistics_minmax_init(&mm, 4);

    /* Push samples and report min/max after each push */
    for (i = 0; i < 8; i++) {
        tiku_kits_statistics_minmax_push(&mm, samples[i]);

        if (tiku_kits_statistics_minmax_count(&mm) >= 1) {
            tiku_kits_statistics_minmax_min(&mm, &lo);
            tiku_kits_statistics_minmax_max(&mm, &hi);
            printf("  After sample[%u]=%ld: min=%ld max=%ld\n",
                   i, (long)samples[i], (long)lo, (long)hi);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Example 4: EWMA (smoothing noisy sensor data)                             */
/*---------------------------------------------------------------------------*/

static void example_ewma(void)
{
    struct tiku_kits_statistics_ewma e;
    tiku_kits_statistics_elem_t smoothed;
    uint16_t i;

    printf("--- EWMA Smoothing ---\n");

    /*
     * alpha=64 with shift=8 (256 total): ~25% weight on new sample.
     * Lower alpha = more smoothing, higher alpha = faster tracking.
     */
    tiku_kits_statistics_ewma_init(&e, 64,
        TIKU_KITS_STATISTICS_EWMA_SHIFT);

    for (i = 0; i < NUM_SAMPLES; i++) {
        tiku_kits_statistics_ewma_push(&e, samples[i]);
        tiku_kits_statistics_ewma_get(&e, &smoothed);
        printf("  raw=%ld  smoothed=%ld\n",
               (long)samples[i], (long)smoothed);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 5: Energy / RMS (signal power estimation)                         */
/*---------------------------------------------------------------------------*/

static void example_energy_rms(void)
{
    struct tiku_kits_statistics_energy en;
    tiku_kits_statistics_elem_t mean_sq, rms;
    uint16_t i;

    printf("--- Energy / RMS ---\n");

    tiku_kits_statistics_energy_init(&en);

    /* Feed samples centered around zero (subtract DC offset of ~512) */
    for (i = 0; i < NUM_SAMPLES; i++) {
        tiku_kits_statistics_energy_push(&en, samples[i] - 512);
    }

    tiku_kits_statistics_energy_mean_sq(&en, &mean_sq);
    tiku_kits_statistics_energy_rms(&en, &rms);

    printf("  Samples:   %lu\n",
           (unsigned long)tiku_kits_statistics_energy_count(&en));
    printf("  Mean(x^2): %ld\n", (long)mean_sq);
    printf("  RMS:       %ld\n", (long)rms);
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_statistics_run(void)
{
    printf("=== TikuKits Statistics Examples ===\n\n");

    example_windowed();
    printf("\n");

    example_welford();
    printf("\n");

    example_minmax();
    printf("\n");

    example_ewma();
    printf("\n");

    example_energy_rms();
    printf("\n");
}
