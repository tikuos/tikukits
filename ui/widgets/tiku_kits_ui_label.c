/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_label.c - Static text label implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_label.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

static void
label_render(const tiku_kits_ui_widget_t *base,
              const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_label_t *lbl = (const tiku_kits_ui_label_t *)base;
    tiku_kits_gfx_rect_t r = { base->x, base->y, base->w, base->h };
    tiku_kits_gfx_draw_string_in_rect(s, &r,
        lbl->text, lbl->font, lbl->color, lbl->scale, lbl->align);
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_label_ops = {
    .render        = label_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,    /* labels never take focus */
};

void
tiku_kits_ui_label_init(tiku_kits_ui_label_t *lbl,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h,
                         const char *text,
                         const tiku_kits_gfx_font_t *font,
                         uint8_t color, uint8_t scale,
                         tiku_kits_gfx_align_t align)
{
    if (lbl == NULL) return;
    lbl->base.ops      = &tiku_kits_ui_label_ops;
    lbl->base.x        = x;
    lbl->base.y        = y;
    lbl->base.w        = w;
    lbl->base.h        = h;
    lbl->base.visible  = 1;
    lbl->base.focused  = 0;
    lbl->base.dirty  = 0;
    lbl->base.user_data = NULL;
    lbl->text          = text;
    lbl->font          = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    lbl->color         = color;
    lbl->scale         = (scale > 0) ? scale : 1;
    lbl->align         = align;
}
