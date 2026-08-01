/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_curve.h - Ellipse, arc, and pie primitives
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Extends the core gfx kit with curved-shape rendering. All four
 * primitives self-clip against the surface bounds and use only
 * integer arithmetic -- no trig, no floating point.
 *
 * Angle convention (for arc / pie):
 * - 0 deg points right (+x), angles increase counter-clockwise
 * in screen space (y down means visually clockwise on display).
 * - Wraparound is supported: arc(., 350, 10, ...) draws a 20-degree
 * wedge straddling 0.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef TIKU_KITS_GFX_CURVE_H_
#define TIKU_KITS_GFX_CURVE_H_

#include "tiku_kits_gfx.h"

/*---------------------------------------------------------------------------*/
/* ELLIPSES                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Draw an axis-aligned ellipse outline centered at (cx, cy).
 *
 * @p rx, @p ry are the half-axes in pixels. Degenerates gracefully:
 *   - rx == 0 && ry == 0  -> single pixel
 *   - rx == 0             -> vertical line of length 2*ry+1
 *   - ry == 0             -> horizontal line of length 2*rx+1
 */
void tiku_kits_gfx_ellipse(const tiku_kits_gfx_surface_t *s,
                            int16_t cx, int16_t cy,
                            uint16_t rx, uint16_t ry,
                            uint8_t color);

/** Filled axis-aligned ellipse (scanline). */
void tiku_kits_gfx_fill_ellipse(const tiku_kits_gfx_surface_t *s,
                                 int16_t cx, int16_t cy,
                                 uint16_t rx, uint16_t ry,
                                 uint8_t color);

/*---------------------------------------------------------------------------*/
/* CIRCULAR ARCS                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Draw a circular arc outline centered at (cx, cy).
 *
 * @param start_deg  Start angle in degrees (0 = right; CCW positive).
 * @param end_deg    End angle in degrees, modulo 360.
 *
 * If end_deg <= start_deg the arc wraps through 0. Both endpoints
 * are inclusive. To draw a full circle use start = 0, end = 360.
 */
void tiku_kits_gfx_arc(const tiku_kits_gfx_surface_t *s,
                        int16_t cx, int16_t cy, uint16_t r,
                        int16_t start_deg, int16_t end_deg,
                        uint8_t color);

/**
 * @brief Filled pie slice (sector): the arc above plus two radial
 *        edges back to (cx, cy), filled.
 *
 * Useful for gauges and donut indicators (subtract a smaller pie
 * for a ring).
 */
void tiku_kits_gfx_pie(const tiku_kits_gfx_surface_t *s,
                        int16_t cx, int16_t cy, uint16_t r,
                        int16_t start_deg, int16_t end_deg,
                        uint8_t color);

#endif /* TIKU_KITS_GFX_CURVE_H_ */
