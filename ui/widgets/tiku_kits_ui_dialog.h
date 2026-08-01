/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_dialog.h - Modal dialog overlay
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Renders a filled rounded card with a title, wrapped body message,
 * and up to 3 buttons in a row at the bottom. Caller toggles
 * `base.visible` to show / hide. INC/DEC navigate buttons; ACTIVATE
 * fires the selected button's callback; BACK fires button 0
 * (typically "cancel").
 */

#ifndef TIKU_KITS_UI_DIALOG_H_
#define TIKU_KITS_UI_DIALOG_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

#ifndef TIKU_KITS_UI_DIALOG_MAX_BUTTONS
#define TIKU_KITS_UI_DIALOG_MAX_BUTTONS 3
#endif

typedef void (*tiku_kits_ui_dialog_cb_t)(uint8_t button_idx,
                                          void *user_data);

typedef struct {
    tiku_kits_ui_widget_t base;
    const char *title;
    const char *message;
    const tiku_kits_gfx_font_t *title_font;
    const tiku_kits_gfx_font_t *body_font;
    uint8_t title_scale;
    uint8_t body_scale;
    const char *buttons[TIKU_KITS_UI_DIALOG_MAX_BUTTONS];
    uint8_t n_buttons;
    int8_t  selected_button;
    tiku_kits_ui_dialog_cb_t on_button;
} tiku_kits_ui_dialog_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_dialog_ops;

void tiku_kits_ui_dialog_init(tiku_kits_ui_dialog_t *dlg,
                               int16_t x, int16_t y,
                               uint16_t w, uint16_t h,
                               const char *title,
                               const char *message,
                               const tiku_kits_gfx_font_t *title_font,
                               const tiku_kits_gfx_font_t *body_font,
                               uint8_t title_scale,
                               uint8_t body_scale,
                               tiku_kits_ui_dialog_cb_t on_button,
                               void *user_data);

int tiku_kits_ui_dialog_add_button(tiku_kits_ui_dialog_t *dlg,
                                    const char *label);

#endif /* TIKU_KITS_UI_DIALOG_H_ */
