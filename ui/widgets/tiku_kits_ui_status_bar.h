/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_status_bar.h - Top/bottom status strip with three slots
 *
 * Renders a strip with optional left, centre, and right child
 * widgets. Children are positioned by the status bar at render
 * time -- the caller's (x, y) inside each child is treated as a
 * pixel offset from its slot anchor.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_STATUS_BAR_H_
#define TIKU_KITS_UI_STATUS_BAR_H_

#include "../tiku_kits_ui.h"

typedef struct {
    tiku_kits_ui_widget_t base;
    tiku_kits_ui_widget_t *left;
    tiku_kits_ui_widget_t *centre;
    tiku_kits_ui_widget_t *right;
    uint8_t bordered;
} tiku_kits_ui_status_bar_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_status_bar_ops;

void tiku_kits_ui_status_bar_init(tiku_kits_ui_status_bar_t *sb,
                                   int16_t x, int16_t y,
                                   uint16_t w, uint16_t h,
                                   uint8_t bordered);

void tiku_kits_ui_status_bar_set_slots(tiku_kits_ui_status_bar_t *sb,
                                        tiku_kits_ui_widget_t *left,
                                        tiku_kits_ui_widget_t *centre,
                                        tiku_kits_ui_widget_t *right);

#endif /* TIKU_KITS_UI_STATUS_BAR_H_ */
