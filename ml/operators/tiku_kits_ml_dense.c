/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_dense.c - validated dense inference with staged publication.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tiku_kits_ml_dense.h"
#include "tiku_kits_ml_dense_backend.h"
#include <hal/compute/tiku_compute_dot.h>

/** @brief Check readable or writable span representation and natural alignment. */
static int valid_span(const void *ptr, uint64_t bytes, uintptr_t alignment)
{
    uintptr_t base = (uintptr_t)ptr;
    return ptr != NULL && base % alignment == 0 && bytes <= SIZE_MAX &&
           bytes <= UINTPTR_MAX && bytes <= UINTPTR_MAX - base;
}

/** @brief Test overlap of validated spans. */
static int overlaps(const void *a, uint64_t an, const void *b, uint64_t bn)
{
    uintptr_t ap = (uintptr_t)a, bp = (uintptr_t)b;
    return ap < bp + (uintptr_t)bn && bp < ap + (uintptr_t)an;
}

/** @brief Check a readable span and its disjointness from writable buffers. */
static tiku_kits_ml_dense_status_t readable(const void *ptr, uint64_t bytes,
    uintptr_t align, int8_t *out, uint32_t n, void *scratch)
{
    if (!valid_span(ptr, bytes, align)) {
        return TIKU_KITS_ML_DENSE_ERR_PARAM;
    }
    if (overlaps(ptr, bytes, out, n) || overlaps(ptr, bytes, scratch, n)) {
        return TIKU_KITS_ML_DENSE_ERR_OVERLAP;
    }
    return TIKU_KITS_ML_DENSE_OK;
}

/** @brief Double rounding: high multiply toward positive ties, divide away at ties. */
static int32_t requantize(int32_t value, int32_t multiplier, int8_t shift)
{
    int64_t wide = value;
    int64_t product;
    int64_t scaled;
    if (shift > 0) {
        wide *= INT64_C(1) << shift;
    }
    product = wide * multiplier + (INT64_C(1) << 30);
    scaled = product >= 0 ? product / (INT64_C(1) << 31) :
             -((-product + (INT64_C(1) << 31) - 1) / (INT64_C(1) << 31));
    if (shift < 0) {
        unsigned int right = (unsigned int)(-(int)shift);
        int64_t magnitude = scaled < 0 ? -scaled : scaled;
        magnitude = (magnitude + (INT64_C(1) << (right - 1U))) >> right;
        scaled = scaled < 0 ? -magnitude : magnitude;
    }
    return (int32_t)scaled;
}

size_t tiku_kits_ml_dense_scratch_size(const tiku_kits_ml_dense_t *model)
{
    if (!valid_span(model, sizeof(*model), _Alignof(tiku_kits_ml_dense_t))) {
        return 0;
    }
#if SIZE_MAX < UINT32_MAX
    if (model->outputs > SIZE_MAX) {
        return 0;
    }
#endif
    return (size_t)model->outputs;
}

tiku_kits_ml_dense_status_t tiku_kits_ml_dense_s8(
    const tiku_kits_ml_dense_t *model, const int8_t *input, int8_t *output,
    void *scratch, size_t scratch_bytes)
{
    tiku_kits_ml_dense_t m;
    tiku_kits_ml_dense_status_t status;
    uint64_t weight_bytes;
    uint32_t r;
    int8_t *staged = scratch;
    if (!valid_span(model, sizeof(*model), _Alignof(tiku_kits_ml_dense_t))) {
        return TIKU_KITS_ML_DENSE_ERR_PARAM;
    }
    m = *model;
    if (m.inputs == 0 || m.outputs == 0 || m.stride < m.inputs ||
        (m.quant_count != 1 && m.quant_count != m.outputs) ||
        m.input_zero_point < -128 || m.input_zero_point > 127 ||
        m.output_zero_point < -128 || m.output_zero_point > 127 ||
        m.activation_min < -128 || m.activation_max > 127 ||
        m.activation_min > m.activation_max) {
        return TIKU_KITS_ML_DENSE_ERR_PARAM;
    }
    if (scratch == NULL || (uint64_t)scratch_bytes < m.outputs) {
        return TIKU_KITS_ML_DENSE_ERR_SCRATCH;
    }
    if (!valid_span(output, m.outputs, 1) || !valid_span(scratch, m.outputs, 1)) {
        return TIKU_KITS_ML_DENSE_ERR_PARAM;
    }
    if (overlaps(output, m.outputs, scratch, m.outputs)) {
        return TIKU_KITS_ML_DENSE_ERR_OVERLAP;
    }
    weight_bytes = (uint64_t)(m.outputs - 1U) * m.stride + m.inputs;
#define CHECK_READ(ptr, bytes, alignment) do { \
    status = readable(ptr, bytes, alignment, output, m.outputs, scratch); \
    if (status != TIKU_KITS_ML_DENSE_OK) { return status; } \
} while (0)
    CHECK_READ(model, sizeof(*model), _Alignof(tiku_kits_ml_dense_t));
    CHECK_READ(input, m.inputs, 1);
    CHECK_READ(m.weights, weight_bytes, 1);
    CHECK_READ(m.multiplier, (uint64_t)m.quant_count * sizeof(int32_t), _Alignof(int32_t));
    CHECK_READ(m.shift, m.quant_count, 1);
    if (m.bias != NULL) {
        CHECK_READ(m.bias, (uint64_t)m.outputs * sizeof(int32_t), _Alignof(int32_t));
    }
