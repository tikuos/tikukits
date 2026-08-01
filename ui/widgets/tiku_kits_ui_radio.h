/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_radio.h - Radio button + radio group
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Mutually exclusive selection across N radio widgets that share a
 * single tiku_kits_ui_radio_group_t. The group tracks which index
 * is currently selected; activating any button updates the group's
 * selection and re-renders all members on the next paint.
 *
 * Caller layout:
 *
 * static tiku_kits_ui_radio_group_t my_group;
 * static tiku_kits_ui_radio_t       a, b, c;
 *
 * tiku_kits_ui_radio_group_init(&my_group, 0, on_change, NULL);
 * tiku_kits_ui_radio_init(&a, ..., "Option A", &my_group, 0);
 * tiku_kits_ui_radio_init(&b, ..., "Option B", &my_group, 1);
 * tiku_kits_ui_radio_init(&c, ..., "Option C", &my_group, 2);
 */

#ifndef TIKU_KITS_UI_RADIO_H_
#define TIKU_KITS_UI_RADIO_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

/*---------------------------------------------------------------------------*/
/* GROUP                                                                     */
/*---------------------------------------------------------------------------*/

typedef void (*tiku_kits_ui_radio_cb_t)(int8_t selected_idx, void *user_data);

typedef struct {
    int8_t selected_idx;        /* -1 = none */
    tiku_kits_ui_radio_cb_t on_change;
    void *user_data;
} tiku_kits_ui_radio_group_t;

void tiku_kits_ui_radio_group_init(tiku_kits_ui_radio_group_t *grp,
                                    int8_t initial_selected,
                                    tiku_kits_ui_radio_cb_t on_change,
                                    void *user_data);

/** Programmatic selection. Does NOT fire on_change. */
void tiku_kits_ui_radio_group_select(tiku_kits_ui_radio_group_t *grp,
                                      int8_t idx);

/*---------------------------------------------------------------------------*/
/* RADIO BUTTON                                                              */
/*---------------------------------------------------------------------------*/

typedef struct {
    tiku_kits_ui_widget_t base;
    const char *label;
    const tiku_kits_gfx_font_t *font;
    uint8_t scale;
    tiku_kits_ui_radio_group_t *group;
    int8_t group_idx;
} tiku_kits_ui_radio_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_radio_ops;

void tiku_kits_ui_radio_init(tiku_kits_ui_radio_t *rb,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h,
                              const char *label,
                              const tiku_kits_gfx_font_t *font,
                              uint8_t scale,
                              tiku_kits_ui_radio_group_t *group,
                              int8_t group_idx);

#endif /* TIKU_KITS_UI_RADIO_H_ */
