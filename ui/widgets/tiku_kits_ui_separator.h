/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_separator.h - Decorative separator line
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_SEPARATOR_H_
#define TIKU_KITS_UI_SEPARATOR_H_

#include "../tiku_kits_ui.h"

typedef enum {
    TIKU_KITS_UI_SEP_HORIZONTAL = 0,
    TIKU_KITS_UI_SEP_VERTICAL   = 1,
} tiku_kits_ui_separator_dir_t;

typedef struct {
    tiku_kits_ui_widget_t base;
    tiku_kits_ui_separator_dir_t dir;
    uint8_t thickness;
} tiku_kits_ui_separator_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_separator_ops;

void tiku_kits_ui_separator_init(tiku_kits_ui_separator_t *sep,
                                  int16_t x, int16_t y,
                                  uint16_t w, uint16_t h,
                                  tiku_kits_ui_separator_dir_t dir,
                                  uint8_t thickness);

#endif /* TIKU_KITS_UI_SEPARATOR_H_ */
