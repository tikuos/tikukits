/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_text.h - Bitmap font + text rendering for the gfx kit
 *
 * Renders 1-bit-per-pixel bitmap fonts on any tiku_kits_gfx_surface.
 * Supports both monospaced and proportional (variable-width) fonts,
 * multi-line layout with word-wrap, and horizontal alignment.
 *
 * Glyph format (column-major, 1bpp, LSB = top row):
 *   - Each glyph occupies `width * bytes_per_column` bytes, where
 *     `bytes_per_column = ceil(height / 8)`.
 *   - Within each byte, bit 0 = topmost row of that column, bit 6 =
 *     next row down, etc. For fonts up to 8 rows tall, one byte per
 *     column. For taller fonts (e.g. 8x16), two bytes per column
 *     stacked top-to-bottom.
 *   - For proportional fonts, each glyph still occupies the full
 *     `width * bytes_per_column` bytes (some columns are empty);
 *     the per-glyph advance is taken from the `widths` array.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_GFX_TEXT_H_
#define TIKU_KITS_GFX_TEXT_H_

#include "tiku_kits_gfx.h"

/*---------------------------------------------------------------------------*/
/* FONT DESCRIPTOR                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Static description of a bitmap font.
 *
 * Apps may declare additional fonts in their own code or as part
 * of a separate font module under `tikukits/gfx/fonts/`.
 *
 * Field meanings:
 *   - `width`           Maximum (or fixed) glyph width in pixels.
 *                       Used for monospaced layout when `widths`
 *                       is NULL.
 *   - `height`          Glyph height in pixels.
 *   - `first`, `last`   Inclusive character range covered by the
 *                       font (interpreted as raw bytes).
 *   - `bytes_per_column` ceil(height / 8). 1 for fonts up to 8 px
 *                       tall, 2 for 9..16 px, etc.
 *   - `glyphs`          Glyph data, indexed by (c - first).
 *   - `widths`          Optional per-glyph advance widths (one byte
 *                       per glyph). NULL = monospaced (use `width`
 *                       for every glyph). Variable-width fonts use
 *                       this to render proportionally.
 *   - `ascent`          Pixels from glyph top to baseline (typically
 *                       `height - descent`). Optional metadata.
 *   - `descent`         Pixels from baseline to glyph bottom.
 *                       Optional metadata.
 *   - `line_height`     Recommended vertical advance for multi-line
 *                       text. If 0, falls back to `height + 1`.
 *   - `fallback`        Optional next-resort font. When the primary
 *                       font lacks a glyph for the requested
 *                       character, draw_char and friends walk this
 *                       chain. NULL ends the chain. Authors must
 *                       avoid cycles (e.g. font_a.fallback = &font_b
 *                       and font_b.fallback = &font_a).
 */
typedef struct tiku_kits_gfx_font tiku_kits_gfx_font_t;
struct tiku_kits_gfx_font {
    uint8_t width;
    uint8_t height;
    uint8_t first;
    uint8_t last;
    uint8_t bytes_per_column;
    const uint8_t *glyphs;
    const uint8_t *widths;
    uint8_t ascent;
    uint8_t descent;
    uint8_t line_height;
    const tiku_kits_gfx_font_t *fallback;
};

/*---------------------------------------------------------------------------*/
/* UTF-8 DECODER                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Decode one UTF-8 codepoint and advance @p p past it.
 *
 * On a malformed sequence the function consumes one byte and
 * returns 0xFFFD (the Unicode replacement character).
 *
 * Useful when a font's glyph table is indexed by codepoint above
 * 0x7F (Latin-1, Greek, symbols, etc.). For ASCII-only callers the
 * existing draw_char / draw_string ASCII paths remain.
 *
 * @return The decoded codepoint, or 0 if @p p reached '\0'.
 */
uint32_t tiku_kits_gfx_utf8_next(const char **p);

/*---------------------------------------------------------------------------*/
/* TEXT ALIGNMENT                                                            */
/*---------------------------------------------------------------------------*/

typedef enum {
    TIKU_KITS_GFX_ALIGN_LEFT   = 0,
    TIKU_KITS_GFX_ALIGN_CENTER = 1,
    TIKU_KITS_GFX_ALIGN_RIGHT  = 2,
} tiku_kits_gfx_align_t;

/** Vertical alignment used by tiku_kits_gfx_draw_string_in_box. */
typedef enum {
    TIKU_KITS_GFX_VALIGN_TOP    = 0,
    TIKU_KITS_GFX_VALIGN_MIDDLE = 1,
    TIKU_KITS_GFX_VALIGN_BOTTOM = 2,
} tiku_kits_gfx_valign_t;

/*---------------------------------------------------------------------------*/
/* SINGLE-CHARACTER / SINGLE-LINE TEXT                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Draw one character. Returns the rendered glyph's advance
 *        in pixels (== width for monospaced, == widths[c] for
 *        proportional). Returns 0 for out-of-range characters.
 */
