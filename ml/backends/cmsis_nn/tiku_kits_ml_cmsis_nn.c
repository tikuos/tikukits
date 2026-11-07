/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_cmsis_nn.c - reentrant CMSIS-NN dense row adapter.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../../operators/tiku_kits_ml_dense_backend.h"
#include <arm_nnfunctions.h>
#if defined(CMSIS_NN_USE_SINGLE_ROUNDING)
#error "The dense contract requires double rounding"
#endif

int tiku_kits_ml_cmsis_row(const tiku_kits_ml_dense_t *m,
    const int8_t *weights, const int8_t *input, int32_t bias, int64_t weight_sum,
    uint32_t quant, int8_t *output)
{
    int32_t kernel_sum;
    cmsis_nn_context context;
    cmsis_nn_dims inputs, filters, biases, outputs;
    cmsis_nn_fc_params params;
    cmsis_nn_per_tensor_quant_params scale;
    if (m->inputs > INT32_MAX || (m->inputs & 15U) != 0 || m->shift[quant] < -30) {
        return 0;
    }
    kernel_sum = (int32_t)((int64_t)bias - m->input_zero_point * weight_sum);
    context.buf = &kernel_sum;
    context.size = sizeof(kernel_sum);
    inputs = (cmsis_nn_dims){1,1,1,(int32_t)m->inputs};
    filters = (cmsis_nn_dims){(int32_t)m->inputs,1,1,1};
    biases = (cmsis_nn_dims){1,1,1,1};
    outputs = (cmsis_nn_dims){1,1,1,1};
    params.input_offset = -m->input_zero_point;
    params.filter_offset = 0;
    params.output_offset = m->output_zero_point;
    params.activation.min = m->activation_min;
    params.activation.max = m->activation_max;
    scale.multiplier = m->multiplier[quant];
    scale.shift = m->shift[quant];
    return arm_fully_connected_s8(&context, &params, &scale, &inputs, input,
        &filters, weights, &biases, &bias, &outputs, output) == ARM_CMSIS_NN_SUCCESS;
}
