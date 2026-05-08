/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_menu.h - Menu widget
 *
 * A list of label / callback rows. INC/DEC select rows; ACTIVATE
 * fires the selected row's callback. Layout, scrolling and visual
 * style mirror tiku_kits_ui_list -- the difference is the per-row
 * callback rather than a single "on_select" handler.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_MENU_H_
#define TIKU_KITS_UI_MENU_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

#ifndef TIKU_KITS_UI_MENU_MAX_ITEMS
#define TIKU_KITS_UI_MENU_MAX_ITEMS 12
#endif

typedef void (*tiku_kits_ui_menu_action_t)(void *user_data);

typedef struct {
    const char *label;
    tiku_kits_ui_menu_action_t on_click;
    void *user_data;
} tiku_kits_ui_menu_item_t;

typedef struct {
    tiku_kits_ui_widget_t base;
    tiku_kits_ui_menu_item_t items[TIKU_KITS_UI_MENU_MAX_ITEMS];
    uint8_t n_items;
    int8_t  selected;
    int8_t  scroll_offset;
    const tiku_kits_gfx_font_t *font;
    uint8_t scale;
} tiku_kits_ui_menu_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_menu_ops;

void tiku_kits_ui_menu_init(tiku_kits_ui_menu_t *m,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             const tiku_kits_gfx_font_t *font,
                             uint8_t scale);

/** Append a menu item. Returns TIKU_KITS_UI_OK on success. */
int tiku_kits_ui_menu_add(tiku_kits_ui_menu_t *m,
                          const char *label,
                          tiku_kits_ui_menu_action_t on_click,
                          void *user_data);

void tiku_kits_ui_menu_clear(tiku_kits_ui_menu_t *m);

#endif /* TIKU_KITS_UI_MENU_H_ */
