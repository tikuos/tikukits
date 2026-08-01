/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx.c - 2D drawing primitives
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * All shapes are rendered by repeated calls to put_pixel(), which:
 * 1. clips against the surface's local (width, height);
 * 2. translates child-frame coordinates by (origin_x, origin_y);
 * 3. forwards the resulting pixel to the surface's set_pixel.
 *
 * Subsurfaces work by inheriting the parent's set_pixel and ctx
 * but with their own width/height and a translation offset.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "tiku_kits_gfx.h"

/*---------------------------------------------------------------------------*/
/* RECTANGLE HELPERS                                                         */
/*---------------------------------------------------------------------------*/

int
tiku_kits_gfx_rect_contains(const tiku_kits_gfx_rect_t *r,
                             int16_t x, int16_t y)
{
    if (r == NULL || tiku_kits_gfx_rect_empty(r)) return 0;
    if (x < r->x || y < r->y) return 0;
    if (x >= r->x + (int16_t)r->w) return 0;
    if (y >= r->y + (int16_t)r->h) return 0;
    return 1;
}

int
tiku_kits_gfx_rect_intersect(const tiku_kits_gfx_rect_t *a,
                              const tiku_kits_gfx_rect_t *b,
                              tiku_kits_gfx_rect_t *out)
{
    int16_t x0, y0, x1, y1;
    if (a == NULL || b == NULL || out == NULL) return 0;

    x0 = (a->x > b->x) ? a->x : b->x;
    y0 = (a->y > b->y) ? a->y : b->y;
    {
        int16_t ax1 = (int16_t)(a->x + a->w);
        int16_t bx1 = (int16_t)(b->x + b->w);
        int16_t ay1 = (int16_t)(a->y + a->h);
        int16_t by1 = (int16_t)(b->y + b->h);
        x1 = (ax1 < bx1) ? ax1 : bx1;
        y1 = (ay1 < by1) ? ay1 : by1;
    }

    if (x1 <= x0 || y1 <= y0) {
        out->x = 0; out->y = 0; out->w = 0; out->h = 0;
        return 0;
    }
    out->x = x0;
    out->y = y0;
    out->w = (uint16_t)(x1 - x0);
    out->h = (uint16_t)(y1 - y0);
    return 1;
}

/*---------------------------------------------------------------------------*/
/* SUBSURFACE                                                                */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_subsurface(tiku_kits_gfx_surface_t *child,
                          const tiku_kits_gfx_surface_t *parent,
                          int16_t x, int16_t y,
                          uint16_t w, uint16_t h)
{
    if (child == NULL || parent == NULL) return;

    /* Inherit transport from parent. */
    child->set_pixel = parent->set_pixel;
    child->ctx       = parent->ctx;

    /* Accumulate translation: child(0,0) maps to parent(x,y),
     * which itself may already be offset from grandparent. */
    child->origin_x  = (int16_t)(parent->origin_x + x);
    child->origin_y  = (int16_t)(parent->origin_y + y);

    child->width     = w;
    child->height    = h;

    /* Subsurface starts with an empty clip stack of its own; the
     * child's (width, height) does the natural clipping and the
     * parent's clip applies indirectly through its set_pixel. */
    child->clip_depth = 0;
}

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/* Bounds-check, translate, and forward one pixel. */
static inline void
put(const tiku_kits_gfx_surface_t *s, int16_t x, int16_t y, uint8_t color)
{
    int32_t px, py;
    if (x < 0 || y < 0) return;
    if ((uint16_t)x >= s->width || (uint16_t)y >= s->height) return;
    if (s->clip_depth > 0u) {
        const tiku_kits_gfx_rect_t *c =
            &s->clips[s->clip_depth - 1u];
        if (x < c->x || y < c->y) return;
        if (x >= c->x + (int16_t)c->w) return;
        if (y >= c->y + (int16_t)c->h) return;
    }
    px = (int32_t)x + s->origin_x;
    py = (int32_t)y + s->origin_y;
    if (px < 0 || py < 0 || px > 0xFFFF || py > 0xFFFF) return;
    s->set_pixel(s->ctx, (uint16_t)px, (uint16_t)py, color);
}

