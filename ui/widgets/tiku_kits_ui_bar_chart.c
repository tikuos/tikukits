/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_bar_chart.c - Bar chart impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_bar_chart.h"
#include "../tiku_kits_ui_theme.h"

static void
bar_chart_render(const tiku_kits_ui_widget_t *base,
                  const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_bar_chart_t *bc =
        (const tiku_kits_ui_bar_chart_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    int16_t  effective_max;
    uint16_t bar_w;
    uint16_t i;
    uint32_t span;
    uint8_t  gap;

    if (bc->values == NULL || bc->n_bars == 0u ||
        base->w < 2u || base->h < 2u) {
        return;
    }

    /* Compute effective max. */
    if (bc->max > 0) {
        effective_max = bc->max;
    } else {
        int16_t hi = 0;
        for (i = 0; i < bc->n_bars; i++) {
            if (bc->values[i] > hi) hi = bc->values[i];
        }
        effective_max = (hi > 0) ? hi : 1;
    }

    /* Baseline. */
    tiku_kits_gfx_hline(s, base->x, (int16_t)(base->y + base->h - 1),
        base->w, t->color_muted);

    /* Cells share the width plus one trailing gap, so bar widths differ by at
     * most a pixel and the last bar ends at the edge.  A gap as wide as a cell
     * is dropped; more bars than pixels leaves some cells empty. */
    gap  = bc->gap;
    span = (uint32_t)base->w + gap;
    if (span / bc->n_bars <= gap) {
        gap  = 0u;
        span = base->w;
    }

    for (i = 0; i < bc->n_bars; i++) {
        int16_t  v = bc->values[i];
        int16_t  bar_x;
        int16_t  bar_y;
        uint16_t bar_h;
        uint32_t scaled;
        uint16_t left = (uint16_t)((uint32_t)i * span / bc->n_bars);
        uint16_t right = (uint16_t)(((uint32_t)i + 1u) * span / bc->n_bars
                                    - gap);

        if (right <= left) continue;
        bar_w = (uint16_t)(right - left);

        if (v < 0) v = 0;
        if (v > effective_max) v = effective_max;

        scaled = (uint32_t)v * (uint32_t)(base->h - 1u) / effective_max;
        bar_h  = (uint16_t)scaled;
        bar_x  = (int16_t)(base->x + left);
        bar_y  = (int16_t)(base->y + base->h - 1u - bar_h);

        if (bar_h > 0u) {
            tiku_kits_gfx_fill_rect(s, bar_x, bar_y, bar_w, bar_h,
                t->color_accent);
        }
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_bar_chart_ops = {
    .render        = bar_chart_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_bar_chart_init(tiku_kits_ui_bar_chart_t *bc,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             const int16_t *values,
                             uint16_t n_bars,
                             int16_t max)
{
    if (bc == NULL) return;
    bc->base.ops      = &tiku_kits_ui_bar_chart_ops;
    bc->base.x        = x;
    bc->base.y        = y;
    bc->base.w        = w;
    bc->base.h        = h;
    bc->base.visible  = 1;
    bc->base.focused  = 0;
    bc->base.dirty  = 0;
    bc->base.user_data = NULL;
    bc->values        = values;
    bc->n_bars        = n_bars;
    bc->max           = max;
    bc->gap           = 1;
}

void
tiku_kits_ui_bar_chart_set_data(tiku_kits_ui_bar_chart_t *bc,
                                 const int16_t *values,
                                 uint16_t n_bars)
{
    if (bc == NULL) return;
    bc->values = values;
    bc->n_bars = n_bars;
}
