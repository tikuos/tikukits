/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_cnn.c - staged convolution and pooling over NHWC int8 images.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tiku_kits_ml_cnn.h"
#include "tiku_kits_ml_tensor_internal.h"
#include <string.h>

size_t tiku_kits_ml_shape_size(tiku_kits_ml_shape_t s)
{
    uint64_t n;
    if (!s.height || !s.width || !s.channels || s.height > 65535 ||
        s.width > 65535 || s.channels > 65535) { return 0; }
    n = (uint64_t)s.height * s.width * s.channels;
    return n > SIZE_MAX || n > UINT32_MAX ? 0 : (size_t)n;
}

/** @brief Compute output geometry with bounded positive window fields. */
static tiku_kits_ml_shape_t window_shape(tiku_kits_ml_shape_t in,
    const tiku_kits_ml_window_t *w, uint32_t channels)
{
    tiku_kits_ml_shape_t none = {0,0,0}, result;
    uint32_t fields[] = {w->height,w->width,w->stride_h,w->stride_w,w->dilation_h,
        w->dilation_w,w->pad_top,w->pad_bottom,w->pad_left,w->pad_right};
    unsigned i;
    uint64_t h, width, kh, kw;
    if (!tiku_kits_ml_shape_size(in)) { return none; }
    for (i = 0; i < 10; ++i) {
        if (fields[i] > 65535 || (i < 6 && !fields[i])) { return none; }
    }
    h = (uint64_t)in.height + w->pad_top + w->pad_bottom;
    width = (uint64_t)in.width + w->pad_left + w->pad_right;
    kh = (uint64_t)(w->height-1U) * w->dilation_h + 1U;
    kw = (uint64_t)(w->width-1U) * w->dilation_w + 1U;
    if (h < kh || width < kw) { return none; }
    result = (tiku_kits_ml_shape_t){(uint32_t)((h-kh)/w->stride_h+1U),
        (uint32_t)((width-kw)/w->stride_w+1U),channels};
    return tiku_kits_ml_shape_size(result) ? result : none;
}

tiku_kits_ml_shape_t tiku_kits_ml_conv2d_shape(const tiku_kits_ml_conv2d_t *m)
{
    tiku_kits_ml_shape_t none = {0,0,0};
    uint64_t patch;
    if (!ml_span(m,sizeof(*m),_Alignof(tiku_kits_ml_conv2d_t))) { return none; }
    tiku_kits_ml_shape_t result = window_shape(m->input,&m->window,m->rows.outputs);
    if (!result.height) { return none; }
    patch = (uint64_t)m->window.height * m->window.width * m->input.channels;
    if (patch != m->rows.inputs || m->rows.stride < patch ||
        (m->rows.quant_count != 1 && m->rows.quant_count != m->rows.outputs)) { return none; }
    return result;
}

tiku_kits_ml_shape_t tiku_kits_ml_pool2d_shape(const tiku_kits_ml_pool2d_t *m)
{
    tiku_kits_ml_shape_t none = {0,0,0};
    if (!ml_span(m,sizeof(*m),_Alignof(tiku_kits_ml_pool2d_t)) ||
        (m->kind != TIKU_KITS_ML_POOL_MAX && m->kind != TIKU_KITS_ML_POOL_AVERAGE) ||
        m->zero_point < -128 || m->zero_point > 127 ||
        m->activation_min < -128 || m->activation_max > 127 ||
        m->activation_min > m->activation_max) { return none; }
    return window_shape(m->input,&m->window,m->input.channels);
}

size_t tiku_kits_ml_conv2d_scratch_size(const tiku_kits_ml_conv2d_t *m)
{
    size_t out = tiku_kits_ml_shape_size(tiku_kits_ml_conv2d_shape(m));
    uint64_t need;
    if (!out) { return 0; }
    need = (uint64_t)out + m->rows.inputs + m->rows.outputs;
    return need > SIZE_MAX ? 0 : (size_t)need;
}

size_t tiku_kits_ml_pool2d_scratch_size(const tiku_kits_ml_pool2d_t *m)
{
    return tiku_kits_ml_shape_size(tiku_kits_ml_pool2d_shape(m));
}

/** @brief Validate capacities and write/read disjointness before tensor execution. */
static tiku_kits_ml_dense_status_t buffers(const void *m, size_t mn, uintptr_t ma,
    const int8_t *in, size_t supplied, size_t in_size, int8_t *out, size_t cap,
    size_t out_size, void *work, size_t bytes, size_t need)
{
    tiku_kits_ml_dense_status_t rc;
    if (!out_size || supplied != in_size || cap < out_size) { return TIKU_KITS_ML_DENSE_ERR_PARAM; }
    if (!work || bytes < need) { return TIKU_KITS_ML_DENSE_ERR_SCRATCH; }
    if (!ml_span(out,out_size,1) || !ml_span(work,need,1)) { return TIKU_KITS_ML_DENSE_ERR_PARAM; }
    if (ml_overlap(out,out_size,work,need)) { return TIKU_KITS_ML_DENSE_ERR_OVERLAP; }
    rc = ml_protect(m,mn,ma,out,out_size,work,need);
    return rc ? rc : ml_protect(in,in_size,1,out,out_size,work,need);
}

