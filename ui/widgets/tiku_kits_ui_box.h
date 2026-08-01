/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_box.h - Vertical / horizontal layout container
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * vbox stacks children top-to-bottom; hbox stacks them left-to-right.
 * Both share this struct -- the orientation is set at init via
 * `vbox_init` or `hbox_init`. Each child is given a position computed
 * from the box's own (w, h), padding, spacing, and per-child flex
 * weight.
 *
 * Layout algorithm (vbox shown; hbox swaps the axes):
 * 1. Compute available height = h - 2*pad_y - (n - 1)*spacing.
 * 2. For each child with flex == 0, ask for its intrinsic_size
 * and reserve that many pixels.
 * 3. Distribute the remaining space across flex children in
 * proportion to their flex weights.
 * 4. Walk children top-to-bottom assigning x = pad_x +
 * cross_align offset and y = next free pixel.
 *
 * Cross-axis alignment (e.g. horizontal alignment of children
 * inside a vbox) is controlled by `cross_align`; LEFT puts children
 * flush left, CENTER centres each child within (w - 2*pad_x),
 * RIGHT pushes them flush right. Children are NEVER stretched
 * across the cross axis -- they keep their declared (or intrinsic)
 * width.
 *
 * Focus traversal: when the box is focused, FOCUS_DOWN/UP for vbox
 * (FOCUS_RIGHT/LEFT for hbox) advances internal focus among
 * focusable children. ACTIVATE / INC / DEC / BACK / MENU / LONG_PRESS
 * are forwarded to the internally focused child. Any non-axis
 * navigation event (e.g. FOCUS_LEFT in a vbox) returns 0 so the
 * containing window can rotate to a sibling.
 */

#ifndef TIKU_KITS_UI_BOX_H_
#define TIKU_KITS_UI_BOX_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

#ifndef TIKU_KITS_UI_BOX_MAX_CHILDREN
#define TIKU_KITS_UI_BOX_MAX_CHILDREN 8
#endif

typedef enum {
    TIKU_KITS_UI_BOX_VERTICAL   = 0,
    TIKU_KITS_UI_BOX_HORIZONTAL = 1,
} tiku_kits_ui_box_dir_t;

typedef struct {
    tiku_kits_ui_widget_t base;
    tiku_kits_ui_widget_t *children[TIKU_KITS_UI_BOX_MAX_CHILDREN];
    uint8_t   flex[TIKU_KITS_UI_BOX_MAX_CHILDREN];
    uint8_t   n_children;
    uint8_t   dir;            /* tiku_kits_ui_box_dir_t */
    uint8_t   pad_x;
    uint8_t   pad_y;
    uint8_t   spacing;
    uint8_t   cross_align;    /* tiku_kits_gfx_align_t */
    int8_t    focus_idx;
} tiku_kits_ui_box_t;

/* Convenience aliases. */
typedef tiku_kits_ui_box_t tiku_kits_ui_vbox_t;
typedef tiku_kits_ui_box_t tiku_kits_ui_hbox_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_box_ops;

/*---------------------------------------------------------------------------*/
/* INIT                                                                      */
/*---------------------------------------------------------------------------*/

void tiku_kits_ui_vbox_init(tiku_kits_ui_box_t *box,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h);

void tiku_kits_ui_hbox_init(tiku_kits_ui_box_t *box,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/** Add a child to the box. @p flex == 0 means "fixed size, take
 *  intrinsic_size or w/h". @p flex > 0 means "after fixed children
 *  are placed, distribute remaining space proportional to flex". */
int tiku_kits_ui_box_add(tiku_kits_ui_box_t *box,
                          tiku_kits_ui_widget_t *child,
                          uint8_t flex);

void tiku_kits_ui_box_set_padding(tiku_kits_ui_box_t *box,
                                   uint8_t pad_x, uint8_t pad_y);

void tiku_kits_ui_box_set_spacing(tiku_kits_ui_box_t *box,
                                   uint8_t spacing);

/** Cross-axis alignment for children that are smaller than the
 *  box on the cross axis. */
void tiku_kits_ui_box_set_cross_align(tiku_kits_ui_box_t *box,
                                       tiku_kits_gfx_align_t align);

#endif /* TIKU_KITS_UI_BOX_H_ */
