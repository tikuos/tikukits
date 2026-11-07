/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_esp_kernel.c - specialize pinned dense code for corrected bias.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define esp_nn_fully_connected_s8_riscv_pie \
    __attribute__((always_inline)) static inline tiku_esp_fc_impl
#define esp_nn_fully_connected_per_ch_s8_riscv_pie \
    static inline tiku_esp_fc_channels_unused
#include <esp_nn_fully_connected_s8_riscv_pie.c>
#undef esp_nn_fully_connected_s8_riscv_pie
#undef esp_nn_fully_connected_per_ch_s8_riscv_pie

/** @brief Evaluate one row with symmetric offsets and a corrected caller bias. */
void tiku_esp_dense_kernel(const int8_t *input, uint16_t count, const int8_t *weights,
    const int32_t *bias, int8_t *output, int32_t offset, int32_t shift,
    int32_t multiplier, int32_t low, int32_t high)
{
    tiku_esp_fc_impl(input,0,count,weights,0,bias,output,1,offset,shift,multiplier,low,high);
}
