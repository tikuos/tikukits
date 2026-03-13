/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Signal Feature Extraction
 *
 * Demonstrates signal processing features for embedded ML pipelines:
 *   1. Delta (first-order difference)
 *   2. Peak detection with hysteresis
 *   3. Zero-crossing rate
 *   4. Goertzel single-frequency DFT
 *   5. Histogram binning
 *   6. Z-score normalization
 *   7. Min-max scaling
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../sigfeatures/tiku_kits_sigfeatures.h"
#include "../sigfeatures/delta/tiku_kits_sigfeatures_delta.h"
#include "../sigfeatures/peak/tiku_kits_sigfeatures_peak.h"
#include "../sigfeatures/zcr/tiku_kits_sigfeatures_zcr.h"
#include "../sigfeatures/goertzel/tiku_kits_sigfeatures_goertzel.h"
#include "../sigfeatures/histogram/tiku_kits_sigfeatures_histogram.h"
#include "../sigfeatures/zscore/tiku_kits_sigfeatures_zscore.h"
#include "../sigfeatures/scale/tiku_kits_sigfeatures_scale.h"

/*---------------------------------------------------------------------------*/
/* Sample signal: simulated heartbeat-like waveform                          */
/*---------------------------------------------------------------------------*/

static const tiku_kits_sigfeatures_elem_t heartbeat[] = {
    10, 15, 25, 80, 120, 95, 40, 15, 10, 8,
    10, 14, 22, 75, 115, 90, 38, 12, 9, 8,
    11, 16, 28, 85, 125, 98, 42, 16, 10, 9
};

#define HEARTBEAT_LEN (sizeof(heartbeat) / sizeof(heartbeat[0]))

/*---------------------------------------------------------------------------*/
/* Example 1: Delta -- rate of change                                        */
/*---------------------------------------------------------------------------*/

static void example_delta(void)
{
    struct tiku_kits_sigfeatures_delta d;
    tiku_kits_sigfeatures_elem_t result;
    tiku_kits_sigfeatures_elem_t batch_out[HEARTBEAT_LEN - 1];
    uint16_t i;

    printf("--- Delta (First-Order Difference) ---\n");

    /* Streaming mode */
    tiku_kits_sigfeatures_delta_init(&d);
    printf("  Streaming:\n");
    for (i = 0; i < 6; i++) {
        tiku_kits_sigfeatures_delta_push(&d, heartbeat[i]);
        if (tiku_kits_sigfeatures_delta_get(&d, &result)
            == TIKU_KITS_SIGFEATURES_OK) {
            printf("    delta[%u] = %ld\n", i, (long)result);
        }
    }

    /* Batch mode */
    tiku_kits_sigfeatures_delta_compute(
        heartbeat, HEARTBEAT_LEN, batch_out);
    printf("  Batch (first 6): ");
    for (i = 0; i < 6; i++) {
        printf("%ld ", (long)batch_out[i]);
    }
    printf("...\n");
}

/*---------------------------------------------------------------------------*/
/* Example 2: Peak detection (heartbeat/step counting)                       */
/*---------------------------------------------------------------------------*/

static void example_peak_detection(void)
{
    struct tiku_kits_sigfeatures_peak p;
    tiku_kits_sigfeatures_elem_t peak_val;
    uint16_t peak_idx;
    uint16_t i;

    printf("--- Peak Detection (hysteresis=30) ---\n");

    tiku_kits_sigfeatures_peak_init(&p, 30);

    for (i = 0; i < HEARTBEAT_LEN; i++) {
        tiku_kits_sigfeatures_peak_push(&p, heartbeat[i]);

        if (tiku_kits_sigfeatures_peak_detected(&p)) {
            tiku_kits_sigfeatures_peak_last_value(&p, &peak_val);
            tiku_kits_sigfeatures_peak_last_index(&p, &peak_idx);
            printf("  Peak #%u: value=%ld at index=%u\n",
                   tiku_kits_sigfeatures_peak_count(&p),
                   (long)peak_val, peak_idx);
        }
    }

    printf("  Total peaks detected: %u\n",
           tiku_kits_sigfeatures_peak_count(&p));
}

/*---------------------------------------------------------------------------*/
/* Example 3: Zero-crossing rate                                             */
/*---------------------------------------------------------------------------*/

