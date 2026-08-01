/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_chart.c - 2D chart widget impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Layout: an inner plot area inside the widget rect, leaving room
 * on the bottom + left for axes when AXES is enabled. Points are
 * mapped (x_min..x_max, y_min..y_max) -> plot rect.
 *
 * Auto-scaling: if min == max for an axis, walk the points to
 * find the data range and add a 1-unit pad so flat data still
 * shows up.
 */

#include "tiku_kits_ui_chart.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/tiku_kits_gfx_path.h>

/*---------------------------------------------------------------------------*/
/* HELPERS                                                                   */
/*---------------------------------------------------------------------------*/

static int16_t
ix_value(const tiku_kits_ui_chart_t *c, uint16_t i)
{
    if (c->x_values != NULL) return c->x_values[i];
    return (int16_t)i;
}

static void
compute_x_range(const tiku_kits_ui_chart_t *c,
                 int16_t *out_lo, int16_t *out_hi)
{
    uint16_t i;
    int16_t lo, hi;
    if (c->x_min != c->x_max) { *out_lo = c->x_min; *out_hi = c->x_max; return; }
    if (c->n_points == 0u)    { *out_lo = 0; *out_hi = 1; return; }
    lo = hi = ix_value(c, 0);
    for (i = 1; i < c->n_points; i++) {
        int16_t v = ix_value(c, i);
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    if (lo == hi) hi = (int16_t)(lo + 1);
    *out_lo = lo;
    *out_hi = hi;
}

static void
compute_y_range(const tiku_kits_ui_chart_t *c,
                 int16_t *out_lo, int16_t *out_hi)
{
    uint16_t i;
    int16_t lo, hi;
    if (c->y_min != c->y_max) { *out_lo = c->y_min; *out_hi = c->y_max; return; }
    if (c->n_points == 0u)    { *out_lo = 0; *out_hi = 1; return; }
    lo = hi = c->y_values[0];
    for (i = 1; i < c->n_points; i++) {
        int16_t v = c->y_values[i];
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    if (lo == hi) hi = (int16_t)(lo + 1);
    *out_lo = lo;
    *out_hi = hi;
}

/* Map (x, y) data point -> screen pixel inside (px, py, pw, ph) plot area. */
static void
map_point(int16_t dx, int16_t dy,
           int16_t x_lo, int16_t x_hi,
           int16_t y_lo, int16_t y_hi,
           int16_t px, int16_t py,
           uint16_t pw, uint16_t ph,
           int16_t *out_x, int16_t *out_y)
{
    int32_t xn, yn;
    if (dx < x_lo) dx = x_lo;
    if (dx > x_hi) dx = x_hi;
    if (dy < y_lo) dy = y_lo;
    if (dy > y_hi) dy = y_hi;
    xn = ((int32_t)(dx - x_lo) * (int32_t)(pw - 1u))
         / ((int32_t)(x_hi - x_lo));
    /* Flip y so larger values are higher on screen. */
    yn = ((int32_t)(y_hi - dy) * (int32_t)(ph - 1u))
         / ((int32_t)(y_hi - y_lo));
    *out_x = (int16_t)(px + xn);
    *out_y = (int16_t)(py + yn);
}

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static void
chart_render(const tiku_kits_ui_widget_t *base,
              const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_chart_t *c = (const tiku_kits_ui_chart_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint16_t margin_l = (c->flags & TIKU_KITS_UI_CHART_FLAG_AXES) ? 2u : 0u;
    uint16_t margin_b = (c->flags & TIKU_KITS_UI_CHART_FLAG_AXES) ? 2u : 0u;
    int16_t  px = (int16_t)(base->x + (int16_t)margin_l);
    int16_t  py = base->y;
    uint16_t pw = (uint16_t)(base->w - margin_l);
    uint16_t ph = (uint16_t)(base->h - margin_b);
    int16_t  x_lo, x_hi, y_lo, y_hi;
    uint16_t i;
    int16_t  prev_x = 0, prev_y = 0;

    if (base->w < 4u || base->h < 4u) return;
    if (c->n_points == 0u) return;

    compute_x_range(c, &x_lo, &x_hi);
    compute_y_range(c, &y_lo, &y_hi);

    /* --- Optional gridlines --- */
    if (c->flags & TIKU_KITS_UI_CHART_FLAG_GRID) {
        uint8_t xd = (c->grid_x_div > 0) ? c->grid_x_div : 4u;
        uint8_t yd = (c->grid_y_div > 0) ? c->grid_y_div : 4u;
        uint8_t k;
        for (k = 1; k < xd; k++) {
            int16_t gx = (int16_t)(px + (int32_t)k * (pw - 1u) / xd);
            tiku_kits_gfx_line_dashed(s, gx, py, gx,
                (int16_t)(py + ph - 1), 0x5555u, t->color_muted);
        }
        for (k = 1; k < yd; k++) {
            int16_t gy = (int16_t)(py + (int32_t)k * (ph - 1u) / yd);
            tiku_kits_gfx_line_dashed(s, px, gy,
                (int16_t)(px + pw - 1), gy, 0x5555u, t->color_muted);
        }
    }

    /* --- Optional baseline (y == 0 in data space) --- */
    if (c->flags & TIKU_KITS_UI_CHART_FLAG_BASELINE) {
        if (y_lo <= 0 && y_hi >= 0) {
            int16_t bx, by;
            map_point(x_lo, 0, x_lo, x_hi, y_lo, y_hi,
                      px, py, pw, ph, &bx, &by);
            tiku_kits_gfx_hline(s, px, by, pw, t->color_muted);
        }
    }

    /* --- Data points --- */
    for (i = 0; i < c->n_points; i++) {
        int16_t cx, cy;
        map_point(ix_value(c, i), c->y_values[i],
                   x_lo, x_hi, y_lo, y_hi,
                   px, py, pw, ph, &cx, &cy);
        if (c->style & TIKU_KITS_UI_CHART_STYLE_DOTS) {
            tiku_kits_gfx_fill_circle(s, cx, cy, 1u, t->color_fg);
        }
        if (c->style & TIKU_KITS_UI_CHART_STYLE_LINE) {
            if (i == 0u) {
                tiku_kits_gfx_pixel(s, cx, cy, t->color_fg);
            } else {
                tiku_kits_gfx_line(s, prev_x, prev_y, cx, cy, t->color_fg);
            }
        }
        prev_x = cx;
        prev_y = cy;
    }

    /* --- Axes (drawn on top) --- */
    if (c->flags & TIKU_KITS_UI_CHART_FLAG_AXES) {
        tiku_kits_gfx_vline(s, base->x, py, ph, t->color_fg);
        tiku_kits_gfx_hline(s, px, (int16_t)(py + ph), pw, t->color_fg);
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_chart_ops = {
    .render         = chart_render,
    .handle_event   = NULL,
    .is_focusable   = NULL,
    .intrinsic_size = NULL,
};

/*---------------------------------------------------------------------------*/
/* INIT + CONFIG                                                             */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_chart_init(tiku_kits_ui_chart_t *c,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h,
                         const int16_t *y_values,
                         uint16_t n_points)
{
    if (c == NULL) return;
    c->base.ops      = &tiku_kits_ui_chart_ops;
    c->base.x        = x;
    c->base.y        = y;
    c->base.w        = w;
    c->base.h        = h;
    c->base.visible  = 1;
    c->base.focused  = 0;
    c->base.dirty    = 0;
    c->base.user_data = NULL;
    c->x_values  = NULL;
    c->y_values  = y_values;
    c->n_points  = n_points;
    c->x_min = c->x_max = 0;
    c->y_min = c->y_max = 0;
    c->style = TIKU_KITS_UI_CHART_STYLE_LINE;
    c->flags = TIKU_KITS_UI_CHART_FLAG_AXES;
    c->grid_x_div = 0;
    c->grid_y_div = 0;
}

void
tiku_kits_ui_chart_set_x(tiku_kits_ui_chart_t *c, const int16_t *x_values)
{
    if (c == NULL) return;
    c->x_values = x_values;
}

void
tiku_kits_ui_chart_set_y(tiku_kits_ui_chart_t *c,
                          const int16_t *y_values, uint16_t n_points)
{
    if (c == NULL) return;
    c->y_values = y_values;
    c->n_points = n_points;
}

void
tiku_kits_ui_chart_set_x_domain(tiku_kits_ui_chart_t *c,
                                 int16_t min, int16_t max)
{
    if (c == NULL) return;
    c->x_min = min; c->x_max = max;
}

void
tiku_kits_ui_chart_set_y_domain(tiku_kits_ui_chart_t *c,
                                 int16_t min, int16_t max)
{
    if (c == NULL) return;
    c->y_min = min; c->y_max = max;
}

void
tiku_kits_ui_chart_set_style(tiku_kits_ui_chart_t *c, uint8_t style)
{
    if (c == NULL) return;
    c->style = style;
}

void
tiku_kits_ui_chart_set_flags(tiku_kits_ui_chart_t *c, uint8_t flags)
{
    if (c == NULL) return;
    c->flags = flags;
}
