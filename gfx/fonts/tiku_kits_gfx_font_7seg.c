/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_font_7seg.c - 12x16 7-segment digit font
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Cell layout (12 cols x 16 rows):
 *
 * col:  0 1 2 3 4 5 6 7 8 9 10 11
 * row 0/1   .    a a a a a a a a    .
 * row 2-6   f    .                  b
 * .                  .
 * row 7/8   .    g g g g g g g g    .
 * row 9-13  e    .                  c
 * .                  .
 * row 14/15 .    d d d d d d d d    .
 *
 * Segments are 2 px thick. Side verticals (b/c/e/f) span 5 rows
 * each; horizontals (a/g/d) span 8 cols each.
 *
 * Storage: column-major, LSB top, 2 bytes per column.
 */

#include "tiku_kits_gfx_font_7seg.h"

/*---------------------------------------------------------------------------*/
/* SHORTHAND COLUMN BYTES                                                    */
/*---------------------------------------------------------------------------*/

/* Each glyph repeats a small set of column patterns; naming them
 * makes the glyph tables read like the segment lists. */

/* Empty column. */
#define EMPTY            0x00, 0x00

/* Side-vertical column variants. */
#define V_BOTH           0x7C, 0x3E   /* rows 2-6 (top) + 9-13 (bot)   */
#define V_TOP_ONLY       0x7C, 0x00   /* rows 2-6 only                 */
#define V_BOT_ONLY       0x00, 0x3E   /* rows 9-13 only                */

/* Centre-cell column variants (for cols 2..9). */
#define M_AGD            0x83, 0xC1   /* rows 0-1, 7-8, 14-15          */
#define M_AD             0x03, 0xC0   /* rows 0-1 (a) + 14-15 (d)      */
#define M_A_ONLY         0x03, 0x00   /* rows 0-1 (a)                  */
#define M_G_ONLY         0x80, 0x01   /* rows 7-8 (g)                  */

/* Lower-right dot for '.' */
#define DOT_BR           0x00, 0xC0   /* rows 14-15 only               */

/* Two stacked dots for ':' */
#define COLON_DOTS       0x30, 0x0C   /* rows 4-5 + 10-11              */

/*---------------------------------------------------------------------------*/
/* GLYPHS (14 of them, 24 bytes each)                                        */
/*---------------------------------------------------------------------------*/

static const uint8_t seven_seg_glyphs[] = {

    /* '-' minus (g segment only) */
    EMPTY,    EMPTY,
    M_G_ONLY, M_G_ONLY, M_G_ONLY, M_G_ONLY,
    M_G_ONLY, M_G_ONLY, M_G_ONLY, M_G_ONLY,
    EMPTY,    EMPTY,

    /* '.' decimal point (single 2x2 dot at lower-right of the cell) */
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
    EMPTY, EMPTY, EMPTY,
    DOT_BR, DOT_BR,
    EMPTY,

    /* '/' placeholder (empty) */
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,

    /* '0' digit (a, b, c, d, e, f) */
    V_BOTH, V_BOTH,
    M_AD,   M_AD,   M_AD,   M_AD,
    M_AD,   M_AD,   M_AD,   M_AD,
    V_BOTH, V_BOTH,

    /* '1' digit (b, c only -- right-aligned) */
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
    EMPTY, EMPTY, EMPTY, EMPTY,
    V_BOTH, V_BOTH,

    /* '2' digit (a, b, d, e, g) */
    V_BOT_ONLY, V_BOT_ONLY,
    M_AGD,      M_AGD,      M_AGD,      M_AGD,
    M_AGD,      M_AGD,      M_AGD,      M_AGD,
    V_TOP_ONLY, V_TOP_ONLY,

    /* '3' digit (a, b, c, d, g) */
    EMPTY, EMPTY,
    M_AGD, M_AGD, M_AGD, M_AGD,
    M_AGD, M_AGD, M_AGD, M_AGD,
    V_BOTH, V_BOTH,

    /* '4' digit (b, c, f, g) */
    V_TOP_ONLY, V_TOP_ONLY,
    M_G_ONLY,   M_G_ONLY,   M_G_ONLY,   M_G_ONLY,
    M_G_ONLY,   M_G_ONLY,   M_G_ONLY,   M_G_ONLY,
    V_BOTH,     V_BOTH,

    /* '5' digit (a, c, d, f, g) */
    V_TOP_ONLY, V_TOP_ONLY,
    M_AGD,      M_AGD,      M_AGD,      M_AGD,
    M_AGD,      M_AGD,      M_AGD,      M_AGD,
    V_BOT_ONLY, V_BOT_ONLY,

    /* '6' digit (a, c, d, e, f, g) */
    V_BOTH, V_BOTH,
    M_AGD,  M_AGD,  M_AGD,  M_AGD,
    M_AGD,  M_AGD,  M_AGD,  M_AGD,
    V_BOT_ONLY, V_BOT_ONLY,

    /* '7' digit (a, b, c) */
    EMPTY, EMPTY,
    M_A_ONLY, M_A_ONLY, M_A_ONLY, M_A_ONLY,
    M_A_ONLY, M_A_ONLY, M_A_ONLY, M_A_ONLY,
    V_BOTH, V_BOTH,

    /* '8' digit (all segments) */
    V_BOTH, V_BOTH,
    M_AGD,  M_AGD,  M_AGD,  M_AGD,
    M_AGD,  M_AGD,  M_AGD,  M_AGD,
    V_BOTH, V_BOTH,

    /* '9' digit (a, b, c, d, f, g) */
    V_TOP_ONLY, V_TOP_ONLY,
    M_AGD,      M_AGD,      M_AGD,      M_AGD,
    M_AGD,      M_AGD,      M_AGD,      M_AGD,
    V_BOTH,     V_BOTH,

    /* ':' colon (two square dots, vertically centred) */
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
    COLON_DOTS, COLON_DOTS,
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
};

const tiku_kits_gfx_font_t tiku_kits_gfx_font_7seg = {
    .width            = 12,
    .height           = 16,
    .first            = 0x2D,   /* '-' */
    .last             = 0x3A,   /* ':' */
    .bytes_per_column = 2,
    .glyphs           = seven_seg_glyphs,
    .widths           = NULL,
    .ascent           = 16,
    .descent          = 0,
    .line_height      = 18,
};
