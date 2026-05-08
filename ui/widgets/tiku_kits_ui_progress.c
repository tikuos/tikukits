/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_progress.c - Determinate progress bar impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_progress.h"
#include "../tiku_kits_ui_theme.h"

static void
progress_render(const tiku_kits_ui_widget_t *base,
                 const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_progress_t *p = (const tiku_kits_ui_progress_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    int32_t  filled;
    uint16_t fill_w;

    /* Outline. */
    tiku_kits_gfx_rect(s, base->x, base->y, base->w, base->h, t->color_fg);

    if (p->max <= 0 || p->value <= 0 || base->w < 4u) return;

    filled = (int32_t)p->value * (int32_t)(base->w - 2u);
    if (p->value >= p->max) {
        fill_w = (uint16_t)(base->w - 2u);
    } else {
        fill_w = (uint16_t)(filled / p->max);
    }
    if (fill_w == 0u) return;

    tiku_kits_gfx_fill_rect(s,
        (int16_t)(base->x + 1), (int16_t)(base->y + 1),
        fill_w, (uint16_t)(base->h - 2u),
        t->color_accent);
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_progress_ops = {
    .render        = progress_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_progress_init(tiku_kits_ui_progress_t *p,
                            int16_t x, int16_t y,
                            uint16_t w, uint16_t h,
                            int16_t value, int16_t max)
{
    if (p == NULL) return;
    p->base.ops      = &tiku_kits_ui_progress_ops;
    p->base.x        = x;
    p->base.y        = y;
    p->base.w        = w;
    p->base.h        = h;
    p->base.visible  = 1;
    p->base.focused  = 0;
    p->base.dirty  = 0;
    p->base.user_data = NULL;
    p->value = value;
    p->max   = (max > 0) ? max : 100;
}

void
tiku_kits_ui_progress_set(tiku_kits_ui_progress_t *p, int16_t value)
{
    if (p == NULL) return;
    if (value < 0) value = 0;
    if (value > p->max) value = p->max;
    p->value = value;
}
