/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_progress.h - Determinate progress bar widget
 *
 * Non-focusable: progress is for status display, not interaction.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_PROGRESS_H_
#define TIKU_KITS_UI_PROGRESS_H_

#include "../tiku_kits_ui.h"

typedef struct {
    tiku_kits_ui_widget_t base;
    int16_t value;
    int16_t max;
} tiku_kits_ui_progress_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_progress_ops;

void tiku_kits_ui_progress_init(tiku_kits_ui_progress_t *p,
                                 int16_t x, int16_t y,
                                 uint16_t w, uint16_t h,
                                 int16_t value, int16_t max);

void tiku_kits_ui_progress_set(tiku_kits_ui_progress_t *p, int16_t value);

#endif /* TIKU_KITS_UI_PROGRESS_H_ */
