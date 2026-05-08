/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_checkbox.h - Checkbox widget
 *
 * A focusable boolean control: a square box on the left followed by
 * a label. ACTIVATE toggles the checked state and fires on_change.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_CHECKBOX_H_
#define TIKU_KITS_UI_CHECKBOX_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

typedef void (*tiku_kits_ui_checkbox_cb_t)(int checked, void *user_data);

typedef struct {
    tiku_kits_ui_widget_t base;
    const char *label;
    const tiku_kits_gfx_font_t *font;
    uint8_t scale;
    uint8_t checked;
    tiku_kits_ui_checkbox_cb_t on_change;
} tiku_kits_ui_checkbox_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_checkbox_ops;

void tiku_kits_ui_checkbox_init(tiku_kits_ui_checkbox_t *cb,
                                 int16_t x, int16_t y,
                                 uint16_t w, uint16_t h,
                                 const char *label,
                                 const tiku_kits_gfx_font_t *font,
                                 uint8_t scale,
                                 uint8_t checked,
                                 tiku_kits_ui_checkbox_cb_t on_change,
                                 void *user_data);

/** Programmatic state setter. Does NOT fire on_change. */
void tiku_kits_ui_checkbox_set_checked(tiku_kits_ui_checkbox_t *cb,
                                        uint8_t checked);

#endif /* TIKU_KITS_UI_CHECKBOX_H_ */
