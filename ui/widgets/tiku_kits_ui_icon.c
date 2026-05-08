/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_icon.c - Bitmap icon implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_icon.h"
#include "../tiku_kits_ui_theme.h"

static void
icon_render(const tiku_kits_ui_widget_t *base,
             const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_icon_t *ico = (const tiku_kits_ui_icon_t *)base;
    int16_t bx, by;

    if (ico->bordered) {
        const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
        const uint8_t border = base->focused ? t->color_focus : t->color_fg;
        tiku_kits_gfx_rect(s, base->x, base->y, base->w, base->h, border);
    }

    /* Center the bitmap inside the widget rect. */
    bx = (int16_t)(base->x + (base->w > ico->bw
                                ? (int16_t)((base->w - ico->bw) / 2u) : 0));
    by = (int16_t)(base->y + (base->h > ico->bh
                                ? (int16_t)((base->h - ico->bh) / 2u) : 0));
    tiku_kits_gfx_bitmap(s, bx, by, ico->bitmap,
                          ico->bw, ico->bh, ico->color);
}

static int
icon_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    const tiku_kits_ui_icon_t *ico = (const tiku_kits_ui_icon_t *)base;
    if (evt != TIKU_KITS_UI_EVT_ACTIVATE) return 0;
    if (ico->on_click != NULL) {
        ico->on_click(base->user_data);
        return 1;
    }
    return 0;
}

static int
icon_is_focusable(const tiku_kits_ui_widget_t *base)
{
    const tiku_kits_ui_icon_t *ico = (const tiku_kits_ui_icon_t *)base;
    return ico->on_click != NULL;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_icon_ops = {
    .render        = icon_render,
    .handle_event  = icon_handle_event,
    .is_focusable  = icon_is_focusable,
};

void
tiku_kits_ui_icon_init(tiku_kits_ui_icon_t *ico,
                        int16_t x, int16_t y,
                        uint16_t w, uint16_t h,
                        const uint8_t *bitmap,
                        uint16_t bw, uint16_t bh,
                        uint8_t color,
                        uint8_t bordered,
                        tiku_kits_ui_icon_cb_t on_click,
                        void *user_data)
{
    if (ico == NULL) return;
    ico->base.ops      = &tiku_kits_ui_icon_ops;
    ico->base.x        = x;
    ico->base.y        = y;
    ico->base.w        = w;
    ico->base.h        = h;
    ico->base.visible  = 1;
    ico->base.focused  = 0;
    ico->base.dirty  = 0;
    ico->base.user_data = user_data;
    ico->bitmap        = bitmap;
    ico->bw            = bw;
    ico->bh            = bh;
    ico->color         = color;
    ico->bordered      = bordered;
    ico->on_click      = on_click;
}
