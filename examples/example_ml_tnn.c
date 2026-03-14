/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Tiny Neural Network
 *
 * Demonstrates single-hidden-layer feedforward neural network:
 *   1. Temperature zone classifier -- 1 input, 3 classes
 *   2. Gesture recognition         -- 2 inputs, 2 classes
 *   3. Pre-trained deployment      -- loading saved weights
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ml/tiku_kits_ml.h"
#include "../ml/classification/tiku_kits_ml_tnn.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Temperature zone classifier                                    */
/*---------------------------------------------------------------------------*/

static void example_temperature_zones(void)
{
    struct tiku_kits_ml_tnn net;
    uint8_t cls;
    uint8_t i;

    printf("--- Example 1: Temperature Zones ---\n");

    /*
     * 1 input (temperature), 4 hidden neurons, 3 output classes.
     * Class 0 = cold (< 10), Class 1 = normal (10-30), Class 2 = hot (> 30).
     */
    tiku_kits_ml_tnn_init(&net, 1, 4, 3, 8);
    tiku_kits_ml_tnn_set_lr(&net, 6); /* ~0.025 in Q8 */

    for (i = 0; i < 60; i++) {
        {
            tiku_kits_ml_elem_t x[] = {-5};
            tiku_kits_ml_tnn_train(&net, x, 0);
        }
        {
            tiku_kits_ml_elem_t x[] = {5};
            tiku_kits_ml_tnn_train(&net, x, 0);
        }
        {
            tiku_kits_ml_elem_t x[] = {15};
            tiku_kits_ml_tnn_train(&net, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {25};
            tiku_kits_ml_tnn_train(&net, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {35};
            tiku_kits_ml_tnn_train(&net, x, 2);
        }
        {
            tiku_kits_ml_elem_t x[] = {45};
            tiku_kits_ml_tnn_train(&net, x, 2);
        }
    }

    /* Classify */
    {
        tiku_kits_ml_elem_t x[] = {0};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        printf("  temp=0  -> zone %u\n", cls);
    }
    {
        tiku_kits_ml_elem_t x[] = {20};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        printf("  temp=20 -> zone %u\n", cls);
    }
    {
        tiku_kits_ml_elem_t x[] = {40};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        printf("  temp=40 -> zone %u\n", cls);
    }

    printf("  Trained on %u samples\n\n",
           tiku_kits_ml_tnn_count(&net));
}

/*---------------------------------------------------------------------------*/
/* Example 2: Gesture recognition (2-D accelerometer)                        */
/*---------------------------------------------------------------------------*/

static void example_gesture_recognition(void)
{
    struct tiku_kits_ml_tnn net;
    uint8_t cls;
    uint8_t i;

    printf("--- Example 2: Gesture Recognition ---\n");

    /*
     * 2 inputs (accel_x, accel_y), 4 hidden, 2 classes.
     * Class 0 = "push" (positive accel), Class 1 = "pull" (negative accel).
     */
    tiku_kits_ml_tnn_init(&net, 2, 4, 2, 8);
    tiku_kits_ml_tnn_set_lr(&net, 13); /* ~0.05 in Q8 */

    for (i = 0; i < 40; i++) {
        {
            tiku_kits_ml_elem_t x[] = {20, 15};
            tiku_kits_ml_tnn_train(&net, x, 0);
        }
        {
            tiku_kits_ml_elem_t x[] = {25, 10};
            tiku_kits_ml_tnn_train(&net, x, 0);
        }
        {
            tiku_kits_ml_elem_t x[] = {-20, -15};
            tiku_kits_ml_tnn_train(&net, x, 1);
        }
        {
            tiku_kits_ml_elem_t x[] = {-25, -10};
            tiku_kits_ml_tnn_train(&net, x, 1);
        }
    }

    {
        tiku_kits_ml_elem_t x[] = {30, 20};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        printf("  accel=(30,20)   -> %s\n",
               cls == 0 ? "PUSH" : "PULL");
    }
    {
        tiku_kits_ml_elem_t x[] = {-30, -20};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        printf("  accel=(-30,-20) -> %s\n",
               cls == 0 ? "PUSH" : "PULL");
    }

    printf("  Trained on %u samples\n\n",
           tiku_kits_ml_tnn_count(&net));
}

/*---------------------------------------------------------------------------*/
/* Example 3: Pre-trained model deployment                                   */
/*---------------------------------------------------------------------------*/

static void example_pretrained_deploy(void)
{
    struct tiku_kits_ml_tnn net;
    uint8_t cls;
    int32_t scores[2];

    printf("--- Example 3: Pre-trained Deployment ---\n");

    /*
     * Load a pre-trained 1-input, 2-hidden, 2-output network.
     * Decision: x > 0 -> class 0, x < 0 -> class 1.
     */
    tiku_kits_ml_tnn_init(&net, 1, 2, 2, 8);

    {
        int32_t wh[] = {0, 256, 0, -256};
        tiku_kits_ml_tnn_set_weights(&net, 0, wh);
    }
    {
        int32_t wo[] = {0, 256, -256, 0, -256, 256};
        tiku_kits_ml_tnn_set_weights(&net, 1, wo);
    }

    {
        tiku_kits_ml_elem_t x[] = {10};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        tiku_kits_ml_tnn_forward(&net, x, scores);
        printf("  x=10 -> class %u (scores: %ld, %ld)\n",
               cls, (long)scores[0], (long)scores[1]);
    }
    {
        tiku_kits_ml_elem_t x[] = {-10};
        tiku_kits_ml_tnn_predict(&net, x, &cls);
        tiku_kits_ml_tnn_forward(&net, x, scores);
        printf("  x=-10 -> class %u (scores: %ld, %ld)\n",
               cls, (long)scores[0], (long)scores[1]);
    }

    printf("\n");
}

/*---------------------------------------------------------------------------*/
/* ENTRY POINT                                                               */
/*---------------------------------------------------------------------------*/

void example_ml_tnn_run(void)
{
    printf("=== TikuKits ML: Tiny Neural Network Examples ===\n\n");
    example_temperature_zones();
    example_gesture_recognition();
    example_pretrained_deploy();
    printf("=== Done ===\n");
}