tiku_kits_ml_dense_status_t tiku_kits_ml_conv2d_s8(const tiku_kits_ml_conv2d_t *m,
    const int8_t *input, size_t input_bytes, int8_t *output, size_t output_capacity,
    void *scratch, size_t scratch_bytes)
{
    tiku_kits_ml_shape_t shape = tiku_kits_ml_conv2d_shape(m);
    size_t n = tiku_kits_ml_shape_size(shape), need = tiku_kits_ml_conv2d_scratch_size(m);
    uint32_t y,x,ky,kx,c;
    tiku_kits_ml_dense_status_t rc;
    if (!n || !need) { return TIKU_KITS_ML_DENSE_ERR_PARAM; }
    rc = buffers(m,sizeof(*m),_Alignof(tiku_kits_ml_conv2d_t),input,input_bytes,
        tiku_kits_ml_shape_size(m->input),output,output_capacity,n,scratch,scratch_bytes,need);
    if (rc) { return rc; }
    rc = ml_protect_rows(&m->rows,output,n,scratch,need);
    if (rc) { return rc; }
    int8_t *staged = scratch, *patch = staged + n;
    const tiku_kits_ml_window_t *w = &m->window;
    for (y = 0; y < shape.height; ++y) {
        for (x = 0; x < shape.width; ++x) {
            size_t p = 0;
            for (ky = 0; ky < w->height; ++ky) {
                int64_t iy = (int64_t)y * w->stride_h + (int64_t)ky * w->dilation_h - w->pad_top;
                for (kx = 0; kx < w->width; ++kx) {
                    int64_t ix = (int64_t)x * w->stride_w + (int64_t)kx * w->dilation_w - w->pad_left;
                    for (c = 0; c < m->input.channels; ++c) {
                        patch[p++] = iy < 0 || ix < 0 || iy >= m->input.height || ix >= m->input.width ?
                            (int8_t)m->rows.input_zero_point :
                            input[((size_t)iy * m->input.width + (size_t)ix) * m->input.channels + c];
                    }
                }
            }
            rc = tiku_kits_ml_dense_s8(&m->rows,patch,
                staged + ((size_t)y * shape.width + x) * shape.channels,
                patch + m->rows.inputs,m->rows.outputs);
            if (rc) { return rc; }
        }
    }
    memcpy(output,staged,n);
    return TIKU_KITS_ML_DENSE_OK;
}

tiku_kits_ml_dense_status_t tiku_kits_ml_pool2d_s8(const tiku_kits_ml_pool2d_t *m,
    const int8_t *input, size_t input_bytes, int8_t *output, size_t output_capacity,
    void *scratch, size_t scratch_bytes)
{
    tiku_kits_ml_shape_t shape = tiku_kits_ml_pool2d_shape(m);
    size_t n = tiku_kits_ml_shape_size(shape);
    uint32_t y,x,c,ky,kx;
    int8_t *staged = scratch;
    tiku_kits_ml_dense_status_t rc;
    if (!n) { return TIKU_KITS_ML_DENSE_ERR_PARAM; }
    rc = buffers(m,sizeof(*m),_Alignof(tiku_kits_ml_pool2d_t),input,input_bytes,
        tiku_kits_ml_shape_size(m->input),output,output_capacity,n,scratch,scratch_bytes,n);
    if (rc) { return rc; }
    const tiku_kits_ml_window_t *w = &m->window;
    for (y = 0; y < shape.height; ++y) {
        for (x = 0; x < shape.width; ++x) {
            for (c = 0; c < shape.channels; ++c) {
                int64_t sum = 0, value = -128;
                uint32_t count = 0;
                for (ky = 0; ky < w->height; ++ky) {
                    int64_t iy = (int64_t)y * w->stride_h + (int64_t)ky * w->dilation_h - w->pad_top;
                    for (kx = 0; kx < w->width; ++kx) {
                        int64_t ix = (int64_t)x * w->stride_w + (int64_t)kx * w->dilation_w - w->pad_left;
                        if (iy >= 0 && ix >= 0 && iy < m->input.height && ix < m->input.width) {
                            int8_t sample = input[((size_t)iy * m->input.width + (size_t)ix) * shape.channels + c];
                            sum += sample;
                            if (sample > value) { value = sample; }
                            ++count;
                        }
                    }
                }
                if (!count) { return TIKU_KITS_ML_DENSE_ERR_PARAM; }
                if (m->kind == TIKU_KITS_ML_POOL_AVERAGE) {
                    value = sum >= 0 ? (sum + count/2U)/count : -((-sum + count/2U)/count);
                }
                value = value < m->activation_min ? m->activation_min : value;
                value = value > m->activation_max ? m->activation_max : value;
                staged[((size_t)y * shape.width + x) * shape.channels + c] = (int8_t)value;
            }
        }
    }
    memcpy(output,staged,n);
    return TIKU_KITS_ML_DENSE_OK;
}
