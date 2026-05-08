/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_toast.c - Toast banner impl
 *
 * Renders a filled accent-colour pill with the message centred.
 * Each render decrements a frame counter; when it reaches zero
 * the widget marks itself invisible.
 *
 * Note: the cast away from `const` in the render path is
 * deliberate -- the lifetime counter is logically widget state,
 * but the ops vtable signs render() as const for symmetry with
 * widgets that don't mutate. Future revs may move dirty/lifetime
 * state into a side struct.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_toast.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

static void
toast_render(const tiku_kits_ui_widget_t *base,
              const tiku_kits_gfx_surface_t *s)
{
    tiku_kits_ui_toast_t *tt = (tiku_kits_ui_toast_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();

    if (tt->message == NULL) return;
    if (tt->lifetime_renders > 0u && tt->renders_seen >= tt->lifetime_renders) {
        ((tiku_kits_ui_widget_t *)base)->visible = 0;
        return;
    }

    /* Pill background. */
    tiku_kits_gfx_fill_round_rect(s,
        base->x, base->y, base->w, base->h,
        (uint16_t)(base->h / 2u), t->color_fg);

    /* Message. */
    {
        tiku_kits_gfx_rect_t r = {
            base->x, base->y, base->w, base->h
        };
        uint16_t glyph_h = (uint16_t)(tt->font->height * tt->scale);
        r.y = (int16_t)(base->y +
                        (int16_t)((base->h > glyph_h)
                                    ? (base->h - glyph_h) / 2 : 0));
        r.h = glyph_h;
        tiku_kits_gfx_draw_string_in_rect(s, &r, tt->message,
            tt->font, t->color_bg, tt->scale,
            TIKU_KITS_GFX_ALIGN_CENTER);
    }

    tt->renders_seen++;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_toast_ops = {
    .render        = toast_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_toast_init(tiku_kits_ui_toast_t *tt,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h,
                         const tiku_kits_gfx_font_t *font,
                         uint8_t scale)
{
    if (tt == NULL) return;
    tt->base.ops      = &tiku_kits_ui_toast_ops;
    tt->base.x        = x;
    tt->base.y        = y;
    tt->base.w        = w;
    tt->base.h        = h;
    tt->base.visible  = 0;       /* hidden until shown */
    tt->base.focused  = 0;
    tt->base.dirty  = 0;
    tt->base.user_data = NULL;
    tt->message          = NULL;
    tt->font             = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    tt->scale            = (scale > 0) ? scale : 1;
    tt->lifetime_renders = 0;
    tt->renders_seen     = 0;
}

void
tiku_kits_ui_toast_show(tiku_kits_ui_toast_t *tt,
                         const char *message,
                         uint16_t lifetime_renders)
{
    if (tt == NULL) return;
    tt->message          = message;
    tt->lifetime_renders = lifetime_renders;
    tt->renders_seen     = 0;
    tt->base.visible     = 1;
}

void
tiku_kits_ui_toast_hide(tiku_kits_ui_toast_t *tt)
{
    if (tt == NULL) return;
    tt->base.visible = 0;
}
