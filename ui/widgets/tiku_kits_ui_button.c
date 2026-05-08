/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_button.c - BeOS-styled button implementation
 *
 * Visual recipe:
 *   - 1-px outer border (BLACK normally, RED when focused)
 *   - 1-px white highlight on top and left edges (just inside
 *     the outer border) -> outset bevel
 *   - 1-px black shadow on bottom and right edges (just inside)
 *   - Centered text label
 *
 * The bevel works on a 2-colour background (everything else is
 * white) because the highlight is drawn explicitly even though it
 * matches the background; on a coloured background it would still
 * read as a bevel because the contrast against the shadow side
 * defines the 3-D illusion.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_button.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

static void
button_render(const tiku_kits_ui_widget_t *base,
               const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_button_t *btn = (const tiku_kits_ui_button_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    const uint8_t border = base->focused ? t->color_focus : t->color_fg;
    const int16_t  x = base->x;
    const int16_t  y = base->y;
    const uint16_t w = base->w;
    const uint16_t h = base->h;

    /* --- Outer hairline border. --- */
    tiku_kits_gfx_rect(s, x, y, w, h, border);

    /* --- Optional outset bevel inside the border --- */
    if (t->flags & TIKU_KITS_UI_THEME_FLAG_BEVEL_BUTTONS) {
        /* Top/left highlight in bg colour, bottom/right shadow in fg. */
        tiku_kits_gfx_hline(s, (int16_t)(x + 1), (int16_t)(y + 1),
                            (uint16_t)(w - 2), t->color_bg);
        tiku_kits_gfx_vline(s, (int16_t)(x + 1), (int16_t)(y + 1),
                            (uint16_t)(h - 2), t->color_bg);
        tiku_kits_gfx_hline(s, (int16_t)(x + 1), (int16_t)(y + h - 2),
                            (uint16_t)(w - 2), t->color_fg);
        tiku_kits_gfx_vline(s, (int16_t)(x + w - 2), (int16_t)(y + 1),
                            (uint16_t)(h - 2), t->color_fg);
    }

    /* --- Optional dotted-outline focus indicator --- */
    if (base->focused
        && (t->flags & TIKU_KITS_UI_THEME_FLAG_DOTTED_FOCUS)) {
        /* Draw a 1-px dotted outline 1 pixel inside the border. */
        int16_t i;
        for (i = (int16_t)(x + 2); i < (int16_t)(x + w - 2); i += 2) {
            tiku_kits_gfx_pixel(s, i, (int16_t)(y + 2), t->color_focus);
            tiku_kits_gfx_pixel(s, i, (int16_t)(y + h - 3), t->color_focus);
        }
        for (i = (int16_t)(y + 2); i < (int16_t)(y + h - 2); i += 2) {
            tiku_kits_gfx_pixel(s, (int16_t)(x + 2), i, t->color_focus);
            tiku_kits_gfx_pixel(s, (int16_t)(x + w - 3), i, t->color_focus);
        }
    }

    /* --- Centered label --- */
    {
        tiku_kits_gfx_rect_t r = { x, y, w, h };
        uint16_t glyph_h = (uint16_t)(btn->font->height * btn->scale);
        r.y = (int16_t)(y + (int16_t)((h > glyph_h) ? (h - glyph_h) / 2 : 0));
        r.h = glyph_h;
        tiku_kits_gfx_draw_string_in_rect(s, &r, btn->text,
            btn->font, t->color_fg, btn->scale,
            TIKU_KITS_GFX_ALIGN_CENTER);
    }
}

static int
button_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    const tiku_kits_ui_button_t *btn = (const tiku_kits_ui_button_t *)base;
    if (evt != TIKU_KITS_UI_EVT_ACTIVATE) return 0;
    if (btn->on_click != NULL) {
        btn->on_click(base->user_data);
        return 1;
    }
    return 0;
}

static int
button_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_button_ops = {
    .render        = button_render,
    .handle_event  = button_handle_event,
    .is_focusable  = button_is_focusable,
};

void
tiku_kits_ui_button_init(tiku_kits_ui_button_t *btn,
                          int16_t x, int16_t y,
                          uint16_t w, uint16_t h,
                          const char *text,
                          const tiku_kits_gfx_font_t *font,
                          uint8_t scale,
                          tiku_kits_ui_button_cb_t on_click,
                          void *user_data)
{
    if (btn == NULL) return;
    btn->base.ops      = &tiku_kits_ui_button_ops;
    btn->base.x        = x;
    btn->base.y        = y;
    btn->base.w        = w;
    btn->base.h        = h;
    btn->base.visible  = 1;
    btn->base.focused  = 0;
    btn->base.dirty  = 0;
    btn->base.user_data = user_data;
    btn->text          = text;
    btn->font          = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    btn->scale         = (scale > 0) ? scale : 1;
    btn->on_click      = on_click;
}
