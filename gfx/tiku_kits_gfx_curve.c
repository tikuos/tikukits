/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_curve.c - Ellipse / arc / pie primitives
 *
 * Implementation notes
 * --------------------
 * Ellipses use a scanline algorithm: walk y in [0, ry], compute
 * floor(x) on the curve via integer sqrt, plot the four symmetry
 * points; then walk x in [1, rx-1] and plot the y symmetry points.
 * The two sweeps together cover the curve without 1-pixel gaps; a
 * couple of cardinal pixels may be plotted twice but that's
 * idempotent for any real put_pixel.
 *
 * Arcs use the midpoint circle algorithm but filter each octant
 * point against the requested arc range using a cross-product
 * inclusion test in user space (y up, CCW positive). The test
 * needs only sin/cos of the two boundary angles; values come from
 * a 91-entry int8 sin table (Q0.7).
 *
 * Pies use the same inclusion test but iterate over the bounding
 * disc, filling pixels that satisfy both the disc test and the arc
 * test. O(r^2) but acceptable for embedded UI use cases.
 *
 * Overflow envelope: with the int32 multiplications used here,
 * ellipse rx * ry up to ~46000 (e.g. 200 x 230) is safe; arc /
 * pie radii up to ~32000 are safe. Embedded panels are well
 * within these bounds.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_gfx_curve.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* SIN / COS LOOKUP (Q0.7, 91 entries for 0..90 deg)                         */
/*---------------------------------------------------------------------------*/

/* sin_table[a] == round(sin(a deg) * 127) for a in 0..90.
 * Cost: 91 bytes of constant data. Worst-case angular accuracy is
 * about 0.5 deg which suffices for the cross-product inclusion
 * test used by arc / pie. */
static const int8_t sin_table[91] = {
       0,   2,   4,   7,   9,  11,  13,  15,  18,  20,
      22,  24,  26,  29,  31,  33,  35,  37,  39,  41,
      43,  46,  48,  50,  52,  54,  56,  58,  60,  62,
      64,  65,  67,  69,  71,  73,  75,  76,  78,  80,
      82,  83,  85,  87,  88,  90,  91,  93,  94,  96,
      97,  99, 100, 101, 103, 104, 105, 107, 108, 109,
     110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
     119, 120, 121, 121, 122, 123, 123, 124, 124, 125,
     125, 125, 126, 126, 126, 127, 127, 127, 127, 127,
     127
};

/* Normalize @p deg to [0, 360). */
static int16_t
norm_deg(int16_t deg)
{
    while (deg < 0)    deg = (int16_t)(deg + 360);
    while (deg >= 360) deg = (int16_t)(deg - 360);
    return deg;
}

/* sin in Q0.7 over the full 0..359 range. */
static int8_t
sin_q7(int16_t deg)
{
    int16_t a = norm_deg(deg);
    if (a <=  90) return  sin_table[a];
    if (a <= 180) return  sin_table[180 - a];
    if (a <= 270) return (int8_t)(-sin_table[a - 180]);
    return (int8_t)(-sin_table[360 - a]);
}

static int8_t
cos_q7(int16_t deg)
{
    return sin_q7((int16_t)(deg + 90));
}

/*---------------------------------------------------------------------------*/
/* INTEGER SQUARE ROOT                                                       */
/*---------------------------------------------------------------------------*/

/* Returns floor(sqrt(v)). Standard digit-by-digit (binary) method;
 * runs in O(log4 v) iterations -- about 16 for any uint32. */