uint16_t tiku_kits_gfx_draw_char(const tiku_kits_gfx_surface_t *s,
                                  int16_t x, int16_t y, char c,
                                  const tiku_kits_gfx_font_t *font,
                                  uint8_t color, uint8_t scale);

/**
 * @brief Draw a single-line null-terminated string. No wrapping;
 *        text past the surface edge is clipped.
 */
void tiku_kits_gfx_draw_string(const tiku_kits_gfx_surface_t *s,
                                int16_t x, int16_t y, const char *str,
                                const tiku_kits_gfx_font_t *font,
                                uint8_t color, uint8_t scale);

/**
 * @brief Compute the rendered pixel width of @p str at @p scale.
 *        Honours per-glyph widths when present.
 */
uint16_t tiku_kits_gfx_text_width(const char *str,
                                   const tiku_kits_gfx_font_t *font,
                                   uint8_t scale);

/**
 * @brief Recommended line height in display pixels at @p scale.
 *        Uses font->line_height if non-zero, else (height + 1).
 */
uint16_t tiku_kits_gfx_line_height(const tiku_kits_gfx_font_t *font,
                                    uint8_t scale);

/*---------------------------------------------------------------------------*/
/* MULTI-LINE / RECT-CONSTRAINED TEXT                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Draw a single line of text inside @p rect with the given
 *        horizontal alignment. Vertical position is the top of the
 *        rect; for vertical centering, compute the y offset from
 *        text height and pass an adjusted rect.
 *
 * @return Number of characters rendered (== strlen if it all fit).
 */
uint16_t tiku_kits_gfx_draw_string_in_rect(
    const tiku_kits_gfx_surface_t *s,
    const tiku_kits_gfx_rect_t *rect,
    const char *str,
    const tiku_kits_gfx_font_t *font,
    uint8_t color, uint8_t scale,
    tiku_kits_gfx_align_t align);

/**
 * @brief Render @p str inside @p rect, breaking at word boundaries.
 *        Lines that are too long for the rect width are broken at
 *        the last whitespace before overflow; words longer than
 *        the rect width are broken character-wise.
 *
 *        Vertical advance per line uses tiku_kits_gfx_line_height.
 *        Lines that fall outside @p rect are not drawn.
 *
 * @return Number of lines rendered.
 */
uint16_t tiku_kits_gfx_draw_text_wrapped(
    const tiku_kits_gfx_surface_t *s,
    const tiku_kits_gfx_rect_t *rect,
    const char *str,
    const tiku_kits_gfx_font_t *font,
    uint8_t color, uint8_t scale,
    tiku_kits_gfx_align_t align);

/*---------------------------------------------------------------------------*/
/* PHASE 0 ADDITIONS: VALIGN, TRUNCATION, MEASUREMENT                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Single-line text inside @p rect with both horizontal and
 *        vertical alignment.
 *
 * Like tiku_kits_gfx_draw_string_in_rect but also honours @p valign
 * (TOP / MIDDLE / BOTTOM). The text's vertical extent is one
 * line height (font->line_height or font->height + 1, scaled).
 */
uint16_t tiku_kits_gfx_draw_string_in_box(
    const tiku_kits_gfx_surface_t *s,
    const tiku_kits_gfx_rect_t *rect,
    const char *str,
    const tiku_kits_gfx_font_t *font,
    uint8_t color, uint8_t scale,
    tiku_kits_gfx_align_t halign,
    tiku_kits_gfx_valign_t valign);

/**
 * @brief Single-line text with ellipsis truncation when @p str
 *        does not fit @p rect width.
 *
 * Renders as much of @p str as fits, followed by "..." (or the
 * supplied @p ellipsis if non-NULL) when truncated. Vertically
 * top-aligned within @p rect. Pass @p ellipsis = NULL for the
 * default "..."; pass "" to disable the ellipsis (hard truncate).
 *
 * @return Number of source characters actually rendered (excluding
 *         the ellipsis run).
 */
uint16_t tiku_kits_gfx_draw_string_truncated(
    const tiku_kits_gfx_surface_t *s,
    const tiku_kits_gfx_rect_t *rect,
    const char *str,
    const char *ellipsis,
    const tiku_kits_gfx_font_t *font,
    uint8_t color, uint8_t scale,
    tiku_kits_gfx_align_t halign);

/**
 * @brief Compute how many lines / how much vertical space @p str
 *        would consume when wrapped to @p max_width.
 *
 * Performs the same word-wrap pass as tiku_kits_gfx_draw_text_wrapped
 * without rendering. Lets layout containers size paragraphs and
 * scrollers compute their content height up-front.
 *
 * @param out_lines     Optional: number of wrapped lines.
 * @param out_height_px Optional: total vertical extent in display
 *                      pixels (lines * line_height).
 */
void tiku_kits_gfx_measure_wrapped(
    const char *str,
    const tiku_kits_gfx_font_t *font,
    uint8_t scale,
    uint16_t max_width,
    uint16_t *out_lines,
    uint16_t *out_height_px);

#endif /* TIKU_KITS_GFX_TEXT_H_ */
