/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_chain.h - execution of static quantized dense-layer chains.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TIKU_KITS_ML_CHAIN_H_
#define TIKU_KITS_ML_CHAIN_H_
#include "../operators/tiku_kits_ml_dense.h"

/** @brief Immutable ordered layers with matching dimensions and zero points. */
typedef struct {
    const tiku_kits_ml_dense_t *layers;
    uint32_t count;
} tiku_kits_ml_chain_t;

/** @brief Return required workspace bytes; zero denotes invalid shape metadata. */
size_t tiku_kits_ml_chain_workspace_size(const tiku_kits_ml_chain_t *model);

/**
 * @brief Execute dense layers with byte-aligned, caller-owned workspace.
 * @note Workspace and output are disjoint from each other and readable model/input spans.
 * @return Dense status; errors preserve output, but workspace may change.
 */
tiku_kits_ml_dense_status_t tiku_kits_ml_chain_s8(
    const tiku_kits_ml_chain_t *model, const int8_t *input, size_t input_count,
    int8_t *output, size_t output_capacity, void *workspace, size_t workspace_bytes);
#endif
