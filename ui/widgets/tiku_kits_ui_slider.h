/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_slider.h - Horizontal value slider
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Visual: a thin horizontal track with a square thumb at the
 * value's position. Focused thumbs render in the focus colour;
 * un-focused in the foreground colour.
 *
 * Events:
 * ACTIVATE  -> increment by step (wraps at max).
 * INC       -> increment by step (clamps at max).
 * DEC       -> decrement by step (clamps at min).
 */

#ifndef TIKU_KITS_UI_SLIDER_H_
#define TIKU_KITS_UI_SLIDER_H_

#include "../tiku_kits_ui.h"

typedef void (*tiku_kits_ui_slider_cb_t)(int16_t value, void *user_data);

typedef struct {
    tiku_kits_ui_widget_t base;
    int16_t min;
    int16_t max;
    int16_t step;
    int16_t value;
    tiku_kits_ui_slider_cb_t on_change;
} tiku_kits_ui_slider_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_slider_ops;

void tiku_kits_ui_slider_init(tiku_kits_ui_slider_t *sl,
                               int16_t x, int16_t y,
                               uint16_t w, uint16_t h,
                               int16_t min, int16_t max,
                               int16_t step, int16_t value,
                               tiku_kits_ui_slider_cb_t on_change,
                               void *user_data);

/** Programmatic state setter. Clamps to [min, max]. Does NOT
 *  fire on_change. */
void tiku_kits_ui_slider_set_value(tiku_kits_ui_slider_t *sl, int16_t value);

#endif /* TIKU_KITS_UI_SLIDER_H_ */
