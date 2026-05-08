/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_bar_chart.h - Vertical bar chart widget
 *
 * Renders N bars whose heights are proportional to the input
 * values. With @p max == 0 the chart auto-scales to the largest
 * value; otherwise values are clipped to [0, max].
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_BAR_CHART_H_
#define TIKU_KITS_UI_BAR_CHART_H_

#include "../tiku_kits_ui.h"

typedef struct {
    tiku_kits_ui_widget_t base;
    const int16_t *values;
    uint16_t       n_bars;
    int16_t        max;       /* 0 = auto-scale */
    uint8_t        gap;       /* pixels between bars (default 1) */
} tiku_kits_ui_bar_chart_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_bar_chart_ops;

void tiku_kits_ui_bar_chart_init(tiku_kits_ui_bar_chart_t *bc,
                                  int16_t x, int16_t y,
                                  uint16_t w, uint16_t h,
                                  const int16_t *values,
                                  uint16_t n_bars,
                                  int16_t max);

void tiku_kits_ui_bar_chart_set_data(tiku_kits_ui_bar_chart_t *bc,
                                      const int16_t *values,
                                      uint16_t n_bars);

#endif /* TIKU_KITS_UI_BAR_CHART_H_ */
