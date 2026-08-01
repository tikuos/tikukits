/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_box.c - vbox / hbox impl
 *
 * Both directions share this implementation; the .dir field selects
 * which axis is "main" (children laid along) and which is "cross"
 * (children stretched / aligned across).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_box.h"
#include "../tiku_kits_ui_theme.h"

/*---------------------------------------------------------------------------*/
/* HELPERS                                                                   */
/*---------------------------------------------------------------------------*/

static int
is_vertical(const tiku_kits_ui_box_t *box)
{
    return box->dir == (uint8_t)TIKU_KITS_UI_BOX_VERTICAL;
}

/* Compute total flex weight of all children with flex > 0. */
static uint16_t
total_flex(const tiku_kits_ui_box_t *box)
{
    uint16_t i, sum = 0;
    for (i = 0; i < box->n_children; i++) {
        sum = (uint16_t)(sum + box->flex[i]);
    }
    return sum;
}

/* Compute the natural main-axis size of a child (with no flex). */
static uint16_t
child_main_size(const tiku_kits_ui_box_t *box,
                 tiku_kits_ui_widget_t *c)
{
    tiku_kits_gfx_size_t s;
    uint16_t avail_cross = is_vertical(box)
                            ? (uint16_t)(box->base.w - 2u * box->pad_x)
                            : (uint16_t)(box->base.h - 2u * box->pad_y);
    tiku_kits_ui_widget_intrinsic_size(c, avail_cross, &s);
    return is_vertical(box) ? s.h : s.w;
}

static int8_t
find_focusable(const tiku_kits_ui_box_t *box, int8_t start, int8_t step)
{
    int8_t n = (int8_t)box->n_children;
    int8_t i = start;
    int8_t tries;
    if (n == 0) return -1;
    for (tries = 0; tries < n; tries++) {
        i = (int8_t)(i + step);
        if (i < 0)  return -1;
        if (i >= n) return -1;
        if (tiku_kits_ui_widget_is_focusable(box->children[i])) return i;
    }
    return -1;
}

static void
set_internal_focus(tiku_kits_ui_box_t *box, int8_t idx)
{
    int8_t i;
    for (i = 0; i < (int8_t)box->n_children; i++) {
        box->children[i]->focused = (i == idx) ? 1u : 0u;
    }
    box->focus_idx = idx;
}

/*---------------------------------------------------------------------------*/
/* LAYOUT + RENDER                                                           */
/*---------------------------------------------------------------------------*/

