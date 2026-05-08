/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_toggle.h - Pill-shaped toggle / switch widget
 *
 * Visual: a horizontal pill (rounded rect) with a knob that snaps
 * to the right end when ON and the left end when OFF. The track
 * fills with the accent colour when ON, the bg colour when OFF.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_TOGGLE_H_
#define TIKU_KITS_UI_TOGGLE_H_

#include "../tiku_kits_ui.h"

typedef void (*tiku_kits_ui_toggle_cb_t)(int on, void *user_data);

typedef struct {
    tiku_kits_ui_widget_t base;
    uint8_t on;
    tiku_kits_ui_toggle_cb_t on_change;
} tiku_kits_ui_toggle_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_toggle_ops;

void tiku_kits_ui_toggle_init(tiku_kits_ui_toggle_t *tg,
                               int16_t x, int16_t y,
                               uint16_t w, uint16_t h,
                               uint8_t on,
                               tiku_kits_ui_toggle_cb_t on_change,
                               void *user_data);

/** Programmatic state setter. Does NOT fire on_change. */
void tiku_kits_ui_toggle_set(tiku_kits_ui_toggle_t *tg, uint8_t on);

#endif /* TIKU_KITS_UI_TOGGLE_H_ */
