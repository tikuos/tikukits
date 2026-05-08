/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_status_bar.c - Status bar impl
 *
 * Each slot widget is rendered into its own subsurface so it sees
 * (0, 0) as the anchor of its slot. Slot widths are: left = 1/3,
 * centre = 1/3, right = 1/3 of the bar. This is a deliberately
 * simple split -- if you need finer control, render the slots
 * yourself.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_status_bar.h"
#include "../tiku_kits_ui_theme.h"

static void
render_slot(const tiku_kits_gfx_surface_t *parent,
             tiku_kits_ui_widget_t *child,
             int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    tiku_kits_gfx_surface_t sub;
    if (child == NULL || !child->visible) return;
    tiku_kits_gfx_subsurface(&sub, parent, x, y, w, h);
    tiku_kits_ui_widget_render(child, &sub);
}

static void
sb_render(const tiku_kits_ui_widget_t *base,
           const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_status_bar_t *sb =
        (const tiku_kits_ui_status_bar_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint16_t slot_w = (uint16_t)(base->w / 3u);

    if (sb->bordered) {
        tiku_kits_gfx_rect(s, base->x, base->y, base->w, base->h,
            t->color_fg);
    }

    /* Left, centre, right slots. */
    render_slot(s, sb->left,
        base->x, base->y, slot_w, base->h);
    render_slot(s, sb->centre,
        (int16_t)(base->x + (int16_t)slot_w), base->y,
        slot_w, base->h);
    render_slot(s, sb->right,
        (int16_t)(base->x + (int16_t)(2u * slot_w)), base->y,
        (uint16_t)(base->w - 2u * slot_w), base->h);
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_status_bar_ops = {
    .render        = sb_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_status_bar_init(tiku_kits_ui_status_bar_t *sb,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h,
                              uint8_t bordered)
{
    if (sb == NULL) return;
    sb->base.ops      = &tiku_kits_ui_status_bar_ops;
    sb->base.x        = x;
    sb->base.y        = y;
    sb->base.w        = w;
    sb->base.h        = h;
    sb->base.visible  = 1;
    sb->base.focused  = 0;
    sb->base.dirty  = 0;
    sb->base.user_data = NULL;
    sb->left     = NULL;
    sb->centre   = NULL;
    sb->right    = NULL;
    sb->bordered = bordered;
}

void
tiku_kits_ui_status_bar_set_slots(tiku_kits_ui_status_bar_t *sb,
                                   tiku_kits_ui_widget_t *left,
                                   tiku_kits_ui_widget_t *centre,
                                   tiku_kits_ui_widget_t *right)
{
    if (sb == NULL) return;
    sb->left   = left;
    sb->centre = centre;
    sb->right  = right;
}
