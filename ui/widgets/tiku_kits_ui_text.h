/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_text.h - Multi-line wrapped text widget
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Like tiku_kits_ui_label_t but renders multi-line content with
 * word-wrap. The widget's (w, h) bounds the text area; lines that
 * fall outside are not drawn. Honours embedded `\n` for forced
 * line breaks.
 *
 * Use this for paragraphs, descriptions, multi-line status. For
 * single-line labels use tiku_kits_ui_label_t (lighter).
 */

#ifndef TIKU_KITS_UI_TEXT_H_
#define TIKU_KITS_UI_TEXT_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

typedef struct {
    tiku_kits_ui_widget_t base;
    const char *text;
    const tiku_kits_gfx_font_t *font;
    uint8_t color;
    uint8_t scale;
    tiku_kits_gfx_align_t align;
} tiku_kits_ui_text_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_text_ops;

void tiku_kits_ui_text_init(tiku_kits_ui_text_t *txt,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             const char *text,
                             const tiku_kits_gfx_font_t *font,
                             uint8_t color, uint8_t scale,
                             tiku_kits_gfx_align_t align);

#endif /* TIKU_KITS_UI_TEXT_H_ */
