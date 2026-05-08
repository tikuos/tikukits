/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_calendar.h - Month-grid calendar / date picker
 *
 * Renders a Sunday-first month grid with the selected day
 * highlighted. Navigation:
 *
 *   FOCUS_LEFT  / DEC  -> previous day
 *   FOCUS_RIGHT / INC  -> next day
 *   FOCUS_UP            -> previous week
 *   FOCUS_DOWN          -> next week
 *   ACTIVATE            -> fire on_select(year, month, day)
 *   BACK                -> step back one month (selected_day clamps)
 *   MENU                -> step forward one month
 *
 * Dates wrap correctly across month / year boundaries (handles
 * 28 / 30 / 31 / leap-Feb).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_CALENDAR_H_
#define TIKU_KITS_UI_CALENDAR_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

typedef void (*tiku_kits_ui_calendar_cb_t)(uint16_t year, uint8_t month,
                                             uint8_t day, void *user_data);

typedef struct {
    tiku_kits_ui_widget_t base;
    uint16_t year;
    uint8_t  month;          /* 1..12 */
    uint8_t  selected_day;   /* 1..31 */
    const tiku_kits_gfx_font_t *font;
    uint8_t  scale;
    tiku_kits_ui_calendar_cb_t on_select;
} tiku_kits_ui_calendar_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_calendar_ops;

void tiku_kits_ui_calendar_init(tiku_kits_ui_calendar_t *cal,
                                  int16_t x, int16_t y,
                                  uint16_t w, uint16_t h,
                                  uint16_t year, uint8_t month, uint8_t day,
                                  const tiku_kits_gfx_font_t *font,
                                  uint8_t scale,
                                  tiku_kits_ui_calendar_cb_t on_select,
                                  void *user_data);

void tiku_kits_ui_calendar_set_date(tiku_kits_ui_calendar_t *cal,
                                      uint16_t year, uint8_t month,
                                      uint8_t day);

#endif /* TIKU_KITS_UI_CALENDAR_H_ */
