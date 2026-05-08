/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_radio.c - Radio button + group impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_radio.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

/*---------------------------------------------------------------------------*/
/* GROUP                                                                     */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_radio_group_init(tiku_kits_ui_radio_group_t *grp,
                               int8_t initial_selected,
                               tiku_kits_ui_radio_cb_t on_change,
                               void *user_data)
{
    if (grp == NULL) return;
    grp->selected_idx = initial_selected;
    grp->on_change    = on_change;
    grp->user_data    = user_data;
}

void
tiku_kits_ui_radio_group_select(tiku_kits_ui_radio_group_t *grp,
                                 int8_t idx)
{
    if (grp == NULL) return;
    grp->selected_idx = idx;
}

/*---------------------------------------------------------------------------*/
/* RADIO BUTTON RENDER                                                       */
/*---------------------------------------------------------------------------*/

static uint16_t
indicator_size(const tiku_kits_ui_radio_t *rb)
{
    uint16_t s = (uint16_t)(rb->font->height * rb->scale + 4u);
    if (s > rb->base.h) s = rb->base.h;
    if (s < 6u) s = 6u;
    return s;
}

static void
radio_render(const tiku_kits_ui_widget_t *base,
              const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_radio_t *rb = (const tiku_kits_ui_radio_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint16_t bs   = indicator_size(rb);
    uint16_t r    = (uint16_t)(bs / 2u);
    int16_t  cx   = (int16_t)(base->x + (int16_t)r);
    int16_t  cy   = (int16_t)(base->y + (int16_t)((base->h > bs)
                                                     ? (base->h - bs) / 2 : 0)
                              + (int16_t)r);
    uint8_t  edge = base->focused ? t->color_focus : t->color_fg;
    int      selected = (rb->group != NULL
                          && rb->group->selected_idx == rb->group_idx);

    /* Outer ring. */
    tiku_kits_gfx_circle(s, cx, cy, r, edge);

    /* Inner filled dot when selected. */
    if (selected && r > 2u) {
        tiku_kits_gfx_fill_circle(s, cx, cy, (uint16_t)(r - 2u), t->color_fg);
    }

    /* Label. */
    if (rb->label != NULL && rb->label[0] != '\0') {
        tiku_kits_gfx_rect_t rt;
        uint16_t glyph_h = (uint16_t)(rb->font->height * rb->scale);
        rt.x = (int16_t)(base->x + (int16_t)bs + 4);
        rt.y = (int16_t)(base->y +
                         (int16_t)((base->h > glyph_h)
                                    ? (base->h - glyph_h) / 2 : 0));
        rt.w = (uint16_t)(base->w - bs - 4u);
        rt.h = glyph_h;
        tiku_kits_gfx_draw_string_in_rect(s, &rt, rb->label,
            rb->font, t->color_fg, rb->scale,
            TIKU_KITS_GFX_ALIGN_LEFT);
    }
}

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

static int
radio_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_radio_t *rb = (tiku_kits_ui_radio_t *)base;
    if (evt != TIKU_KITS_UI_EVT_ACTIVATE) return 0;
    if (rb->group == NULL) return 0;

    if (rb->group->selected_idx != rb->group_idx) {
        rb->group->selected_idx = rb->group_idx;
        if (rb->group->on_change != NULL) {
            rb->group->on_change(rb->group_idx, rb->group->user_data);
        }
    }
    return 1;
}

static int
radio_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_radio_ops = {
    .render        = radio_render,
    .handle_event  = radio_handle_event,
    .is_focusable  = radio_is_focusable,
};

/*---------------------------------------------------------------------------*/
/* INIT                                                                      */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_radio_init(tiku_kits_ui_radio_t *rb,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h,
                         const char *label,
                         const tiku_kits_gfx_font_t *font,
                         uint8_t scale,
                         tiku_kits_ui_radio_group_t *group,
                         int8_t group_idx)
{
    if (rb == NULL) return;
    rb->base.ops      = &tiku_kits_ui_radio_ops;
    rb->base.x        = x;
    rb->base.y        = y;
    rb->base.w        = w;
    rb->base.h        = h;
    rb->base.visible  = 1;
    rb->base.focused  = 0;
    rb->base.dirty  = 0;
    rb->base.user_data = NULL;
    rb->label         = label;
    rb->font          = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    rb->scale         = (scale > 0) ? scale : 1;
    rb->group         = group;
    rb->group_idx     = group_idx;
}
