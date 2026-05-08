/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_path.c - Polyline / polygon / dashed line impl
 *
 * Polyline / polygon outlines walk the input vertex array and
 * delegate to the existing line primitive. Filled polygons use
 * the standard scanline fill: for each y in the polygon's vertical
 * extent, find x intersections with each edge, sort, and fill
 * pairs of intersections with hlines (even-odd rule).
 *
 * Dashed lines reuse the Bresenham step loop, masking pixels by a
 * 16-bit step counter against the user-supplied pattern.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_gfx_path.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* HELPERS                                                                   */
/*---------------------------------------------------------------------------*/

static inline int16_t iabs16(int16_t v) { return v < 0 ? (int16_t)(-v) : v; }

/*---------------------------------------------------------------------------*/
/* POLYLINE / POLYGON OUTLINE                                                */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_polyline(const tiku_kits_gfx_surface_t *s,
                        const tiku_kits_gfx_point_t *pts,
                        uint16_t n_pts, uint8_t closed,
                        uint8_t color)
{
    uint16_t i;
    if (s == NULL || s->set_pixel == NULL || pts == NULL || n_pts < 2u) {
        return;
    }
    for (i = 1; i < n_pts; i++) {
        tiku_kits_gfx_line(s,
            pts[i - 1].x, pts[i - 1].y,
            pts[i].x,     pts[i].y, color);
    }
    if (closed) {
        tiku_kits_gfx_line(s,
            pts[n_pts - 1].x, pts[n_pts - 1].y,
            pts[0].x,         pts[0].y, color);
    }
}

void
tiku_kits_gfx_polygon(const tiku_kits_gfx_surface_t *s,
                       const tiku_kits_gfx_point_t *pts,
                       uint16_t n_pts, uint8_t color)
{
    tiku_kits_gfx_polyline(s, pts, n_pts, 1u, color);
}

/*---------------------------------------------------------------------------*/
/* SCANLINE POLYGON FILL                                                     */
/*---------------------------------------------------------------------------*/

/* Insert @p val into the sorted slice xs[0..*n) ascending, capped at
 * @p cap. Drops the value silently if the slice is full. */
static void
sorted_insert(int16_t *xs, uint16_t *n, uint16_t cap, int16_t val)
{
    uint16_t i;
    if (*n >= cap) return;
    i = *n;
    while (i > 0 && xs[i - 1] > val) {
        xs[i] = xs[i - 1];
        i--;
    }
    xs[i] = val;
    (*n)++;
}

void
tiku_kits_gfx_fill_polygon(const tiku_kits_gfx_surface_t *s,
                            const tiku_kits_gfx_point_t *pts,
                            uint16_t n_pts, uint8_t color)
{
    int16_t  ymin, ymax, y;
    int16_t  xs[TIKU_KITS_GFX_POLYGON_MAX_INTERSECTIONS];
    uint16_t i;

    if (s == NULL || s->set_pixel == NULL || pts == NULL || n_pts < 3u) {
        return;
    }

    /* Compute vertical extent. */
    ymin = pts[0].y;
    ymax = pts[0].y;
    for (i = 1; i < n_pts; i++) {
        if (pts[i].y < ymin) ymin = pts[i].y;
        if (pts[i].y > ymax) ymax = pts[i].y;
    }

    /* Scanline. For each y, find intersections of edges with that
     * scanline (using the standard half-open convention: include
     * the lower endpoint, exclude the upper endpoint, to prevent
     * shared-vertex double-counting). */
    for (y = ymin; y <= ymax; y++) {
        uint16_t n_xs = 0;

        for (i = 0; i < n_pts; i++) {
            int16_t y0 = pts[i].y;
            int16_t y1 = pts[(i + 1u) % n_pts].y;
            int16_t x0 = pts[i].x;
            int16_t x1 = pts[(i + 1u) % n_pts].x;
            int16_t lo_y, hi_y, lo_x, hi_x;

            if (y0 == y1) continue;            /* horizontal edge */

            if (y0 < y1) { lo_y = y0; hi_y = y1; lo_x = x0; hi_x = x1; }
            else         { lo_y = y1; hi_y = y0; lo_x = x1; hi_x = x0; }

            /* Half-open: y in [lo_y, hi_y). */
            if (y < lo_y || y >= hi_y) continue;

            {
                int32_t dx = (int32_t)hi_x - (int32_t)lo_x;
                int32_t dy = (int32_t)hi_y - (int32_t)lo_y;
                int32_t numer = dx * (int32_t)(y - lo_y);
                /* Round-to-nearest interpolation. */
                int32_t add = (numer >= 0) ? (dy / 2) : (-dy / 2);
                int32_t x_at_y = (int32_t)lo_x + (numer + add) / dy;
                sorted_insert(xs, &n_xs,
                    TIKU_KITS_GFX_POLYGON_MAX_INTERSECTIONS,
                    (int16_t)x_at_y);
            }
        }

        /* Fill in pairs (even-odd rule). */
        {
            uint16_t k;
            for (k = 0; k + 1u < n_xs; k += 2u) {
                int16_t  xa = xs[k];
                int16_t  xb = xs[k + 1u];
                if (xa > xb) { int16_t t = xa; xa = xb; xb = t; }
                tiku_kits_gfx_hline(s, xa, y,
                    (uint16_t)(xb - xa + 1), color);
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
/* DASHED LINE                                                               */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_line_dashed(const tiku_kits_gfx_surface_t *s,
                           int16_t x0, int16_t y0,
                           int16_t x1, int16_t y1,
                           uint16_t pattern, uint8_t color)
{
    int16_t dx, dy, sx, sy, err, e2;
    uint16_t step = 0;

    if (s == NULL || s->set_pixel == NULL) return;
    if (pattern == 0u) return;

    dx = iabs16((int16_t)(x1 - x0));
    dy = iabs16((int16_t)(y1 - y0));
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = (int16_t)(dx - dy);

    for (;;) {
        if ((pattern >> (step & 0x0Fu)) & 1u) {
            tiku_kits_gfx_pixel(s, x0, y0, color);
        }
        step++;
        if (x0 == x1 && y0 == y1) break;
        e2 = (int16_t)(2 * err);
        if (e2 > -dy) { err = (int16_t)(err - dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}
