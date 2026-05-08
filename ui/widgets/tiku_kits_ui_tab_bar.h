/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_tab_bar.h - Horizontal segmented tab selector
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_TAB_BAR_H_
#define TIKU_KITS_UI_TAB_BAR_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

#ifndef TIKU_KITS_UI_TAB_BAR_MAX_TABS
#define TIKU_KITS_UI_TAB_BAR_MAX_TABS 6
#endif

typedef void (*tiku_kits_ui_tab_bar_cb_t)(int8_t idx, void *user_data);

typedef struct {
    tiku_kits_ui_widget_t base;
    const char *labels[TIKU_KITS_UI_TAB_BAR_MAX_TABS];
    uint8_t n_tabs;
    int8_t  selected;
    const tiku_kits_gfx_font_t *font;
    uint8_t scale;
    tiku_kits_ui_tab_bar_cb_t on_change;
} tiku_kits_ui_tab_bar_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_tab_bar_ops;

void tiku_kits_ui_tab_bar_init(tiku_kits_ui_tab_bar_t *tb,
                                int16_t x, int16_t y,
                                uint16_t w, uint16_t h,
                                const tiku_kits_gfx_font_t *font,
                                uint8_t scale,
                                tiku_kits_ui_tab_bar_cb_t on_change,
                                void *user_data);

int tiku_kits_ui_tab_bar_add(tiku_kits_ui_tab_bar_t *tb, const char *label);

void tiku_kits_ui_tab_bar_set_selected(tiku_kits_ui_tab_bar_t *tb,
                                        int8_t idx);

#endif /* TIKU_KITS_UI_TAB_BAR_H_ */
