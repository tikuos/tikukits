/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_text.c - Multi-line wrapped text widget implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_text.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

static void
text_render(const tiku_kits_ui_widget_t *base,
             const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_text_t *txt = (const tiku_kits_ui_text_t *)base;
    tiku_kits_gfx_rect_t r = { base->x, base->y, base->w, base->h };
    tiku_kits_gfx_draw_text_wrapped(s, &r,
        txt->text, txt->font, txt->color, txt->scale, txt->align);
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_text_ops = {
    .render        = text_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_text_init(tiku_kits_ui_text_t *txt,
                        int16_t x, int16_t y,
                        uint16_t w, uint16_t h,
                        const char *text,
                        const tiku_kits_gfx_font_t *font,
                        uint8_t color, uint8_t scale,
                        tiku_kits_gfx_align_t align)
{
    if (txt == NULL) return;
    txt->base.ops      = &tiku_kits_ui_text_ops;
    txt->base.x        = x;
    txt->base.y        = y;
    txt->base.w        = w;
    txt->base.h        = h;
    txt->base.visible  = 1;
    txt->base.focused  = 0;
    txt->base.dirty  = 0;
    txt->base.user_data = NULL;
    txt->text          = text;
    txt->font          = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    txt->color         = color;
    txt->scale         = (scale > 0) ? scale : 1;
    txt->align         = align;
}
