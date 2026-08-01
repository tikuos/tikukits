/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_font_symbols.h - 16x16 starter symbol font
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A small bitmap font of common UI glyphs at 16x16 cell size,
 * exposed through the standard tiku_kits_gfx_font_t interface so
 * apps can draw icons inline with text via tiku_kits_gfx_draw_char.
 *
 * Glyph map (ASCII offset -> icon):
 * '0'  battery (full)
 * '1'  wifi (signal bars)
 * '2'  check
 * '3'  cross
 *
 * To add more glyphs, either:
 * 1. Hand-code into tiku_kits_gfx_font_symbols.c and bump @last
 * in the descriptor; OR
 * 2. Skip the font entirely and ship icons as
 * tiku_kits_gfx_image_t assets baked from PNGs through
 * tools/icon_bake.py.
 *
 * Total cost: ~128 B glyph data + descriptor.
 */

#ifndef TIKU_KITS_GFX_FONT_SYMBOLS_H_
#define TIKU_KITS_GFX_FONT_SYMBOLS_H_

#include "../tiku_kits_gfx_text.h"

extern const tiku_kits_gfx_font_t tiku_kits_gfx_font_symbols;

/* Convenience character codes for the bundled glyphs. */
#define TIKU_KITS_GFX_SYM_BATTERY  '0'
#define TIKU_KITS_GFX_SYM_WIFI     '1'
#define TIKU_KITS_GFX_SYM_CHECK    '2'
#define TIKU_KITS_GFX_SYM_CROSS    '3'

#endif /* TIKU_KITS_GFX_FONT_SYMBOLS_H_ */
