/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_qr.c - QR widget impl
 *
 * The widget paints the quiet zone with the theme background, then
 * walks the row-major MSB-first module array and fills a
 * module_px x module_px square at each set bit with the theme
 * foreground.
 *
 * Module data is row-major MSB-first; bytes-per-row =
 * ceil(size / 8). For a 21-module QR that's 3 bytes/row; for a
 * 33-module code, 5 bytes/row.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_qr.h"
#include "../tiku_kits_ui_theme.h"

static int
qr_module_set(const tiku_kits_ui_qr_t *q, uint16_t mx, uint16_t my)
{
    uint16_t bytes_per_row;
    uint8_t  byte;
    if (q->modules == NULL) return 0;
    if (mx >= q->size || my >= q->size) return 0;
    bytes_per_row = (uint16_t)((q->size + 7u) / 8u);
    byte = q->modules[(uint32_t)my * bytes_per_row + (mx >> 3)];
    return (byte & (uint8_t)(0x80u >> (mx & 7u))) ? 1 : 0;
}

static void
qr_render(const tiku_kits_ui_widget_t *base,
           const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_qr_t *q = (const tiku_kits_ui_qr_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint16_t total_modules;
    uint16_t total_px;
    uint16_t i, j;

    if (q->size == 0u || q->module_px == 0u) return;

    total_modules = (uint16_t)(q->size + 2u * q->quiet_modules);
    total_px      = (uint16_t)(total_modules * q->module_px);

    /* Quiet zone background (full square). */
    tiku_kits_gfx_fill_rect(s, base->x, base->y, total_px, total_px,
        t->color_bg);

    /* Modules. */
    for (j = 0; j < q->size; j++) {
        for (i = 0; i < q->size; i++) {
            if (!qr_module_set(q, i, j)) continue;
            tiku_kits_gfx_fill_rect(s,
                (int16_t)(base->x + (q->quiet_modules + i) * q->module_px),
                (int16_t)(base->y + (q->quiet_modules + j) * q->module_px),
                q->module_px, q->module_px,
                t->color_fg);
        }
    }
}

static void
qr_intrinsic_size(const tiku_kits_ui_widget_t *base,
                   uint16_t avail_w, tiku_kits_gfx_size_t *out)
{
    const tiku_kits_ui_qr_t *q = (const tiku_kits_ui_qr_t *)base;
    uint16_t side;
    (void)avail_w;
    side = (uint16_t)((q->size + 2u * q->quiet_modules) * q->module_px);
    out->w = side;
    out->h = side;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_qr_ops = {
    .render         = qr_render,
    .handle_event   = NULL,
    .is_focusable   = NULL,
    .intrinsic_size = qr_intrinsic_size,
};

void
tiku_kits_ui_qr_init(tiku_kits_ui_qr_t *q,
                       int16_t x, int16_t y,
                       uint16_t w, uint16_t h,
                       const uint8_t *modules,
                       uint16_t size,
                       uint8_t module_px,
                       uint8_t quiet_modules)
{
    if (q == NULL) return;
    q->base.ops      = &tiku_kits_ui_qr_ops;
    q->base.x        = x;
    q->base.y        = y;
    q->base.w        = w;
    q->base.h        = h;
    q->base.visible  = 1;
    q->base.focused  = 0;
    q->base.dirty    = 0;
    q->base.user_data = NULL;
    q->modules       = modules;
    q->size          = size;
    q->module_px     = (module_px > 0) ? module_px : 1u;
    q->quiet_modules = quiet_modules;
}

void
tiku_kits_ui_qr_set_modules(tiku_kits_ui_qr_t *q,
                              const uint8_t *modules,
                              uint16_t size)
{
    if (q == NULL) return;
    q->modules = modules;
    q->size    = size;
}
