/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_indicator.c - Battery / WiFi / signal impl
 *
 * Programmatic geometry; no font dependency. Each kind paints its
 * level in a recognisable shape:
 *
 *   BATTERY: rounded body + cap, body filled left-to-right with
 *            blocks proportional to level / 4.
 *   WIFI / SIGNAL: 4 vertical bars, heights stepping up; bars
 *            beyond `level` rendered as outlines only.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_indicator.h"
#include "../tiku_kits_ui_theme.h"

/*---------------------------------------------------------------------------*/
/* BATTERY                                                                   */
/*---------------------------------------------------------------------------*/

static void
draw_battery(const tiku_kits_ui_indicator_t *ind,
              const tiku_kits_gfx_surface_t *s,
              const tiku_kits_ui_theme_t *t)
{
    int16_t  x = ind->base.x;
    int16_t  y = ind->base.y;
    uint16_t w = ind->base.w;
    uint16_t h = ind->base.h;
    uint16_t cap_w, body_w;
    uint8_t  level = (ind->level > 4u) ? 4u : ind->level;

    if (w < 6u || h < 4u) return;

    cap_w  = (uint16_t)(w / 6u);
    if (cap_w < 2u) cap_w = 2u;
    body_w = (uint16_t)(w - cap_w);

    /* Body outline. */
    tiku_kits_gfx_rect(s, x, y, body_w, h, t->color_fg);

    /* Cap on the right. */
    {
        uint16_t cap_h = (uint16_t)(h / 2u);
        int16_t  cap_y = (int16_t)(y + (int16_t)((h - cap_h) / 2u));
        tiku_kits_gfx_fill_rect(s,
            (int16_t)(x + (int16_t)body_w), cap_y,
            cap_w, cap_h, t->color_fg);
    }

    /* Internal fill: 4 cells (margin 2 px each side). */
    if (level > 0u && body_w > 4u && h > 4u) {
        uint16_t inner_w = (uint16_t)(body_w - 4u);
        uint16_t cell_w  = (uint16_t)(inner_w / 4u);
        uint16_t i;
        for (i = 0; i < level; i++) {
            tiku_kits_gfx_fill_rect(s,
                (int16_t)(x + 2 + i * cell_w),
                (int16_t)(y + 2),
                cell_w,
                (uint16_t)(h - 4u),
                t->color_fg);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* WIFI / SIGNAL (vertical bars)                                             */
/*---------------------------------------------------------------------------*/

static void
draw_bars(const tiku_kits_ui_indicator_t *ind,
           const tiku_kits_gfx_surface_t *s,
           const tiku_kits_ui_theme_t *t)
{
    int16_t  x = ind->base.x;
    int16_t  y = ind->base.y;
    uint16_t w = ind->base.w;
    uint16_t h = ind->base.h;
    uint8_t  level = (ind->level > 4u) ? 4u : ind->level;
    uint16_t bar_w, gap, slot_w;
    uint16_t i;

    if (w < 8u || h < 4u) return;

    /* 4 bars + 3 gaps of 1 px each. */
    bar_w = (uint16_t)((w - 3u) / 4u);
    if (bar_w < 1u) bar_w = 1u;
    gap = 1u;
    slot_w = (uint16_t)(bar_w + gap);

    for (i = 0; i < 4u; i++) {
        uint16_t bar_h = (uint16_t)((h * (i + 1u)) / 4u);
        int16_t  bx = (int16_t)(x + i * slot_w);
        int16_t  by = (int16_t)(y + h - bar_h);
        if (i < level) {
            tiku_kits_gfx_fill_rect(s, bx, by, bar_w, bar_h, t->color_fg);
        } else {
            tiku_kits_gfx_rect(s, bx, by, bar_w, bar_h, t->color_muted);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static void
indicator_render(const tiku_kits_ui_widget_t *base,
                  const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_indicator_t *ind =
        (const tiku_kits_ui_indicator_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();

    switch (ind->kind) {
    case TIKU_KITS_UI_INDICATOR_BATTERY:
        draw_battery(ind, s, t);
        break;
    case TIKU_KITS_UI_INDICATOR_WIFI:
    case TIKU_KITS_UI_INDICATOR_SIGNAL:
        draw_bars(ind, s, t);
        break;
    default:
        break;
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_indicator_ops = {
    .render        = indicator_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

/*---------------------------------------------------------------------------*/
/* INIT + STATE                                                              */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_indicator_init(tiku_kits_ui_indicator_t *ind,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             tiku_kits_ui_indicator_kind_t kind,
                             uint8_t level)
{
    if (ind == NULL) return;
    ind->base.ops      = &tiku_kits_ui_indicator_ops;
    ind->base.x        = x;
    ind->base.y        = y;
    ind->base.w        = w;
    ind->base.h        = h;
    ind->base.visible  = 1;
    ind->base.focused  = 0;
    ind->base.dirty  = 0;
    ind->base.user_data = NULL;
    ind->kind  = kind;
    ind->level = (level > 4u) ? 4u : level;
}

void
tiku_kits_ui_indicator_set_level(tiku_kits_ui_indicator_t *ind,
                                  uint8_t level)
{
    if (ind == NULL) return;
    ind->level = (level > 4u) ? 4u : level;
}
