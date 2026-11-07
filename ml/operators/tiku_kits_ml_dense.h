/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_dense.h - quantized int8 dense inference.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TIKU_KITS_ML_DENSE_H_
#define TIKU_KITS_ML_DENSE_H_
#include <stddef.h>
#include <stdint.h>

/** @brief Dense status codes; errors preserve the output buffer. */
typedef enum {
    TIKU_KITS_ML_DENSE_OK = 0,
    TIKU_KITS_ML_DENSE_ERR_PARAM = -1,
    TIKU_KITS_ML_DENSE_ERR_RANGE = -2,
    TIKU_KITS_ML_DENSE_ERR_OVERLAP = -3,
    TIKU_KITS_ML_DENSE_ERR_SCRATCH = -4
} tiku_kits_ml_dense_status_t;

/** @brief Dense dimensions, symmetric weights and integer quantization metadata. */
typedef struct {
    uint32_t inputs;
    uint32_t outputs;
    uint32_t stride;               /**< weight row stride in bytes */
    const int8_t *weights;         /**< values in -127..127 */
    const int32_t *bias;           /**< one per output, or null for zero */
    const int32_t *multiplier;     /**< nonnegative Q31 multipliers */
    const int8_t *shift;           /**< exponents in -31..30 */
    uint32_t quant_count;          /**< 1, or outputs for per-channel scaling */
    int32_t input_zero_point;      /**< -128..127 */
    int32_t output_zero_point;     /**< -128..127 */
    int32_t activation_min;        /**< -128..127 */
    int32_t activation_max;        /**< activation_min..127 */
} tiku_kits_ml_dense_t;

/** @brief Return output staging bytes, or zero for null/unaligned metadata. */
size_t tiku_kits_ml_dense_scratch_size(const tiku_kits_ml_dense_t *model);

/**
 * @brief Evaluate one dense layer through shared compute and double rounding.
 * @note Scratch holds outputs bytes, aligned to one byte, and is disjoint from
 *       metadata, inputs and output. Output is disjoint from readable operands.
 * @note Complete and left-shifted accumulators must fit int32 before scaling.
 * @return Status; scratch may change on error, output does not.
 */
tiku_kits_ml_dense_status_t tiku_kits_ml_dense_s8(
    const tiku_kits_ml_dense_t *model, const int8_t *input, int8_t *output,
    void *scratch, size_t scratch_bytes);
#endif
