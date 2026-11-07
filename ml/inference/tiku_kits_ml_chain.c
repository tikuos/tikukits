/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_chain.c - validated model spans and staged chain execution.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tiku_kits_ml_chain.h"

/** @brief Check natural alignment and representable nonempty memory extents. */
static int valid_span(const void *p, uint64_t bytes, uintptr_t alignment)
{
    uintptr_t base = (uintptr_t)p;
    return p != NULL && base % alignment == 0 && bytes <= SIZE_MAX &&
           bytes <= UINTPTR_MAX && bytes <= UINTPTR_MAX - base;
}

/** @brief Check ordered tensor shapes and compute the largest intermediate width. */
static size_t width(const tiku_kits_ml_chain_t *model)
{
    uint32_t i, largest = 0;
    if (!valid_span(model,sizeof(*model),_Alignof(tiku_kits_ml_chain_t)) ||
        model->count == 0 || !valid_span(model->layers,
            (uint64_t)model->count * sizeof(*model->layers),_Alignof(tiku_kits_ml_dense_t))) {
        return 0;
    }
    for (i = 0; i < model->count; ++i) {
        const tiku_kits_ml_dense_t *m = &model->layers[i];
        if (m->inputs == 0 || m->outputs == 0 || m->stride < m->inputs ||
            (m->quant_count != 1 && m->quant_count != m->outputs) ||
            (i != 0 && (m->inputs != model->layers[i-1].outputs ||
                        m->input_zero_point != model->layers[i-1].output_zero_point))) {
            return 0;
        }
        if (m->outputs > largest) {
            largest = m->outputs;
        }
    }
    if ((uint64_t)largest * 3U > SIZE_MAX) {
        return 0;
    }
    return (size_t)largest;
}

size_t tiku_kits_ml_chain_workspace_size(const tiku_kits_ml_chain_t *model)
{
    return width(model) * 3U;
}

/** @brief Test overlap after both address spans have been validated. */
static int overlap(const void *a, uint64_t an, const void *b, uint64_t bn)
{
    uintptr_t ap = (uintptr_t)a, bp = (uintptr_t)b;
    return ap < bp + (uintptr_t)bn && bp < ap + (uintptr_t)an;
}

/** @brief Protect a readable span from output and intermediate writes. */
static tiku_kits_ml_dense_status_t protect(const void *p, uint64_t bytes,
    uintptr_t alignment, void *workspace, size_t work_bytes, int8_t *out, size_t out_bytes)
{
    if (!valid_span(p,bytes,alignment)) {
        return TIKU_KITS_ML_DENSE_ERR_PARAM;
    }
    if (overlap(p,bytes,workspace,work_bytes) || overlap(p,bytes,out,out_bytes)) {
        return TIKU_KITS_ML_DENSE_ERR_OVERLAP;
    }
    return TIKU_KITS_ML_DENSE_OK;
}

tiku_kits_ml_dense_status_t tiku_kits_ml_chain_s8(
    const tiku_kits_ml_chain_t *model, const int8_t *input, size_t input_count,
    int8_t *output, size_t output_capacity, void *workspace, size_t workspace_bytes)
{
    size_t span = width(model), need = span * 3U, outputs, j;
    uint32_t i;
    tiku_kits_ml_dense_status_t status;
    const int8_t *current = input;
    int8_t *storage = workspace;
    if (span == 0) {
        return TIKU_KITS_ML_DENSE_ERR_PARAM;
    }
    outputs = (size_t)model->layers[model->count-1].outputs;
    if ((uint64_t)input_count != model->layers[0].inputs || output_capacity < outputs) {
        return TIKU_KITS_ML_DENSE_ERR_PARAM;
    }
    if (workspace == NULL || workspace_bytes < need) {
        return TIKU_KITS_ML_DENSE_ERR_SCRATCH;
    }
    if (!valid_span(workspace,need,1) || !valid_span(output,outputs,1)) {
        return TIKU_KITS_ML_DENSE_ERR_PARAM;
    }
    if (overlap(workspace,need,output,outputs)) {
        return TIKU_KITS_ML_DENSE_ERR_OVERLAP;
    }
#define PROTECT(p, bytes, alignment) do { \
    status = protect(p,bytes,alignment,workspace,need,output,outputs); \
    if (status != TIKU_KITS_ML_DENSE_OK) { return status; } \
} while (0)
    PROTECT(model,sizeof(*model),_Alignof(tiku_kits_ml_chain_t));
    PROTECT(model->layers,(uint64_t)model->count * sizeof(*model->layers),_Alignof(tiku_kits_ml_dense_t));
    PROTECT(input,input_count,1);
    for (i = 0; i < model->count; ++i) {
        const tiku_kits_ml_dense_t *m = &model->layers[i];
        PROTECT(m->weights,(uint64_t)(m->outputs-1U) * m->stride + m->inputs,1);
        PROTECT(m->multiplier,(uint64_t)m->quant_count * sizeof(int32_t),_Alignof(int32_t));
        PROTECT(m->shift,m->quant_count,1);
        if (m->bias != NULL) {
            PROTECT(m->bias,(uint64_t)m->outputs * sizeof(int32_t),_Alignof(int32_t));
        }
    }
#undef PROTECT
    for (i = 0; i < model->count; ++i) {
        int8_t *next = storage + ((i & 1U) != 0 ? span : 0);
        status = tiku_kits_ml_dense_s8(&model->layers[i],current,next,
                                      storage + span * 2U,span);
        if (status != TIKU_KITS_ML_DENSE_OK) {
            return status;
        }
        current = next;
    }
    for (j = 0; j < outputs; ++j) {
        output[j] = current[j];
    }
    return TIKU_KITS_ML_DENSE_OK;
}
