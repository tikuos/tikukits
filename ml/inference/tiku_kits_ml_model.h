/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_model.h - static mixed-operator quantized model execution.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TIKU_KITS_ML_MODEL_H_
#define TIKU_KITS_ML_MODEL_H_
#include "../operators/tiku_kits_ml_cnn.h"

/** @brief Sequential operators supported by the int8 model executor. */
typedef enum {
    TIKU_KITS_ML_LAYER_DENSE = 0,
    TIKU_KITS_ML_LAYER_CONV2D = 1,
    TIKU_KITS_ML_LAYER_POOL2D = 2,
    TIKU_KITS_ML_LAYER_RESHAPE = 3
} tiku_kits_ml_layer_kind_t;

/** @brief Immutable operator metadata; dense consumes its input in flat order. */
typedef struct {
    tiku_kits_ml_layer_kind_t kind;
    union {
        tiku_kits_ml_dense_t dense;
        tiku_kits_ml_conv2d_t conv;
        tiku_kits_ml_pool2d_t pool;
        tiku_kits_ml_shape_t reshape;
    } op;
} tiku_kits_ml_layer_t;

/** @brief One input and one output with an ordered array of immutable operators. */
typedef struct {
    tiku_kits_ml_shape_t input;
    int32_t input_zero_point;
    const tiku_kits_ml_layer_t *layers;
    uint32_t count;
} tiku_kits_ml_model_t;

/** @brief Return byte-aligned intermediate and operator workspace; zero is invalid. */
size_t tiku_kits_ml_model_workspace_size(const tiku_kits_ml_model_t *model);
/** @brief Return output shape, or zero fields for invalid model geometry. */
tiku_kits_ml_shape_t tiku_kits_ml_model_output_shape(const tiku_kits_ml_model_t *model);
/**
 * @brief Execute dense, convolution, pooling and reshape layers without allocation.
 * @note Model, parameter and input spans must remain immutable during the call.
 * @note Workspace/output are disjoint from each other and every readable span.
 * @return Dense status; failures preserve output, but workspace may change.
 */
tiku_kits_ml_dense_status_t tiku_kits_ml_model_s8(const tiku_kits_ml_model_t *model,
    const int8_t *input, size_t input_bytes, int8_t *output, size_t output_capacity,
    void *workspace, size_t workspace_bytes);
#endif
