/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Categorical Naive Bayes Classifier
 *
 * Demonstrates Naive Bayes on binned sensor data:
 *   1. Temperature zone classifier  -- 1 feature, 3 zones
 *   2. Activity recognition          -- 2 features (accel + gyro bins)
 *   3. Log-likelihood inspection     -- viewing raw scores
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ml/tiku_kits_ml.h"
#include "../ml/classification/tiku_kits_ml_nbayes.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Temperature zone classifier (binned sensor reading)            */
/*---------------------------------------------------------------------------*/

static void example_temperature_zones(void)
{
    struct tiku_kits_ml_nbayes nb;
    uint8_t cls;

    /*
     * Bin a temperature sensor (0-50 C) into 5 bins:
     *   bin 0: 0-9 C, bin 1: 10-19 C, bin 2: 20-29 C,
     *   bin 3: 30-39 C, bin 4: 40-50 C
     *
     * Classes:
     *   0 = cold  (bins 0-1)
     *   1 = normal (bins 2-3)
     *   2 = hot   (bin 4)
     */
    static const char *zone_names[] = {"COLD", "NORMAL", "HOT"};

    printf("--- Temperature Zone (Naive Bayes) ---\n");

    tiku_kits_ml_nbayes_init(&nb, 1, 5, 3, 8);

    /* Cold readings: bins 0 and 1 */
    {
        uint8_t x0[] = {0};
        uint8_t x1[] = {1};
        uint8_t i;
        for (i = 0; i < 5; i++) {
            tiku_kits_ml_nbayes_train(&nb, x0, 0);
        }
        for (i = 0; i < 3; i++) {
            tiku_kits_ml_nbayes_train(&nb, x1, 0);
        }
    }

    /* Normal readings: bins 2 and 3 */
    {
        uint8_t x2[] = {2};
        uint8_t x3[] = {3};
        uint8_t i;
        for (i = 0; i < 6; i++) {
            tiku_kits_ml_nbayes_train(&nb, x2, 1);
        }
        for (i = 0; i < 4; i++) {
            tiku_kits_ml_nbayes_train(&nb, x3, 1);
        }
    }

    /* Hot readings: bin 4 */
    {
        uint8_t x4[] = {4};
        uint8_t i;
        for (i = 0; i < 5; i++) {
            tiku_kits_ml_nbayes_train(&nb, x4, 2);
        }
    }

    printf("  Trained %u samples (%u cold, %u normal, %u hot)\n",
           tiku_kits_ml_nbayes_count(&nb),
           tiku_kits_ml_nbayes_class_count(&nb, 0),
           tiku_kits_ml_nbayes_class_count(&nb, 1),
           tiku_kits_ml_nbayes_class_count(&nb, 2));

    /* Classify new readings */
    {
        uint8_t queries[] = {0, 1, 2, 3, 4};
        uint8_t i;
        for (i = 0; i < 5; i++) {
            uint8_t q[] = {queries[i]};
            tiku_kits_ml_nbayes_predict(&nb, q, &cls);
            printf("  Bin %u -> %s\n", queries[i], zone_names[cls]);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Example 2: Activity recognition (2 binned features)                       */
/*---------------------------------------------------------------------------*/

static void example_activity_recognition(void)
{
    struct tiku_kits_ml_nbayes nb;
    uint8_t cls;

    /*
     * Features (binned from raw sensors):
     *   feature 0: accelerometer energy (4 bins: very_low, low, med, high)
     *   feature 1: gyroscope energy (4 bins: very_low, low, med, high)
     *
     * Activities:
     *   0 = sitting  (low accel, low gyro)
     *   1 = walking  (med accel, med gyro)
     *   2 = running  (high accel, high gyro)
     */
    static const char *activities[] = {"SITTING", "WALKING", "RUNNING"};

    printf("--- Activity Recognition (Naive Bayes, 2-D) ---\n");

    tiku_kits_ml_nbayes_init(&nb, 2, 4, 3, 8);

    /* Sitting: low accel (0-1), low gyro (0-1) */
    {
        uint8_t x[] = {0, 0};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    {
        uint8_t x[] = {1, 0};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }
    {
        uint8_t x[] = {0, 1};
        tiku_kits_ml_nbayes_train(&nb, x, 0);
    }

    /* Walking: medium accel (1-2), medium gyro (1-2) */
    {
        uint8_t x[] = {1, 1};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t x[] = {2, 1};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }
    {
        uint8_t x[] = {2, 2};
        tiku_kits_ml_nbayes_train(&nb, x, 1);
        tiku_kits_ml_nbayes_train(&nb, x, 1);
    }

    /* Running: high accel (2-3), high gyro (2-3) */
    {
        uint8_t x[] = {3, 3};
        tiku_kits_ml_nbayes_train(&nb, x, 2);
        tiku_kits_ml_nbayes_train(&nb, x, 2);
    }
    {
        uint8_t x[] = {3, 2};
        tiku_kits_ml_nbayes_train(&nb, x, 2);
    }
    {
        uint8_t x[] = {2, 3};
        tiku_kits_ml_nbayes_train(&nb, x, 2);
    }

    printf("  Trained %u samples\n",
           tiku_kits_ml_nbayes_count(&nb));

    /* Classify */
    {
        uint8_t q1[] = {0, 0};
        tiku_kits_ml_nbayes_predict(&nb, q1, &cls);
        printf("  Accel=verylow, Gyro=verylow -> %s\n",
               activities[cls]);
    }
    {
        uint8_t q2[] = {2, 2};
        tiku_kits_ml_nbayes_predict(&nb, q2, &cls);
        printf("  Accel=med, Gyro=med -> %s\n",
               activities[cls]);
    }
    {
        uint8_t q3[] = {3, 3};
        tiku_kits_ml_nbayes_predict(&nb, q3, &cls);
        printf("  Accel=high, Gyro=high -> %s\n",
               activities[cls]);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 3: Log-likelihood score inspection                                */
/*---------------------------------------------------------------------------*/

static void example_score_inspection(void)
{
    struct tiku_kits_ml_nbayes nb;
    int32_t scores[TIKU_KITS_ML_NBAYES_MAX_CLASSES];
    uint8_t cls;

    printf("--- Log-Likelihood Inspection ---\n");

    tiku_kits_ml_nbayes_init(&nb, 1, 4, 2, 8);

    /* Class 0: heavily on bin 0 */
    {
        uint8_t x[] = {0};
        uint8_t i;
        for (i = 0; i < 10; i++) {
            tiku_kits_ml_nbayes_train(&nb, x, 0);
        }
    }

    /* Class 1: heavily on bin 3 */
    {
        uint8_t x[] = {3};
        uint8_t i;
        for (i = 0; i < 10; i++) {
            tiku_kits_ml_nbayes_train(&nb, x, 1);
        }
    }

    /* Inspect scores for bin 0 */
    {
        uint8_t q[] = {0};
        tiku_kits_ml_nbayes_predict_log_proba(&nb, q, scores);
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        printf("  Bin 0: score[0]=%ld, score[1]=%ld -> class %u\n",
               (long)scores[0], (long)scores[1], cls);
    }

    /* Inspect scores for bin 3 */
    {
        uint8_t q[] = {3};
        tiku_kits_ml_nbayes_predict_log_proba(&nb, q, scores);
        tiku_kits_ml_nbayes_predict(&nb, q, &cls);
        printf("  Bin 3: score[0]=%ld, score[1]=%ld -> class %u\n",
               (long)scores[0], (long)scores[1], cls);
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ml_nbayes_run(void)
{
    printf("=== TikuKits Naive Bayes Examples ===\n\n");

    example_temperature_zones();
    printf("\n");

    example_activity_recognition();
    printf("\n");

    example_score_inspection();
    printf("\n");
}
