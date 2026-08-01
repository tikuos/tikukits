/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_text.c - Bitmap text rendering on a gfx surface
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Renders monospaced and proportional bitmap fonts with optional
 * scaling, multi-line wrapping, and horizontal alignment.
 *
 * Word-wrap algorithm:
 * For each rect width, scan str to find the longest prefix that
 * fits and ends at a word boundary. If no boundary is found
 * within the available width, break at the last character that
 * fits (character-wise breaking for over-long words). Render the
 * line, advance y, and repeat from the position after the break.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "tiku_kits_gfx_text.h"

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/* Walk the font's fallback chain looking for one whose first..last
 * range covers @p uc. Bounds the walk to a small depth so a
 * malformed cycle won't hang. Returns NULL if no font in the
 * chain has the glyph. */
static const tiku_kits_gfx_font_t *
font_for_glyph(const tiku_kits_gfx_font_t *font, uint8_t uc)
{
    int hops = 8;
    while (font != NULL && hops-- > 0) {
        if (uc >= font->first && uc <= font->last) return font;
        font = font->fallback;
    }
    return NULL;
}

/* Width of one glyph in font pixels, accounting for proportional widths
 * and falling back to the chain when needed. */
static uint8_t
glyph_width(const tiku_kits_gfx_font_t *font, char c)
{
    const tiku_kits_gfx_font_t *f = font_for_glyph(font, (uint8_t)c);
    if (f == NULL) return font->width;
    if (f->widths != NULL) return f->widths[(uint8_t)c - f->first];
    return f->width;
}

/* Render width of one glyph in display pixels at @p scale. */
static uint16_t
glyph_advance_px(const tiku_kits_gfx_font_t *font, char c, uint8_t scale)
{
    return (uint16_t)((glyph_width(font, c) + 1u) * scale);
}

/*---------------------------------------------------------------------------*/
/* UTF-8 DECODER                                                             */
/*---------------------------------------------------------------------------*/

uint32_t
tiku_kits_gfx_utf8_next(const char **p)
{
    const uint8_t *u;
    uint8_t  b0;
    uint32_t cp;
    int      need;

    if (p == NULL || *p == NULL) return 0;
    u = (const uint8_t *)*p;
    b0 = *u;
    if (b0 == 0u) return 0;

    if ((b0 & 0x80u) == 0u) {
        /* ASCII fast path. */
        *p = (const char *)(u + 1);
        return b0;
    }

    if ((b0 & 0xE0u) == 0xC0u)      { cp = b0 & 0x1Fu; need = 1; }
    else if ((b0 & 0xF0u) == 0xE0u) { cp = b0 & 0x0Fu; need = 2; }
    else if ((b0 & 0xF8u) == 0xF0u) { cp = b0 & 0x07u; need = 3; }
    else {
        /* Stray continuation byte or 5/6-byte (invalid) lead. */
        *p = (const char *)(u + 1);
        return 0xFFFDu;
    }

    {
        int i;
        for (i = 0; i < need; i++) {
            uint8_t b = u[1 + i];
            if ((b & 0xC0u) != 0x80u) {
                *p = (const char *)(u + 1);
                return 0xFFFDu;
            }
            cp = (cp << 6) | (b & 0x3Fu);
        }
    }
    *p = (const char *)(u + 1 + need);
    return cp;
}

/*---------------------------------------------------------------------------*/
/* PUBLIC: SINGLE-CHAR / SINGLE-LINE                                         */
/*---------------------------------------------------------------------------*/

uint16_t
tiku_kits_gfx_draw_char(const tiku_kits_gfx_surface_t *s,
                         int16_t x, int16_t y, char c,
                         const tiku_kits_gfx_font_t *font,
                         uint8_t color, uint8_t scale)
{
    const tiku_kits_gfx_font_t *f;
    uint16_t glyph_size, glyph_idx;
    const uint8_t *glyph;
    uint8_t col, byte, row, dx, dy, gw;

    if (s == NULL || s->set_pixel == NULL || font == NULL || scale == 0) {
        return 0;
    }

    /* Resolve the actual font to use via the fallback chain. */
    f = font_for_glyph(font, (uint8_t)c);
    if (f == NULL || f->glyphs == NULL) return 0;

    /* Glyph storage uses f->width per glyph regardless of
     * proportional rendering -- only the *advance* shrinks. */
    glyph_size = (uint16_t)(f->width * f->bytes_per_column);
    glyph_idx  = (uint16_t)(((unsigned char)c - f->first) * glyph_size);
    glyph      = &f->glyphs[glyph_idx];
    gw         = (f->widths != NULL) ? f->widths[(uint8_t)c - f->first]
                                      : f->width;

    /* Render only the columns that the proportional width says
     * are part of this glyph (gw <= f->width). */
    for (col = 0; col < gw; col++) {
        for (byte = 0; byte < f->bytes_per_column; byte++) {
            uint8_t b = glyph[col * f->bytes_per_column + byte];
            uint8_t bits_this_byte = (uint8_t)
                ((f->height - byte * 8u) >= 8u ? 8u
                                               : (f->height - byte * 8u));
            for (row = 0; row < bits_this_byte; row++) {
                if (b & (uint8_t)(1u << row)) {
                    for (dy = 0; dy < scale; dy++) {
                        for (dx = 0; dx < scale; dx++) {
                            tiku_kits_gfx_pixel(s,
                                (int16_t)(x + col * scale + dx),
                                (int16_t)(y + (byte * 8u + row) * scale + dy),
                                color);
                        }
                    }
                }
            }
        }
    }
    return (uint16_t)((gw + 1u) * scale);  /* glyph + 1-px gap, scaled */
}