static void example_zcr(void)
{
    struct tiku_kits_sigfeatures_zcr z;
    uint16_t i;

    /* AC-coupled signal: subtract DC offset of ~50 */
    static const tiku_kits_sigfeatures_elem_t ac_signal[] = {
        10, -5, 8, -12, 3, -7, 15, -20,
        6, -3, 11, -8, 4, -14, 9, -6
    };

    printf("--- Zero-Crossing Rate (window=8) ---\n");

    tiku_kits_sigfeatures_zcr_init(&z, 8);

    for (i = 0; i < 16; i++) {
        tiku_kits_sigfeatures_zcr_push(&z, ac_signal[i]);

        if (tiku_kits_sigfeatures_zcr_full(&z)) {
            printf("  After sample[%u]: crossings=%u / %u samples\n",
                   i,
                   tiku_kits_sigfeatures_zcr_crossings(&z),
                   tiku_kits_sigfeatures_zcr_count(&z));
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Example 4: Goertzel -- single-frequency energy detection                  */
/*---------------------------------------------------------------------------*/

static void example_goertzel(void)
{
    struct tiku_kits_sigfeatures_goertzel g;
    int64_t mag_sq;

    /*
     * Detect energy at bin k=8 in a 16-sample block.
     * coeff_q14 = round(2 * cos(2*pi*8/16) * 16384)
     *           = round(2 * cos(pi) * 16384)
     *           = round(-2 * 16384)
     *           = -32768
     */
    static const tiku_kits_sigfeatures_elem_t tone[] = {
        100, -100, 100, -100, 100, -100, 100, -100,
        100, -100, 100, -100, 100, -100, 100, -100
    };

    printf("--- Goertzel Single-Frequency DFT ---\n");

    /* Alternating +/-100 at N/2 (Nyquist bin k=8 for N=16) */
    tiku_kits_sigfeatures_goertzel_init(&g, -32768, 16);
    tiku_kits_sigfeatures_goertzel_block(
        &g, tone, 16, &mag_sq);
    printf("  Nyquist bin energy (alternating signal): %lld\n",
           (long long)mag_sq);

    /* DC signal: all samples are 100 (energy at k=8 should be low) */
    static const tiku_kits_sigfeatures_elem_t dc[] = {
        100, 100, 100, 100, 100, 100, 100, 100,
        100, 100, 100, 100, 100, 100, 100, 100
    };

    tiku_kits_sigfeatures_goertzel_init(&g, -32768, 16);
    tiku_kits_sigfeatures_goertzel_block(&g, dc, 16, &mag_sq);
    printf("  Nyquist bin energy (DC signal):          %lld\n",
           (long long)mag_sq);
}

/*---------------------------------------------------------------------------*/
/* Example 5: Histogram -- distribution of signal values                     */
/*---------------------------------------------------------------------------*/

static void example_histogram(void)
{
    struct tiku_kits_sigfeatures_histogram h;
    uint8_t mode_bin;
    uint8_t i;

    printf("--- Histogram (8 bins, [0..160), width=20) ---\n");

    /* 8 bins: [0,20), [20,40), [40,60), ... [140,160) */
    tiku_kits_sigfeatures_histogram_init(&h, 8, 0, 20);

    for (i = 0; i < HEARTBEAT_LEN; i++) {
        tiku_kits_sigfeatures_histogram_push(&h, heartbeat[i]);
    }

    printf("  Bin counts:\n");
    for (i = 0; i < 8; i++) {
        printf("    [%3d, %3d): %u\n",
               i * 20, (i + 1) * 20,
               tiku_kits_sigfeatures_histogram_get_bin(&h, i));
    }

    printf("  Total:     %u\n",
           tiku_kits_sigfeatures_histogram_total(&h));
    printf("  Underflow: %u\n",
           tiku_kits_sigfeatures_histogram_underflow(&h));
    printf("  Overflow:  %u\n",
           tiku_kits_sigfeatures_histogram_overflow(&h));

    tiku_kits_sigfeatures_histogram_mode_bin(&h, &mode_bin);
    printf("  Mode bin:  %u ([%d, %d))\n",
           mode_bin, mode_bin * 20, (mode_bin + 1) * 20);
}

/*---------------------------------------------------------------------------*/
/* Example 6: Z-score normalization                                          */
/*---------------------------------------------------------------------------*/

static void example_zscore(void)
{
    struct tiku_kits_sigfeatures_zscore z;
    int32_t z_score;
    uint16_t i;

    printf("--- Z-Score Normalization ---\n");

    /*
     * Assume pre-computed statistics: mean=50, stddev=35
     * Using shift=8 for Q8 fixed-point output.
     */
    tiku_kits_sigfeatures_zscore_init(&z, 50, 35, 8);

    printf("  mean=50, stddev=35, shift=8 (Q8)\n");
    for (i = 0; i < 6; i++) {
        tiku_kits_sigfeatures_zscore_normalize(
            &z, heartbeat[i], &z_score);
        printf("    x=%3ld  z_q8=%4ld  z_int=%ld\n",
               (long)heartbeat[i],
               (long)z_score,
               (long)(z_score >> 8));
    }
}

/*---------------------------------------------------------------------------*/
/* Example 7: Min-max scaling (ADC to uint8_t range)                         */
/*---------------------------------------------------------------------------*/

static void example_scale(void)
{
    struct tiku_kits_sigfeatures_scale s;
    int32_t scaled;
    uint16_t i;

    printf("--- Min-Max Scaling ---\n");

    /* Map heartbeat range [0, 130] to [0, 255] for uint8_t output */
    tiku_kits_sigfeatures_scale_init(
        &s, 0, 130, TIKU_KITS_SIGFEATURES_SCALE_U8, 16);

    printf("  Input range [0, 130] -> Output [0, 255]\n");
    for (i = 0; i < 10; i++) {
        tiku_kits_sigfeatures_scale_normalize(
            &s, heartbeat[i], &scaled);
        printf("    x=%3ld  scaled=%3ld\n",
               (long)heartbeat[i], (long)scaled);
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_sigfeatures_run(void)
{
    printf("=== TikuKits Signal Features Examples ===\n\n");

    example_delta();
    printf("\n");

    example_peak_detection();
    printf("\n");

    example_zcr();
    printf("\n");

    example_goertzel();
    printf("\n");

    example_histogram();
    printf("\n");

    example_zscore();
    printf("\n");

    example_scale();
    printf("\n");
}
