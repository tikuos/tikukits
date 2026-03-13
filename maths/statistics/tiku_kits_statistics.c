/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_statistics.c - Running statistics library
 *
 * Platform-independent implementation of streaming and windowed
 * statistics trackers. All storage is statically allocated; no heap.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_statistics.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief 64-bit integer square root (floor)
 * @param x Non-negative input
 * @return floor(sqrt(x)), or 0 if x <= 0
 *
 * Non-restoring bit-shift algorithm. No multiplication.
 * Used internally by energy RMS where mean_sq is int64_t.
 */
static int64_t isqrt64(int64_t x)
{
    uint64_t val;
    uint64_t result = 0;
    uint64_t bit;

    if (x <= 0) {
        return 0;
    }

    val = (uint64_t)x;
    bit = (uint64_t)1 << 62;

    while (bit > val) {
        bit >>= 2;
    }

    while (bit != 0) {
        if (val >= result + bit) {
            val -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return (int64_t)result;
}

/*===========================================================================*/
/*                                                                           */
/* WINDOWED STATISTICS TRACKER                                               */
/*                                                                           */
/*===========================================================================*/

int tiku_kits_statistics_init(struct tiku_kits_statistics *s,
                              uint16_t window)
{
    if (s == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (window == 0
        || window > TIKU_KITS_STATISTICS_MAX_WINDOW) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    s->capacity = window;
    s->head = 0;
    s->count = 0;
    s->sum = 0;
    memset(s->buf, 0, sizeof(s->buf));
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_reset(struct tiku_kits_statistics *s)
{
    if (s == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    s->head = 0;
    s->count = 0;
    s->sum = 0;
    memset(s->buf, 0, sizeof(s->buf));
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_push(struct tiku_kits_statistics *s,
                              tiku_kits_statistics_elem_t value)
{
    if (s == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    if (s->count >= s->capacity) {
        s->sum -= s->buf[s->head];
    } else {
        s->count++;
    }

    s->buf[s->head] = value;
    s->sum += value;
    s->head = (s->head + 1) % s->capacity;

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_mean(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result)
{
    if (s == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (s->count == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    *result = s->sum / (tiku_kits_statistics_elem_t)s->count;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_variance(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result)
{
    tiku_kits_statistics_elem_t mean;
    tiku_kits_statistics_elem_t diff;
    tiku_kits_statistics_elem_t sum_sq;
    uint16_t i;

    if (s == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (s->count == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    mean = s->sum / (tiku_kits_statistics_elem_t)s->count;
    sum_sq = 0;

    for (i = 0; i < s->count; i++) {
        diff = s->buf[i] - mean;
        sum_sq += diff * diff;
    }

    *result = sum_sq / (tiku_kits_statistics_elem_t)s->count;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_min(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result)
{
    tiku_kits_statistics_elem_t val;
    uint16_t i;

    if (s == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (s->count == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    val = s->buf[0];
    for (i = 1; i < s->count; i++) {
        if (s->buf[i] < val) {
            val = s->buf[i];
        }
    }

    *result = val;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_max(
    const struct tiku_kits_statistics *s,
    tiku_kits_statistics_elem_t *result)
{
    tiku_kits_statistics_elem_t val;
    uint16_t i;

    if (s == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (s->count == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    val = s->buf[0];
    for (i = 1; i < s->count; i++) {
        if (s->buf[i] > val) {
            val = s->buf[i];
        }
    }

    *result = val;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_statistics_count(
    const struct tiku_kits_statistics *s)
{
    if (s == NULL) {
        return 0;
    }
    return s->count;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_full(
    const struct tiku_kits_statistics *s)
{
    if (s == NULL) {
        return 0;
    }
    return (s->count >= s->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_statistics_capacity(
    const struct tiku_kits_statistics *s)
{
    if (s == NULL) {
        return 0;
    }
    return s->capacity;
}

/*===========================================================================*/
/*                                                                           */
/* WELFORD RUNNING MEAN / VARIANCE                                           */
/*                                                                           */
/*===========================================================================*/

int tiku_kits_statistics_welford_init(
    struct tiku_kits_statistics_welford *w)
{
    if (w == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    w->mean = 0;
    w->m2 = 0;
    w->n = 0;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_welford_reset(
    struct tiku_kits_statistics_welford *w)
{
    if (w == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    w->mean = 0;
    w->m2 = 0;
    w->n = 0;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_welford_push(
    struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t value)
{
    tiku_kits_statistics_elem_t delta;
    tiku_kits_statistics_elem_t delta2;

    if (w == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    w->n++;
    delta = value - w->mean;
    w->mean += delta / (tiku_kits_statistics_elem_t)w->n;
    delta2 = value - w->mean;
    w->m2 += (int64_t)delta * (int64_t)delta2;

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_welford_mean(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result)
{
    if (w == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (w->n == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    *result = w->mean;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_welford_variance(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result)
{
    if (w == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (w->n == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    *result = (tiku_kits_statistics_elem_t)
        (w->m2 / (int64_t)w->n);
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_welford_stddev(
    const struct tiku_kits_statistics_welford *w,
    tiku_kits_statistics_elem_t *result)
{
    tiku_kits_statistics_elem_t var;
    int rc;

    rc = tiku_kits_statistics_welford_variance(w, &var);
    if (rc != TIKU_KITS_MATHS_OK) {
        return rc;
    }

    *result = tiku_kits_statistics_isqrt(var);
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

uint32_t tiku_kits_statistics_welford_count(
    const struct tiku_kits_statistics_welford *w)
{
    if (w == NULL) {
        return 0;
    }
    return w->n;
}

/*===========================================================================*/
/*                                                                           */
/* SLIDING-WINDOW MIN / MAX (MONOTONIC DEQUE)                                */
/*                                                                           */
/*===========================================================================*/

/*
 * Deque helper macros. Each deque is a circular array of uint16_t
 * sequence numbers, indexed by (front + offset) % MAX_WINDOW.
 *
 * DQ_BACK(front, size) -- index of the back (newest) element
 * DQ_NEXT(front, size) -- index of the next push-back slot
 */
#define DQ_MOD TIKU_KITS_STATISTICS_MAX_WINDOW

#define DQ_BACK_IDX(front, size) \
    (((front) + (size) - 1) % DQ_MOD)

#define DQ_NEXT_IDX(front, size) \
    (((front) + (size)) % DQ_MOD)

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_minmax_init(
    struct tiku_kits_statistics_minmax *m,
    uint16_t window)
{
    if (m == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (window == 0
        || window > TIKU_KITS_STATISTICS_MAX_WINDOW) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    m->capacity = window;
    m->seq = 0;
    m->count = 0;
    m->min_front = 0;
    m->min_size = 0;
    m->max_front = 0;
    m->max_size = 0;
    memset(m->buf, 0, sizeof(m->buf));
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_minmax_reset(
    struct tiku_kits_statistics_minmax *m)
{
    if (m == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    m->seq = 0;
    m->count = 0;
    m->min_front = 0;
    m->min_size = 0;
    m->max_front = 0;
    m->max_size = 0;
    memset(m->buf, 0, sizeof(m->buf));
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_minmax_push(
    struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t value)
{
    uint16_t back_idx;
    uint16_t back_seq;

    if (m == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    /* Store sample in circular buffer */
    m->buf[m->seq % m->capacity] = value;

    /*
     * Min-deque: maintain ascending values from front to back.
     * 1. Expire front entries outside the window.
     * 2. Pop back entries >= new value (they can never be min).
     * 3. Push current seq to back.
     */
    while (m->min_size > 0
           && (uint16_t)(m->seq - m->min_dq[m->min_front])
              >= m->capacity) {
        m->min_front = (m->min_front + 1) % DQ_MOD;
        m->min_size--;
    }

    while (m->min_size > 0) {
        back_idx = DQ_BACK_IDX(m->min_front, m->min_size);
        back_seq = m->min_dq[back_idx];
        if (m->buf[back_seq % m->capacity] >= value) {
            m->min_size--;
        } else {
            break;
        }
    }

    m->min_dq[DQ_NEXT_IDX(m->min_front, m->min_size)]
        = m->seq;
    m->min_size++;

    /*
     * Max-deque: maintain descending values from front to back.
     * Same logic with reversed comparison.
     */
    while (m->max_size > 0
           && (uint16_t)(m->seq - m->max_dq[m->max_front])
              >= m->capacity) {
        m->max_front = (m->max_front + 1) % DQ_MOD;
        m->max_size--;
    }

    while (m->max_size > 0) {
        back_idx = DQ_BACK_IDX(m->max_front, m->max_size);
        back_seq = m->max_dq[back_idx];
        if (m->buf[back_seq % m->capacity] <= value) {
            m->max_size--;
        } else {
            break;
        }
    }

    m->max_dq[DQ_NEXT_IDX(m->max_front, m->max_size)]
        = m->seq;
    m->max_size++;

    /* Advance sequence counter and count */
    m->seq++;
    if (m->count < m->capacity) {
        m->count++;
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_minmax_min(
    const struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t *result)
{
    uint16_t front_seq;

    if (m == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (m->count == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    front_seq = m->min_dq[m->min_front];
    *result = m->buf[front_seq % m->capacity];
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_minmax_max(
    const struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t *result)
{
    uint16_t front_seq;

    if (m == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (m->count == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    front_seq = m->max_dq[m->max_front];
    *result = m->buf[front_seq % m->capacity];
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_statistics_minmax_count(
    const struct tiku_kits_statistics_minmax *m)
{
    if (m == NULL) {
        return 0;
    }
    return m->count;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_minmax_full(
    const struct tiku_kits_statistics_minmax *m)
{
    if (m == NULL) {
        return 0;
    }
    return (m->count >= m->capacity) ? 1 : 0;
}

/*===========================================================================*/
/*                                                                           */
/* EXPONENTIALLY WEIGHTED MOVING AVERAGE (EWMA)                              */
/*                                                                           */
/*===========================================================================*/

int tiku_kits_statistics_ewma_init(
    struct tiku_kits_statistics_ewma *e,
    uint16_t alpha, uint8_t shift)
{
    if (e == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (shift > 16) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    e->value = 0;
    e->alpha = alpha;
    e->shift = shift;
    e->ready = 0;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_ewma_reset(
    struct tiku_kits_statistics_ewma *e)
{
    if (e == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    e->value = 0;
    e->ready = 0;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_ewma_push(
    struct tiku_kits_statistics_ewma *e,
    tiku_kits_statistics_elem_t value)
{
    int64_t diff;

    if (e == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    if (!e->ready) {
        e->value = value;
        e->ready = 1;
    } else {
        diff = (int64_t)value - (int64_t)e->value;
        e->value += (tiku_kits_statistics_elem_t)
            (((int64_t)e->alpha * diff) >> e->shift);
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_ewma_get(
    const struct tiku_kits_statistics_ewma *e,
    tiku_kits_statistics_elem_t *result)
{
    if (e == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (!e->ready) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    *result = e->value;
    return TIKU_KITS_MATHS_OK;
}

/*===========================================================================*/
/*                                                                           */
/* RUNNING ENERGY / RMS                                                      */
/*                                                                           */
/*===========================================================================*/

int tiku_kits_statistics_energy_init(
    struct tiku_kits_statistics_energy *e)
{
    if (e == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    e->mean_sq = 0;
    e->n = 0;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_energy_reset(
    struct tiku_kits_statistics_energy *e)
{
    if (e == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    e->mean_sq = 0;
    e->n = 0;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_energy_push(
    struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t value)
{
    int64_t sq;
    int64_t delta;

    if (e == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    e->n++;
    sq = (int64_t)value * (int64_t)value;
    delta = sq - e->mean_sq;
    e->mean_sq += delta / (int64_t)e->n;

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_energy_mean_sq(
    const struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t *result)
{
    if (e == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (e->n == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    *result = (tiku_kits_statistics_elem_t)e->mean_sq;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_statistics_energy_rms(
    const struct tiku_kits_statistics_energy *e,
    tiku_kits_statistics_elem_t *result)
{
    if (e == NULL || result == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    if (e->n == 0) {
        return TIKU_KITS_MATHS_ERR_SIZE;
    }

    *result = (tiku_kits_statistics_elem_t)isqrt64(e->mean_sq);
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

uint32_t tiku_kits_statistics_energy_count(
    const struct tiku_kits_statistics_energy *e)
{
    if (e == NULL) {
        return 0;
    }
    return e->n;
}

/*===========================================================================*/
/*                                                                           */
/* UTILITY                                                                   */
/*                                                                           */
/*===========================================================================*/

tiku_kits_statistics_elem_t tiku_kits_statistics_isqrt(
    tiku_kits_statistics_elem_t x)
{
    uint32_t val;
    uint32_t result = 0;
    uint32_t bit;

    if (x <= 0) {
        return 0;
    }

    val = (uint32_t)x;

    /* Start with highest power-of-4 that does not exceed val */
    bit = (uint32_t)1
        << ((sizeof(tiku_kits_statistics_elem_t) * 8 - 2)
            & ~(uint32_t)1);
    while (bit > val) {
        bit >>= 2;
    }

    /* Non-restoring bit-shift square root */
    while (bit != 0) {
        if (val >= result + bit) {
            val -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }

    return (tiku_kits_statistics_elem_t)result;
}
