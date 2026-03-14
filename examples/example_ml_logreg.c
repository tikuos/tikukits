/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Logistic Regression
 *
 * Demonstrates binary logistic regression for common embedded use cases:
 *   1. Threshold classifier  -- classify sensor readings as normal/alarm
 *   2. Online learning       -- adapt a classifier from streaming data
 *   3. Pre-trained inference -- deploy weights trained offline
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ml/tiku_kits_ml.h"
#include "../ml/classification/tiku_kits_ml_logreg.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Threshold classifier (temperature alarm)                       */
/*---------------------------------------------------------------------------*/

static void example_threshold_classifier(void)
{
    struct tiku_kits_ml_logreg lg;
    uint16_t epoch, i;
    int32_t prob_q;
    uint8_t cls;

    /*
     * Train a 1-D classifier to trigger an alarm when temperature
     * exceeds a threshold. Training data (temp in deci-C):
     *   Normal (class 0): 200, 220, 240, 250, 260
     *   Alarm  (class 1): 300, 320, 340, 360, 380
     */
    static const tiku_kits_ml_elem_t x_train[][1] = {
        {200}, {220}, {240}, {250}, {260},
        {300}, {320}, {340}, {360}, {380}
    };
    static const uint8_t y_train[] = {
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 1
    };
    const uint16_t n_samples = 10;

    printf("--- Threshold Classifier (Temperature Alarm) ---\n");

    tiku_kits_ml_logreg_init(&lg, 1, 8);
    tiku_kits_ml_logreg_set_lr(&lg, 3); /* ~0.01 in Q8 */

    /* Train for several epochs */
    for (epoch = 0; epoch < 50; epoch++) {
        for (i = 0; i < n_samples; i++) {
            tiku_kits_ml_logreg_train(&lg, x_train[i], y_train[i]);
        }
    }

    printf("  Trained with %u samples over 50 epochs\n",
           tiku_kits_ml_logreg_count(&lg));

    /* Classify new readings */
    {
        tiku_kits_ml_elem_t x_test[] = {230};
        tiku_kits_ml_logreg_predict_proba(&lg, x_test, &prob_q);
        tiku_kits_ml_logreg_predict(&lg, x_test, &cls);
        printf("  Temp=%ld.%ld C -> P(alarm)=%ld.%02ld  class=%u\n",
               (long)(x_test[0] / 10), (long)(x_test[0] % 10),
               (long)(prob_q * 100 / 256 / 100),
               (long)(prob_q * 100 / 256 % 100),
               cls);
    }
    {
        tiku_kits_ml_elem_t x_test[] = {350};
        tiku_kits_ml_logreg_predict_proba(&lg, x_test, &prob_q);
        tiku_kits_ml_logreg_predict(&lg, x_test, &cls);
        printf("  Temp=%ld.%ld C -> P(alarm)=%ld.%02ld  class=%u\n",
               (long)(x_test[0] / 10), (long)(x_test[0] % 10),
               (long)(prob_q * 100 / 256 / 100),
               (long)(prob_q * 100 / 256 % 100),
               cls);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 2: Pre-trained inference (vibration fault detection)              */
/*---------------------------------------------------------------------------*/

static void example_pretrained_inference(void)
{
    struct tiku_kits_ml_logreg lg;
    int32_t prob_q;
    uint8_t cls;

    /*
     * Deploy a model trained offline for vibration-based fault
     * detection. Features: (rms_accel, peak_freq_hz).
     *
     * Pre-computed weights (Q8):
     *   bias  = -1280  (-5.0: healthy baseline)
     *   w_rms =   512  ( 2.0: higher RMS -> more likely fault)
     *   w_freq = -128  (-0.5: lower freq -> more likely fault)
     */
    int32_t pretrained_w[] = {-1280, 512, -128};

    printf("--- Pre-trained Inference (Fault Detection) ---\n");

    tiku_kits_ml_logreg_init(&lg, 2, 8);
    tiku_kits_ml_logreg_set_weights(&lg, pretrained_w);

    /* Healthy motor: low RMS, high frequency */
    {
        tiku_kits_ml_elem_t x[] = {2, 50};
        tiku_kits_ml_logreg_predict_proba(&lg, x, &prob_q);
        tiku_kits_ml_logreg_predict(&lg, x, &cls);
        printf("  RMS=%ld, Freq=%ld Hz -> P(fault)=%ld/%ld  class=%u\n",
               (long)x[0], (long)x[1],
               (long)prob_q, (long)(1 << 8), cls);
    }

    /* Faulty motor: high RMS, low frequency */
    {
        tiku_kits_ml_elem_t x[] = {8, 10};
        tiku_kits_ml_logreg_predict_proba(&lg, x, &prob_q);
        tiku_kits_ml_logreg_predict(&lg, x, &cls);
        printf("  RMS=%ld, Freq=%ld Hz -> P(fault)=%ld/%ld  class=%u\n",
               (long)x[0], (long)x[1],
               (long)prob_q, (long)(1 << 8), cls);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 3: Online learning (adaptive motion detector)                     */
/*---------------------------------------------------------------------------*/

static void example_online_learning(void)
{
    struct tiku_kits_ml_logreg lg;
    int32_t weights[3];
    uint16_t i;

    /*
     * An adaptive classifier that learns from accelerometer data
     * whether the device is stationary (0) or in motion (1).
     * Features: (accel_x_mag, accel_y_mag)
     *
     * The classifier adapts online as new labeled data arrives.
     */
    static const tiku_kits_ml_elem_t stream_x[][2] = {
        /* stationary samples */
        {1, 0}, {0, 1}, {1, 1}, {2, 0}, {0, 2},
        /* motion samples */
        {10, 8}, {12, 9}, {8, 11}, {15, 7}, {9, 14},
        /* more stationary */
        {1, 2}, {2, 1}, {0, 0}, {1, 0}, {2, 2},
        /* more motion */
        {11, 10}, {13, 8}, {7, 12}, {14, 9}, {10, 13}
    };
    static const uint8_t stream_y[] = {
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 1,
        0, 0, 0, 0, 0,
        1, 1, 1, 1, 1
    };
    const uint16_t n_stream = 20;

    printf("--- Online Learning (Motion Detector) ---\n");

    tiku_kits_ml_logreg_init(&lg, 2, 8);
    tiku_kits_ml_logreg_set_lr(&lg, 13); /* ~0.05 in Q8 */

    for (i = 0; i < n_stream; i++) {
        tiku_kits_ml_logreg_train(&lg, stream_x[i], stream_y[i]);

        if ((i + 1) % 10 == 0) {
            tiku_kits_ml_logreg_get_weights(&lg, weights);
            printf("  After %u samples: bias=%ld, w1=%ld, w2=%ld\n",
                   (unsigned)(i + 1),
                   (long)weights[0], (long)weights[1],
                   (long)weights[2]);
        }
    }

    /* Test on new data */
    {
        uint8_t cls;
        tiku_kits_ml_elem_t x_still[] = {1, 1};
        tiku_kits_ml_elem_t x_move[] = {12, 10};

        tiku_kits_ml_logreg_predict(&lg, x_still, &cls);
        printf("  Stationary test (1,1) -> class=%u\n", cls);

        tiku_kits_ml_logreg_predict(&lg, x_move, &cls);
        printf("  Motion test (12,10)   -> class=%u\n", cls);
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ml_logreg_run(void)
{
    printf("=== TikuKits Logistic Regression Examples ===\n\n");

    example_threshold_classifier();
    printf("\n");

    example_pretrained_inference();
    printf("\n");

    example_online_learning();
    printf("\n");
}
