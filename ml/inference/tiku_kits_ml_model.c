/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_model.c - mixed-layer geometry, preflight and staged execution.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tiku_kits_ml_model.h"
#include "../operators/tiku_kits_ml_tensor_internal.h"
#include <string.h>

/** @brief Checked model layout and maximum intermediate/operator storage. */
typedef struct {
    tiku_kits_ml_shape_t output;
    size_t span, scratch, bytes;
} model_layout_t;

/** @brief Compare logical image shapes. */
static int same_shape(tiku_kits_ml_shape_t a, tiku_kits_ml_shape_t b)
{
    return a.height == b.height && a.width == b.width && a.channels == b.channels;
}

/** @brief Check sequential shape and zero-point connections and size the workspace. */
static model_layout_t layout(const tiku_kits_ml_model_t *model)
{
    model_layout_t none = {{0,0,0},0,0,0}, result = none;
    tiku_kits_ml_shape_t shape;
    int32_t zero;
    uint32_t i;
    uint64_t need;
    if (!ml_span(model,sizeof(*model),_Alignof(tiku_kits_ml_model_t)) ||
        !model->count || !tiku_kits_ml_shape_size(model->input) ||
        model->input_zero_point < -128 || model->input_zero_point > 127 ||
        !ml_span(model->layers,(uint64_t)model->count * sizeof(*model->layers),
                 _Alignof(tiku_kits_ml_layer_t))) { return none; }
    shape = model->input;
    zero = model->input_zero_point;
    for (i = 0; i < model->count; ++i) {
        const tiku_kits_ml_layer_t *l = &model->layers[i];
        const tiku_kits_ml_dense_t *rows = NULL;
        size_t scratch = 0, bytes = tiku_kits_ml_shape_size(shape);
        switch (l->kind) {
        case TIKU_KITS_ML_LAYER_DENSE:
            rows = &l->op.dense;
            if (rows->inputs != bytes || !rows->outputs || rows->stride < rows->inputs ||
                (rows->quant_count != 1 && rows->quant_count != rows->outputs)) { return none; }
            shape = (tiku_kits_ml_shape_t){1,1,rows->outputs};
            scratch = rows->outputs;
            break;
        case TIKU_KITS_ML_LAYER_CONV2D:
            if (!same_shape(shape,l->op.conv.input)) { return none; }
            shape = tiku_kits_ml_conv2d_shape(&l->op.conv);
            scratch = tiku_kits_ml_conv2d_scratch_size(&l->op.conv);
            rows = &l->op.conv.rows;
            if (!scratch) { return none; }
            break;
        case TIKU_KITS_ML_LAYER_POOL2D:
            if (!same_shape(shape,l->op.pool.input) || zero != l->op.pool.zero_point) { return none; }
            shape = tiku_kits_ml_pool2d_shape(&l->op.pool);
            scratch = tiku_kits_ml_pool2d_scratch_size(&l->op.pool);
            break;
        case TIKU_KITS_ML_LAYER_RESHAPE:
            shape = l->op.reshape;
            if (bytes != tiku_kits_ml_shape_size(shape)) { return none; }
            break;
        default:
            return none;
        }
        if (rows != NULL) {
            if (rows->input_zero_point != zero || rows->output_zero_point < -128 ||
                rows->output_zero_point > 127 || rows->activation_min < -128 ||
                rows->activation_max > 127 || rows->activation_min > rows->activation_max) { return none; }
            zero = rows->output_zero_point;
        }
        bytes = tiku_kits_ml_shape_size(shape);
        if (!bytes) { return none; }
        if (bytes > result.span) { result.span = bytes; }
        if (scratch > result.scratch) { result.scratch = scratch; }
    }
    need = (uint64_t)result.span * 2U + result.scratch;
    if (need > SIZE_MAX) { return none; }
    result.bytes = (size_t)need;
    result.output = shape;
    return result;
}

size_t tiku_kits_ml_model_workspace_size(const tiku_kits_ml_model_t *model)
{
    return layout(model).bytes;
}