static void
box_render(const tiku_kits_ui_widget_t *base,
            const tiku_kits_gfx_surface_t *s)
{
    tiku_kits_ui_box_t *box = (tiku_kits_ui_box_t *)base;
    uint16_t i;

    /* --- Compute fixed total + remaining flex space. --- */
    uint16_t main_size = is_vertical(box) ? base->h : base->w;
    uint16_t cross_size = is_vertical(box) ? base->w : base->h;
    uint16_t main_pad   = is_vertical(box) ? box->pad_y : box->pad_x;
    uint16_t cross_pad  = is_vertical(box) ? box->pad_x : box->pad_y;

    uint16_t avail = (uint16_t)(main_size - 2u * main_pad);
    uint16_t spacing_total = (box->n_children > 1u)
        ? (uint16_t)((box->n_children - 1u) * box->spacing) : 0u;
    if (spacing_total > avail) spacing_total = avail;
    uint16_t inner = (uint16_t)(avail - spacing_total);

    uint16_t fixed_total = 0;
    uint16_t flex_sum    = total_flex(box);
    for (i = 0; i < box->n_children; i++) {
        if (box->flex[i] == 0u) {
            fixed_total = (uint16_t)(fixed_total + child_main_size(box,
                                                    box->children[i]));
        }
    }
    uint16_t flex_pool = (inner > fixed_total)
        ? (uint16_t)(inner - fixed_total) : 0u;

    /* --- Walk children, assigning (x, y, w, h) and rendering. --- */
    int16_t cursor = (int16_t)(is_vertical(box)
                                ? base->y + (int16_t)main_pad
                                : base->x + (int16_t)main_pad);
    int16_t cross_origin = (int16_t)(is_vertical(box)
                                ? base->x + (int16_t)cross_pad
                                : base->y + (int16_t)cross_pad);
    uint16_t cross_inner = (uint16_t)(cross_size - 2u * cross_pad);

    for (i = 0; i < box->n_children; i++) {
        tiku_kits_ui_widget_t *c = box->children[i];
        uint16_t my_main;
        tiku_kits_gfx_size_t isz;
        uint16_t my_cross;

        if (box->flex[i] > 0u && flex_sum > 0u) {
            my_main = (uint16_t)((uint32_t)flex_pool * box->flex[i]
                                  / flex_sum);
        } else {
            my_main = child_main_size(box, c);
        }
        if (my_main > avail) my_main = avail;

        /* Determine cross-axis size + alignment. */
        tiku_kits_ui_widget_intrinsic_size(c, cross_inner, &isz);
        my_cross = is_vertical(box) ? isz.w : isz.h;
        if (my_cross > cross_inner) my_cross = cross_inner;

        {
            int16_t cross_pos = cross_origin;
            switch ((tiku_kits_gfx_align_t)box->cross_align) {
            case TIKU_KITS_GFX_ALIGN_CENTER:
                if (my_cross < cross_inner) {
                    cross_pos = (int16_t)(cross_origin +
                        (int16_t)((cross_inner - my_cross) / 2u));
                }
                break;
            case TIKU_KITS_GFX_ALIGN_RIGHT:
                if (my_cross < cross_inner) {
                    cross_pos = (int16_t)(cross_origin +
                        (int16_t)(cross_inner - my_cross));
                }
                break;
            default:
                break;
            }

            if (is_vertical(box)) {
                c->x = cross_pos;
                c->y = cursor;
                c->w = my_cross;
                c->h = my_main;
            } else {
                c->x = cursor;
                c->y = cross_pos;
                c->w = my_main;
                c->h = my_cross;
            }
        }

        tiku_kits_ui_widget_render(c, s);

        cursor = (int16_t)(cursor + (int16_t)my_main +
            (i + 1u < box->n_children ? (int16_t)box->spacing : 0));
    }
}

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

static int
box_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_box_t *box = (tiku_kits_ui_box_t *)base;
    int axis_match;
    int step = 0;

    if (box->n_children == 0u) return 0;

    /* Decide whether the requested event maps to the main axis. */
    if (is_vertical(box)) {
        if (evt == TIKU_KITS_UI_EVT_FOCUS_DOWN) { axis_match = 1; step = 1; }
        else if (evt == TIKU_KITS_UI_EVT_FOCUS_UP) { axis_match = 1; step = -1; }
        else { axis_match = 0; }
    } else {
        if (evt == TIKU_KITS_UI_EVT_FOCUS_RIGHT) { axis_match = 1; step = 1; }
        else if (evt == TIKU_KITS_UI_EVT_FOCUS_LEFT) { axis_match = 1; step = -1; }
        else { axis_match = 0; }
    }

    if (axis_match) {
        int8_t next = find_focusable(box, box->focus_idx, (int8_t)step);
        if (next < 0) return 0;          /* Let parent rotate. */
        set_internal_focus(box, next);
        return 1;
    }

    /* Forward other events to the internally focused child. */
    if (box->focus_idx >= 0 && box->focus_idx < (int8_t)box->n_children) {
        tiku_kits_ui_widget_t *fw = box->children[box->focus_idx];
        if (fw->ops->handle_event != NULL) {
            return fw->ops->handle_event(fw, evt);
        }
    }
    return 0;
}

static int
box_is_focusable(const tiku_kits_ui_widget_t *base)
{
    const tiku_kits_ui_box_t *box = (const tiku_kits_ui_box_t *)base;
    uint8_t i;
    for (i = 0; i < box->n_children; i++) {
        if (tiku_kits_ui_widget_is_focusable(box->children[i])) return 1;
    }
    return 0;
}

