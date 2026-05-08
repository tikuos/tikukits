/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_sparkline.h - Tiny line plot of N int16 samples
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_SPARKLINE_H_
#define TIKU_KITS_UI_SPARKLINE_H_

#include "../tiku_kits_ui.h"

typedef struct {
    tiku_kits_ui_widget_t base;
    const int16_t *samples;
    uint16_t       n_samples;
    int16_t        min;          /* if min == max, auto-scale */
    int16_t        max;
    uint8_t        show_baseline;
} tiku_kits_ui_sparkline_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_sparkline_ops;

/** @p min == @p max is interpreted as auto-scale (uses sample range). */
void tiku_kits_ui_sparkline_init(tiku_kits_ui_sparkline_t *sp,
                                  int16_t x, int16_t y,
                                  uint16_t w, uint16_t h,
                                  const int16_t *samples,
                                  uint16_t n_samples,
                                  int16_t min, int16_t max);

void tiku_kits_ui_sparkline_set_data(tiku_kits_ui_sparkline_t *sp,
                                      const int16_t *samples,
                                      uint16_t n_samples);

#endif /* TIKU_KITS_UI_SPARKLINE_H_ */
