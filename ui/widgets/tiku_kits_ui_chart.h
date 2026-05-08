/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_chart.h - 2D chart widget (line / scatter / both)
 *
 * Plots N (x, y) points inside a rectangle with optional axes,
 * gridlines, and value labels. Designed for sensor logs, simple
 * dashboards, and any scenario where a sparkline isn't enough.
 *
 * Two data conventions:
 *   - x_values == NULL: indices 0..n-1 (time-series style).
 *   - x_values != NULL: paired (x_values[i], y_values[i]) points.
 *
 * Domain selection:
 *   - x_min == x_max: auto-scale to the input data.
 *   - y_min == y_max: same.
 *
 * Style flags can be OR-combined: LINE | DOTS draws both.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_CHART_H_
#define TIKU_KITS_UI_CHART_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

#define TIKU_KITS_UI_CHART_STYLE_LINE   (1u << 0)
#define TIKU_KITS_UI_CHART_STYLE_DOTS   (1u << 1)

#define TIKU_KITS_UI_CHART_FLAG_AXES        (1u << 0)  /* draw L+B axes */
#define TIKU_KITS_UI_CHART_FLAG_GRID        (1u << 1)  /* major gridlines */
#define TIKU_KITS_UI_CHART_FLAG_BASELINE    (1u << 2)  /* y=0 reference  */

typedef struct {
    tiku_kits_ui_widget_t base;
    const int16_t *x_values;       /* NULL = use indices */
    const int16_t *y_values;
    uint16_t       n_points;
    int16_t        x_min, x_max;
    int16_t        y_min, y_max;
    uint8_t        style;          /* LINE | DOTS */
    uint8_t        flags;          /* AXES | GRID | BASELINE */
    uint8_t        grid_x_div;     /* number of major x divisions (0 = auto) */
    uint8_t        grid_y_div;     /* number of major y divisions (0 = auto) */
} tiku_kits_ui_chart_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_chart_ops;

void tiku_kits_ui_chart_init(tiku_kits_ui_chart_t *c,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h,
                              const int16_t *y_values,
                              uint16_t n_points);

/** Provide an explicit x array (paired data); pass NULL to revert
 *  to "indices 0..n-1". */
void tiku_kits_ui_chart_set_x(tiku_kits_ui_chart_t *c,
                               const int16_t *x_values);

void tiku_kits_ui_chart_set_y(tiku_kits_ui_chart_t *c,
                               const int16_t *y_values,
                               uint16_t n_points);

/** Pin the x-axis domain. Pass min == max to revert to auto. */
void tiku_kits_ui_chart_set_x_domain(tiku_kits_ui_chart_t *c,
                                      int16_t min, int16_t max);

/** Pin the y-axis domain. Pass min == max to revert to auto. */
void tiku_kits_ui_chart_set_y_domain(tiku_kits_ui_chart_t *c,
                                      int16_t min, int16_t max);

void tiku_kits_ui_chart_set_style(tiku_kits_ui_chart_t *c, uint8_t style);
void tiku_kits_ui_chart_set_flags(tiku_kits_ui_chart_t *c, uint8_t flags);

#endif /* TIKU_KITS_UI_CHART_H_ */
