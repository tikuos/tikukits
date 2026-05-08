/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_clock.h - Digital clock widget
 *
 * Renders HH:MM or HH:MM:SS using a configurable font (defaults
 * to the bundled 7-segment font for that retro-LED look).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_CLOCK_H_
#define TIKU_KITS_UI_CLOCK_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

typedef enum {
    TIKU_KITS_UI_CLOCK_HM  = 0,
    TIKU_KITS_UI_CLOCK_HMS = 1,
} tiku_kits_ui_clock_format_t;

typedef struct {
    tiku_kits_ui_widget_t base;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    tiku_kits_ui_clock_format_t format;
    const tiku_kits_gfx_font_t *font;
    uint8_t scale;
} tiku_kits_ui_clock_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_clock_ops;

void tiku_kits_ui_clock_init(tiku_kits_ui_clock_t *clk,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h,
                              tiku_kits_ui_clock_format_t format,
                              const tiku_kits_gfx_font_t *font,
                              uint8_t scale);

void tiku_kits_ui_clock_set_time(tiku_kits_ui_clock_t *clk,
                                  uint8_t h, uint8_t m, uint8_t s);

#endif /* TIKU_KITS_UI_CLOCK_H_ */
