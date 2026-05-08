/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_image.c - Generic image widget impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_image.h"
#include "../tiku_kits_ui_theme.h"

static void
image_render(const tiku_kits_ui_widget_t *base,
              const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_image_t *iw = (const tiku_kits_ui_image_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();

    if (iw->bordered) {
        tiku_kits_gfx_rect(s, base->x, base->y, base->w, base->h,
            t->color_fg);
    }

    if (iw->image == NULL) return;

    if (iw->stretch) {
        tiku_kits_gfx_image_blit_scaled(s,
            base->x, base->y, base->w, base->h, iw->image, iw->color);
    } else {
        int16_t bx = (int16_t)(base->x +
            (base->w > iw->image->width
                ? (int16_t)((base->w - iw->image->width) / 2u) : 0));
        int16_t by = (int16_t)(base->y +
            (base->h > iw->image->height
                ? (int16_t)((base->h - iw->image->height) / 2u) : 0));
        tiku_kits_gfx_image_blit(s, bx, by, iw->image, iw->color);
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_image_ops = {
    .render        = image_render,
    .handle_event  = NULL,
    .is_focusable  = NULL,
};

void
tiku_kits_ui_image_init(tiku_kits_ui_image_t *iw,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h,
                         const tiku_kits_gfx_image_t *image,
                         uint8_t color,
                         uint8_t bordered,
                         uint8_t stretch)
{
    if (iw == NULL) return;
    iw->base.ops      = &tiku_kits_ui_image_ops;
    iw->base.x        = x;
    iw->base.y        = y;
    iw->base.w        = w;
    iw->base.h        = h;
    iw->base.visible  = 1;
    iw->base.focused  = 0;
    iw->base.dirty  = 0;
    iw->base.user_data = NULL;
    iw->image    = image;
    iw->color    = color;
    iw->bordered = bordered;
    iw->stretch  = stretch;
}

void
tiku_kits_ui_image_set_image(tiku_kits_ui_image_t *iw,
                              const tiku_kits_gfx_image_t *image)
{
    if (iw == NULL) return;
    iw->image = image;
}
