/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_icon.h - 1bpp bitmap icon widget
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_ICON_H_
#define TIKU_KITS_UI_ICON_H_

#include "../tiku_kits_ui.h"

/**
 * @brief Click callback for clickable icons.
 */
typedef void (*tiku_kits_ui_icon_cb_t)(void *user_data);

/**
 * @brief A 1bpp bitmap icon, optionally clickable.
 *
 * Icons render the bitmap at (x, y) with @p color. If @p bordered
 * is non-zero, draws a 1-px black hairline frame around the
 * bitmap (frame colour switches to RED when focused, mirroring
 * the button widget).
 *
 * Icons with a non-NULL @p on_click are focusable. Static
 * decorative icons (on_click = NULL) are not.
 */
typedef struct {
    tiku_kits_ui_widget_t base;
    const uint8_t *bitmap;
    uint16_t       bw;        /* bitmap pixel width  */
    uint16_t       bh;        /* bitmap pixel height */
    uint8_t        color;
    uint8_t        bordered;  /* 1 = draw frame; 0 = naked bitmap */
    tiku_kits_ui_icon_cb_t on_click;
} tiku_kits_ui_icon_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_icon_ops;

void tiku_kits_ui_icon_init(tiku_kits_ui_icon_t *ico,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             const uint8_t *bitmap,
                             uint16_t bw, uint16_t bh,
                             uint8_t color,
                             uint8_t bordered,
                             tiku_kits_ui_icon_cb_t on_click,
                             void *user_data);

#endif /* TIKU_KITS_UI_ICON_H_ */
