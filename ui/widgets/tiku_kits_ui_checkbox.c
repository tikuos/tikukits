/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_checkbox.c - Checkbox widget impl
 *
 * Visual:
 *   [#]  Label text
 * The box is a (font_height + 2)-pixel square at the left, with a
 * 1-px border (focus colour when focused). When checked, a thick
 * tick is drawn inside.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_checkbox.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static uint16_t
box_size(const tiku_kits_ui_checkbox_t *cb)
{
    /* Size of the indicator square, capped at the widget height. */
    uint16_t s = (uint16_t)(cb->font->height * cb->scale + 4u);
    if (s > cb->base.h) s = cb->base.h;
    return s;
}

static void
draw_check(const tiku_kits_gfx_surface_t *s,
            int16_t x, int16_t y, uint16_t side, uint8_t color)
{
    /* Diagonal tick: lower-left -> middle-bottom, then up to upper-right. */
    int16_t mid_x = (int16_t)(x + side / 3);
    int16_t mid_y = (int16_t)(y + side - side / 3);
    tiku_kits_gfx_line_thick(s,
        (int16_t)(x + 2), (int16_t)(y + side / 2),
        mid_x, mid_y,
        2, color);
    tiku_kits_gfx_line_thick(s,
        mid_x, mid_y,
        (int16_t)(x + side - 2), (int16_t)(y + 2),
        2, color);
}

static void
checkbox_render(const tiku_kits_ui_widget_t *base,
                 const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_checkbox_t *cb = (const tiku_kits_ui_checkbox_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint16_t bs   = box_size(cb);
    int16_t  bx   = base->x;
    int16_t  by   = (int16_t)(base->y + (int16_t)((base->h > bs)
                                                    ? (base->h - bs) / 2 : 0));
    uint8_t  edge = base->focused ? t->color_focus : t->color_fg;

    /* Box outline. */
    tiku_kits_gfx_rect(s, bx, by, bs, bs, edge);

    /* Tick mark when checked. */
    if (cb->checked) {
        draw_check(s, bx, by, bs, t->color_fg);
    }

    /* Label, if any. */
    if (cb->label != NULL && cb->label[0] != '\0') {
        tiku_kits_gfx_rect_t r;
        uint16_t glyph_h = (uint16_t)(cb->font->height * cb->scale);
        r.x = (int16_t)(bx + (int16_t)bs + 4);
        r.y = (int16_t)(base->y +
                        (int16_t)((base->h > glyph_h)
                                    ? (base->h - glyph_h) / 2 : 0));
        r.w = (uint16_t)(base->w - bs - 4u);
        r.h = glyph_h;
        tiku_kits_gfx_draw_string_in_rect(s, &r, cb->label,
            cb->font, t->color_fg, cb->scale,
            TIKU_KITS_GFX_ALIGN_LEFT);
    }
}

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

static int
checkbox_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_checkbox_t *cb = (tiku_kits_ui_checkbox_t *)base;
    if (evt != TIKU_KITS_UI_EVT_ACTIVATE) return 0;
    cb->checked = (uint8_t)(cb->checked ? 0 : 1);
    if (cb->on_change != NULL) {
        cb->on_change((int)cb->checked, base->user_data);
    }
    return 1;
}

static int
checkbox_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_checkbox_ops = {
    .render        = checkbox_render,
    .handle_event  = checkbox_handle_event,
    .is_focusable  = checkbox_is_focusable,
};

/*---------------------------------------------------------------------------*/
/* INIT + STATE                                                              */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_checkbox_init(tiku_kits_ui_checkbox_t *cb,
                            int16_t x, int16_t y,
                            uint16_t w, uint16_t h,
                            const char *label,
                            const tiku_kits_gfx_font_t *font,
                            uint8_t scale,
                            uint8_t checked,
                            tiku_kits_ui_checkbox_cb_t on_change,
                            void *user_data)
{
    if (cb == NULL) return;
    cb->base.ops      = &tiku_kits_ui_checkbox_ops;
    cb->base.x        = x;
    cb->base.y        = y;
    cb->base.w        = w;
    cb->base.h        = h;
    cb->base.visible  = 1;
    cb->base.focused  = 0;
    cb->base.dirty  = 0;
    cb->base.user_data = user_data;
    cb->label         = label;
    cb->font          = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    cb->scale         = (scale > 0) ? scale : 1;
    cb->checked       = (uint8_t)(checked ? 1 : 0);
    cb->on_change     = on_change;
}

void
tiku_kits_ui_checkbox_set_checked(tiku_kits_ui_checkbox_t *cb,
                                   uint8_t checked)
{
    if (cb == NULL) return;
    cb->checked = (uint8_t)(checked ? 1 : 0);
}
