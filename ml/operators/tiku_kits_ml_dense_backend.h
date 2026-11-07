/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_dense_backend.h - validated dense row backend contract.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TIKU_KITS_ML_DENSE_BACKEND_H_
#define TIKU_KITS_ML_DENSE_BACKEND_H_
#include "tiku_kits_ml_dense.h"
#if TIKU_KITS_ML_CMSIS_NN
/**
 * @brief Evaluate one validated row through CMSIS-NN; return one when handled.
 * @note The caller proves int32 bounds for raw and corrected accumulation and shifts.
 * @note No global state or retained pointers; output points to caller staging.
 */
int tiku_kits_ml_cmsis_row(const tiku_kits_ml_dense_t *model,
    const int8_t *weights, const int8_t *input, int32_t bias, int64_t weight_sum,
    uint32_t quant, int8_t *output);
#endif
#endif
