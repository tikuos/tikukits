/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_gauge.c - Circular arc gauge impl
 *
 * Outline = full arc range; filled portion = arc whose end is
 * interpolated between arc_start and arc_start + arc_span.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_gauge.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/tiku_kits_gfx_curve.h>

static void
gauge_render(const tiku_kits_ui_widget_t *base,
              const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_gauge_t *g = (const tiku_kits_ui_gauge_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    int16_t  cx = (int16_t)(base->x + base->w / 2);
    int16_t  cy = (int16_t)(base->y + base->h / 2);
    uint16_t r  = (base->w < base->h) ? (uint16_t)(base->w / 2 - 1)
                                      : (uint16_t)(base->h / 2 - 1);
    int16_t  span = g->arc_span_deg;
    int32_t  range, filled_span;

    if (r == 0) return;

    /* Background arc (full sweep). */
    tiku_kits_gfx_arc(s, cx, cy, r,
        g->arc_start_deg,
        (int16_t)(g->arc_start_deg + span),
        t->color_muted);

    /* Filled foreground arc. */
    range = (int32_t)g->max - (int32_t)g->min;
    if (range > 0 && g->value > g->min) {
        int16_t  v = (g->value > g->max) ? g->max : g->value;
        filled_span = (int32_t)span * ((int32_t)v - g->min) / range;
        if (filled_span > 0) {
            tiku_kits_gfx_arc(s, cx, cy, r,
                g->arc_start_deg,
                (int16_t)(g->arc_start_deg + (int16_t)filled_span),
                t->color_accent);
            /* Thicker stroke: redraw at r-1 too. */
            if (r > 1u) {
                tiku_kits_gfx_arc(s, cx, cy, (uint16_t)(r - 1u),
                    g->arc_start_deg,
                    (int16_t)(g->arc_start_deg + (int16_t)filled_span),
                    t->color_accent);
            }
        }
    }

    /* Centre label. */
    if (g->label != NULL && g->label_font != NULL) {
        tiku_kits_gfx_rect_t lr;
        uint16_t tw = tiku_kits_gfx_text_width(g->label,
                                                g->label_font, g->label_scale);
        uint16_t th = (uint16_t)(g->label_font->height * g->label_scale);
        lr.x = (int16_t)(cx - (int16_t)tw / 2);
        lr.y = (int16_t)(cy - (int16_t)th / 2);
        lr.w = tw;
        lr.h = th;
        tiku_kits_gfx_draw_string_in_rect(s, &lr, g->label,
            g->label_font, t->color_fg, g->label_scale,
            TIKU_KITS_GFX_ALIGN_CENTER);
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_gauge_ops = {
    .render        = gauge_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_gauge_init(tiku_kits_ui_gauge_t *g,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h,
                         int16_t min, int16_t max, int16_t value)
{
    if (g == NULL) return;
    if (max < min) { int16_t tmp = min; min = max; max = tmp; }
    g->base.ops      = &tiku_kits_ui_gauge_ops;
    g->base.x        = x;
    g->base.y        = y;
    g->base.w        = w;
    g->base.h        = h;
    g->base.visible  = 1;
    g->base.focused  = 0;
    g->base.dirty  = 0;
    g->base.user_data = NULL;
    g->min   = min;
    g->max   = max;
    g->value = value;
    /* Default arc: 225 -> 315 going CCW = 270 deg sweep, bottom open. */
    g->arc_start_deg = 225;
    g->arc_span_deg  = 270;
    g->label         = NULL;
    g->label_font    = NULL;
    g->label_scale   = 1;
}

void
tiku_kits_ui_gauge_set_arc(tiku_kits_ui_gauge_t *g,
                            int16_t arc_start_deg,
                            int16_t arc_span_deg)
{
    if (g == NULL) return;
    g->arc_start_deg = arc_start_deg;
    g->arc_span_deg  = arc_span_deg;
}

void
tiku_kits_ui_gauge_set_value(tiku_kits_ui_gauge_t *g, int16_t value)
{
    if (g == NULL) return;
    if (value < g->min) value = g->min;
    if (value > g->max) value = g->max;
    g->value = value;
}

void
tiku_kits_ui_gauge_set_label(tiku_kits_ui_gauge_t *g,
                              const char *label,
                              const tiku_kits_gfx_font_t *font,
                              uint8_t scale)
{
    if (g == NULL) return;
    g->label       = label;
    g->label_font  = font;
    g->label_scale = (scale > 0) ? scale : 1;
}