static inline int16_t iabs16(int16_t v) { return v < 0 ? (int16_t)(-v) : v; }
static inline uint16_t umin16(uint16_t a, uint16_t b) { return a < b ? a : b; }

/*---------------------------------------------------------------------------*/
/* PIXEL                                                                     */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_pixel(const tiku_kits_gfx_surface_t *s,
                     int16_t x, int16_t y, uint8_t color)
{
    if (s == NULL || s->set_pixel == NULL) return;
    put(s, x, y, color);
}

/*---------------------------------------------------------------------------*/
/* SURFACE-WIDE OPERATIONS                                                   */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_clear(const tiku_kits_gfx_surface_t *s, uint8_t color)
{
    if (s == NULL || s->set_pixel == NULL) return;
    tiku_kits_gfx_fill_rect(s, 0, 0, s->width, s->height, color);
}

void
tiku_kits_gfx_fill(const tiku_kits_gfx_surface_t *s, uint8_t color)
{
    tiku_kits_gfx_clear(s, color);
}

void
tiku_kits_gfx_clear_rect(const tiku_kits_gfx_surface_t *s,
                          const tiku_kits_gfx_rect_t *r,
                          uint8_t color)
{
    if (s == NULL || r == NULL) return;
    tiku_kits_gfx_fill_rect(s, r->x, r->y, r->w, r->h, color);
}

/*---------------------------------------------------------------------------*/
/* CLIP STACK                                                                */
/*---------------------------------------------------------------------------*/

int
tiku_kits_gfx_push_clip(tiku_kits_gfx_surface_t *s,
                         const tiku_kits_gfx_rect_t *clip)
{
    tiku_kits_gfx_rect_t bounds;
    tiku_kits_gfx_rect_t merged;

    if (s == NULL || clip == NULL) return -1;
    if (s->clip_depth >= TIKU_KITS_GFX_CLIP_STACK_DEPTH) return -1;

    /* Compose with the current clip (or full surface bounds when
     * the stack is empty) so push() is always intersecting. */
    if (s->clip_depth > 0u) {
        bounds = s->clips[s->clip_depth - 1u];
    } else {
        bounds.x = 0;
        bounds.y = 0;
        bounds.w = s->width;
        bounds.h = s->height;
    }
    if (!tiku_kits_gfx_rect_intersect(&bounds, clip, &merged)) {
        /* Empty intersection: push an empty rect so nothing renders. */
        merged.x = 0; merged.y = 0; merged.w = 0; merged.h = 0;
    }
    s->clips[s->clip_depth++] = merged;
    return 0;
}

void
tiku_kits_gfx_pop_clip(tiku_kits_gfx_surface_t *s)
{
    if (s == NULL || s->clip_depth == 0u) return;
    s->clip_depth--;
}

void
tiku_kits_gfx_reset_clip(tiku_kits_gfx_surface_t *s)
{
    if (s == NULL) return;
    s->clip_depth = 0;
}

/*---------------------------------------------------------------------------*/
/* AXIS-ALIGNED LINES                                                        */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_hline(const tiku_kits_gfx_surface_t *s,
                     int16_t x, int16_t y, uint16_t w, uint8_t color)
{
    uint16_t i;
    if (s == NULL || s->set_pixel == NULL) return;
    for (i = 0; i < w; i++) {
        put(s, (int16_t)(x + i), y, color);
    }
}

void
tiku_kits_gfx_vline(const tiku_kits_gfx_surface_t *s,
                     int16_t x, int16_t y, uint16_t h, uint8_t color)
{
    uint16_t i;
    if (s == NULL || s->set_pixel == NULL) return;
    for (i = 0; i < h; i++) {
        put(s, x, (int16_t)(y + i), color);
    }
}