static uint32_t
isqrt32(uint32_t v)
{
    uint32_t r = 0;
    uint32_t b = 0x40000000u;
    while (b > v) b >>= 2;
    while (b != 0) {
        uint32_t rb = r + b;
        if (v >= rb) {
            v -= rb;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}

/*---------------------------------------------------------------------------*/
/* ARC INCLUSION TEST                                                        */
/*---------------------------------------------------------------------------*/

/* Test whether the screen-relative offset (dx, dy) lies inside the
 * arc bounded by direction vectors (sx, sy) [start] and (ex, ey)
 * [end]. All direction vectors are user-space (y up), Q0.7. The
 * caller passes in @p wrap = 1 if the arc spans > 180 deg.
 *
 * In user space the point under test is (dx, -dy). Cross products
 * cross(start, P) and cross(P, end) are positive when P lies CCW
 * of start and CCW of end respectively. */
static int
point_in_arc(int16_t dx, int16_t dy,
              int8_t sx, int8_t sy,
              int8_t ex, int8_t ey,
              int wrap)
{
    int32_t cs = -((int32_t)sx * (int32_t)dy + (int32_t)sy * (int32_t)dx);
    int32_t ce =  ((int32_t)dx * (int32_t)ey + (int32_t)dy * (int32_t)ex);
    if (wrap) {
        return (cs >= 0) || (ce >= 0);
    }
    return (cs >= 0) && (ce >= 0);
}

/*---------------------------------------------------------------------------*/
/* ELLIPSE: x AT y AND y AT x                                                */
/*---------------------------------------------------------------------------*/

/* x = floor(rx * sqrt(1 - y^2/ry^2)).  Returns 0 if y >= ry. */
static int16_t
ell_x_at_y(uint16_t rx, uint16_t ry, uint16_t y)
{
    uint32_t rx2, ry2, dy2, numer;
    if (y >= ry) return 0;
    rx2   = (uint32_t)rx * (uint32_t)rx;
    ry2   = (uint32_t)ry * (uint32_t)ry;
    dy2   = (uint32_t)y  * (uint32_t)y;
    numer = rx2 * (ry2 - dy2);
    return (int16_t)isqrt32(numer / ry2);
}

static int16_t
ell_y_at_x(uint16_t rx, uint16_t ry, uint16_t x)
{
    uint32_t rx2, ry2, dx2, numer;
    if (x >= rx) return 0;
    rx2   = (uint32_t)rx * (uint32_t)rx;
    ry2   = (uint32_t)ry * (uint32_t)ry;
    dx2   = (uint32_t)x  * (uint32_t)x;
    numer = ry2 * (rx2 - dx2);
    return (int16_t)isqrt32(numer / rx2);
}

/*---------------------------------------------------------------------------*/
/* ELLIPSE OUTLINE                                                           */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_ellipse(const tiku_kits_gfx_surface_t *s,
                       int16_t cx, int16_t cy,
                       uint16_t rx, uint16_t ry,
                       uint8_t color)
{
    uint16_t i;

    if (s == NULL || s->set_pixel == NULL) return;

    if (rx == 0 && ry == 0) {
        tiku_kits_gfx_pixel(s, cx, cy, color);
        return;
    }
    if (rx == 0) {
        tiku_kits_gfx_vline(s, cx, (int16_t)(cy - (int16_t)ry),
                             (uint16_t)(2u * ry + 1u), color);
        return;
    }
    if (ry == 0) {
        tiku_kits_gfx_hline(s, (int16_t)(cx - (int16_t)rx), cy,
                             (uint16_t)(2u * rx + 1u), color);
        return;
    }

    /* y sweep: top + bottom rows. */
    for (i = 0; i <= ry; i++) {
        int16_t x = ell_x_at_y(rx, ry, i);
        tiku_kits_gfx_pixel(s, (int16_t)(cx + x), (int16_t)(cy - (int16_t)i), color);
        tiku_kits_gfx_pixel(s, (int16_t)(cx - x), (int16_t)(cy - (int16_t)i), color);
        if (i != 0) {
            tiku_kits_gfx_pixel(s, (int16_t)(cx + x), (int16_t)(cy + (int16_t)i), color);
            tiku_kits_gfx_pixel(s, (int16_t)(cx - x), (int16_t)(cy + (int16_t)i), color);
        }
    }

    /* x sweep: fills any near-cardinal gaps where dy/dx is large. */
    for (i = 1; i < rx; i++) {
        int16_t y = ell_y_at_x(rx, ry, i);
        tiku_kits_gfx_pixel(s, (int16_t)(cx + (int16_t)i), (int16_t)(cy + y), color);
        tiku_kits_gfx_pixel(s, (int16_t)(cx + (int16_t)i), (int16_t)(cy - y), color);
        tiku_kits_gfx_pixel(s, (int16_t)(cx - (int16_t)i), (int16_t)(cy + y), color);
        tiku_kits_gfx_pixel(s, (int16_t)(cx - (int16_t)i), (int16_t)(cy - y), color);
    }
}

/*---------------------------------------------------------------------------*/
/* ELLIPSE FILL                                                              */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_fill_ellipse(const tiku_kits_gfx_surface_t *s,
                            int16_t cx, int16_t cy,
                            uint16_t rx, uint16_t ry,
                            uint8_t color)
{
    int16_t y;

    if (s == NULL || s->set_pixel == NULL) return;

    if (rx == 0 && ry == 0) {
        tiku_kits_gfx_pixel(s, cx, cy, color);
        return;
    }
    if (rx == 0) {
        tiku_kits_gfx_vline(s, cx, (int16_t)(cy - (int16_t)ry),
                             (uint16_t)(2u * ry + 1u), color);
        return;
    }
    if (ry == 0) {
        tiku_kits_gfx_hline(s, (int16_t)(cx - (int16_t)rx), cy,
                             (uint16_t)(2u * rx + 1u), color);
        return;
    }

    for (y = -(int16_t)ry; y <= (int16_t)ry; y++) {
        uint16_t ay = (uint16_t)(y < 0 ? -y : y);
        int16_t  x  = ell_x_at_y(rx, ry, ay);
        tiku_kits_gfx_hline(s, (int16_t)(cx - x), (int16_t)(cy + y),
                             (uint16_t)(2 * x + 1), color);
    }
}

/*---------------------------------------------------------------------------*/
/* ARC + PIE COMMON SETUP                                                    */
/*---------------------------------------------------------------------------*/

typedef struct {
    int8_t sx, sy;       /* user-space start direction (Q0.7) */
    int8_t ex, ey;       /* user-space end direction          */
    int    full;         /* 1 = render full circle / disc     */
    int    wrap;         /* 1 = arc spans > 180 deg           */
    int    empty;        /* 1 = degenerate (no render)        */
} arc_setup_t;

static void
arc_setup(arc_setup_t *out, int16_t start_deg, int16_t end_deg)
{
    int16_t span_signed = (int16_t)(end_deg - start_deg);
    int16_t start_n, end_n;
    int16_t span_ccw;

    out->full = 0;
    out->wrap = 0;
    out->empty = 0;

    if (span_signed >= 360 || span_signed <= -360) {
        out->full = 1;
        return;
    }

    start_n = norm_deg(start_deg);
    end_n   = norm_deg(end_deg);
    if (start_n == end_n) {
        out->empty = 1;
        return;
    }

    span_ccw = (int16_t)(((int32_t)end_n - (int32_t)start_n + 360) % 360);
    out->wrap = (span_ccw > 180);

    out->sx = cos_q7(start_n);
    out->sy = sin_q7(start_n);
    out->ex = cos_q7(end_n);
    out->ey = sin_q7(end_n);
}

/*---------------------------------------------------------------------------*/
/* ARC OUTLINE                                                               */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_arc(const tiku_kits_gfx_surface_t *s,
                   int16_t cx, int16_t cy, uint16_t r,
                   int16_t start_deg, int16_t end_deg,
                   uint8_t color)
{
    arc_setup_t a;
    int16_t x, y, d;

    if (s == NULL || s->set_pixel == NULL || r == 0) return;

    arc_setup(&a, start_deg, end_deg);
    if (a.empty) return;
    if (a.full) {
        tiku_kits_gfx_circle(s, cx, cy, r, color);
        return;
    }

    /* Midpoint circle walk; for each octant point test arc inclusion. */
    x = (int16_t)r;
    y = 0;
    d = (int16_t)(1 - x);

    while (x >= y) {
        const int16_t pts[8][2] = {
            {  x,  y }, { -x,  y }, {  x, -y }, { -x, -y },
            {  y,  x }, { -y,  x }, {  y, -x }, { -y, -x }
        };
        uint8_t i;
        for (i = 0; i < 8; i++) {
            int16_t dx = pts[i][0];
            int16_t dy = pts[i][1];
            if (point_in_arc(dx, dy, a.sx, a.sy, a.ex, a.ey, a.wrap)) {
                tiku_kits_gfx_pixel(s,
                    (int16_t)(cx + dx), (int16_t)(cy + dy), color);
            }
        }
        y++;
        if (d <= 0) d = (int16_t)(d + 2 * y + 1);
        else { x--; d = (int16_t)(d + 2 * (y - x) + 1); }
    }
}

/*---------------------------------------------------------------------------*/
/* PIE FILL                                                                  */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_pie(const tiku_kits_gfx_surface_t *s,
                   int16_t cx, int16_t cy, uint16_t r,
                   int16_t start_deg, int16_t end_deg,
                   uint8_t color)
{
    arc_setup_t a;
    int16_t dx, dy;
    int32_t r2;

    if (s == NULL || s->set_pixel == NULL || r == 0) return;

    arc_setup(&a, start_deg, end_deg);
    if (a.empty) return;
    if (a.full) {
        tiku_kits_gfx_fill_circle(s, cx, cy, r, color);
        return;
    }

    r2 = (int32_t)r * (int32_t)r;
    for (dy = -(int16_t)r; dy <= (int16_t)r; dy++) {
        for (dx = -(int16_t)r; dx <= (int16_t)r; dx++) {
            int32_t d2 = (int32_t)dx * (int32_t)dx
                       + (int32_t)dy * (int32_t)dy;
            if (d2 > r2) continue;
            if (point_in_arc(dx, dy, a.sx, a.sy, a.ex, a.ey, a.wrap)) {
                tiku_kits_gfx_pixel(s,
                    (int16_t)(cx + dx), (int16_t)(cy + dy), color);
            }
        }
    }
}
