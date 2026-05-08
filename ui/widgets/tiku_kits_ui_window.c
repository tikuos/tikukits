/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_window.c - Window container implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_window.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

/*---------------------------------------------------------------------------*/
/* TITLE TAB GEOMETRY                                                        */
/*---------------------------------------------------------------------------*/

/* Tab height = font height * scale + 2 px vertical padding on each
 * side. Tab projects ABOVE the window frame so children get the
 * full window content area below the frame's top edge. */
static uint16_t
tab_height(const tiku_kits_ui_window_t *win)
{
    if (win->title == NULL || win->title_font == NULL) return 0;
    return (uint16_t)(win->title_font->height * win->title_scale + 4u);
}

/* Tab width = text width + 8 px horizontal padding. Pinned to a
 * minimum of 24 px so empty / single-character titles still look
 * like a tab and not a dot. */
static uint16_t
tab_width(const tiku_kits_ui_window_t *win)
{
    uint16_t tw;
    if (win->title == NULL || win->title_font == NULL) return 0;
    tw = tiku_kits_gfx_text_width(win->title, win->title_font, win->title_scale);
    tw = (uint16_t)(tw + 8u);
    if (tw < 24u) tw = 24u;
    return tw;
}

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static void
window_render(const tiku_kits_ui_widget_t *base,
               const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_window_t *win = (const tiku_kits_ui_window_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint16_t th  = tab_height(win);
    uint16_t tw  = tab_width(win);
    int16_t  fx  = base->x;
    int16_t  fy  = (int16_t)(base->y + th);     /* frame starts below tab */
    uint16_t fw  = base->w;
    uint16_t fh  = (uint16_t)(base->h - th);
    uint8_t  i;

    /* --- Title region: tab (BeOS) or full-width bar (flat) --- */
    if (th > 0) {
        if (t->flags & TIKU_KITS_UI_THEME_FLAG_TAB_TITLE) {
            tiku_kits_gfx_fill_round_rect(s,
                fx, base->y, tw, th, t->corner_r,
                t->color_accent);
            tiku_kits_gfx_draw_string(s,
                (int16_t)(fx + 4), (int16_t)(base->y + 2),
                win->title, win->title_font,
                t->color_fg, win->title_scale);
        } else {
            /* Full-width title bar in accent colour. */
            tiku_kits_gfx_fill_rect(s,
                fx, base->y, fw, th, t->color_accent);
            tiku_kits_gfx_draw_string(s,
                (int16_t)(fx + 4), (int16_t)(base->y + 2),
                win->title, win->title_font,
                t->color_fg, win->title_scale);
        }
    }

    /* --- Frame around content area --- */
    {
        uint8_t fw_px = (t->frame_w > 0) ? t->frame_w : 1;
        for (i = 0; i < fw_px; i++) {
            tiku_kits_gfx_rect(s,
                (int16_t)(fx + i), (int16_t)(fy + i),
                (uint16_t)(fw - 2u * i), (uint16_t)(fh - 2u * i),
                t->color_fg);
        }
    }

    /* --- Content subsurface: children draw at (0, 0)-relative --- */
    {
        tiku_kits_gfx_surface_t content;
        uint8_t fw_px = (t->frame_w > 0) ? t->frame_w : 1;
        tiku_kits_gfx_subsurface(&content, s,
            (int16_t)(fx + fw_px), (int16_t)(fy + fw_px),
            (uint16_t)(fw - 2u * fw_px), (uint16_t)(fh - 2u * fw_px));
        for (i = 0; i < win->n_children; i++) {
            tiku_kits_ui_widget_render(win->children[i], &content);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* FOCUS WALK                                                                */
/*---------------------------------------------------------------------------*/

/* Find the next focusable child starting from start_idx + step.
 * step is +1 or -1. Returns the new index, or -1 if no focusable
 * child exists at all. Wraps at the ends. */
static int8_t
find_focusable(const tiku_kits_ui_window_t *win, int8_t start_idx, int8_t step)
{
    int8_t n = (int8_t)win->n_children;
    int8_t i = start_idx;
    int8_t tries;

    if (n == 0) return -1;
    for (tries = 0; tries < n; tries++) {
        i = (int8_t)(i + step);
        if (i < 0)   i = (int8_t)(n - 1);
        if (i >= n)  i = 0;
        if (tiku_kits_ui_widget_is_focusable(win->children[i])) {
            return i;
        }
    }
    /* No focusable child anywhere. */
    return -1;
}

static void
set_focus(tiku_kits_ui_window_t *win, int8_t new_idx)
{
    int8_t i;
    for (i = 0; i < (int8_t)win->n_children; i++) {
        win->children[i]->focused = (i == new_idx) ? 1u : 0u;
    }
    win->focus_idx = new_idx;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

const tiku_kits_ui_widget_ops_t tiku_kits_ui_window_ops = {
    .render        = window_render,
    .handle_event  = NULL,        /* containers don't take direct events */
    .is_focusable  = NULL,        /* containers themselves aren't focusable */
};

void
tiku_kits_ui_window_init(tiku_kits_ui_window_t *win,
                          int16_t x, int16_t y,
                          uint16_t w, uint16_t h,
                          const char *title,
                          const tiku_kits_gfx_font_t *title_font,
                          uint8_t title_scale)
{
    if (win == NULL) return;
    win->base.ops      = &tiku_kits_ui_window_ops;
    win->base.x        = x;
    win->base.y        = y;
    win->base.w        = w;
    win->base.h        = h;
    win->base.visible  = 1;
    win->base.focused  = 0;
    win->base.dirty  = 0;
    win->base.user_data = NULL;
    win->title         = title;
    win->title_font    = (title_font != NULL) ? title_font
                                              : &tiku_kits_gfx_font_5x7;
    win->title_scale   = (title_scale > 0) ? title_scale : 1;
    win->n_children    = 0;
    win->focus_idx     = -1;
}

int
tiku_kits_ui_window_add(tiku_kits_ui_window_t *win,
                         tiku_kits_ui_widget_t *child)
{
    if (win == NULL || child == NULL) {
        return TIKU_KITS_UI_ERR_PARAM;
    }
    if (win->n_children >= TIKU_KITS_UI_WINDOW_MAX_CHILDREN) {
        return TIKU_KITS_UI_ERR_FULL;
    }
    win->children[win->n_children++] = child;

    /* If this is the first focusable child, give it focus. */
    if (win->focus_idx < 0 && tiku_kits_ui_widget_is_focusable(child)) {
        set_focus(win, (int8_t)(win->n_children - 1));
    }
    return TIKU_KITS_UI_OK;
}

void
tiku_kits_ui_window_render(const tiku_kits_ui_window_t *win,
                            const tiku_kits_gfx_surface_t *s)
{
    tiku_kits_ui_widget_render(&win->base, s);
}

/*---------------------------------------------------------------------------*/
/* DIRTY-RECT RENDER                                                         */
/*---------------------------------------------------------------------------*/

/* Expand bounding rect @p dst to also cover @p src. If @p dst is
 * empty (w == 0 || h == 0) it becomes a copy of @p src. */
static void
rect_union(tiku_kits_gfx_rect_t *dst, const tiku_kits_gfx_rect_t *src)
{
    int16_t x0, y0, x1, y1;
    if (src->w == 0u || src->h == 0u) return;
    if (dst->w == 0u || dst->h == 0u) { *dst = *src; return; }
    x0 = (dst->x < src->x) ? dst->x : src->x;
    y0 = (dst->y < src->y) ? dst->y : src->y;
    x1 = ((int16_t)(dst->x + dst->w) > (int16_t)(src->x + src->w))
            ? (int16_t)(dst->x + dst->w) : (int16_t)(src->x + src->w);
    y1 = ((int16_t)(dst->y + dst->h) > (int16_t)(src->y + src->h))
            ? (int16_t)(dst->y + dst->h) : (int16_t)(src->y + src->h);
    dst->x = x0;
    dst->y = y0;
    dst->w = (uint16_t)(x1 - x0);
    dst->h = (uint16_t)(y1 - y0);
}

uint16_t
tiku_kits_ui_window_render_dirty(tiku_kits_ui_window_t *win,
                                  const tiku_kits_gfx_surface_t *s,
                                  uint8_t bg_color,
                                  tiku_kits_gfx_rect_t *out_dirty_rect)
{
    const tiku_kits_ui_theme_t *t;
    tiku_kits_gfx_surface_t content;
    tiku_kits_gfx_rect_t    bbox = { 0, 0, 0, 0 };
    uint16_t th, fh;
    uint16_t fw_px;
    int16_t  fx, fy;
    uint16_t painted = 0;
    uint8_t  i;

    if (out_dirty_rect != NULL) *out_dirty_rect = bbox;
    if (win == NULL || s == NULL) return 0;

    t  = tiku_kits_ui_theme_current();
    th = tab_height(win);
    fw_px = (t->frame_w > 0) ? t->frame_w : 1;
    fx = win->base.x;
    fy = (int16_t)(win->base.y + th);
    fh = (uint16_t)(win->base.h - th);

    /* Build the same content subsurface as window_render. */
    tiku_kits_gfx_subsurface(&content, s,
        (int16_t)(fx + fw_px), (int16_t)(fy + fw_px),
        (uint16_t)(win->base.w - 2u * fw_px),
        (uint16_t)(fh - 2u * fw_px));

    for (i = 0; i < win->n_children; i++) {
        tiku_kits_ui_widget_t *c = win->children[i];
        if (c == NULL || !c->visible || !c->dirty) continue;

        /* Erase child's region on the content subsurface (it
         * receives content-relative coords). */
        tiku_kits_gfx_fill_rect(&content,
            c->x, c->y, c->w, c->h, bg_color);

        /* Render the child. */
        tiku_kits_ui_widget_render(c, &content);

        /* Translate the child rect into screen-absolute coords for
         * the bounding-box accumulator. */
        {
            tiku_kits_gfx_rect_t r = {
                (int16_t)(content.origin_x + c->x),
                (int16_t)(content.origin_y + c->y),
                c->w, c->h
            };
            rect_union(&bbox, &r);
        }

        c->dirty = 0u;
        painted++;
    }

    if (out_dirty_rect != NULL) *out_dirty_rect = bbox;
    return painted;
}

void
tiku_kits_ui_window_event(tiku_kits_ui_window_t *win,
                           tiku_kits_ui_event_t evt)
{
    int handled = 0;
    if (win == NULL) return;

    /* FOCUS_NEXT / PREV are window-level linear navigation.
     * They bypass the focused child entirely. */
    if (evt == TIKU_KITS_UI_EVT_FOCUS_NEXT) {
        set_focus(win, find_focusable(win, win->focus_idx, 1));
        return;
    }
    if (evt == TIKU_KITS_UI_EVT_FOCUS_PREV) {
        set_focus(win, find_focusable(win, win->focus_idx, -1));
        return;
    }

    /* Everything else (ACTIVATE, INC, DEC, BACK, MENU,
     * LONG_PRESS, FOCUS_UP/DOWN/LEFT/RIGHT, NUMERIC_*) is
     * forwarded to the focused child first. */
    if (win->focus_idx >= 0 &&
        win->focus_idx < (int8_t)win->n_children) {
        tiku_kits_ui_widget_t *fw = win->children[win->focus_idx];
        if (fw->ops->handle_event != NULL) {
            handled = fw->ops->handle_event(fw, evt);
        }
    }

    /* When the focused child can't handle a spatial direction
     * event (e.g. a button), fall back to linear navigation so
     * the user isn't stuck. Layout containers (vbox/hbox) intercept
     * the matching axis and return 1, so this fallback only fires
     * for axis-irrelevant widgets. */
    if (!handled) {
        switch (evt) {
        case TIKU_KITS_UI_EVT_FOCUS_DOWN:
        case TIKU_KITS_UI_EVT_FOCUS_RIGHT:
            set_focus(win, find_focusable(win, win->focus_idx, 1));
            break;
        case TIKU_KITS_UI_EVT_FOCUS_UP:
        case TIKU_KITS_UI_EVT_FOCUS_LEFT:
            set_focus(win, find_focusable(win, win->focus_idx, -1));
            break;
        default:
            break;
        }
    }
}
