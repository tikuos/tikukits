/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_dialog.c - Modal dialog impl
 *
 * Layout: title bar (height = title_font * scale + 4) at the top,
 * wrapped body in the middle, button row at the bottom (height =
 * body_font * scale + 8).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_dialog.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

static uint16_t
title_h(const tiku_kits_ui_dialog_t *dlg)
{
    if (dlg->title_font == NULL || dlg->title == NULL) return 0;
    return (uint16_t)(dlg->title_font->height * dlg->title_scale + 4u);
}

static uint16_t
button_row_h(const tiku_kits_ui_dialog_t *dlg)
{
    if (dlg->n_buttons == 0u) return 0;
    return (uint16_t)(dlg->body_font->height * dlg->body_scale + 10u);
}

static void
dialog_render(const tiku_kits_ui_widget_t *base,
               const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_dialog_t *dlg = (const tiku_kits_ui_dialog_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint16_t th = title_h(dlg);
    uint16_t bh = button_row_h(dlg);
    int16_t  bx, by;
    uint16_t bw, hh;

    /* Card background. */
    tiku_kits_gfx_fill_round_rect(s, base->x, base->y, base->w, base->h,
        t->corner_r, t->color_bg);
    tiku_kits_gfx_round_rect(s, base->x, base->y, base->w, base->h,
        t->corner_r, t->color_fg);

    /* Title bar in accent. */
    if (th > 0) {
        tiku_kits_gfx_fill_round_rect(s,
            base->x, base->y, base->w, th,
            t->corner_r, t->color_accent);
        tiku_kits_gfx_draw_string(s,
            (int16_t)(base->x + 6), (int16_t)(base->y + 2),
            dlg->title, dlg->title_font, t->color_fg, dlg->title_scale);
    }

    /* Wrapped message. */
    if (dlg->message != NULL) {
        tiku_kits_gfx_rect_t r;
        r.x = (int16_t)(base->x + 6);
        r.y = (int16_t)(base->y + th + 4);
        r.w = (uint16_t)(base->w - 12u);
        r.h = (uint16_t)(base->h - th - bh - 8u);
        tiku_kits_gfx_draw_text_wrapped(s, &r,
            dlg->message, dlg->body_font,
            t->color_fg, dlg->body_scale,
            TIKU_KITS_GFX_ALIGN_LEFT);
    }

    /* Buttons row. */
    if (dlg->n_buttons == 0u) return;
    bx = (int16_t)(base->x + 6);
    by = (int16_t)(base->y + base->h - bh + 2);
    bw = (uint16_t)((base->w - 12u - 4u * (dlg->n_buttons - 1u))
                    / dlg->n_buttons);
    hh = (uint16_t)(bh - 6u);
    {
        uint8_t i;
        for (i = 0; i < dlg->n_buttons; i++) {
            int16_t  ix = (int16_t)(bx + i * (bw + 4));
            uint8_t  edge = (dlg->selected_button == (int8_t)i)
                              ? t->color_focus : t->color_fg;
            tiku_kits_gfx_rect(s, ix, by, bw, hh, edge);
            {
                tiku_kits_gfx_rect_t r = {
                    ix, (int16_t)(by + 2), bw, (uint16_t)(hh - 4u)
                };
                tiku_kits_gfx_draw_string_in_rect(s, &r,
                    dlg->buttons[i], dlg->body_font,
                    t->color_fg, dlg->body_scale,
                    TIKU_KITS_GFX_ALIGN_CENTER);
            }
        }
    }
}

static void
fire_button(tiku_kits_ui_dialog_t *dlg, uint8_t idx)
{
    if (dlg->on_button != NULL) {
        dlg->on_button(idx, dlg->base.user_data);
    }
}

static int
dialog_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_dialog_t *dlg = (tiku_kits_ui_dialog_t *)base;
    if (dlg->n_buttons == 0u) return 0;

    switch (evt) {
    case TIKU_KITS_UI_EVT_INC:
        dlg->selected_button = (int8_t)((dlg->selected_button + 1)
                                         % dlg->n_buttons);
        return 1;
    case TIKU_KITS_UI_EVT_DEC:
        dlg->selected_button = (int8_t)((dlg->selected_button
                                          + dlg->n_buttons - 1)
                                         % dlg->n_buttons);
        return 1;
    case TIKU_KITS_UI_EVT_ACTIVATE:
        if (dlg->selected_button >= 0
            && dlg->selected_button < (int8_t)dlg->n_buttons) {
            fire_button(dlg, (uint8_t)dlg->selected_button);
        }
        return 1;
    case TIKU_KITS_UI_EVT_BACK:
        fire_button(dlg, 0);
        return 1;
    default:
        return 0;
    }
}

static int
dialog_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_dialog_ops = {
    .render        = dialog_render,
    .handle_event  = dialog_handle_event,
    .is_focusable  = dialog_is_focusable,
};

void
tiku_kits_ui_dialog_init(tiku_kits_ui_dialog_t *dlg,
                          int16_t x, int16_t y,
                          uint16_t w, uint16_t h,
                          const char *title,
                          const char *message,
                          const tiku_kits_gfx_font_t *title_font,
                          const tiku_kits_gfx_font_t *body_font,
                          uint8_t title_scale,
                          uint8_t body_scale,
                          tiku_kits_ui_dialog_cb_t on_button,
                          void *user_data)
{
    if (dlg == NULL) return;
    dlg->base.ops      = &tiku_kits_ui_dialog_ops;
    dlg->base.x        = x;
    dlg->base.y        = y;
    dlg->base.w        = w;
    dlg->base.h        = h;
    dlg->base.visible  = 0;       /* hidden by default */
    dlg->base.focused  = 0;
    dlg->base.dirty  = 0;
    dlg->base.user_data = user_data;
    dlg->title        = title;
    dlg->message      = message;
    dlg->title_font   = (title_font != NULL) ? title_font
                                              : &tiku_kits_gfx_font_5x7;
    dlg->body_font    = (body_font  != NULL) ? body_font
                                              : &tiku_kits_gfx_font_5x7;
    dlg->title_scale  = (title_scale > 0) ? title_scale : 1;
    dlg->body_scale   = (body_scale  > 0) ? body_scale  : 1;
    dlg->n_buttons    = 0;
    dlg->selected_button = -1;
    dlg->on_button    = on_button;
}

int
tiku_kits_ui_dialog_add_button(tiku_kits_ui_dialog_t *dlg,
                                const char *label)
{
    if (dlg == NULL || label == NULL) return TIKU_KITS_UI_ERR_PARAM;
    if (dlg->n_buttons >= TIKU_KITS_UI_DIALOG_MAX_BUTTONS) {
        return TIKU_KITS_UI_ERR_FULL;
    }
    dlg->buttons[dlg->n_buttons++] = label;
    if (dlg->selected_button < 0) dlg->selected_button = 0;
    return TIKU_KITS_UI_OK;
}
