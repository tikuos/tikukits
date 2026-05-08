/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx.h - 2D graphics primitives for framebuffer surfaces
 *
 * A display-agnostic 2D graphics library. Operates on a generic
 * surface abstraction (`tiku_kits_gfx_surface_t`) that wraps any
 * framebuffer-backed display via a per-pixel set function. Apps
 * adapt the surface from any display kit -- e-paper, LCD, OLED --
 * by writing a 4-line `set_pixel` thunk.
 *
 * Coordinate convention: pixel (0, 0) is top-left. Coordinates
 * passed to drawing primitives are int16_t to allow shapes that
 * partially extend off-screen (clipped) or that originate at
 * negative coordinates (e.g. when a subsurface scrolls).
 *
 * Surface composition:
 *   - Top-level surface: wraps a display, origin = (0, 0).
 *   - Subsurface: a child surface that maps child coordinates
 *     into a sub-rectangle of a parent. Drawing on the child uses
 *     local (0, 0)-based coordinates; the kit translates to the
 *     parent automatically. Useful for UI widgets, scrolling
 *     viewports, and clipping.
 *
 * Layout
 * ------
 *
 *     application
 *         |
 *         v
 *     +--------------------------+
 *     | tiku_kits_gfx.h/.c       |   surface, types, primitives
 *     | tiku_kits_gfx_text.h/.c  |   text rendering
 *     | fonts/                   |   bitmap fonts
 *     +--------------------------+
 *         |
 *         v   (set_pixel function pointer)
 *     application's surface adapter
 *         |
 *         v
 *     display kit (e.g. tikukits/epaper)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_GFX_H_
#define TIKU_KITS_GFX_H_

#include <stdint.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_GFX_OK             0
#define TIKU_KITS_GFX_ERR_PARAM    (-1)

/*---------------------------------------------------------------------------*/
/* COLOUR CONSTANTS (convention only -- passed to surface verbatim)          */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_GFX_WHITE     0u
#define TIKU_KITS_GFX_BLACK     1u
#define TIKU_KITS_GFX_RED       2u
#define TIKU_KITS_GFX_YELLOW    3u

/*---------------------------------------------------------------------------*/
/* COORDINATE PRIMITIVES (Tier 3)                                            */
/*---------------------------------------------------------------------------*/

/** A point in surface-local coordinates. */
typedef struct {
    int16_t x;
    int16_t y;
} tiku_kits_gfx_point_t;

/** A size, measured in pixels. */
typedef struct {
    uint16_t w;
    uint16_t h;
} tiku_kits_gfx_size_t;

/** An axis-aligned rectangle in surface-local coordinates. */
typedef struct {
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
} tiku_kits_gfx_rect_t;

/*---------------------------------------------------------------------------*/
/* RECTANGLE HELPERS                                                         */
/*---------------------------------------------------------------------------*/

/** True if rectangle is empty (zero or negative size). */
static inline int
tiku_kits_gfx_rect_empty(const tiku_kits_gfx_rect_t *r)
{
    return (r->w == 0u || r->h == 0u);
}

/** True if (x, y) lies inside @p r. */
int tiku_kits_gfx_rect_contains(const tiku_kits_gfx_rect_t *r,
                                 int16_t x, int16_t y);

/** Compute @p out = @p a intersected with @p b.
 *  Returns 1 if the intersection is non-empty, 0 otherwise.
 *  When the result is 0, @p out is set to an empty rectangle. */
int tiku_kits_gfx_rect_intersect(const tiku_kits_gfx_rect_t *a,
                                  const tiku_kits_gfx_rect_t *b,
                                  tiku_kits_gfx_rect_t *out);

/*---------------------------------------------------------------------------*/
/* SURFACE                                                                   */
/*---------------------------------------------------------------------------*/

/* Maximum nesting depth of the per-surface clip stack. Override at
 * compile time if widgets need to nest more clip regions. Cost: 8
 * bytes per slot. */
#ifndef TIKU_KITS_GFX_CLIP_STACK_DEPTH
#define TIKU_KITS_GFX_CLIP_STACK_DEPTH 4
#endif

