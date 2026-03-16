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
 *
 * Non-restoring bit-shift algorithm operating entirely with shifts,
 * adds, and compares -- no multiplication needed.  Used internally
 * by the energy tracker's RMS computation where mean_sq is int64_t
 * and may exceed 32-bit range.  Runs in at most 32 iterations
 * (bit starts at 2^62 and shifts right by 2 each iteration).
 *
 * @param x Non-negative input
 * @return floor(sqrt(x)), or 0 if x <= 0
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
    /* Start with the highest power-of-4 that fits in 64 bits */
    bit = (uint64_t)1 << 62;

    /* Skip leading zero pairs to reduce iterations */
    while (bit > val) {
        bit >>= 2;
    }

    /* Non-restoring square root: each iteration refines one bit */
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

/**
 * @brief Initialize a statistics tracker with given window size
 *
 * Zeros the entire circular buffer so that reads of unpopulated
 * slots return deterministic values.  The runtime capacity is
 * clamped to TIKU_KITS_STATISTICS_MAX_WINDOW.
 */
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
    /* Zero the full static buffer for a clean initial state */
    memset(s->buf, 0, sizeof(s->buf));
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Clear all samples, preserving the window size
 *
 * Resets count, head, and sum while keeping the configured capacity.
 * The buffer is zeroed for a clean state.
 */
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

/**
 * @brief Push a new sample into the window
 *
 * O(1).  If the window is full, the oldest sample (at head) is
 * subtracted from the running sum before being overwritten, keeping
 * the sum accurate without a rescan.
 */
