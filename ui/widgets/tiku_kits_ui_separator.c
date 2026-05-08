/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_separator.c - Separator line impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_separator.h"
#include "../tiku_kits_ui_theme.h"

static void
sep_render(const tiku_kits_ui_widget_t *base,
            const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_separator_t *sep =
        (const tiku_kits_ui_separator_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint8_t th = (sep->thickness > 0) ? sep->thickness : t->line_w;
    if (th == 0) th = 1;

    if (sep->dir == TIKU_KITS_UI_SEP_HORIZONTAL) {
        int16_t y = (int16_t)(base->y + (base->h > th ? (base->h - th) / 2 : 0));
        tiku_kits_gfx_fill_rect(s, base->x, y, base->w, th, t->color_muted);
    } else {
        int16_t x = (int16_t)(base->x + (base->w > th ? (base->w - th) / 2 : 0));
        tiku_kits_gfx_fill_rect(s, x, base->y, th, base->h, t->color_muted);
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_separator_ops = {
    .render        = sep_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_separator_init(tiku_kits_ui_separator_t *sep,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             tiku_kits_ui_separator_dir_t dir,
                             uint8_t thickness)
{
    if (sep == NULL) return;
    sep->base.ops      = &tiku_kits_ui_separator_ops;
    sep->base.x        = x;
    sep->base.y        = y;
    sep->base.w        = w;
    sep->base.h        = h;
    sep->base.visible  = 1;
    sep->base.focused  = 0;
    sep->base.dirty  = 0;
    sep->base.user_data = NULL;
    sep->dir           = dir;
    sep->thickness     = thickness;
}
