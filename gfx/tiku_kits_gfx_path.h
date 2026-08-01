/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_path.h - Polylines, polygons, dashed lines
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Multi-segment line and polygon rendering for the gfx kit. All
 * primitives operate on caller-allocated arrays of points; no
 * dynamic memory.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef TIKU_KITS_GFX_PATH_H_
#define TIKU_KITS_GFX_PATH_H_

#include "tiku_kits_gfx.h"

/*---------------------------------------------------------------------------*/
/* MAX VERTEX COUNT FOR FILL_POLYGON                                         */
/*---------------------------------------------------------------------------*/

/* Override at compile time if you need to fill polygons with more
 * vertices. Cost is 2 bytes of stack per slot during fill. */
#ifndef TIKU_KITS_GFX_POLYGON_MAX_INTERSECTIONS
#define TIKU_KITS_GFX_POLYGON_MAX_INTERSECTIONS 16
#endif

/*---------------------------------------------------------------------------*/
/* POLYLINE                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Draw a connected sequence of line segments.
 *
 * Renders @p n_pts - 1 line segments connecting consecutive points.
 * Pass @p closed = 1 to also draw a segment from the last point
 * back to the first.
 *
 * @param pts     Array of points (caller-allocated).
 * @param n_pts   Number of points (>= 2).
 * @param closed  1 to close back to pts[0]; 0 for open polyline.
 */
void tiku_kits_gfx_polyline(const tiku_kits_gfx_surface_t *s,
                             const tiku_kits_gfx_point_t *pts,
                             uint16_t n_pts, uint8_t closed,
                             uint8_t color);

/*---------------------------------------------------------------------------*/
/* POLYGON                                                                   */
/*---------------------------------------------------------------------------*/

/** Closed polygon outline (alias for polyline with closed=1). */
void tiku_kits_gfx_polygon(const tiku_kits_gfx_surface_t *s,
                            const tiku_kits_gfx_point_t *pts,
                            uint16_t n_pts, uint8_t color);

/*
 * Works for convex and concave polygons. Self-intersecting polygons
 * fill the parts hit an odd number of times (standard even-odd
 * rule).
 * Uses a fixed-size scratch buffer of
 * `TIKU_KITS_GFX_POLYGON_MAX_INTERSECTIONS` x-coordinates per
 * scanline. If a scanline crosses more edges than that, only the
 * first N intersections are used (extra crossings silently
 * dropped).
 */

/**
 * @brief Filled polygon (even-odd rule, scanline fill).
 */
void tiku_kits_gfx_fill_polygon(const tiku_kits_gfx_surface_t *s,
                                 const tiku_kits_gfx_point_t *pts,
                                 uint16_t n_pts, uint8_t color);

/*---------------------------------------------------------------------------*/
/* DASHED LINES                                                              */
/*---------------------------------------------------------------------------*/

/*
 * The pattern is a bit mask that selects which pixels along the
 * line are drawn: bit 0 corresponds to the starting pixel, bit 1
 * to the next, etc., wrapping every 16 pixels.
 * Common patterns:
 * 0xFFFF  solid (== tiku_kits_gfx_line)
 * 0x5555  dotted (every other pixel)
 * 0xCCCC  short dash (2 on, 2 off)
 * 0x0F0F  long dash (4 on, 4 off)
 * 0x33FF  dash-dot
 */

/**
 * @brief Draw a Bresenham line modulated by a 16-bit pattern.
 */
void tiku_kits_gfx_line_dashed(const tiku_kits_gfx_surface_t *s,
                                int16_t x0, int16_t y0,
                                int16_t x1, int16_t y1,
                                uint16_t pattern, uint8_t color);

#endif /* TIKU_KITS_GFX_PATH_H_ */
