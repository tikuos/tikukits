/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: k-Nearest Neighbors Classifier
 *
 * Demonstrates k-NN classification for common embedded use cases:
 *   1. Temperature zone classifier -- 1-D sensor with 3 zones
 *   2. Gesture recognition         -- 2-D accelerometer features
 *   3. Dynamic k tuning            -- adapting k at runtime
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ml/tiku_kits_ml.h"
#include "../ml/classification/tiku_kits_ml_knn.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Temperature zone classifier (cold / normal / hot)              */
/*---------------------------------------------------------------------------*/

static void example_temperature_zones(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;

    /*
     * Classify temperature readings (deci-C) into zones:
     *   cold   (class 0): around 50-150 (5.0-15.0 C)
     *   normal (class 1): around 200-280 (20.0-28.0 C)
     *   hot    (class 2): around 350-400 (35.0-40.0 C)
     *
     * We load "training" samples from known calibration readings.
     */
    static const struct {
        tiku_kits_ml_elem_t temp;
        uint8_t zone;
    } calibration[] = {
        { 50, 0}, { 80, 0}, {100, 0}, {120, 0}, {150, 0},
        {200, 1}, {220, 1}, {240, 1}, {260, 1}, {280, 1},
        {350, 2}, {370, 2}, {380, 2}, {390, 2}, {400, 2},
    };
    static const char *zone_names[] = {"COLD", "NORMAL", "HOT"};
    uint8_t i;

    printf("--- Temperature Zone Classifier (k-NN) ---\n");

    tiku_kits_ml_knn_init(&knn, 1, 3);

    /* Load calibration data */
    for (i = 0; i < 15; i++) {
        tiku_kits_ml_elem_t x[] = {calibration[i].temp};
        tiku_kits_ml_knn_add(&knn, x, calibration[i].zone);
    }

    printf("  Loaded %u calibration samples, k=%u\n",
           tiku_kits_ml_knn_count(&knn),
           tiku_kits_ml_knn_get_k(&knn));

    /* Classify new readings */
    {
        tiku_kits_ml_elem_t queries[] = {60, 130, 230, 300, 380};
        for (i = 0; i < 5; i++) {
            tiku_kits_ml_elem_t x[] = {queries[i]};
            tiku_kits_ml_knn_predict(&knn, x, &cls);
            printf("  Temp=%ld.%ld C -> %s\n",
                   (long)(queries[i] / 10),
                   (long)(queries[i] % 10),
                   zone_names[cls]);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Example 2: Gesture recognition (2-D accelerometer features)               */
/*---------------------------------------------------------------------------*/

static void example_gesture_recognition(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;

    /*
     * Features: (peak_accel, zero_crossings)
     *
     * Gestures:
     *   tap    (class 0): high peak, low ZCR
     *   shake  (class 1): high peak, high ZCR
     *   tilt   (class 2): low peak, low ZCR
     */
    static const char *gesture_names[] = {"TAP", "SHAKE", "TILT"};

    printf("--- Gesture Recognition (k-NN, 2-D) ---\n");

    tiku_kits_ml_knn_init(&knn, 2, 3);

    /* Tap training data: high peak, low ZCR */
    {
        tiku_kits_ml_elem_t x[] = {80, 2};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {90, 3};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {85, 1};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }

    /* Shake training data: high peak, high ZCR */
    {
        tiku_kits_ml_elem_t x[] = {70, 20};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {75, 25};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }
    {
        tiku_kits_ml_elem_t x[] = {80, 22};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }

    /* Tilt training data: low peak, low ZCR */
    {
        tiku_kits_ml_elem_t x[] = {10, 1};
        tiku_kits_ml_knn_add(&knn, x, 2);
    }
    {
        tiku_kits_ml_elem_t x[] = {15, 2};
        tiku_kits_ml_knn_add(&knn, x, 2);
    }
    {
        tiku_kits_ml_elem_t x[] = {12, 0};
        tiku_kits_ml_knn_add(&knn, x, 2);
    }

    printf("  Loaded %u training samples\n",
           tiku_kits_ml_knn_count(&knn));

    /* Classify */
    {
        tiku_kits_ml_elem_t q[] = {88, 2};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        printf("  Peak=%ld, ZCR=%ld -> %s\n",
               (long)q[0], (long)q[1], gesture_names[cls]);
    }
    {
        tiku_kits_ml_elem_t q[] = {72, 23};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        printf("  Peak=%ld, ZCR=%ld -> %s\n",
               (long)q[0], (long)q[1], gesture_names[cls]);
    }
    {
        tiku_kits_ml_elem_t q[] = {11, 1};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        printf("  Peak=%ld, ZCR=%ld -> %s\n",
               (long)q[0], (long)q[1], gesture_names[cls]);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 3: Dynamic k tuning                                               */
/*---------------------------------------------------------------------------*/

static void example_dynamic_k(void)
{
    struct tiku_kits_ml_knn knn;
    uint8_t cls;

    printf("--- Dynamic k Tuning ---\n");

    tiku_kits_ml_knn_init(&knn, 1, 1);

    /* 3 class-0 samples far from query, 1 class-1 right at query */
    {
        tiku_kits_ml_elem_t x[] = {0};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {2};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {4};
        tiku_kits_ml_knn_add(&knn, x, 0);
    }
    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_knn_add(&knn, x, 1);
    }

    /* k=1: nearest neighbor wins */
    {
        tiku_kits_ml_elem_t q[] = {10};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        printf("  k=1, x=10 -> class %u (nearest)\n", cls);
    }

    /* k=3: majority vote */
    tiku_kits_ml_knn_set_k(&knn, 3);
    {
        tiku_kits_ml_elem_t q[] = {10};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        printf("  k=3, x=10 -> class %u (majority of 3 nearest)\n",
               cls);
    }

    /* k=4: all samples vote (class 0 has 3 votes) */
    tiku_kits_ml_knn_set_k(&knn, 4);
    {
        tiku_kits_ml_elem_t q[] = {10};
        tiku_kits_ml_knn_predict(&knn, q, &cls);
        printf("  k=4, x=10 -> class %u (all 4 vote)\n", cls);
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ml_knn_run(void)
{
    printf("=== TikuKits k-NN Examples ===\n\n");

    example_temperature_zones();
    printf("\n");

    example_gesture_recognition();
    printf("\n");

    example_dynamic_k();
    printf("\n");
}