/**
 * @brief Generic drawable surface.
 *
 * Fields:
 *   - width, height: drawable dimensions (in surface coordinates).
 *   - set_pixel:     callback to paint one pixel; receives PARENT
 *                    coordinates after translation.
 *   - ctx:           opaque pointer threaded through set_pixel.
 *   - origin_x/y:    translation applied before set_pixel. Zero
 *                    for top-level surfaces; non-zero for
 *                    subsurfaces that map a child window onto a
 *                    parent.
 *   - clips/clip_depth: optional clip stack in surface-local
 *                    coordinates. Empty (depth == 0) means no
 *                    additional clip beyond the surface's own
 *                    (width, height) bounds. push / pop manipulate
 *                    the stack; drawing primitives consult the
 *                    top entry when depth > 0.
 *
 * For backward compatibility with C99-designated-initializer
 * usage like `{ .width = ..., .height = ..., .set_pixel = ..., .ctx = ... }`,
 * trailing fields default to zero -- such surfaces behave exactly
 * as they did before the field was added.
 */
typedef struct {
    uint16_t width;
    uint16_t height;
    void (*set_pixel)(void *ctx, uint16_t x, uint16_t y, uint8_t color);
    void *ctx;
    int16_t origin_x;
    int16_t origin_y;
    tiku_kits_gfx_rect_t clips[TIKU_KITS_GFX_CLIP_STACK_DEPTH];
    uint8_t clip_depth;
} tiku_kits_gfx_surface_t;

/**
 * @brief Create a child surface that maps onto a sub-rectangle of @p parent.
 *
 * After the call, drawing on @p child using local (0, 0)-based
 * coordinates will be translated and clipped to the rectangle
 * (x, y, w, h) on the parent. The child shares the parent's
 * `set_pixel` and `ctx` -- no allocation, no callback wrapping.
 *
 * Subsurface use cases:
 *   - UI widgets with their own coordinate system.
 *   - Scrolling viewports (set origin_x/y to negative values).
 *   - Clipping: the child's width/height cap how much can be drawn.
 *
 * Subsurfaces nest: calling subsurface() on an existing subsurface
 * accumulates the translation correctly.
 *
 * @param child   Output surface (caller-allocated)
 * @param parent  Existing surface (must outlive @p child)
 * @param x, y    Top-left of the child within the parent
 * @param w, h    Child surface dimensions
 */
void tiku_kits_gfx_subsurface(tiku_kits_gfx_surface_t *child,
                               const tiku_kits_gfx_surface_t *parent,
                               int16_t x, int16_t y,
                               uint16_t w, uint16_t h);

/*---------------------------------------------------------------------------*/
/* SURFACE-WIDE OPERATIONS                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Paint every addressable pixel of @p s with @p color.
 *
 * Goes through the surface's set_pixel callback so it works on any
 * adapter. For displays that have a faster bulk-clear path (e.g.
 * the e-paper kit's tiku_kits_epaper_clear), call that directly --
 * it bypasses the per-pixel callback overhead.
 */
void tiku_kits_gfx_clear(const tiku_kits_gfx_surface_t *s, uint8_t color);

/**
 * @brief Alias for tiku_kits_gfx_clear; included for naming
 *        symmetry with fill_rect / fill_circle / etc.
 */
void tiku_kits_gfx_fill(const tiku_kits_gfx_surface_t *s, uint8_t color);

/**
 * @brief Paint a sub-rectangle of @p s with @p color.
 *
 * Convenience wrapper around fill_rect that takes a rect_t for
 * symmetry with the clip / measure helpers. Used by the dirty-rect
 * window render path to wipe just the changed regions before
 * repainting.
 */
void tiku_kits_gfx_clear_rect(const tiku_kits_gfx_surface_t *s,
                                const tiku_kits_gfx_rect_t *r,
                                uint8_t color);

/*---------------------------------------------------------------------------*/
/* CLIP STACK                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Push a clip rectangle in surface-local coordinates.
 *
 * Subsequent drawing primitives drop pixels outside @p clip until
 * pop_clip() is called. Pushing into a non-empty stack intersects
 * the new rectangle with the current clip so nested calls
 * accumulate correctly.
 *
 * Returns 0 on success, non-zero if the stack is full
 * (TIKU_KITS_GFX_CLIP_STACK_DEPTH). The client should make a
 * matched pop_clip() call.
 */
