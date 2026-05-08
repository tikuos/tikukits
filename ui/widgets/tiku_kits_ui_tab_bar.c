/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_tab_bar.c - Tab bar impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_tab_bar.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

static void
fire_change(tiku_kits_ui_tab_bar_t *tb)
{
    if (tb->on_change != NULL) {
        tb->on_change(tb->selected, tb->base.user_data);
    }
}

static void
tab_bar_render(const tiku_kits_ui_widget_t *base,
                const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_tab_bar_t *tb = (const tiku_kits_ui_tab_bar_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint16_t tab_w;
    uint8_t  i;

    if (tb->n_tabs == 0u) return;
    tab_w = (uint16_t)(base->w / tb->n_tabs);

    /* Outline. */
    tiku_kits_gfx_rect(s, base->x, base->y, base->w, base->h,
        base->focused ? t->color_focus : t->color_fg);

    for (i = 0; i < tb->n_tabs; i++) {
        int16_t tx = (int16_t)(base->x + i * tab_w);
        uint16_t tw = (i == tb->n_tabs - 1u)
                        ? (uint16_t)(base->w - i * tab_w)
                        : tab_w;
        if (i > 0) {
            tiku_kits_gfx_vline(s, tx, (int16_t)(base->y + 1),
                (uint16_t)(base->h - 2u), t->color_fg);
        }
        if ((int8_t)i == tb->selected) {
            tiku_kits_gfx_fill_rect(s,
                (int16_t)(tx + 1), (int16_t)(base->y + 1),
                (uint16_t)(tw - 1u), (uint16_t)(base->h - 2u),
                t->color_accent);
            {
                tiku_kits_gfx_rect_t r = {
                    tx, (int16_t)(base->y + 2),
                    tw, (uint16_t)(base->h - 4u)
                };
                tiku_kits_gfx_draw_string_in_rect(s, &r,
                    tb->labels[i], tb->font,
                    t->color_fg, tb->scale,
                    TIKU_KITS_GFX_ALIGN_CENTER);
            }
        } else {
            tiku_kits_gfx_rect_t r = {
                tx, (int16_t)(base->y + 2),
                tw, (uint16_t)(base->h - 4u)
            };
            tiku_kits_gfx_draw_string_in_rect(s, &r,
                tb->labels[i], tb->font,
                t->color_fg, tb->scale,
                TIKU_KITS_GFX_ALIGN_CENTER);
        }
    }
}

static int
tab_bar_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_tab_bar_t *tb = (tiku_kits_ui_tab_bar_t *)base;
    if (tb->n_tabs == 0u) return 0;

    switch (evt) {
    case TIKU_KITS_UI_EVT_INC:
    case TIKU_KITS_UI_EVT_ACTIVATE:
        tb->selected = (int8_t)((tb->selected + 1) % tb->n_tabs);
        fire_change(tb);
        return 1;
    case TIKU_KITS_UI_EVT_DEC:
        tb->selected = (int8_t)((tb->selected + tb->n_tabs - 1) % tb->n_tabs);
        fire_change(tb);
        return 1;
    default:
        return 0;
    }
}

static int
tab_bar_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_tab_bar_ops = {
    .render        = tab_bar_render,
    .handle_event  = tab_bar_handle_event,
    .is_focusable  = tab_bar_is_focusable,
};

void
tiku_kits_ui_tab_bar_init(tiku_kits_ui_tab_bar_t *tb,
                           int16_t x, int16_t y,
                           uint16_t w, uint16_t h,
                           const tiku_kits_gfx_font_t *font,
                           uint8_t scale,
                           tiku_kits_ui_tab_bar_cb_t on_change,
                           void *user_data)
{
    if (tb == NULL) return;
    tb->base.ops      = &tiku_kits_ui_tab_bar_ops;
    tb->base.x        = x;
    tb->base.y        = y;
    tb->base.w        = w;
    tb->base.h        = h;
    tb->base.visible  = 1;
    tb->base.focused  = 0;
    tb->base.dirty  = 0;
    tb->base.user_data = user_data;
    tb->n_tabs   = 0;
    tb->selected = -1;
    tb->font     = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    tb->scale    = (scale > 0) ? scale : 1;
    tb->on_change = on_change;
}

int
tiku_kits_ui_tab_bar_add(tiku_kits_ui_tab_bar_t *tb, const char *label)
{
    if (tb == NULL || label == NULL) return TIKU_KITS_UI_ERR_PARAM;
    if (tb->n_tabs >= TIKU_KITS_UI_TAB_BAR_MAX_TABS) {
        return TIKU_KITS_UI_ERR_FULL;
    }
    tb->labels[tb->n_tabs++] = label;
    if (tb->selected < 0) tb->selected = 0;
    return TIKU_KITS_UI_OK;
}

void
tiku_kits_ui_tab_bar_set_selected(tiku_kits_ui_tab_bar_t *tb, int8_t idx)
{
    if (tb == NULL || tb->n_tabs == 0u) return;
    if (idx < 0) idx = 0;
    if (idx >= (int8_t)tb->n_tabs) idx = (int8_t)(tb->n_tabs - 1u);
    tb->selected = idx;
}
