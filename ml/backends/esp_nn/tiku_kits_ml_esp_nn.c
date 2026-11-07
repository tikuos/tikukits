/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_esp_nn.c - bounded, reentrant ESP-NN dense row adapter.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "../../operators/tiku_kits_ml_dense_backend.h"
#include <arch/esp32p4/compute/tiku_compute_p4_scope.h>
#include <hal/compute/tiku_compute_internal.h>

/** @brief Pinned ESP-NN v1.4.2 per-tensor dense entry point. */
extern void tiku_esp_dense_kernel(const int8_t *,
    uint16_t, const int8_t *, const int32_t *, int8_t *,
    int32_t, int32_t, int32_t, int32_t, int32_t);

/** @brief Check a validated byte span against the P4 vector memory window. */
static int vector_memory(const void *p, uint32_t n)
{
    uintptr_t start = (uintptr_t)p;
    return start >= UINT32_C(0x40000000) && start < UINT32_C(0x50000000) &&
           n <= UINT32_C(0x50000000) - start;
}

int tiku_kits_ml_esp_row(const tiku_kits_ml_dense_t *m,
    const int8_t *weights, const int8_t *input, int32_t bias, int64_t weight_sum,
    uint32_t quant, int8_t *output)
{
    tiku_compute_p4_scope_t scope;
    int32_t corrected;
    if (m->inputs < 16 || m->inputs > UINT16_MAX || m->shift[quant] < -30 ||
        !vector_memory(weights,m->inputs) || !vector_memory(input,m->inputs)) {
        return 0;
    }
    if (!tiku_compute_p4_scope_enter(&scope)) {
        return 0;
    }
    corrected = (int32_t)((int64_t)bias - m->input_zero_point * weight_sum);
    tiku_esp_dense_kernel(input,(uint16_t)m->inputs,weights,
        &corrected,output,m->output_zero_point,m->shift[quant],m->multiplier[quant],
        m->activation_min,m->activation_max);
    tiku_compute_p4_scope_leave(&scope);
    tiku_compute_note(TIKU_COMPUTE_DOT_S8,m->inputs & ~15U,m->inputs & 15U);
    return 1;
}