#undef CHECK_READ
    for (r = 0; r < m.outputs; ++r) {
        uint32_t q = m.quant_count == 1 ? 0 : r;
        const int8_t *weights = m.weights + (size_t)r * m.stride;
        int64_t acc;
        int64_t weight_sum = 0;
#if TIKU_KITS_ML_CMSIS_NN
        int64_t abs_sum = 0;
#endif
        int64_t shifted;
        int64_t value;
        uint32_t i;
        if (m.multiplier[q] < 0 || m.shift[q] < -31 || m.shift[q] > 30) {
            return TIKU_KITS_ML_DENSE_ERR_PARAM;
        }
        for (i = 0; i < m.inputs; ++i) {
            if (weights[i] == INT8_MIN) {
                return TIKU_KITS_ML_DENSE_ERR_PARAM;
            }
            weight_sum += weights[i];
#if TIKU_KITS_ML_CMSIS_NN
            abs_sum += weights[i] < 0 ? -(int32_t)weights[i] : weights[i];
#endif
        }
#if TIKU_KITS_ML_CMSIS_NN
        {
            int32_t bias = m.bias != NULL ? m.bias[r] : 0;
            int64_t bias_abs = bias < 0 ? -(int64_t)bias : bias;
            int64_t bound = bias_abs + abs_sum * (128 + (m.input_zero_point < 0 ?
                                            -m.input_zero_point : m.input_zero_point));
            int64_t limit = INT32_MAX - 128;
            if (m.shift[q] > 0) {
                limit >>= m.shift[q];
            }
            if (bound <= limit && tiku_kits_ml_cmsis_row(&m, weights, input,
                    bias, weight_sum, q, &staged[r])) {
                continue;
            }
        }
#endif
        if (tiku_compute_dot_s8(weights, input, m.inputs, &acc) != TIKU_COMPUTE_OK) {
            return TIKU_KITS_ML_DENSE_ERR_PARAM;
        }
        acc -= (int64_t)m.input_zero_point * weight_sum;
        if (m.bias != NULL) {
            acc += m.bias[r];
        }
        if (acc < INT32_MIN || acc > INT32_MAX) {
            return TIKU_KITS_ML_DENSE_ERR_RANGE;
        }
        shifted = acc;
        if (m.shift[q] > 0) {
            shifted *= INT64_C(1) << m.shift[q];
        }
        if (shifted < INT32_MIN || shifted > INT32_MAX) {
            return TIKU_KITS_ML_DENSE_ERR_RANGE;
        }
        value = (int64_t)requantize((int32_t)acc, m.multiplier[q], m.shift[q]) +
                m.output_zero_point;
        value = value < m.activation_min ? m.activation_min : value;
        value = value > m.activation_max ? m.activation_max : value;
        staged[r] = (int8_t)value;
    }
    for (r = 0; r < m.outputs; ++r) {
        output[r] = staged[r];
    }
    return TIKU_KITS_ML_DENSE_OK;
}
