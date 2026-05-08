/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_toggle.c - Toggle / switch widget impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_toggle.h"
#include "../tiku_kits_ui_theme.h"

static void
toggle_render(const tiku_kits_ui_widget_t *base,
               const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_toggle_t *tg = (const tiku_kits_ui_toggle_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    const int16_t  x = base->x;
    const int16_t  y = base->y;
    const uint16_t w = base->w;
    const uint16_t h = base->h;
    uint16_t r;
    uint8_t  edge = base->focused ? t->color_focus : t->color_fg;
    uint16_t knob_r;
    int16_t  knob_cx, knob_cy;

    if (h == 0u || w < h) return;

    r = (uint16_t)(h / 2u);

    /* Track: filled when ON (accent), unfilled when OFF (bg). */
    if (tg->on) {
        tiku_kits_gfx_fill_round_rect(s, x, y, w, h, r, t->color_accent);
    } else {
        tiku_kits_gfx_fill_round_rect(s, x, y, w, h, r, t->color_bg);
    }
    /* Track outline. */
    tiku_kits_gfx_round_rect(s, x, y, w, h, r, edge);

    /* Knob: filled circle at the appropriate end. */
    knob_r  = (uint16_t)((h > 4u) ? (h / 2u - 2u) : 1u);
    knob_cy = (int16_t)(y + h / 2);
    knob_cx = tg->on
                ? (int16_t)(x + w - 1 - r)
                : (int16_t)(x + r);

    tiku_kits_gfx_fill_circle(s, knob_cx, knob_cy, knob_r,
        tg->on ? t->color_bg : t->color_fg);
}

static int
toggle_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_toggle_t *tg = (tiku_kits_ui_toggle_t *)base;
    if (evt != TIKU_KITS_UI_EVT_ACTIVATE) return 0;
    tg->on = (uint8_t)(tg->on ? 0 : 1);
    if (tg->on_change != NULL) {
        tg->on_change((int)tg->on, base->user_data);
    }
    return 1;
}

static int
toggle_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_toggle_ops = {
    .render        = toggle_render,
    .handle_event  = toggle_handle_event,
    .is_focusable  = toggle_is_focusable,
};

void
tiku_kits_ui_toggle_init(tiku_kits_ui_toggle_t *tg,
                          int16_t x, int16_t y,
                          uint16_t w, uint16_t h,
                          uint8_t on,
                          tiku_kits_ui_toggle_cb_t on_change,
                          void *user_data)
{
    if (tg == NULL) return;
    tg->base.ops      = &tiku_kits_ui_toggle_ops;
    tg->base.x        = x;
    tg->base.y        = y;
    tg->base.w        = w;
    tg->base.h        = h;
    tg->base.visible  = 1;
    tg->base.focused  = 0;
    tg->base.dirty  = 0;
    tg->base.user_data = user_data;
    tg->on            = (uint8_t)(on ? 1 : 0);
    tg->on_change     = on_change;
}

void
tiku_kits_ui_toggle_set(tiku_kits_ui_toggle_t *tg, uint8_t on)
{
    if (tg == NULL) return;
    tg->on = (uint8_t)(on ? 1 : 0);
}
