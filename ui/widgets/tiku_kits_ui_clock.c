/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_clock.c - Digital clock impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_clock.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_7seg.h>
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

/* Format `n` (0..99) as two digits into out[0..1]; out[2] = '\0'. */
static void
two_digits(char *out, uint8_t n)
{
    out[0] = (char)('0' + ((n / 10u) % 10u));
    out[1] = (char)('0' + (n % 10u));
    out[2] = '\0';
}

static void
clock_render(const tiku_kits_ui_widget_t *base,
              const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_clock_t *clk = (const tiku_kits_ui_clock_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    const tiku_kits_gfx_font_t *fnt = clk->font;
    char buf[12];
    char hh[3], mm[3], ss[3];

    two_digits(hh, clk->hours);
    two_digits(mm, clk->minutes);
    two_digits(ss, clk->seconds);

    if (clk->format == TIKU_KITS_UI_CLOCK_HMS) {
        buf[0] = hh[0]; buf[1] = hh[1];
        buf[2] = ':';
        buf[3] = mm[0]; buf[4] = mm[1];
        buf[5] = ':';
        buf[6] = ss[0]; buf[7] = ss[1];
        buf[8] = '\0';
    } else {
        buf[0] = hh[0]; buf[1] = hh[1];
        buf[2] = ':';
        buf[3] = mm[0]; buf[4] = mm[1];
        buf[5] = '\0';
    }

    {
        tiku_kits_gfx_rect_t r;
        uint16_t glyph_h = (uint16_t)(fnt->height * clk->scale);
        r.x = base->x;
        r.y = (int16_t)(base->y +
                        (int16_t)((base->h > glyph_h)
                                    ? (base->h - glyph_h) / 2 : 0));
        r.w = base->w;
        r.h = glyph_h;
        tiku_kits_gfx_draw_string_in_rect(s, &r, buf,
            fnt, t->color_fg, clk->scale,
            TIKU_KITS_GFX_ALIGN_CENTER);
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_clock_ops = {
    .render        = clock_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_clock_init(tiku_kits_ui_clock_t *clk,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h,
                         tiku_kits_ui_clock_format_t format,
                         const tiku_kits_gfx_font_t *font,
                         uint8_t scale)
{
    if (clk == NULL) return;
    clk->base.ops      = &tiku_kits_ui_clock_ops;
    clk->base.x        = x;
    clk->base.y        = y;
    clk->base.w        = w;
    clk->base.h        = h;
    clk->base.visible  = 1;
    clk->base.focused  = 0;
    clk->base.dirty  = 0;
    clk->base.user_data = NULL;
    clk->hours   = 0;
    clk->minutes = 0;
    clk->seconds = 0;
    clk->format  = format;
    clk->font    = (font != NULL) ? font : &tiku_kits_gfx_font_7seg;
    clk->scale   = (scale > 0) ? scale : 1;
}

void
tiku_kits_ui_clock_set_time(tiku_kits_ui_clock_t *clk,
                             uint8_t h, uint8_t m, uint8_t s)
{
    if (clk == NULL) return;
    clk->hours   = h;
    clk->minutes = m;
    clk->seconds = s;
}
