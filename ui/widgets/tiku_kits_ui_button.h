/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_button.h - Clickable button widget (BeOS-styled)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_BUTTON_H_
#define TIKU_KITS_UI_BUTTON_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

/**
 * @brief Click callback signature.
 * @param user_data the button's user_data pointer.
 */
typedef void (*tiku_kits_ui_button_cb_t)(void *user_data);

/*
 * Visual: outset bevel (1-px white highlight on top/left, 1-px
 * black shadow on bottom/right) + black hairline border. When the
 * button is focused, the border colour switches to red so the
 * user can see which button will activate on the next ACTIVATE
 * event.
 * Behaviour: on ACTIVATE event (when focused), invokes on_click
 * with user_data. The button does not auto-redraw; the app calls
 * window_render + display refresh after the callback returns if
 * state changed.
 */

/**
 * @brief A clickable button.
 */
typedef struct {
    tiku_kits_ui_widget_t base;
    const char *text;
    const tiku_kits_gfx_font_t *font;
    uint8_t scale;
    tiku_kits_ui_button_cb_t on_click;
} tiku_kits_ui_button_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_button_ops;

void tiku_kits_ui_button_init(tiku_kits_ui_button_t *btn,
                               int16_t x, int16_t y,
                               uint16_t w, uint16_t h,
                               const char *text,
                               const tiku_kits_gfx_font_t *font,
                               uint8_t scale,
                               tiku_kits_ui_button_cb_t on_click,
                               void *user_data);

#endif /* TIKU_KITS_UI_BUTTON_H_ */
