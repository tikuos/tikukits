/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_gauge.h - Circular arc gauge widget
 *
 * Renders a partial-arc dial that fills proportionally to value
 * within [min, max]. Default shape is a 270-deg sweep starting at
 * 225-deg (CCW) -- a classic bottom-open gauge.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_GAUGE_H_
#define TIKU_KITS_UI_GAUGE_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

typedef struct {
    tiku_kits_ui_widget_t base;
    int16_t value;
    int16_t min;
    int16_t max;
    int16_t arc_start_deg;
    int16_t arc_span_deg;
    const tiku_kits_gfx_font_t *label_font;  /* NULL = no centre label */
    uint8_t label_scale;
    const char *label;                        /* NULL = no centre label */
} tiku_kits_ui_gauge_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_gauge_ops;

void tiku_kits_ui_gauge_init(tiku_kits_ui_gauge_t *g,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h,
                              int16_t min, int16_t max, int16_t value);

/** Optional: configure non-default arc range. start_deg uses the
 *  curve kit's convention (0 = right, CCW positive). */
void tiku_kits_ui_gauge_set_arc(tiku_kits_ui_gauge_t *g,
                                 int16_t arc_start_deg,
                                 int16_t arc_span_deg);

void tiku_kits_ui_gauge_set_value(tiku_kits_ui_gauge_t *g, int16_t value);

void tiku_kits_ui_gauge_set_label(tiku_kits_ui_gauge_t *g,
                                   const char *label,
                                   const tiku_kits_gfx_font_t *font,
                                   uint8_t scale);

#endif /* TIKU_KITS_UI_GAUGE_H_ */
