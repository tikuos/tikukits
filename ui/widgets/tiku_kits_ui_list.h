/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_list.h - Vertical scroll-tracking list widget
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Holds N text rows. INC/DEC events move the highlighted row;
 * ACTIVATE fires `on_select(selected_idx)`. The widget tracks an
 * internal scroll offset so the selected row stays visible when
 * the list overflows the widget rect.
 *
 * Caller-allocated rows array; the list keeps a pointer to it.
 */

#ifndef TIKU_KITS_UI_LIST_H_
#define TIKU_KITS_UI_LIST_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

typedef void (*tiku_kits_ui_list_cb_t)(int8_t idx, void *user_data);

typedef struct {
    tiku_kits_ui_widget_t base;
    const char * const *rows;     /* caller-owned array of strings */
    uint8_t   n_rows;
    int8_t    selected;
    int8_t    scroll_offset;
    const tiku_kits_gfx_font_t *font;
    uint8_t   scale;
    uint8_t   row_h;              /* per-row height in px (0 = auto) */
    tiku_kits_ui_list_cb_t on_select;
} tiku_kits_ui_list_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_list_ops;

void tiku_kits_ui_list_init(tiku_kits_ui_list_t *lst,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             const char * const *rows, uint8_t n_rows,
                             const tiku_kits_gfx_font_t *font,
                             uint8_t scale,
                             tiku_kits_ui_list_cb_t on_select,
                             void *user_data);

void tiku_kits_ui_list_set_selected(tiku_kits_ui_list_t *lst, int8_t idx);

#endif /* TIKU_KITS_UI_LIST_H_ */
