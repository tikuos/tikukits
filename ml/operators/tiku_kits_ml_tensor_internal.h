/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ml_tensor_internal.h - shared tensor buffer validation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TIKU_KITS_ML_TENSOR_INTERNAL_H_
#define TIKU_KITS_ML_TENSOR_INTERNAL_H_
#include "tiku_kits_ml_cnn.h"

/** @brief Check a nonempty naturally aligned address span. */
static inline int ml_span(const void *p, uint64_t n, uintptr_t alignment)
{
    uintptr_t a = (uintptr_t)p;
    return p != NULL && a % alignment == 0 && n != 0 && n <= SIZE_MAX &&
           n <= UINTPTR_MAX && n <= UINTPTR_MAX - a;
}

/** @brief Test overlap of validated spans. */
static inline int ml_overlap(const void *a, size_t an, const void *b, size_t bn)
{
    return (uintptr_t)a < (uintptr_t)b + bn && (uintptr_t)b < (uintptr_t)a + an;
}

/** @brief Protect a readable span from the two validated write spans. */
static inline tiku_kits_ml_dense_status_t ml_protect(const void *p, uint64_t n,
    uintptr_t align, void *out, size_t on, void *work, size_t wn)
{
    if (!ml_span(p,n,align)) { return TIKU_KITS_ML_DENSE_ERR_PARAM; }
    if (ml_overlap(p,(size_t)n,out,on) || ml_overlap(p,(size_t)n,work,wn)) {
        return TIKU_KITS_ML_DENSE_ERR_OVERLAP;
    }
    return TIKU_KITS_ML_DENSE_OK;
}

/** @brief Protect dense parameters after shape validation. */
static inline tiku_kits_ml_dense_status_t ml_protect_rows(const tiku_kits_ml_dense_t *m,
    void *out, size_t on, void *work, size_t wn)
{
    tiku_kits_ml_dense_status_t rc;
#define ML_READ(p,n,a) do { rc = ml_protect(p,n,a,out,on,work,wn); if (rc) { return rc; } } while (0)
    ML_READ(m->weights,(uint64_t)(m->outputs-1U) * m->stride + m->inputs,1);
    ML_READ(m->multiplier,(uint64_t)m->quant_count * sizeof(int32_t),_Alignof(int32_t));
    ML_READ(m->shift,m->quant_count,1);
    if (m->bias != NULL) {
        ML_READ(m->bias,(uint64_t)m->outputs * sizeof(int32_t),_Alignof(int32_t));
    }
#undef ML_READ
    return TIKU_KITS_ML_DENSE_OK;
}
#endif