int tiku_kits_gfx_push_clip(tiku_kits_gfx_surface_t *s,
                             const tiku_kits_gfx_rect_t *clip);

/**
 * @brief Drop the topmost clip rectangle. No-op when the stack is
 *        already empty.
 */
void tiku_kits_gfx_pop_clip(tiku_kits_gfx_surface_t *s);

/**
 * @brief Reset the clip stack to empty.
 */
void tiku_kits_gfx_reset_clip(tiku_kits_gfx_surface_t *s);

/*---------------------------------------------------------------------------*/
/* DRAWING PRIMITIVES                                                        */
/*---------------------------------------------------------------------------*/

/* Pixels and lines */
void tiku_kits_gfx_pixel(const tiku_kits_gfx_surface_t *s,
                          int16_t x, int16_t y, uint8_t color);

void tiku_kits_gfx_hline(const tiku_kits_gfx_surface_t *s,
                          int16_t x, int16_t y, uint16_t w, uint8_t color);

void tiku_kits_gfx_vline(const tiku_kits_gfx_surface_t *s,
                          int16_t x, int16_t y, uint16_t h, uint8_t color);

void tiku_kits_gfx_line(const tiku_kits_gfx_surface_t *s,
                         int16_t x0, int16_t y0,
                         int16_t x1, int16_t y1, uint8_t color);

/** Line of configurable thickness. Implementation paints a filled
 *  disc of radius (thickness/2) at every pixel along the Bresenham
 *  path, so the line's endpoints are rounded. Thickness 1 is
 *  identical to tiku_kits_gfx_line. */
void tiku_kits_gfx_line_thick(const tiku_kits_gfx_surface_t *s,
                               int16_t x0, int16_t y0,
                               int16_t x1, int16_t y1,
                               uint8_t thickness, uint8_t color);

/* Rectangles */
void tiku_kits_gfx_rect(const tiku_kits_gfx_surface_t *s,
                         int16_t x, int16_t y,
                         uint16_t w, uint16_t h, uint8_t color);

void tiku_kits_gfx_fill_rect(const tiku_kits_gfx_surface_t *s,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h, uint8_t color);

/** Rounded-corner rectangle outline. Corner radius @p r is clamped
 *  to min(w, h) / 2. */
void tiku_kits_gfx_round_rect(const tiku_kits_gfx_surface_t *s,
                               int16_t x, int16_t y,
                               uint16_t w, uint16_t h, uint16_t r,
                               uint8_t color);

void tiku_kits_gfx_fill_round_rect(const tiku_kits_gfx_surface_t *s,
                                    int16_t x, int16_t y,
                                    uint16_t w, uint16_t h, uint16_t r,
                                    uint8_t color);

/* Triangles */
void tiku_kits_gfx_triangle(const tiku_kits_gfx_surface_t *s,
                             int16_t x0, int16_t y0,
                             int16_t x1, int16_t y1,
                             int16_t x2, int16_t y2,
                             uint8_t color);

/** Filled triangle, scanline algorithm. */
void tiku_kits_gfx_fill_triangle(const tiku_kits_gfx_surface_t *s,
                                  int16_t x0, int16_t y0,
                                  int16_t x1, int16_t y1,
                                  int16_t x2, int16_t y2,
                                  uint8_t color);

/* Circles */
void tiku_kits_gfx_circle(const tiku_kits_gfx_surface_t *s,
                           int16_t cx, int16_t cy, uint16_t r,
                           uint8_t color);

void tiku_kits_gfx_fill_circle(const tiku_kits_gfx_surface_t *s,
                                int16_t cx, int16_t cy, uint16_t r,
                                uint8_t color);

/* Bitmaps */
void tiku_kits_gfx_bitmap(const tiku_kits_gfx_surface_t *s,
                           int16_t x, int16_t y,
                           const uint8_t *bitmap,
                           uint16_t w, uint16_t h,
                           uint8_t color);

#endif /* TIKU_KITS_GFX_H_ */
