/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_grid.c - 2D grid layout impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tracks are equal-sized; each cell (r, c) of an N_rows x N_cols
 * grid gets the same width and height. Children specify their
 * top-left cell and optional spans; the layout computes pixel
 * (x, y, w, h) at render time and renders each child.
 *
 * Direction navigation is grid-coordinate-aware: rather than
 * computing centroids, we walk children by their (row, col)
 * indices. This keeps navigation predictable in regular grids
 * and avoids floating-point distance math.
 */

#include "tiku_kits_ui_grid.h"
#include "../tiku_kits_ui_theme.h"

/*---------------------------------------------------------------------------*/
/* HELPERS                                                                   */
/*---------------------------------------------------------------------------*/

/* Pixel coordinates of cell (r, c) of size (rows x cols) within
 * the grid's content area. */
static void
cell_rect(const tiku_kits_ui_grid_t *g,
           uint8_t r, uint8_t c, uint8_t rs, uint8_t cs,
           int16_t *out_x, int16_t *out_y,
           uint16_t *out_w, uint16_t *out_h)
{
    uint16_t inner_w = (uint16_t)(g->base.w - 2u * g->pad_x);
    uint16_t inner_h = (uint16_t)(g->base.h - 2u * g->pad_y);
    uint16_t gap_x   = (g->n_cols > 1u)
        ? (uint16_t)((g->n_cols - 1u) * g->spacing_x) : 0u;
    uint16_t gap_y   = (g->n_rows > 1u)
        ? (uint16_t)((g->n_rows - 1u) * g->spacing_y) : 0u;
    uint16_t cell_w  = (g->n_cols > 0u && inner_w > gap_x)
        ? (uint16_t)((inner_w - gap_x) / g->n_cols) : 0u;
    uint16_t cell_h  = (g->n_rows > 0u && inner_h > gap_y)
        ? (uint16_t)((inner_h - gap_y) / g->n_rows) : 0u;

    *out_x = (int16_t)(g->base.x + (int16_t)g->pad_x +
        (int16_t)c * (int16_t)(cell_w + g->spacing_x));
    *out_y = (int16_t)(g->base.y + (int16_t)g->pad_y +
        (int16_t)r * (int16_t)(cell_h + g->spacing_y));
    *out_w = (uint16_t)(cs * cell_w + (cs > 0u ? (cs - 1u) * g->spacing_x : 0u));
    *out_h = (uint16_t)(rs * cell_h + (rs > 0u ? (rs - 1u) * g->spacing_y : 0u));
}

static int
abs_int(int v) { return v < 0 ? -v : v; }

/* For direction nav: find focusable child best matching (drow, dcol)
 * starting from current_idx. drow / dcol are -1, 0, or +1 indicating
 * the requested direction. */
static int8_t
find_in_dir(const tiku_kits_ui_grid_t *g, int8_t current,
             int drow, int dcol)
{
    uint8_t i;
    int  cur_r = (current >= 0) ? g->rows[current] : -1;
    int  cur_c = (current >= 0) ? g->cols[current] : -1;
    int  best_score = INT16_MAX;
    int8_t best = -1;

    for (i = 0; i < g->n_children; i++) {
        int rr, cc, dr, dc, score;
        if ((int8_t)i == current) continue;
        if (!tiku_kits_ui_widget_is_focusable(g->children[i])) continue;
        rr = g->rows[i];
        cc = g->cols[i];
        dr = rr - cur_r;
        dc = cc - cur_c;

        /* Must move in the requested primary direction. */
        if (drow ==  1 && dr <= 0) continue;
        if (drow == -1 && dr >= 0) continue;
        if (dcol ==  1 && dc <= 0) continue;
        if (dcol == -1 && dc >= 0) continue;

        /* Score = primary distance + 4 * cross-axis offset. */
        if (drow != 0) {
            score = abs_int(dr) + 4 * abs_int(dc);
        } else {
            score = abs_int(dc) + 4 * abs_int(dr);
        }
        if (score < best_score) {
            best_score = score;
            best = (int8_t)i;
        }
    }
    return best;
}

static void
set_internal_focus(tiku_kits_ui_grid_t *g, int8_t idx)
{
    int8_t i;
    for (i = 0; i < (int8_t)g->n_children; i++) {
        g->children[i]->focused = (i == idx) ? 1u : 0u;
    }
    g->focus_idx = idx;
}

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static void
grid_render(const tiku_kits_ui_widget_t *base,
             const tiku_kits_gfx_surface_t *s)
{
    tiku_kits_ui_grid_t *g = (tiku_kits_ui_grid_t *)base;
    uint8_t i;
    for (i = 0; i < g->n_children; i++) {
        tiku_kits_ui_widget_t *c = g->children[i];
        int16_t  cx, cy;
        uint16_t cw, ch;
        cell_rect(g, g->rows[i], g->cols[i],
                  g->row_spans[i], g->col_spans[i],
                  &cx, &cy, &cw, &ch);
        c->x = cx;
        c->y = cy;
        c->w = cw;
        c->h = ch;
        tiku_kits_ui_widget_render(c, s);
    }
}

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

