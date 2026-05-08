/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_label.h - Static text label
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_LABEL_H_
#define TIKU_KITS_UI_LABEL_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

/**
 * @brief A static text label.
 *
 * Renders @p text at (x, y) inside the parent's content area.
 * Width / height are used for alignment within the rectangle:
 * passing align != LEFT centers / right-aligns within (w, h).
 *
 * Labels are not focusable.
 */
typedef struct {
    tiku_kits_ui_widget_t base;
    const char *text;
    const tiku_kits_gfx_font_t *font;
    uint8_t color;
    uint8_t scale;
    tiku_kits_gfx_align_t align;
} tiku_kits_ui_label_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_label_ops;

void tiku_kits_ui_label_init(tiku_kits_ui_label_t *lbl,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h,
                              const char *text,
                              const tiku_kits_gfx_font_t *font,
                              uint8_t color, uint8_t scale,
                              tiku_kits_gfx_align_t align);

#endif /* TIKU_KITS_UI_LABEL_H_ */
