/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_menu.c - Menu widget impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_menu.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

static uint8_t
row_h(const tiku_kits_ui_menu_t *m)
{
    return (uint8_t)(m->font->height * m->scale + 2u);
}

static uint8_t
visible_rows(const tiku_kits_ui_menu_t *m)
{
    uint8_t rh = row_h(m);
    return (rh == 0) ? 0 : (uint8_t)(m->base.h / rh);
}

static void
ensure_visible(tiku_kits_ui_menu_t *m)
{
    uint8_t vis = visible_rows(m);
    if (vis == 0) return;
    if (m->selected < m->scroll_offset) {
        m->scroll_offset = m->selected;
    }
    if (m->selected >= (int8_t)(m->scroll_offset + vis)) {
        m->scroll_offset = (int8_t)(m->selected - (int8_t)vis + 1);
    }
    if (m->scroll_offset < 0) m->scroll_offset = 0;
}

static void
menu_render(const tiku_kits_ui_widget_t *base,
             const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_menu_t *m = (const tiku_kits_ui_menu_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint8_t rh = row_h(m);
    uint8_t vis = visible_rows(m);
    uint8_t i;

    tiku_kits_gfx_rect(s, base->x, base->y, base->w, base->h,
        base->focused ? t->color_focus : t->color_fg);

    for (i = 0; i < vis; i++) {
        int8_t  ridx = (int8_t)(m->scroll_offset + i);
        int16_t ry = (int16_t)(base->y + 1 + i * rh);
        if (ridx >= (int8_t)m->n_items) break;

        if (ridx == m->selected) {
            tiku_kits_gfx_fill_rect(s,
                (int16_t)(base->x + 1), ry,
                (uint16_t)(base->w - 2u), rh, t->color_fg);
            {
                tiku_kits_gfx_rect_t r = {
                    (int16_t)(base->x + 4), ry,
                    (uint16_t)(base->w - 8u), rh
                };
                tiku_kits_gfx_draw_string_in_rect(s, &r,
                    m->items[ridx].label, m->font,
                    t->color_bg, m->scale,
                    TIKU_KITS_GFX_ALIGN_LEFT);
            }
        } else {
            tiku_kits_gfx_rect_t r = {
                (int16_t)(base->x + 4), (int16_t)(ry + 1),
                (uint16_t)(base->w - 8u), rh
            };
            tiku_kits_gfx_draw_string_in_rect(s, &r,
                m->items[ridx].label, m->font,
                t->color_fg, m->scale,
                TIKU_KITS_GFX_ALIGN_LEFT);
        }
    }
}

static int
menu_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_menu_t *m = (tiku_kits_ui_menu_t *)base;
    if (m->n_items == 0u) return 0;

    switch (evt) {
    case TIKU_KITS_UI_EVT_INC:
        if (m->selected < (int8_t)(m->n_items - 1u)) {
            m->selected++;
            ensure_visible(m);
        }
        return 1;
    case TIKU_KITS_UI_EVT_DEC:
        if (m->selected > 0) {
            m->selected--;
            ensure_visible(m);
        }
        return 1;
    case TIKU_KITS_UI_EVT_ACTIVATE:
        if (m->selected >= 0
            && m->selected < (int8_t)m->n_items
            && m->items[m->selected].on_click != NULL) {
            m->items[m->selected].on_click(m->items[m->selected].user_data);
        }
        return 1;
    default:
        return 0;
    }
}

static int
menu_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_menu_ops = {
    .render        = menu_render,
    .handle_event  = menu_handle_event,
    .is_focusable  = menu_is_focusable,
};

void
tiku_kits_ui_menu_init(tiku_kits_ui_menu_t *m,
                        int16_t x, int16_t y,
                        uint16_t w, uint16_t h,
                        const tiku_kits_gfx_font_t *font,
                        uint8_t scale)
{
    if (m == NULL) return;
    m->base.ops      = &tiku_kits_ui_menu_ops;
    m->base.x        = x;
    m->base.y        = y;
    m->base.w        = w;
    m->base.h        = h;
    m->base.visible  = 1;
    m->base.focused  = 0;
    m->base.dirty  = 0;
    m->base.user_data = NULL;
    m->n_items       = 0;
    m->selected      = -1;
    m->scroll_offset = 0;
    m->font          = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    m->scale         = (scale > 0) ? scale : 1;
}

int
tiku_kits_ui_menu_add(tiku_kits_ui_menu_t *m,
                      const char *label,
                      tiku_kits_ui_menu_action_t on_click,
                      void *user_data)
{
    if (m == NULL || label == NULL) return TIKU_KITS_UI_ERR_PARAM;
    if (m->n_items >= TIKU_KITS_UI_MENU_MAX_ITEMS) {
        return TIKU_KITS_UI_ERR_FULL;
    }
    m->items[m->n_items].label     = label;
    m->items[m->n_items].on_click  = on_click;
    m->items[m->n_items].user_data = user_data;
    m->n_items++;
    if (m->selected < 0) m->selected = 0;
    return TIKU_KITS_UI_OK;
}

void
tiku_kits_ui_menu_clear(tiku_kits_ui_menu_t *m)
{
    if (m == NULL) return;
    m->n_items       = 0;
    m->selected      = -1;
    m->scroll_offset = 0;
}