/*---------------------------------------------------------------------------*/
/* BRESENHAM LINE                                                            */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_line(const tiku_kits_gfx_surface_t *s,
                    int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1, uint8_t color)
{
    int16_t dx, dy, sx, sy, err, e2;
    if (s == NULL || s->set_pixel == NULL) return;

    dx = iabs16((int16_t)(x1 - x0));
    dy = iabs16((int16_t)(y1 - y0));
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = (int16_t)(dx - dy);

    for (;;) {
        put(s, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = (int16_t)(2 * err);
        if (e2 > -dy) { err = (int16_t)(err - dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

/*---------------------------------------------------------------------------*/
/* THICK LINE                                                                */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_line_thick(const tiku_kits_gfx_surface_t *s,
                          int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1,
                          uint8_t thickness, uint8_t color)
{
    int16_t dx, dy, sx, sy, err, e2;
    uint16_t r;

    if (s == NULL || s->set_pixel == NULL) return;
    if (thickness == 0) return;
    if (thickness == 1) {
        tiku_kits_gfx_line(s, x0, y0, x1, y1, color);
        return;
    }

    /* Stamp a filled disc at every Bresenham point. Slow but
     * artifact-free; fine for UI border thicknesses (1-5 px). */
    r = (uint16_t)((thickness - 1u) / 2u);
    if (r == 0) r = 1;

    dx = iabs16((int16_t)(x1 - x0));
    dy = iabs16((int16_t)(y1 - y0));
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = (int16_t)(dx - dy);

    for (;;) {
        tiku_kits_gfx_fill_circle(s, x0, y0, r, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = (int16_t)(2 * err);
        if (e2 > -dy) { err = (int16_t)(err - dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

/*---------------------------------------------------------------------------*/
/* RECTANGLES                                                                */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_rect(const tiku_kits_gfx_surface_t *s,
                    int16_t x, int16_t y,
                    uint16_t w, uint16_t h, uint8_t color)
{
    if (s == NULL || s->set_pixel == NULL || w == 0 || h == 0) return;
    tiku_kits_gfx_hline(s, x, y, w, color);
    tiku_kits_gfx_hline(s, x, (int16_t)(y + h - 1), w, color);
    tiku_kits_gfx_vline(s, x, y, h, color);
    tiku_kits_gfx_vline(s, (int16_t)(x + w - 1), y, h, color);
}

void
tiku_kits_gfx_fill_rect(const tiku_kits_gfx_surface_t *s,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h, uint8_t color)
{
    uint16_t i;
    if (s == NULL || s->set_pixel == NULL) return;
    for (i = 0; i < h; i++) {
        tiku_kits_gfx_hline(s, x, (int16_t)(y + i), w, color);
    }
}

/*---------------------------------------------------------------------------*/
/* ROUNDED RECTANGLES                                                        */
/*---------------------------------------------------------------------------*/

/* Helper: paint a quarter of a circle outline. quadrant indexes:
 *   0 = top-right (octant 0+1 of full circle)
 *   1 = top-left
 *   2 = bottom-left
 *   3 = bottom-right
 * Center is at (cx, cy). Used by round-rect outline. */
static void
quarter_circle(const tiku_kits_gfx_surface_t *s,
                int16_t cx, int16_t cy, uint16_t r,
                uint8_t quadrant, uint8_t color)
{
    int16_t x = (int16_t)r;
    int16_t y = 0;
    int16_t d = (int16_t)(1 - x);

    while (x >= y) {
        switch (quadrant) {
        case 0: /* top-right */
            put(s, (int16_t)(cx + x), (int16_t)(cy - y), color);
            put(s, (int16_t)(cx + y), (int16_t)(cy - x), color);
            break;
        case 1: /* top-left */
            put(s, (int16_t)(cx - x), (int16_t)(cy - y), color);
            put(s, (int16_t)(cx - y), (int16_t)(cy - x), color);
            break;
        case 2: /* bottom-left */
            put(s, (int16_t)(cx - x), (int16_t)(cy + y), color);
            put(s, (int16_t)(cx - y), (int16_t)(cy + x), color);
            break;
        case 3: /* bottom-right */
            put(s, (int16_t)(cx + x), (int16_t)(cy + y), color);
            put(s, (int16_t)(cx + y), (int16_t)(cy + x), color);
            break;
        default: break;
        }
        y++;
        if (d <= 0) d = (int16_t)(d + 2 * y + 1);
        else { x--; d = (int16_t)(d + 2 * (y - x) + 1); }
    }
}

/* Helper: filled top or bottom half of a circle, used by
 * fill_round_rect for the rounded ends. */
static void
half_disc(const tiku_kits_gfx_surface_t *s,
           int16_t cx, int16_t cy, uint16_t r,
           int top_half, uint8_t color)
{
    int16_t x = (int16_t)r;
    int16_t y = 0;
    int16_t d = (int16_t)(1 - x);

    while (x >= y) {
        if (top_half) {
            tiku_kits_gfx_hline(s, (int16_t)(cx - x), (int16_t)(cy - y),
                                 (uint16_t)(2 * x + 1), color);
            tiku_kits_gfx_hline(s, (int16_t)(cx - y), (int16_t)(cy - x),
                                 (uint16_t)(2 * y + 1), color);
        } else {
            tiku_kits_gfx_hline(s, (int16_t)(cx - x), (int16_t)(cy + y),
                                 (uint16_t)(2 * x + 1), color);
            tiku_kits_gfx_hline(s, (int16_t)(cx - y), (int16_t)(cy + x),
                                 (uint16_t)(2 * y + 1), color);
        }
        y++;
        if (d <= 0) d = (int16_t)(d + 2 * y + 1);
        else { x--; d = (int16_t)(d + 2 * (y - x) + 1); }
    }
}

void
tiku_kits_gfx_round_rect(const tiku_kits_gfx_surface_t *s,
                          int16_t x, int16_t y,
                          uint16_t w, uint16_t h, uint16_t r,
                          uint8_t color)
{
    uint16_t max_r;
    if (s == NULL || s->set_pixel == NULL || w == 0 || h == 0) return;

    max_r = umin16(w, h) / 2u;
    if (r > max_r) r = max_r;

    if (r == 0) {
        tiku_kits_gfx_rect(s, x, y, w, h, color);
        return;
    }

    /* Straight edges (excluding the corner regions). */
    tiku_kits_gfx_hline(s, (int16_t)(x + r), y,
                        (uint16_t)(w - 2u * r), color);
    tiku_kits_gfx_hline(s, (int16_t)(x + r), (int16_t)(y + h - 1),
                        (uint16_t)(w - 2u * r), color);
    tiku_kits_gfx_vline(s, x, (int16_t)(y + r),
                        (uint16_t)(h - 2u * r), color);
    tiku_kits_gfx_vline(s, (int16_t)(x + w - 1), (int16_t)(y + r),
                        (uint16_t)(h - 2u * r), color);

    /* Four corner arcs. */
    quarter_circle(s, (int16_t)(x + w - 1 - r), (int16_t)(y + r),         r, 0, color);
    quarter_circle(s, (int16_t)(x + r),         (int16_t)(y + r),         r, 1, color);
    quarter_circle(s, (int16_t)(x + r),         (int16_t)(y + h - 1 - r), r, 2, color);
    quarter_circle(s, (int16_t)(x + w - 1 - r), (int16_t)(y + h - 1 - r), r, 3, color);
}

void
tiku_kits_gfx_fill_round_rect(const tiku_kits_gfx_surface_t *s,
                               int16_t x, int16_t y,
                               uint16_t w, uint16_t h, uint16_t r,
                               uint8_t color)
{
    uint16_t max_r;
    if (s == NULL || s->set_pixel == NULL || w == 0 || h == 0) return;

    max_r = umin16(w, h) / 2u;
    if (r > max_r) r = max_r;

    if (r == 0) {
        tiku_kits_gfx_fill_rect(s, x, y, w, h, color);
        return;
    }

    /* Central rectangle + two flush rectangles for the unrounded
     * portions of the top and bottom edges. */
    tiku_kits_gfx_fill_rect(s, (int16_t)(x + r), y,
                              (uint16_t)(w - 2u * r), h, color);
    tiku_kits_gfx_fill_rect(s, x, (int16_t)(y + r),
                              r, (uint16_t)(h - 2u * r), color);
    tiku_kits_gfx_fill_rect(s, (int16_t)(x + w - r), (int16_t)(y + r),
                              r, (uint16_t)(h - 2u * r), color);

    /* Filled corner discs (top-left/right via half-disc top, etc.). */
    half_disc(s, (int16_t)(x + r),         (int16_t)(y + r),         r, 1, color);
    half_disc(s, (int16_t)(x + w - 1 - r), (int16_t)(y + r),         r, 1, color);
    half_disc(s, (int16_t)(x + r),         (int16_t)(y + h - 1 - r), r, 0, color);
    half_disc(s, (int16_t)(x + w - 1 - r), (int16_t)(y + h - 1 - r), r, 0, color);
}

/*---------------------------------------------------------------------------*/
/* TRIANGLES                                                                 */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_triangle(const tiku_kits_gfx_surface_t *s,
                        int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1,
                        int16_t x2, int16_t y2,
                        uint8_t color)
{
    if (s == NULL || s->set_pixel == NULL) return;
    tiku_kits_gfx_line(s, x0, y0, x1, y1, color);
    tiku_kits_gfx_line(s, x1, y1, x2, y2, color);
    tiku_kits_gfx_line(s, x2, y2, x0, y0, color);
}

/* Standard scanline triangle fill. Sort vertices by y, then for
 * each scanline find the two x intersections with the active
 * edges and fill between them. */
void
tiku_kits_gfx_fill_triangle(const tiku_kits_gfx_surface_t *s,
                             int16_t x0, int16_t y0,
                             int16_t x1, int16_t y1,
                             int16_t x2, int16_t y2,
                             uint8_t color)
{
    int16_t a, b, y, last;
    int32_t dx01, dy01, dx02, dy02, dx12, dy12;
    int32_t sa, sb;

    if (s == NULL || s->set_pixel == NULL) return;

    /* Sort by y so y0 <= y1 <= y2. */
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
    if (y1 > y2) { int16_t t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }

    /* Degenerate: triangle is a single horizontal line. */
    if (y0 == y2) {
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        tiku_kits_gfx_hline(s, a, y0, (uint16_t)(b - a + 1), color);
        return;
    }

    dx01 = x1 - x0; dy01 = y1 - y0;
    dx02 = x2 - x0; dy02 = y2 - y0;
    dx12 = x2 - x1; dy12 = y2 - y1;
    sa = 0; sb = 0;

    /* Upper part: y0 .. y1 (inclusive of y1 for flat-bottom case). */
    last = (y1 == y2) ? y1 : (int16_t)(y1 - 1);
    for (y = y0; y <= last; y++) {
        a = (int16_t)(x0 + sa / (dy01 == 0 ? 1 : dy01));
        b = (int16_t)(x0 + sb / (dy02 == 0 ? 1 : dy02));
        sa += dx01;
        sb += dx02;
        if (a > b) { int16_t t = a; a = b; b = t; }
        tiku_kits_gfx_hline(s, a, y, (uint16_t)(b - a + 1), color);
    }

    /* Lower part: y1 .. y2. */
    sa = (int32_t)dx12 * (y - y1);
    sb = (int32_t)dx02 * (y - y0);
    for (; y <= y2; y++) {
        a = (int16_t)(x1 + sa / (dy12 == 0 ? 1 : dy12));
        b = (int16_t)(x0 + sb / (dy02 == 0 ? 1 : dy02));
        sa += dx12;
        sb += dx02;
        if (a > b) { int16_t t = a; a = b; b = t; }
        tiku_kits_gfx_hline(s, a, y, (uint16_t)(b - a + 1), color);
    }
}

/*---------------------------------------------------------------------------*/
/* CIRCLES (midpoint algorithm)                                              */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_circle(const tiku_kits_gfx_surface_t *s,
                      int16_t cx, int16_t cy, uint16_t r, uint8_t color)
{
    int16_t x, y, d;
    if (s == NULL || s->set_pixel == NULL || r == 0) return;

    x = (int16_t)r;
    y = 0;
    d = (int16_t)(1 - x);

    while (x >= y) {
        put(s, (int16_t)(cx + x), (int16_t)(cy + y), color);
        put(s, (int16_t)(cx - x), (int16_t)(cy + y), color);
        put(s, (int16_t)(cx + x), (int16_t)(cy - y), color);
        put(s, (int16_t)(cx - x), (int16_t)(cy - y), color);
        put(s, (int16_t)(cx + y), (int16_t)(cy + x), color);
        put(s, (int16_t)(cx - y), (int16_t)(cy + x), color);
        put(s, (int16_t)(cx + y), (int16_t)(cy - x), color);
        put(s, (int16_t)(cx - y), (int16_t)(cy - x), color);
        y++;
        if (d <= 0) d = (int16_t)(d + 2 * y + 1);
        else { x--; d = (int16_t)(d + 2 * (y - x) + 1); }
    }
}

void
tiku_kits_gfx_fill_circle(const tiku_kits_gfx_surface_t *s,
                           int16_t cx, int16_t cy, uint16_t r, uint8_t color)
{
    int16_t x, y, d;
    if (s == NULL || s->set_pixel == NULL || r == 0) return;

    x = (int16_t)r;
    y = 0;
    d = (int16_t)(1 - x);

    while (x >= y) {
        tiku_kits_gfx_hline(s, (int16_t)(cx - x), (int16_t)(cy + y),
                             (uint16_t)(2 * x + 1), color);
        tiku_kits_gfx_hline(s, (int16_t)(cx - x), (int16_t)(cy - y),
                             (uint16_t)(2 * x + 1), color);
        tiku_kits_gfx_hline(s, (int16_t)(cx - y), (int16_t)(cy + x),
                             (uint16_t)(2 * y + 1), color);
        tiku_kits_gfx_hline(s, (int16_t)(cx - y), (int16_t)(cy - x),
                             (uint16_t)(2 * y + 1), color);
        y++;
        if (d <= 0) d = (int16_t)(d + 2 * y + 1);
        else { x--; d = (int16_t)(d + 2 * (y - x) + 1); }
    }
}

/*---------------------------------------------------------------------------*/
/* BITMAP BLIT                                                               */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_bitmap(const tiku_kits_gfx_surface_t *s,
                      int16_t x, int16_t y,
                      const uint8_t *bitmap,
                      uint16_t w, uint16_t h, uint8_t color)
{
    uint16_t row, col;
    uint16_t bytes_per_row;

    if (s == NULL || s->set_pixel == NULL || bitmap == NULL) return;
    bytes_per_row = (uint16_t)((w + 7u) / 8u);

    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++) {
            const uint8_t byte = bitmap[row * bytes_per_row + (col >> 3)];
            const uint8_t mask = (uint8_t)(0x80u >> (col & 7u));
            if (byte & mask) {
                put(s, (int16_t)(x + col), (int16_t)(y + row), color);
            }
        }
    }
}