void
tiku_kits_gfx_draw_string(const tiku_kits_gfx_surface_t *s,
                           int16_t x, int16_t y, const char *str,
                           const tiku_kits_gfx_font_t *font,
                           uint8_t color, uint8_t scale)
{
    if (s == NULL || str == NULL || font == NULL || scale == 0) return;

    while (*str) {
        uint16_t adv = tiku_kits_gfx_draw_char(s, x, y, *str, font,
                                                color, scale);
        x = (int16_t)(x + adv);
        str++;
    }
}

uint16_t
tiku_kits_gfx_text_width(const char *str,
                          const tiku_kits_gfx_font_t *font,
                          uint8_t scale)
{
    uint32_t total = 0;
    if (str == NULL || font == NULL || scale == 0) return 0;
    while (*str) {
        total += glyph_advance_px(font, *str, scale);
        str++;
    }
    /* Strip the trailing inter-glyph gap that the last glyph added. */
    if (total >= scale) total -= scale;
    return (uint16_t)(total > 0xFFFFu ? 0xFFFFu : total);
}

uint16_t
tiku_kits_gfx_line_height(const tiku_kits_gfx_font_t *font, uint8_t scale)
{
    if (font == NULL || scale == 0) return 0;
    if (font->line_height != 0) {
        return (uint16_t)(font->line_height * scale);
    }
    return (uint16_t)((font->height + 1u) * scale);
}

/*---------------------------------------------------------------------------*/
/* SINGLE-LINE WITH ALIGNMENT                                                */
/*---------------------------------------------------------------------------*/

/* Compute starting x within @p rect for a string of @p str_w pixels
 * given the alignment. */
static int16_t
align_x(const tiku_kits_gfx_rect_t *rect,
         uint16_t str_w, tiku_kits_gfx_align_t align)
{
    switch (align) {
    case TIKU_KITS_GFX_ALIGN_RIGHT:
        return (int16_t)(rect->x + (int16_t)rect->w - (int16_t)str_w);
    case TIKU_KITS_GFX_ALIGN_CENTER:
        if (str_w >= rect->w) return rect->x;
        return (int16_t)(rect->x + (int16_t)((rect->w - str_w) / 2u));
    case TIKU_KITS_GFX_ALIGN_LEFT:
    default:
        return rect->x;
    }
}

uint16_t
tiku_kits_gfx_draw_string_in_rect(
    const tiku_kits_gfx_surface_t *s,
    const tiku_kits_gfx_rect_t *rect,
    const char *str,
    const tiku_kits_gfx_font_t *font,
    uint8_t color, uint8_t scale,
    tiku_kits_gfx_align_t align)
{
    uint16_t str_w;
    uint16_t count = 0;
    const char *p;

    if (s == NULL || rect == NULL || str == NULL || font == NULL) return 0;

    str_w = tiku_kits_gfx_text_width(str, font, scale);
    {
        int16_t x = align_x(rect, str_w, align);
        int16_t y = rect->y;
        for (p = str; *p; p++) {
            uint16_t adv = tiku_kits_gfx_draw_char(s, x, y, *p, font,
                                                    color, scale);
            x = (int16_t)(x + adv);
            count++;
        }
    }
    return count;
}

/*---------------------------------------------------------------------------*/
/* WORD-WRAP                                                                 */
/*---------------------------------------------------------------------------*/

/* Find a break point: longest prefix of @p str that renders within
 * @p max_w pixels. Returns the number of characters in the prefix.
 * On exit, @p out_w is the rendered pixel width of the prefix.
 *
 * Tries to break at whitespace; falls back to character-wise
 * break if no whitespace lies within the limit. */
