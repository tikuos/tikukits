/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_sparkline.c - Sparkline impl
 *
 * Walk consecutive samples; map value -> y, index -> x; draw line
 * segment to the previous sample's pixel. Auto-scales when init'd
 * with min == max.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_sparkline.h"
#include "../tiku_kits_ui_theme.h"

static void
compute_range(const tiku_kits_ui_sparkline_t *sp,
               int16_t *out_min, int16_t *out_max)
{
    if (sp->min != sp->max) {
        *out_min = sp->min;
        *out_max = sp->max;
        return;
    }
    if (sp->n_samples == 0u) {
        *out_min = 0; *out_max = 1;
        return;
    }
    {
        int16_t lo = sp->samples[0];
        int16_t hi = sp->samples[0];
        uint16_t i;
        for (i = 1; i < sp->n_samples; i++) {
            int16_t v = sp->samples[i];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        if (lo == hi) hi = (int16_t)(lo + 1);
        *out_min = lo;
        *out_max = hi;
    }
}

static int16_t
sample_to_y(int16_t v, int16_t lo, int16_t hi,
             int16_t y0, uint16_t h)
{
    int32_t denom = (int32_t)hi - (int32_t)lo;
    int32_t t;
    if (h < 2u || denom == 0) return y0;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    /* Higher value -> closer to top. */
    t = ((int32_t)hi - (int32_t)v) * (int32_t)(h - 1u) / denom;
    return (int16_t)(y0 + t);
}

static void
sparkline_render(const tiku_kits_ui_widget_t *base,
                  const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_sparkline_t *sp =
        (const tiku_kits_ui_sparkline_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    int16_t lo, hi;
    uint16_t i;
    int16_t prev_x = 0, prev_y = 0;

    if (sp->n_samples == 0u || base->w < 2u || base->h < 2u) return;

    compute_range(sp, &lo, &hi);

    if (sp->show_baseline) {
        tiku_kits_gfx_hline(s, base->x, (int16_t)(base->y + base->h - 1),
            base->w, t->color_muted);
    }

    for (i = 0; i < sp->n_samples; i++) {
        int32_t  x = (sp->n_samples > 1u)
                       ? ((int32_t)i * (int32_t)(base->w - 1u)
                            / (int32_t)(sp->n_samples - 1u))
                       : 0;
        int16_t  px = (int16_t)(base->x + x);
        int16_t  py = sample_to_y(sp->samples[i], lo, hi,
                                   base->y, base->h);
        if (i == 0u) {
            tiku_kits_gfx_pixel(s, px, py, t->color_fg);
        } else {
            tiku_kits_gfx_line(s, prev_x, prev_y, px, py, t->color_fg);
        }
        prev_x = px;
        prev_y = py;
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_sparkline_ops = {
    .render        = sparkline_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_sparkline_init(tiku_kits_ui_sparkline_t *sp,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             const int16_t *samples,
                             uint16_t n_samples,
                             int16_t min, int16_t max)
{
    if (sp == NULL) return;
    sp->base.ops      = &tiku_kits_ui_sparkline_ops;
    sp->base.x        = x;
    sp->base.y        = y;
    sp->base.w        = w;
    sp->base.h        = h;
    sp->base.visible  = 1;
    sp->base.focused  = 0;
    sp->base.dirty  = 0;
    sp->base.user_data = NULL;
    sp->samples       = samples;
    sp->n_samples     = n_samples;
    sp->min           = min;
    sp->max           = max;
    sp->show_baseline = 0;
}

void
tiku_kits_ui_sparkline_set_data(tiku_kits_ui_sparkline_t *sp,
                                 const int16_t *samples,
                                 uint16_t n_samples)
{
    if (sp == NULL) return;
    sp->samples   = samples;
    sp->n_samples = n_samples;
}