static int
grid_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_grid_t *g = (tiku_kits_ui_grid_t *)base;
    int drow = 0, dcol = 0;

    if (g->n_children == 0u) return 0;

    switch (evt) {
    case TIKU_KITS_UI_EVT_FOCUS_DOWN:  drow = 1;  break;
    case TIKU_KITS_UI_EVT_FOCUS_UP:    drow = -1; break;
    case TIKU_KITS_UI_EVT_FOCUS_RIGHT: dcol = 1;  break;
    case TIKU_KITS_UI_EVT_FOCUS_LEFT:  dcol = -1; break;
    default:
        /* Forward to focused child. */
        if (g->focus_idx >= 0 && g->focus_idx < (int8_t)g->n_children) {
            tiku_kits_ui_widget_t *fw = g->children[g->focus_idx];
            if (fw->ops->handle_event != NULL) {
                return fw->ops->handle_event(fw, evt);
            }
        }
        return 0;
    }

    {
        int8_t next = find_in_dir(g, g->focus_idx, drow, dcol);
        if (next < 0) return 0;
        set_internal_focus(g, next);
        return 1;
    }
}

static int
grid_is_focusable(const tiku_kits_ui_widget_t *base)
{
    const tiku_kits_ui_grid_t *g = (const tiku_kits_ui_grid_t *)base;
    uint8_t i;
    for (i = 0; i < g->n_children; i++) {
        if (tiku_kits_ui_widget_is_focusable(g->children[i])) return 1;
    }
    return 0;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_grid_ops = {
    .render         = grid_render,
    .handle_event   = grid_handle_event,
    .is_focusable   = grid_is_focusable,
    .intrinsic_size = NULL,
};

/*---------------------------------------------------------------------------*/
/* INIT + CONFIG                                                             */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_grid_init(tiku_kits_ui_grid_t *g,
                        int16_t x, int16_t y,
                        uint16_t w, uint16_t h,
                        uint8_t n_rows, uint8_t n_cols)
{
    if (g == NULL) return;
    g->base.ops      = &tiku_kits_ui_grid_ops;
    g->base.x        = x;
    g->base.y        = y;
    g->base.w        = w;
    g->base.h        = h;
    g->base.visible  = 1;
    g->base.focused  = 0;
    g->base.dirty  = 0;
    g->base.user_data = NULL;
    g->n_children    = 0;
    g->n_rows        = (n_rows > 0) ? n_rows : 1;
    g->n_cols        = (n_cols > 0) ? n_cols : 1;
    g->pad_x         = 0;
    g->pad_y         = 0;
    g->spacing_x     = 0;
    g->spacing_y     = 0;
    g->focus_idx     = -1;
}

int
tiku_kits_ui_grid_add(tiku_kits_ui_grid_t *g,
                       tiku_kits_ui_widget_t *child,
                       uint8_t row, uint8_t col,
                       uint8_t row_span, uint8_t col_span)
{
    if (g == NULL || child == NULL) return TIKU_KITS_UI_ERR_PARAM;
    if (row >= g->n_rows || col >= g->n_cols) return TIKU_KITS_UI_ERR_PARAM;
    if (g->n_children >= TIKU_KITS_UI_GRID_MAX_CHILDREN) {
        return TIKU_KITS_UI_ERR_FULL;
    }
    g->children[g->n_children]  = child;
    g->rows[g->n_children]      = row;
    g->cols[g->n_children]      = col;
    g->row_spans[g->n_children] = (row_span > 0) ? row_span : 1;
    g->col_spans[g->n_children] = (col_span > 0) ? col_span : 1;
    g->n_children++;
    if (g->focus_idx < 0 && tiku_kits_ui_widget_is_focusable(child)) {
        set_internal_focus(g, (int8_t)(g->n_children - 1));
    }
    return TIKU_KITS_UI_OK;
}

void
tiku_kits_ui_grid_set_padding(tiku_kits_ui_grid_t *g,
                                uint8_t pad_x, uint8_t pad_y)
{
    if (g == NULL) return;
    g->pad_x = pad_x;
    g->pad_y = pad_y;
}

void
tiku_kits_ui_grid_set_spacing(tiku_kits_ui_grid_t *g,
                                uint8_t spacing_x, uint8_t spacing_y)
{
    if (g == NULL) return;
    g->spacing_x = spacing_x;
    g->spacing_y = spacing_y;
}
