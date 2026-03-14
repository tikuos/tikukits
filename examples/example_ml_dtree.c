/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Decision Tree Classifier
 *
 * Demonstrates pre-built decision trees for common embedded use cases:
 *   1. Temperature zone classifier -- 3-class from a single sensor
 *   2. Vibration fault detection   -- multi-feature tree
 *   3. Confidence-aware triage     -- predict_proba for risk decisions
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../ml/tiku_kits_ml.h"
#include "../ml/classification/tiku_kits_ml_dtree.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Temperature zone classifier (cold / normal / hot)              */
/*---------------------------------------------------------------------------*/

static void example_temperature_zones(void)
{
    struct tiku_kits_ml_dtree dt;
    uint8_t cls;

    /*
     * Classify temperature readings (deci-C) into zones:
     *   cold   (class 0): temp <= 150  (15.0 C)
     *   normal (class 1): 150 < temp <= 300  (30.0 C)
     *   hot    (class 2): temp > 300
     *
     *         [0] temp<=150
     *        /            \
     *   [1] c=0      [2] temp<=300
     *                /            \
     *           [3] c=1        [4] c=2
     */
    struct tiku_kits_ml_dtree_node nodes[] = {
        {150, 0, 1, 2, 0},               /* root */
        {0,   0, 0xFF, 0xFF, 0},         /* cold */
        {300, 0, 3, 4, 0},               /* second split */
        {0,   0, 0xFF, 0xFF, 1},         /* normal */
        {0,   0, 0xFF, 0xFF, 2},         /* hot */
    };
    static const char *zone_names[] = {"COLD", "NORMAL", "HOT"};

    printf("--- Temperature Zone Classifier ---\n");

    tiku_kits_ml_dtree_init(&dt, 1, 8);
    tiku_kits_ml_dtree_set_tree(&dt, nodes, 5);

    printf("  Tree: %u nodes, depth %u\n",
           tiku_kits_ml_dtree_node_count(&dt),
           tiku_kits_ml_dtree_depth(&dt));

    /* Classify readings */
    {
        tiku_kits_ml_elem_t temps[] = {50, 150, 220, 300, 350};
        uint8_t i;
        for (i = 0; i < 5; i++) {
            tiku_kits_ml_elem_t x[] = {temps[i]};
            tiku_kits_ml_dtree_predict(&dt, x, &cls);
            printf("  Temp=%ld.%ld C -> %s\n",
                   (long)(temps[i] / 10), (long)(temps[i] % 10),
                   zone_names[cls]);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Example 2: Vibration fault detection (2-feature tree)                     */
/*---------------------------------------------------------------------------*/

static void example_vibration_fault(void)
{
    struct tiku_kits_ml_dtree dt;
    uint8_t cls;

    /*
     * Features: (rms_accel, peak_freq_hz)
     *
     * Trained offline to classify:
     *   healthy (0): low RMS or high frequency
     *   warning (1): moderate RMS with low frequency
     *   fault   (2): high RMS with low frequency
     *
     *            [0] rms<=4
     *           /          \
     *      [1] c=0     [2] freq<=30
     *                 /            \
     *          [3] rms<=7      [4] c=0
     *          /        \
     *     [5] c=1    [6] c=2
     */
    struct tiku_kits_ml_dtree_node nodes[] = {
        {4,  0, 1, 2, 0},               /* rms <= 4 */
        {0,  0, 0xFF, 0xFF, 0},         /* healthy */
        {30, 1, 3, 4, 0},               /* freq <= 30 */
        {7,  0, 5, 6, 0},               /* rms <= 7 */
        {0,  0, 0xFF, 0xFF, 0},         /* healthy */
        {0,  0, 0xFF, 0xFF, 1},         /* warning */
        {0,  0, 0xFF, 0xFF, 2},         /* fault */
    };
    static const char *status[] = {"HEALTHY", "WARNING", "FAULT"};

    printf("--- Vibration Fault Detection ---\n");

    tiku_kits_ml_dtree_init(&dt, 2, 8);
    tiku_kits_ml_dtree_set_tree(&dt, nodes, 7);

    printf("  Tree: %u nodes, depth %u\n",
           tiku_kits_ml_dtree_node_count(&dt),
           tiku_kits_ml_dtree_depth(&dt));

    /* Test cases */
    {
        tiku_kits_ml_elem_t x1[] = {2, 50};
        tiku_kits_ml_dtree_predict(&dt, x1, &cls);
        printf("  RMS=%ld, Freq=%ld -> %s\n",
               (long)x1[0], (long)x1[1], status[cls]);
    }
    {
        tiku_kits_ml_elem_t x2[] = {6, 20};
        tiku_kits_ml_dtree_predict(&dt, x2, &cls);
        printf("  RMS=%ld, Freq=%ld -> %s\n",
               (long)x2[0], (long)x2[1], status[cls]);
    }
    {
        tiku_kits_ml_elem_t x3[] = {9, 15};
        tiku_kits_ml_dtree_predict(&dt, x3, &cls);
        printf("  RMS=%ld, Freq=%ld -> %s\n",
               (long)x3[0], (long)x3[1], status[cls]);
    }
    {
        tiku_kits_ml_elem_t x4[] = {8, 40};
        tiku_kits_ml_dtree_predict(&dt, x4, &cls);
        printf("  RMS=%ld, Freq=%ld -> %s\n",
               (long)x4[0], (long)x4[1], status[cls]);
    }
}

/*---------------------------------------------------------------------------*/
/* Example 3: Confidence-aware sensor triage                                 */
/*---------------------------------------------------------------------------*/

static void example_confidence_triage(void)
{
    struct tiku_kits_ml_dtree dt;
    uint8_t cls;
    int32_t conf;

    /*
     * Same temperature tree but with confidence values in leaves.
     * Confidence encodes how pure each leaf region is in training data.
     *
     *         [0] temp<=150
     *        /            \
     *   [1] c=0 (95%)  [2] temp<=300
     *                  /            \
     *          [3] c=1 (85%)    [4] c=2 (90%)
     *
     * Q8 values: 95% = 243, 85% = 218, 90% = 230
     */
    struct tiku_kits_ml_dtree_node nodes[] = {
        {150, 0, 1, 2, 0},
        {243, 0, 0xFF, 0xFF, 0},         /* cold, 95% */
        {300, 0, 3, 4, 0},
        {218, 0, 0xFF, 0xFF, 1},         /* normal, 85% */
        {230, 0, 0xFF, 0xFF, 2},         /* hot, 90% */
    };
    static const char *zone_names[] = {"COLD", "NORMAL", "HOT"};

    printf("--- Confidence-Aware Triage ---\n");

    tiku_kits_ml_dtree_init(&dt, 1, 8);
    tiku_kits_ml_dtree_set_tree(&dt, nodes, 5);

    /* Classify with confidence */
    {
        tiku_kits_ml_elem_t temps[] = {100, 250, 350};
        uint8_t i;
        for (i = 0; i < 3; i++) {
            tiku_kits_ml_elem_t x[] = {temps[i]};
            tiku_kits_ml_dtree_predict(&dt, x, &cls);
            tiku_kits_ml_dtree_predict_proba(&dt, x, &conf);
            printf("  Temp=%ld.%ld C -> %s (conf=%ld/%ld = %ld%%)\n",
                   (long)(temps[i] / 10), (long)(temps[i] % 10),
                   zone_names[cls],
                   (long)conf, (long)(1 << 8),
                   (long)(conf * 100 / 256));
        }
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_ml_dtree_run(void)
{
    printf("=== TikuKits Decision Tree Examples ===\n\n");

    example_temperature_zones();
    printf("\n");

    example_vibration_fault();
    printf("\n");

    example_confidence_triage();
    printf("\n");
}