static uint16_t
find_break(const char *str, const tiku_kits_gfx_font_t *font,
            uint8_t scale, uint16_t max_w, uint16_t *out_w)
{
    uint16_t i = 0;
    uint16_t cur_w = 0;
    uint16_t last_ws = 0;     /* index just past last whitespace seen */
    uint16_t last_ws_w = 0;

    while (str[i] != '\0' && str[i] != '\n') {
        uint16_t adv = glyph_advance_px(font, str[i], scale);
        /* Check fit BEFORE adding, so the bound is never exceeded. */
        uint16_t new_w = (uint16_t)(cur_w + adv);
        /* The trailing inter-glyph gap doesn't really need to fit,
         * but the math stays simple by requiring it. */
        if (new_w > max_w) {
            if (last_ws > 0) {
                *out_w = (uint16_t)(last_ws_w >= scale
                                     ? last_ws_w - scale : 0);
                return last_ws;
            }
            /* No whitespace within max_w -> break at the last char
             * that does fit (character-wise break). */
            *out_w = (uint16_t)(cur_w >= scale ? cur_w - scale : 0);
            return i;
        }
        cur_w = new_w;
        if (str[i] == ' ') {
            last_ws    = (uint16_t)(i + 1);
            last_ws_w  = cur_w;
        }
        i++;
    }
    /* Whole string (up to end / newline) fits. */
    *out_w = (uint16_t)(cur_w >= scale ? cur_w - scale : 0);
    return i;
}

/* Render @p len characters of @p str at (x, y), then return. */
static void
draw_n(const tiku_kits_gfx_surface_t *s,
        int16_t x, int16_t y, const char *str, uint16_t len,
        const tiku_kits_gfx_font_t *font,
        uint8_t color, uint8_t scale)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        uint16_t adv = tiku_kits_gfx_draw_char(s, x, y, str[i], font,
                                                color, scale);
        x = (int16_t)(x + adv);
    }
}

uint16_t
tiku_kits_gfx_draw_text_wrapped(
    const tiku_kits_gfx_surface_t *s,
    const tiku_kits_gfx_rect_t *rect,
    const char *str,
    const tiku_kits_gfx_font_t *font,
    uint8_t color, uint8_t scale,
    tiku_kits_gfx_align_t align)
{
    uint16_t lh, lines = 0;
    int16_t y;

    if (s == NULL || rect == NULL || str == NULL || font == NULL) return 0;
    if (rect->w == 0u || rect->h == 0u) return 0;

    lh = tiku_kits_gfx_line_height(font, scale);
    y  = rect->y;

    while (*str) {
        uint16_t line_w;
        uint16_t take;

        /* Skip leading spaces on a fresh line (avoids stranded
         * leading whitespace after a wrap). */
        while (*str == ' ') str++;
        if (*str == '\0') break;

        take = find_break(str, font, scale, rect->w, &line_w);
        if (take == 0) break;  /* defensive: shouldn't happen */

        /* Stop when the next line would fall outside the rect. */
        if (y + (int16_t)font->height * scale > rect->y + (int16_t)rect->h) {
            break;
        }

        {
            int16_t x = align_x(rect, line_w, align);
            draw_n(s, x, y, str, take, font, color, scale);
        }
        lines++;
        y    = (int16_t)(y + lh);
        str += take;
        if (*str == '\n') str++;  /* consume forced break */
    }
    return lines;
}

/*---------------------------------------------------------------------------*/
/* PHASE 0: VALIGN, TRUNCATION, MEASUREMENT                                  */
/*---------------------------------------------------------------------------*/

uint16_t
tiku_kits_gfx_draw_string_in_box(
    const tiku_kits_gfx_surface_t *s,
    const tiku_kits_gfx_rect_t *rect,
    const char *str,
    const tiku_kits_gfx_font_t *font,
    uint8_t color, uint8_t scale,
    tiku_kits_gfx_align_t halign,
    tiku_kits_gfx_valign_t valign)
{
    tiku_kits_gfx_rect_t shifted;
    uint16_t glyph_h;

    if (s == NULL || rect == NULL || str == NULL || font == NULL) return 0;
    if (scale == 0) scale = 1;

    glyph_h = (uint16_t)(font->height * scale);
    shifted = *rect;

    switch (valign) {
    case TIKU_KITS_GFX_VALIGN_MIDDLE:
        if (rect->h > glyph_h) {
            shifted.y = (int16_t)(rect->y +
                (int16_t)((rect->h - glyph_h) / 2u));
        }
        break;
    case TIKU_KITS_GFX_VALIGN_BOTTOM:
        if (rect->h > glyph_h) {
            shifted.y = (int16_t)(rect->y +
                (int16_t)(rect->h - glyph_h));
        }
        break;
    case TIKU_KITS_GFX_VALIGN_TOP:
    default:
        /* No shift. */
        break;
    }
    shifted.h = glyph_h;

    return tiku_kits_gfx_draw_string_in_rect(s, &shifted, str,
        font, color, scale, halign);
}

