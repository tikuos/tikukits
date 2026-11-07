/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_cnn.h - quantized NHWC convolution and pooling contracts.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TIKU_KITS_ML_CNN_H_
#define TIKU_KITS_ML_CNN_H_
#include "tiku_kits_ml_dense.h"

/** @brief One image, with contiguous channels inside width inside height. */
typedef struct { uint32_t height, width, channels; } tiku_kits_ml_shape_t;

/** @brief Spatial window; dimensions, strides, dilation and pads are at most 65535. */
typedef struct {
    uint32_t height, width, stride_h, stride_w, dilation_h, dilation_w;
    uint32_t pad_top, pad_bottom, pad_left, pad_right;
} tiku_kits_ml_window_t;

/** @brief OHWI filters use dense rows of kernel height times width times channels. */
typedef struct {
    tiku_kits_ml_shape_t input;
    tiku_kits_ml_window_t window;
    tiku_kits_ml_dense_t rows;
} tiku_kits_ml_conv2d_t;

/** @brief Max or average over valid input cells; padding is excluded. */
typedef enum { TIKU_KITS_ML_POOL_MAX = 0, TIKU_KITS_ML_POOL_AVERAGE = 1 } tiku_kits_ml_pool_kind_t;

/** @brief Pooling preserves scale and zero point; averages round ties away from zero. */
typedef struct {
    tiku_kits_ml_shape_t input;
    tiku_kits_ml_window_t window;
    tiku_kits_ml_pool_kind_t kind;
    int32_t zero_point, activation_min, activation_max;
} tiku_kits_ml_pool2d_t;

/** @brief Return image bytes, or zero for zero dimensions, dimensions over 65535 or overflow. */
size_t tiku_kits_ml_shape_size(tiku_kits_ml_shape_t shape);
/** @brief Return convolution output shape; zero fields denote invalid geometry. */
tiku_kits_ml_shape_t tiku_kits_ml_conv2d_shape(const tiku_kits_ml_conv2d_t *model);
/** @brief Return pooling output shape; zero fields denote invalid geometry. */
tiku_kits_ml_shape_t tiku_kits_ml_pool2d_shape(const tiku_kits_ml_pool2d_t *model);
/** @brief Return byte-aligned staging, patch and dense scratch bytes; zero is invalid. */
size_t tiku_kits_ml_conv2d_scratch_size(const tiku_kits_ml_conv2d_t *model);
/** @brief Return byte-aligned output staging bytes; zero is invalid. */
size_t tiku_kits_ml_pool2d_scratch_size(const tiku_kits_ml_pool2d_t *model);
/**
 * @brief Convolve one image; pad samples equal the input zero point.
 * @note Input, model and parameter spans are disjoint from output and scratch.
 * @return Dense status; errors preserve output, but scratch may change.
 */
tiku_kits_ml_dense_status_t tiku_kits_ml_conv2d_s8(const tiku_kits_ml_conv2d_t *model,
    const int8_t *input, size_t input_bytes, int8_t *output, size_t output_capacity,
    void *scratch, size_t scratch_bytes);
/**
 * @brief Pool one image; an output window with no input samples is invalid.
 * @note Model and input are disjoint from output and scratch; writes cannot overlap.
 * @return Dense status; errors preserve output, but scratch may change.
 */
tiku_kits_ml_dense_status_t tiku_kits_ml_pool2d_s8(const tiku_kits_ml_pool2d_t *model,
    const int8_t *input, size_t input_bytes, int8_t *output, size_t output_capacity,
    void *scratch, size_t scratch_bytes);
#endif
