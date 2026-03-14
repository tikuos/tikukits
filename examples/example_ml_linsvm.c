/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Linear Support Vector Machine
 *
 * Demonstrates binary linear SVM classifier:
 *   1. Temperature threshold  -- 1-D, hot vs cold
 *   2. Motion detection       -- 2-D, accel X + Y
 *   3. Pre-trained deployment -- loading saved weights
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ml/tiku_kits_ml.h"
#include "../ml/classification/tiku_kits_ml_linsvm.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Temperature threshold (hot vs cold)                            */
/*---------------------------------------------------------------------------*/

static void example_temperature_threshold(void)
{
    struct tiku_kits_ml_linsvm svm;
    int8_t cls;
    uint8_t i;

    printf("--- Example 1: Temperature Threshold ---\n");

    /*
     * Train on 1 feature (temperature reading).
     * Label +1 = "hot" (temp > 25), -1 = "cold" (temp < 25).
     * Q8 fixed-point (shift=8).
     */
    tiku_kits_ml_linsvm_init(&svm, 1, 8);
    tiku_kits_ml_linsvm_set_lr(&svm, 26);     /* ~0.1 */
    tiku_kits_ml_linsvm_set_lambda(&svm, 3);   /* ~0.01 */

    /* Train with repeated passes */
    for (i = 0; i < 30; i++) {
        tiku_kits_ml_elem_t xh[] = {30};
        tiku_kits_ml_elem_t xc[] = {15};
        tiku_kits_ml_linsvm_train(&svm, xh, 1);
        tiku_kits_ml_linsvm_train(&svm, xc, -1);
    }

    /* Classify new readings */
    {
        tiku_kits_ml_elem_t x[] = {35};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        printf("  temp=35 -> %s\n", cls == 1 ? "HOT" : "COLD");
    }
    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        printf("  temp=10 -> %s\n", cls == 1 ? "HOT" : "COLD");
    }

    printf("  Trained on %u samples\n\n",
           tiku_kits_ml_linsvm_count(&svm));
}

/*---------------------------------------------------------------------------*/
/* Example 2: Motion detection (2-D accelerometer)                           */
/*---------------------------------------------------------------------------*/

static void example_motion_detection(void)
{
    struct tiku_kits_ml_linsvm svm;
    int8_t cls;
    int32_t margin;
    uint8_t i;

    printf("--- Example 2: Motion Detection ---\n");

    /*
     * Train on 2 features (accel_x, accel_y).
     * +1 = "motion", -1 = "still".
     * Motion samples have large magnitude, still near zero.
     */
    tiku_kits_ml_linsvm_init(&svm, 2, 8);
    tiku_kits_ml_linsvm_set_lr(&svm, 26);
    tiku_kits_ml_linsvm_set_lambda(&svm, 3);

    for (i = 0; i < 25; i++) {
        /* Motion patterns */
        {
            tiku_kits_ml_elem_t x[] = {50, 30};
            tiku_kits_ml_linsvm_train(&svm, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {-40, 45};
            tiku_kits_ml_linsvm_train(&svm, x, 1);
        }
        /* Still patterns */
        {
            tiku_kits_ml_elem_t x[] = {1, -1};
            tiku_kits_ml_linsvm_train(&svm, x, -1);
        }
        {
            tiku_kits_ml_elem_t x[] = {-2, 0};
            tiku_kits_ml_linsvm_train(&svm, x, -1);
        }
    }

    /* Classify new readings with margin */
    {
        tiku_kits_ml_elem_t x[] = {60, 20};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        tiku_kits_ml_linsvm_decision(&svm, x, &margin);
        printf("  accel=(60,20) -> %s (margin=%ld)\n",
               cls == 1 ? "MOTION" : "STILL", (long)margin);
    }
    {
        tiku_kits_ml_elem_t x[] = {0, 1};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        tiku_kits_ml_linsvm_decision(&svm, x, &margin);
        printf("  accel=(0,1)   -> %s (margin=%ld)\n",
               cls == 1 ? "MOTION" : "STILL", (long)margin);
    }

    printf("  Trained on %u samples\n\n",
           tiku_kits_ml_linsvm_count(&svm));
}

/*---------------------------------------------------------------------------*/
/* Example 3: Pre-trained model deployment                                   */
/*---------------------------------------------------------------------------*/

static void example_pretrained_deploy(void)
{
    struct tiku_kits_ml_linsvm svm;
    int8_t cls;
    int32_t w_out[3];

    printf("--- Example 3: Pre-trained Deployment ---\n");

    /*
     * Deploy a model trained offline. The weights were computed
     * on a host machine and baked into the firmware.
     *
     * Decision boundary: f(x) = -2560 + 256*x1 + 128*x2
     *   = -10 + 1.0*x1 + 0.5*x2  (in real units, Q8)
     *
     * Points where x1 + 0.5*x2 > 10 are class +1.
     */
    tiku_kits_ml_linsvm_init(&svm, 2, 8);
    {
        int32_t w[] = {-2560, 256, 128};
        tiku_kits_ml_linsvm_set_weights(&svm, w);
    }

    {
        tiku_kits_ml_elem_t x[] = {15, 5};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        printf("  (15,5) -> class %d\n", cls);
    }
    {
        tiku_kits_ml_elem_t x[] = {5, 2};
        tiku_kits_ml_linsvm_predict(&svm, x, &cls);
        printf("  (5,2)  -> class %d\n", cls);
    }

    /* Read back weights */
    tiku_kits_ml_linsvm_get_weights(&svm, w_out);
    printf("  Weights: bias=%ld, w1=%ld, w2=%ld\n\n",
           (long)w_out[0], (long)w_out[1], (long)w_out[2]);
}

/*---------------------------------------------------------------------------*/
/* ENTRY POINT                                                               */
/*---------------------------------------------------------------------------*/

void example_ml_linsvm_run(void)
{
    printf("=== TikuKits ML: Linear SVM Examples ===\n\n");
    example_temperature_threshold();
    example_motion_detection();
    example_pretrained_deploy();
    printf("=== Done ===\n");
}
