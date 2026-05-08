/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_indicator.h - Battery / WiFi / signal indicators
 *
 * Single widget covering three common status icons. Pick the
 * @kind at init time, then update level (0..4) at runtime. Renders
 * are programmatic geometry (no font dependency); the widget rect
 * scales to fit. Recommended size: 16x10 for battery, 16x12 for
 * wifi/signal.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_INDICATOR_H_
#define TIKU_KITS_UI_INDICATOR_H_

#include "../tiku_kits_ui.h"

typedef enum {
    TIKU_KITS_UI_INDICATOR_BATTERY = 0,
    TIKU_KITS_UI_INDICATOR_WIFI    = 1,
    TIKU_KITS_UI_INDICATOR_SIGNAL  = 2,
} tiku_kits_ui_indicator_kind_t;

typedef struct {
    tiku_kits_ui_widget_t base;
    tiku_kits_ui_indicator_kind_t kind;
    uint8_t level;       /* 0..4 (0 = empty / no signal, 4 = full) */
} tiku_kits_ui_indicator_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_indicator_ops;

void tiku_kits_ui_indicator_init(tiku_kits_ui_indicator_t *ind,
                                  int16_t x, int16_t y,
                                  uint16_t w, uint16_t h,
                                  tiku_kits_ui_indicator_kind_t kind,
                                  uint8_t level);

void tiku_kits_ui_indicator_set_level(tiku_kits_ui_indicator_t *ind,
                                       uint8_t level);

#endif /* TIKU_KITS_UI_INDICATOR_H_ */