tiku_kits_ml_shape_t tiku_kits_ml_model_output_shape(const tiku_kits_ml_model_t *model)
{
    return layout(model).output;
}

tiku_kits_ml_dense_status_t tiku_kits_ml_model_s8(const tiku_kits_ml_model_t *model,
    const int8_t *input, size_t input_bytes, int8_t *output, size_t output_capacity,
    void *workspace, size_t workspace_bytes)
{
    model_layout_t plan = layout(model);
    size_t n = tiku_kits_ml_shape_size(plan.output), current_bytes = input_bytes;
    const int8_t *current = input;
    int8_t *work = workspace;
    uint32_t i;
    tiku_kits_ml_dense_status_t rc;
    if (!plan.bytes || input_bytes != tiku_kits_ml_shape_size(model->input) ||
        output_capacity < n) { return TIKU_KITS_ML_DENSE_ERR_PARAM; }
    if (!workspace || workspace_bytes < plan.bytes) { return TIKU_KITS_ML_DENSE_ERR_SCRATCH; }
    if (!ml_span(output,n,1) || !ml_span(workspace,plan.bytes,1)) { return TIKU_KITS_ML_DENSE_ERR_PARAM; }
    if (ml_overlap(output,n,workspace,plan.bytes)) { return TIKU_KITS_ML_DENSE_ERR_OVERLAP; }
#define MODEL_READ(p,b,a) do { rc = ml_protect(p,b,a,output,n,workspace,plan.bytes); if (rc) { return rc; } } while (0)
    MODEL_READ(model,sizeof(*model),_Alignof(tiku_kits_ml_model_t));
    MODEL_READ(model->layers,(uint64_t)model->count * sizeof(*model->layers),_Alignof(tiku_kits_ml_layer_t));
    MODEL_READ(input,input_bytes,1);
#undef MODEL_READ
    for (i = 0; i < model->count; ++i) {
        const tiku_kits_ml_layer_t *l = &model->layers[i];
        const tiku_kits_ml_dense_t *rows = l->kind == TIKU_KITS_ML_LAYER_DENSE ? &l->op.dense :
            l->kind == TIKU_KITS_ML_LAYER_CONV2D ? &l->op.conv.rows : NULL;
        if (rows != NULL) {
            rc = ml_protect_rows(rows,output,n,workspace,plan.bytes);
            if (rc) { return rc; }
        }
    }
    for (i = 0; i < model->count; ++i) {
        const tiku_kits_ml_layer_t *l = &model->layers[i];
        int8_t *next = work + (i & 1U) * plan.span;
        void *scratch = work + 2U * plan.span;
        switch (l->kind) {
        case TIKU_KITS_ML_LAYER_DENSE:
            rc = tiku_kits_ml_dense_s8(&l->op.dense,current,next,scratch,plan.scratch);
            current_bytes = l->op.dense.outputs;
            break;
        case TIKU_KITS_ML_LAYER_CONV2D:
            rc = tiku_kits_ml_conv2d_s8(&l->op.conv,current,current_bytes,next,plan.span,scratch,plan.scratch);
            current_bytes = tiku_kits_ml_shape_size(tiku_kits_ml_conv2d_shape(&l->op.conv));
            break;
        case TIKU_KITS_ML_LAYER_POOL2D:
            rc = tiku_kits_ml_pool2d_s8(&l->op.pool,current,current_bytes,next,plan.span,scratch,plan.scratch);
            current_bytes = tiku_kits_ml_shape_size(tiku_kits_ml_pool2d_shape(&l->op.pool));
            break;
        case TIKU_KITS_ML_LAYER_RESHAPE:
            memcpy(next,current,current_bytes);
            rc = TIKU_KITS_ML_DENSE_OK;
            break;
        default:
            return TIKU_KITS_ML_DENSE_ERR_PARAM;
        }
        if (rc) { return rc; }
        current = next;
    }
    memcpy(output,current,n);
    return TIKU_KITS_ML_DENSE_OK;
}
