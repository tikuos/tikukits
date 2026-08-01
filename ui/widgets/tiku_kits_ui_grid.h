/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_grid.h - 2D grid layout container
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Lays children out on an N_rows x N_cols grid with equal-sized
 * tracks. Each child specifies its (row, col) plus optional spans.
 * Cells without a child render as blank space.
 *
 * Focus navigation honours grid topology: FOCUS_DOWN moves to the
 * focusable child with the next-greater row and a row-aligned (or
 * nearest) column; FOCUS_UP/LEFT/RIGHT do the obvious thing. When
 * direction navigation hits the grid edge, handle_event returns 0
 * so the containing window can rotate to a sibling widget.
 */

#ifndef TIKU_KITS_UI_GRID_H_
#define TIKU_KITS_UI_GRID_H_

#include "../tiku_kits_ui.h"

#ifndef TIKU_KITS_UI_GRID_MAX_CHILDREN
#define TIKU_KITS_UI_GRID_MAX_CHILDREN 12
#endif

typedef struct {
    tiku_kits_ui_widget_t base;
    tiku_kits_ui_widget_t *children[TIKU_KITS_UI_GRID_MAX_CHILDREN];
    uint8_t   rows[TIKU_KITS_UI_GRID_MAX_CHILDREN];
    uint8_t   cols[TIKU_KITS_UI_GRID_MAX_CHILDREN];
    uint8_t   row_spans[TIKU_KITS_UI_GRID_MAX_CHILDREN];
    uint8_t   col_spans[TIKU_KITS_UI_GRID_MAX_CHILDREN];
    uint8_t   n_children;
    uint8_t   n_rows;
    uint8_t   n_cols;
    uint8_t   pad_x;
    uint8_t   pad_y;
    uint8_t   spacing_x;
    uint8_t   spacing_y;
    int8_t    focus_idx;
} tiku_kits_ui_grid_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_grid_ops;

void tiku_kits_ui_grid_init(tiku_kits_ui_grid_t *g,
                             int16_t x, int16_t y,
                             uint16_t w, uint16_t h,
                             uint8_t n_rows, uint8_t n_cols);

/** Place @p child at (row, col) spanning (row_span, col_span)
 *  cells. Pass spans = 1 for a single cell. */
int tiku_kits_ui_grid_add(tiku_kits_ui_grid_t *g,
                           tiku_kits_ui_widget_t *child,
                           uint8_t row, uint8_t col,
                           uint8_t row_span, uint8_t col_span);

void tiku_kits_ui_grid_set_padding(tiku_kits_ui_grid_t *g,
                                    uint8_t pad_x, uint8_t pad_y);

void tiku_kits_ui_grid_set_spacing(tiku_kits_ui_grid_t *g,
                                    uint8_t spacing_x, uint8_t spacing_y);

#endif /* TIKU_KITS_UI_GRID_H_ */
