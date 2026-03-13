/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Distance & Similarity Metrics
 *
 * Demonstrates distance functions for nearest-neighbor classification
 * on embedded targets:
 *   - Manhattan (L1) distance
 *   - Squared Euclidean distance
 *   - Dot product
 *   - Cosine similarity components
 *   - Hamming distance (bitwise)
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>

#include "../maths/tiku_kits_maths.h"
#include "../maths/distance/tiku_kits_distance.h"

/*---------------------------------------------------------------------------*/
/* Example 1: Manhattan (L1) distance -- cheapest metric                     */
/*---------------------------------------------------------------------------*/

static void example_manhattan(void)
{
    tiku_kits_distance_elem_t a[] = {10, 20, 30};
    tiku_kits_distance_elem_t b[] = {13, 17, 35};
    int64_t dist;

    printf("--- Manhattan (L1) Distance ---\n");

    tiku_kits_distance_manhattan(a, b, 3, &dist);
    printf("  a = [10, 20, 30]\n");
    printf("  b = [13, 17, 35]\n");
    printf("  L1(a, b) = %lld\n", (long long)dist);
    /* |10-13| + |20-17| + |30-35| = 3 + 3 + 5 = 11 */
}

/*---------------------------------------------------------------------------*/
/* Example 2: Squared Euclidean distance                                     */
/*---------------------------------------------------------------------------*/

static void example_euclidean_sq(void)
{
    tiku_kits_distance_elem_t a[] = {1, 2, 3};
    tiku_kits_distance_elem_t b[] = {4, 0, 5};
    int64_t dist;

    printf("--- Squared Euclidean Distance ---\n");

    tiku_kits_distance_euclidean_sq(a, b, 3, &dist);
    printf("  a = [1, 2, 3]\n");
    printf("  b = [4, 0, 5]\n");
    printf("  ||a-b||^2 = %lld\n", (long long)dist);
    /* (1-4)^2 + (2-0)^2 + (3-5)^2 = 9 + 4 + 4 = 17 */
}

/*---------------------------------------------------------------------------*/
/* Example 3: Nearest-neighbor classifier using squared Euclidean            */
/*---------------------------------------------------------------------------*/

static void example_nearest_neighbor(void)
{
    /* Three class prototypes (e.g. gesture templates) */
    tiku_kits_distance_elem_t class_a[] = {100, 200, 300};
    tiku_kits_distance_elem_t class_b[] = {400, 500, 600};
    tiku_kits_distance_elem_t class_c[] = {150, 250, 350};

    /* Unknown sample to classify */
    tiku_kits_distance_elem_t query[] = {140, 230, 320};

    int64_t dist_a, dist_b, dist_c;

    printf("--- Nearest-Neighbor Classifier ---\n");

    tiku_kits_distance_euclidean_sq(query, class_a, 3, &dist_a);
    tiku_kits_distance_euclidean_sq(query, class_b, 3, &dist_b);
    tiku_kits_distance_euclidean_sq(query, class_c, 3, &dist_c);

    printf("  Distance to class A: %lld\n", (long long)dist_a);
    printf("  Distance to class B: %lld\n", (long long)dist_b);
    printf("  Distance to class C: %lld\n", (long long)dist_c);

    /* Pick the closest class */
    if (dist_a <= dist_b && dist_a <= dist_c) {
        printf("  Classified as: A\n");
    } else if (dist_b <= dist_c) {
        printf("  Classified as: B\n");
    } else {
        printf("  Classified as: C\n");
    }
}

/*---------------------------------------------------------------------------*/
/* Example 4: Dot product                                                    */
/*---------------------------------------------------------------------------*/

static void example_dot_product(void)
{
    tiku_kits_distance_elem_t a[] = {1, 3, -5};
    tiku_kits_distance_elem_t b[] = {4, -2, -1};
    int64_t dot;

    printf("--- Dot Product ---\n");

    tiku_kits_distance_dot(a, b, 3, &dot);
    printf("  a = [1, 3, -5]\n");
    printf("  b = [4, -2, -1]\n");
    printf("  dot(a, b) = %lld\n", (long long)dot);
    /* 1*4 + 3*(-2) + (-5)*(-1) = 4 - 6 + 5 = 3 */
}

/*---------------------------------------------------------------------------*/
/* Example 5: Cosine similarity (unnormalized vectors)                       */
/*---------------------------------------------------------------------------*/

static void example_cosine_similarity(void)
{
    tiku_kits_distance_elem_t a[] = {1, 2, 3};
    tiku_kits_distance_elem_t b[] = {2, 4, 6};  /* same direction as a */
    tiku_kits_distance_elem_t c[] = {3, -1, 0}; /* different direction */
    int64_t dot_ab, dot_aa, dot_bb;

    printf("--- Cosine Similarity ---\n");

    /* a vs b (parallel vectors: cos=1) */
    tiku_kits_distance_cosine_sq(a, b, 3, &dot_ab, &dot_aa, &dot_bb);
    printf("  a=[1,2,3] vs b=[2,4,6]:\n");
    printf("    dot(a,b)=%lld  |a|^2=%lld  |b|^2=%lld\n",
           (long long)dot_ab, (long long)dot_aa, (long long)dot_bb);
    printf("    cos^2 = %lld^2 / (%lld * %lld) = %lld / %lld\n",
           (long long)dot_ab, (long long)dot_aa, (long long)dot_bb,
           (long long)(dot_ab * dot_ab),
           (long long)(dot_aa * dot_bb));

    /* a vs c */
    tiku_kits_distance_cosine_sq(a, c, 3, &dot_ab, &dot_aa, &dot_bb);
    printf("  a=[1,2,3] vs c=[3,-1,0]:\n");
    printf("    dot(a,c)=%lld  |a|^2=%lld  |c|^2=%lld\n",
           (long long)dot_ab, (long long)dot_aa, (long long)dot_bb);
}

/*---------------------------------------------------------------------------*/
/* Example 6: Hamming distance (binary features)                             */
/*---------------------------------------------------------------------------*/

static void example_hamming(void)
{
    uint8_t a[] = {0xFF, 0x00, 0xAA};
    uint8_t b[] = {0x0F, 0x00, 0x55};
    uint32_t dist;

    printf("--- Hamming Distance ---\n");

    tiku_kits_distance_hamming(a, b, 3, &dist);
    printf("  a = [0xFF, 0x00, 0xAA]\n");
    printf("  b = [0x0F, 0x00, 0x55]\n");
    printf("  Hamming distance: %lu bits\n", (unsigned long)dist);
    /* XOR: [0xF0, 0x00, 0xFF] -> popcount = 4 + 0 + 8 = 12 */
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_distance_run(void)
{
    printf("=== TikuKits Distance Examples ===\n\n");

    example_manhattan();
    printf("\n");

    example_euclidean_sq();
    printf("\n");

    example_nearest_neighbor();
    printf("\n");

    example_dot_product();
    printf("\n");

    example_cosine_similarity();
    printf("\n");

    example_hamming();
    printf("\n");
}
