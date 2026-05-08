/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_list.c - List widget impl
 *
 * Selected row drawn with inverted colours (bg-on-fg). Scroll
 * offset auto-adjusts on INC/DEC so the selection remains visible.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_list.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

/*---------------------------------------------------------------------------*/
/* HELPERS                                                                   */
/*---------------------------------------------------------------------------*/

static uint8_t
effective_row_h(const tiku_kits_ui_list_t *lst)
{
    if (lst->row_h > 0) return lst->row_h;
    return (uint8_t)(lst->font->height * lst->scale + 2u);
}

static uint8_t
visible_rows(const tiku_kits_ui_list_t *lst)
{
    uint8_t rh = effective_row_h(lst);
    if (rh == 0) return 0;
    return (uint8_t)(lst->base.h / rh);
}

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static void
list_render(const tiku_kits_ui_widget_t *base,
             const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_list_t *lst = (const tiku_kits_ui_list_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint8_t rh = effective_row_h(lst);
    uint8_t vis = visible_rows(lst);
    uint8_t i;
    int8_t  start = lst->scroll_offset;
    if (start < 0) start = 0;

    /* Optional widget border. */
    tiku_kits_gfx_rect(s, base->x, base->y, base->w, base->h,
        base->focused ? t->color_focus : t->color_fg);

    for (i = 0; i < vis; i++) {
        int8_t row_idx = (int8_t)(start + i);
        int16_t row_y = (int16_t)(base->y + 1 + i * rh);
        if (row_idx >= (int8_t)lst->n_rows) break;
        if (lst->rows[row_idx] == NULL) continue;

        if (row_idx == lst->selected) {
            /* Inverted highlight. */
            tiku_kits_gfx_fill_rect(s,
                (int16_t)(base->x + 1), row_y,
                (uint16_t)(base->w - 2u), rh, t->color_fg);
            {
                tiku_kits_gfx_rect_t r = {
                    (int16_t)(base->x + 4), row_y,
                    (uint16_t)(base->w - 8u), rh
                };
                tiku_kits_gfx_draw_string_in_rect(s, &r,
                    lst->rows[row_idx], lst->font, t->color_bg, lst->scale,
                    TIKU_KITS_GFX_ALIGN_LEFT);
            }
        } else {
            tiku_kits_gfx_rect_t r = {
                (int16_t)(base->x + 4), (int16_t)(row_y + 1),
                (uint16_t)(base->w - 8u), rh
            };
            tiku_kits_gfx_draw_string_in_rect(s, &r,
                lst->rows[row_idx], lst->font, t->color_fg, lst->scale,
                TIKU_KITS_GFX_ALIGN_LEFT);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

static void
ensure_visible(tiku_kits_ui_list_t *lst)
{
    uint8_t vis = visible_rows(lst);
    if (vis == 0) return;
    if (lst->selected < lst->scroll_offset) {
        lst->scroll_offset = lst->selected;
    }
    if (lst->selected >= (int8_t)(lst->scroll_offset + vis)) {
        lst->scroll_offset = (int8_t)(lst->selected - (int8_t)vis + 1);
    }
    if (lst->scroll_offset < 0) lst->scroll_offset = 0;
}

static int
list_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_list_t *lst = (tiku_kits_ui_list_t *)base;
    if (lst->n_rows == 0u) return 0;

    switch (evt) {
    case TIKU_KITS_UI_EVT_INC:
        if (lst->selected < (int8_t)(lst->n_rows - 1u)) {
            lst->selected++;
            ensure_visible(lst);
        }
        return 1;
    case TIKU_KITS_UI_EVT_DEC:
        if (lst->selected > 0) {
            lst->selected--;
            ensure_visible(lst);
        }
        return 1;
    case TIKU_KITS_UI_EVT_ACTIVATE:
        if (lst->on_select != NULL && lst->selected >= 0) {
            lst->on_select(lst->selected, base->user_data);
        }
        return 1;
    default:
        return 0;
    }
}

static int
list_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_list_ops = {
    .render        = list_render,
    .handle_event  = list_handle_event,
    .is_focusable  = list_is_focusable,
};

/*---------------------------------------------------------------------------*/
/* INIT + STATE                                                              */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_list_init(tiku_kits_ui_list_t *lst,
                        int16_t x, int16_t y,
                        uint16_t w, uint16_t h,
                        const char * const *rows, uint8_t n_rows,
                        const tiku_kits_gfx_font_t *font,
                        uint8_t scale,
                        tiku_kits_ui_list_cb_t on_select,
                        void *user_data)
{
    if (lst == NULL) return;
    lst->base.ops      = &tiku_kits_ui_list_ops;
    lst->base.x        = x;
    lst->base.y        = y;
    lst->base.w        = w;
    lst->base.h        = h;
    lst->base.visible  = 1;
    lst->base.focused  = 0;
    lst->base.dirty  = 0;
    lst->base.user_data = user_data;
    lst->rows           = rows;
    lst->n_rows         = n_rows;
    lst->selected       = (n_rows > 0) ? 0 : -1;
    lst->scroll_offset  = 0;
    lst->font           = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    lst->scale          = (scale > 0) ? scale : 1;
    lst->row_h          = 0;
    lst->on_select      = on_select;
}

void
tiku_kits_ui_list_set_selected(tiku_kits_ui_list_t *lst, int8_t idx)
{
    if (lst == NULL || lst->n_rows == 0u) return;
    if (idx < 0) idx = 0;
    if (idx >= (int8_t)lst->n_rows) idx = (int8_t)(lst->n_rows - 1u);
    lst->selected = idx;
    ensure_visible(lst);
}