uint16_t
tiku_kits_gfx_draw_string_truncated(
    const tiku_kits_gfx_surface_t *s,
    const tiku_kits_gfx_rect_t *rect,
    const char *str,
    const char *ellipsis,
    const tiku_kits_gfx_font_t *font,
    uint8_t color, uint8_t scale,
    tiku_kits_gfx_align_t halign)
{
    const char *eli;
    uint16_t    full_w, eli_w;
    uint16_t    rendered = 0;

    if (s == NULL || rect == NULL || str == NULL || font == NULL) return 0;
    if (scale == 0) scale = 1;

    eli = (ellipsis != NULL) ? ellipsis : "...";
    full_w = tiku_kits_gfx_text_width(str, font, scale);

    if (full_w <= rect->w) {
        return tiku_kits_gfx_draw_string_in_rect(s, rect, str,
            font, color, scale, halign);
    }

    /* Need to truncate. Compute how many leading characters fit
     * once the ellipsis width is reserved. */
    eli_w = tiku_kits_gfx_text_width(eli, font, scale);
    if (eli_w >= rect->w) {
        /* Not enough room even for the ellipsis -- just draw what
         * raw text fits, no marker. */
        eli_w = 0;
        eli   = "";
    }

    {
        uint16_t budget = (uint16_t)(rect->w - eli_w);
        uint16_t cur_w  = 0;
        uint16_t i      = 0;
        const char *p   = str;

        while (p[i] != '\0') {
            uint8_t  uc = (uint8_t)p[i];
            uint8_t  gw = (uc < font->first || uc > font->last)
                            ? font->width
                            : (font->widths != NULL
                                  ? font->widths[uc - font->first]
                                  : font->width);
            uint16_t adv = (uint16_t)((gw + 1u) * scale);
            if ((uint32_t)cur_w + adv > budget) break;
            cur_w = (uint16_t)(cur_w + adv);
            i++;
        }
        rendered = i;

        /* Draw the kept prefix. */
        {
            int16_t x = rect->x;
            int16_t y = rect->y;
            uint16_t w_used = (uint16_t)(cur_w + eli_w);
            if (w_used >= scale) w_used = (uint16_t)(w_used - scale);
            switch (halign) {
            case TIKU_KITS_GFX_ALIGN_RIGHT:
                x = (int16_t)(rect->x + (int16_t)rect->w - (int16_t)w_used);
                break;
            case TIKU_KITS_GFX_ALIGN_CENTER:
                if (w_used < rect->w) {
                    x = (int16_t)(rect->x +
                        (int16_t)((rect->w - w_used) / 2u));
                }
                break;
            default:
                break;
            }
            for (i = 0; i < rendered; i++) {
                uint16_t adv = tiku_kits_gfx_draw_char(s, x, y,
                                p[i], font, color, scale);
                x = (int16_t)(x + adv);
            }
            if (eli_w > 0) {
                uint16_t k;
                for (k = 0; eli[k] != '\0'; k++) {
                    uint16_t adv = tiku_kits_gfx_draw_char(s, x, y,
                                    eli[k], font, color, scale);
                    x = (int16_t)(x + adv);
                }
            }
        }
    }

    return rendered;
}

void
tiku_kits_gfx_measure_wrapped(
    const char *str,
    const tiku_kits_gfx_font_t *font,
    uint8_t scale,
    uint16_t max_width,
    uint16_t *out_lines,
    uint16_t *out_height_px)
{
    uint16_t lh;
    uint16_t lines = 0;

    if (out_lines)     *out_lines     = 0;
    if (out_height_px) *out_height_px = 0;

    if (str == NULL || font == NULL || max_width == 0u) return;
    if (scale == 0) scale = 1;

    lh = tiku_kits_gfx_line_height(font, scale);

    while (*str) {
        uint16_t line_w;
        uint16_t take;
        while (*str == ' ') str++;
        if (*str == '\0') break;
        take = find_break(str, font, scale, max_width, &line_w);
        if (take == 0) break;
        lines++;
        str += take;
        if (*str == '\n') str++;
    }

    if (out_lines)     *out_lines     = lines;
    if (out_height_px) *out_height_px = (uint16_t)(lines * lh);
}