static void
box_intrinsic_size(const tiku_kits_ui_widget_t *base,
                    uint16_t avail_w,
                    tiku_kits_gfx_size_t *out)
{
    const tiku_kits_ui_box_t *box = (const tiku_kits_ui_box_t *)base;
    uint16_t i;
    uint32_t main_sum = 0;
    uint16_t cross_max = 0;

    for (i = 0; i < box->n_children; i++) {
        tiku_kits_gfx_size_t cs;
        tiku_kits_ui_widget_intrinsic_size(box->children[i], avail_w, &cs);
        if (is_vertical(box)) {
            main_sum  += cs.h;
            if (cs.w > cross_max) cross_max = cs.w;
        } else {
            main_sum  += cs.w;
            if (cs.h > cross_max) cross_max = cs.h;
        }
    }
    if (box->n_children > 1u) {
        main_sum += (uint32_t)(box->n_children - 1u) * box->spacing;
    }
    if (is_vertical(box)) {
        main_sum += 2u * box->pad_y;
        cross_max = (uint16_t)(cross_max + 2u * box->pad_x);
        out->w = cross_max;
        out->h = (main_sum > 0xFFFFu) ? 0xFFFFu : (uint16_t)main_sum;
    } else {
        main_sum += 2u * box->pad_x;
        cross_max = (uint16_t)(cross_max + 2u * box->pad_y);
        out->w = (main_sum > 0xFFFFu) ? 0xFFFFu : (uint16_t)main_sum;
        out->h = cross_max;
    }
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_box_ops = {
    .render         = box_render,
    .handle_event   = box_handle_event,
    .is_focusable   = box_is_focusable,
    .intrinsic_size = box_intrinsic_size,
};

/*---------------------------------------------------------------------------*/
/* INIT + CONFIG                                                             */
/*---------------------------------------------------------------------------*/

static void
box_init_common(tiku_kits_ui_box_t *box,
                 int16_t x, int16_t y, uint16_t w, uint16_t h,
                 tiku_kits_ui_box_dir_t dir)
{
    if (box == NULL) return;
    box->base.ops      = &tiku_kits_ui_box_ops;
    box->base.x        = x;
    box->base.y        = y;
    box->base.w        = w;
    box->base.h        = h;
    box->base.visible  = 1;
    box->base.focused  = 0;
    box->base.dirty  = 0;
    box->base.user_data = NULL;
    box->n_children    = 0;
    box->dir           = (uint8_t)dir;
    box->pad_x         = 0;
    box->pad_y         = 0;
    box->spacing       = 0;
    box->cross_align   = (uint8_t)TIKU_KITS_GFX_ALIGN_LEFT;
    box->focus_idx     = -1;
}

void
tiku_kits_ui_vbox_init(tiku_kits_ui_box_t *box,
                        int16_t x, int16_t y,
                        uint16_t w, uint16_t h)
{
    box_init_common(box, x, y, w, h, TIKU_KITS_UI_BOX_VERTICAL);
}

void
tiku_kits_ui_hbox_init(tiku_kits_ui_box_t *box,
                        int16_t x, int16_t y,
                        uint16_t w, uint16_t h)
{
    box_init_common(box, x, y, w, h, TIKU_KITS_UI_BOX_HORIZONTAL);
}

int
tiku_kits_ui_box_add(tiku_kits_ui_box_t *box,
                      tiku_kits_ui_widget_t *child,
                      uint8_t flex)
{
    if (box == NULL || child == NULL) return TIKU_KITS_UI_ERR_PARAM;
    if (box->n_children >= TIKU_KITS_UI_BOX_MAX_CHILDREN) {
        return TIKU_KITS_UI_ERR_FULL;
    }
    box->children[box->n_children] = child;
    box->flex[box->n_children]     = flex;
    box->n_children++;
    if (box->focus_idx < 0 && tiku_kits_ui_widget_is_focusable(child)) {
        set_internal_focus(box, (int8_t)(box->n_children - 1));
    }
    return TIKU_KITS_UI_OK;
}

void
tiku_kits_ui_box_set_padding(tiku_kits_ui_box_t *box,
                              uint8_t pad_x, uint8_t pad_y)
{
    if (box == NULL) return;
    box->pad_x = pad_x;
    box->pad_y = pad_y;
}

void
tiku_kits_ui_box_set_spacing(tiku_kits_ui_box_t *box, uint8_t spacing)
{
    if (box == NULL) return;
    box->spacing = spacing;
}

void
tiku_kits_ui_box_set_cross_align(tiku_kits_ui_box_t *box,
                                   tiku_kits_gfx_align_t align)
{
    if (box == NULL) return;
    box->cross_align = (uint8_t)align;
}