int tiku_kits_statistics_push(struct tiku_kits_statistics *s,
                              tiku_kits_statistics_elem_t value)
{
    if (s == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    if (s->count >= s->capacity) {
        /* Evict oldest: subtract its contribution from the running sum */
        s->sum -= s->buf[s->head];
    } else {
        s->count++;
    }

    s->buf[s->head] = value;
    s->sum += value;
    /* Advance head circularly within the capacity range */
    s->head = (s->head + 1) % s->capacity;

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the arithmetic mean of the current window
 *
 * O(1) -- divides the incrementally maintained running sum by the
 * sample count.  Integer division truncates toward zero.
 */
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

/**
 * @brief Compute the population variance of the current window
 *
 * Two-pass approach: first compute the mean from the running sum,
 * then scan the buffer to accumulate sum((x_i - mean)^2).  O(n)
 * where n is the current count.  Population variance (denominator n,
 * not n-1).
 */
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

    /* Pass 1: mean from the running sum (O(1)) */
    mean = s->sum / (tiku_kits_statistics_elem_t)s->count;
    sum_sq = 0;

    /* Pass 2: accumulate squared deviations */
    for (i = 0; i < s->count; i++) {
        diff = s->buf[i] - mean;
        sum_sq += diff * diff;
    }

    *result = sum_sq / (tiku_kits_statistics_elem_t)s->count;
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the minimum value in the current window
 *
 * Linear scan over the buffer.  O(n) where n is the current count.
 * Acceptable for the small window sizes typical in embedded use.
 */
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

/**
 * @brief Get the maximum value in the current window
 *
 * Linear scan over the buffer.  O(n) where n is the current count.
 * Acceptable for the small window sizes typical in embedded use.
 */
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

/**
 * @brief Return the current number of samples in the window
 *
 * Safe to call with a NULL pointer -- returns 0.
 */
uint16_t tiku_kits_statistics_count(
    const struct tiku_kits_statistics *s)
{
    if (s == NULL) {
        return 0;
    }
    return s->count;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check if the window is full (count >= capacity)
 *
 * Safe to call with a NULL pointer -- returns 0 (not full).
 */
int tiku_kits_statistics_full(
    const struct tiku_kits_statistics *s)
{
    if (s == NULL) {
        return 0;
    }
    return (s->count >= s->capacity) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the configured window capacity
 *
 * Safe to call with a NULL pointer -- returns 0.
 */
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

/**
 * @brief Initialize a Welford tracker to an empty state
 *
 * Zeros mean, M2, and count.  No buffer allocation needed.
 */
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

/**
 * @brief Reset a Welford tracker, clearing all samples
 *
 * Equivalent to a fresh init -- mean, M2, and count are zeroed.
 */
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

/**
 * @brief Push a new sample into the Welford tracker
 *
 * Implements Welford's online algorithm in O(1):
 *   delta  = value - old_mean
 *   mean  += delta / n
 *   delta2 = value - new_mean
 *   M2    += delta * delta2
 *
 * The delta product is widened to int64_t to prevent overflow when
 * element values are large int32_t.
 */
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
    /* delta before mean update */
    delta = value - w->mean;
    w->mean += delta / (tiku_kits_statistics_elem_t)w->n;
    /* delta2 after mean update -- key to Welford's numerical stability */
    delta2 = value - w->mean;
    /* Widen to int64_t so the product does not overflow */
    w->m2 += (int64_t)delta * (int64_t)delta2;

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the running mean
 *
 * O(1) -- the mean is maintained incrementally by push().
 */
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

/**
 * @brief Get the running population variance
 *
 * Population variance = M2 / n.  Single int64_t division.
 */
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

/**
 * @brief Get the running population standard deviation
 *
 * Delegates to welford_variance(), then applies integer sqrt.
 * Result is floor(sqrt(variance)).
 */
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

/**
 * @brief Return the number of samples pushed
 *
 * Safe to call with a NULL pointer -- returns 0.
 */
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

/**
 * @brief Initialize a sliding-window min/max tracker
 *
 * Clears both deques, resets the sequence counter, and zeros the
 * sample buffer.
 */
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

/**
 * @brief Reset a min/max tracker, clearing all samples
 *
 * Equivalent to a fresh init with the same capacity.
 */
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

/**
 * @brief Push a new sample into the min/max tracker
 *
 * O(1) amortized.  For each deque (min and max):
 *   1. Expire front entries whose sequence number falls outside
 *      the window (unsigned modular age >= capacity).
 *   2. Pop back entries dominated by the new value (they can never
 *      become the min/max while the new value is in the window).
 *   3. Push the current sequence number onto the back.
 *
 * Each element enters and leaves each deque at most once, so the
 * total work over n pushes is O(n).
 */
int tiku_kits_statistics_minmax_push(
    struct tiku_kits_statistics_minmax *m,
    tiku_kits_statistics_elem_t value)
{
    uint16_t back_idx;
    uint16_t back_seq;

    if (m == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    /* Store sample in circular buffer at current sequence position */
    m->buf[m->seq % m->capacity] = value;

    /*
     * Min-deque: maintain ascending values from front to back.
     * Step 1: Expire front entries outside the window.
     */
    while (m->min_size > 0
           && (uint16_t)(m->seq - m->min_dq[m->min_front])
              >= m->capacity) {
        m->min_front = (m->min_front + 1) % DQ_MOD;
        m->min_size--;
    }

    /* Step 2: Pop back entries >= new value (they can never be min) */
    while (m->min_size > 0) {
        back_idx = DQ_BACK_IDX(m->min_front, m->min_size);
        back_seq = m->min_dq[back_idx];
        if (m->buf[back_seq % m->capacity] >= value) {
            m->min_size--;
        } else {
            break;
        }
    }

    /* Step 3: Push current sequence number onto back */
    m->min_dq[DQ_NEXT_IDX(m->min_front, m->min_size)]
        = m->seq;
    m->min_size++;

    /*
     * Max-deque: maintain descending values from front to back.
     * Same three-step logic with reversed comparison.
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

    /* Advance the monotonic sequence counter */
    m->seq++;
    if (m->count < m->capacity) {
        m->count++;
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the minimum value in the current window
 *
 * O(1) -- the front of the min-deque always holds the sequence
 * number of the current minimum.
 */
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

    /* Look up the sample at the sequence number stored at the
     * front of the min-deque */
    front_seq = m->min_dq[m->min_front];
    *result = m->buf[front_seq % m->capacity];
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the maximum value in the current window
 *
 * O(1) -- the front of the max-deque always holds the sequence
 * number of the current maximum.
 */
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

    /* Look up the sample at the sequence number stored at the
     * front of the max-deque */
    front_seq = m->max_dq[m->max_front];
    *result = m->buf[front_seq % m->capacity];
    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of samples in the window
 *
 * Safe to call with a NULL pointer -- returns 0.
 */
uint16_t tiku_kits_statistics_minmax_count(
    const struct tiku_kits_statistics_minmax *m)
{
    if (m == NULL) {
        return 0;
    }
    return m->count;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check if the window is full (count >= capacity)
 *
 * Safe to call with a NULL pointer -- returns 0 (not full).
 */
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

/**
 * @brief Initialize an EWMA tracker
 *
 * Stores the smoothing parameters and clears the ready flag.  The
 * first push() will seed the EWMA directly from the sample.
 */
int tiku_kits_statistics_ewma_init(
    struct tiku_kits_statistics_ewma *e,
    uint16_t alpha, uint8_t shift)
{
    if (e == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }
    /* Shift > 16 would make the intermediate product unreliable
     * even in int64_t, so reject it. */
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

/**
 * @brief Reset an EWMA tracker, clearing state
 *
 * Clears value and ready flag while preserving alpha and shift.
 */
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

/**
 * @brief Push a new sample into the EWMA tracker
 *
 * First sample seeds the EWMA directly (no lag).  Subsequent
 * samples apply: ewma += alpha * (value - ewma) >> shift.
 * The diff and product are computed in int64_t to prevent overflow.
 */
int tiku_kits_statistics_ewma_push(
    struct tiku_kits_statistics_ewma *e,
    tiku_kits_statistics_elem_t value)
{
    int64_t diff;

    if (e == NULL) {
        return TIKU_KITS_MATHS_ERR_NULL;
    }

    if (!e->ready) {
        /* Seed directly from the first sample -- no startup transient */
        e->value = value;
        e->ready = 1;
    } else {
        /* Widen to int64_t so alpha * diff does not overflow */
        diff = (int64_t)value - (int64_t)e->value;
        e->value += (tiku_kits_statistics_elem_t)
            (((int64_t)e->alpha * diff) >> e->shift);
    }

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current EWMA value
 *
 * O(1) -- returns the most recent smoothed value.  Fails if no
 * samples have been pushed (ready flag not set).
 */
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

/**
 * @brief Initialize an energy/RMS tracker to an empty state
 *
 * Zeros mean_sq and count.  No buffer needed -- the energy tracker
 * computes cumulative statistics incrementally.
 */
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

/**
 * @brief Reset an energy tracker, clearing all samples
 *
 * Equivalent to a fresh init -- mean_sq and count are zeroed.
 */
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

/**
 * @brief Push a new sample into the energy tracker
 *
 * Computes x^2 in int64_t (safe for the full int32_t range) and
 * updates the running mean incrementally:
 *   mean_sq += (x^2 - mean_sq) / n
 * This keeps mean_sq bounded and avoids accumulator overflow.
 */
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
    /* Widen to int64_t before squaring to avoid overflow */
    sq = (int64_t)value * (int64_t)value;
    /* Incremental mean update -- same idea as Welford for x^2 */
    delta = sq - e->mean_sq;
    e->mean_sq += delta / (int64_t)e->n;

    return TIKU_KITS_MATHS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Get the running mean of x^2 (signal power)
 *
 * O(1) -- truncates the int64_t mean_sq to the element type.
 */
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

/**
 * @brief Get the running RMS (root mean square)
 *
 * Applies the 64-bit integer square root to mean_sq and truncates
 * the result to the element type.
 */
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

/**
 * @brief Return the number of samples pushed
 *
 * Safe to call with a NULL pointer -- returns 0.
 */
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

/**
 * @brief Integer square root (floor) for element-width values
 *
 * Non-restoring bit-shift algorithm.  No multiplication -- only
 * shifts, adds, and comparisons.  The starting bit position is
 * derived from the element type's width so the same code works for
 * int16_t and int32_t elements.  Runs in O(b/2) iterations where
 * b is the bit width.
 */
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

    /* Start with the highest power-of-4 that fits in the element type.
     * The mask ~1u ensures the shift count is even. */
    bit = (uint32_t)1
        << ((sizeof(tiku_kits_statistics_elem_t) * 8 - 2)
            & ~(uint32_t)1);
    /* Skip leading zero pairs to reduce iterations */
    while (bit > val) {
        bit >>= 2;
    }

    /* Non-restoring bit-shift square root: each iteration refines
     * one bit of the result */
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
