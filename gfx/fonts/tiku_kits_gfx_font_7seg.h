/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_font_7seg.h - 12x16 7-segment-style digit font
 *
 * Stylised numeric font shaped like a 7-segment LED display.
 * Designed for clocks, dashboards, and digital indicators.
 *
 * Glyph map covers '-' (0x2D) through ':' (0x3A):
 *
 *   '-'  minus       (segment g only)
 *   '.'  decimal     (lower-right dot)
 *   '/'  empty       (placeholder)
 *   '0'  digit 0     (a, b, c, d, e, f)
 *   '1'  digit 1     (b, c)
 *   '2'  digit 2     (a, b, d, e, g)
 *   '3'  digit 3     (a, b, c, d, g)
 *   '4'  digit 4     (b, c, f, g)
 *   '5'  digit 5     (a, c, d, f, g)
 *   '6'  digit 6     (a, c, d, e, f, g)
 *   '7'  digit 7     (a, b, c)
 *   '8'  digit 8     (all)
 *   '9'  digit 9     (a, b, c, d, f, g)
 *   ':'  colon       (two square dots)
 *
 * Total cost: ~336 B FRAM (14 glyphs x 24 B + descriptor).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_GFX_FONT_7SEG_H_
#define TIKU_KITS_GFX_FONT_7SEG_H_

#include "../tiku_kits_gfx_text.h"

extern const tiku_kits_gfx_font_t tiku_kits_gfx_font_7seg;

#endif /* TIKU_KITS_GFX_FONT_7SEG_H_ */
