/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_font_symbols.c - 16x16 starter symbol font
 *
 * Glyph format (column-major, 1 bpp, LSB = top row):
 *   - Each glyph is 16 columns x 16 rows.
 *   - bytes_per_column = 2: byte 0 holds rows 0..7 (bit 0 = top),
 *     byte 1 holds rows 8..15 (bit 0 = row 8).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_gfx_font_symbols.h"

/*---------------------------------------------------------------------------*/
/* GLYPH DATA                                                                */
/*---------------------------------------------------------------------------*/

/* Visual layout of each glyph is included as a comment so the bytes
 * can be verified by eye. For each row, '#' = pixel set, '.' = clear.
 * Each glyph occupies 16 columns x 2 bytes = 32 bytes. */
static const uint8_t symbol_glyphs[] = {

    /* '0' BATTERY (full)
     * . . . . . . . . . . . . . . . .   row 0
     * . . . . . . . . . . . . . . . .   row 1
     * . . . . . . . . . . . . . . . .   row 2
     * . . . . . . . . . . . . . . . .   row 3
     * . # # # # # # # # # # # # . . .   row 4
     * . # . . . . . . . . . . # . . .   row 5
     * . # . # # # # # # # # . # # # .   row 6
     * . # . # # # # # # # # . # # # .   row 7
     * . # . # # # # # # # # . # # # .   row 8
     * . # . # # # # # # # # . # # # .   row 9
     * . # . . . . . . . . . . # . . .   row 10
     * . # # # # # # # # # # # # . . .   row 11
     * . . . . . . . . . . . . . . . .   row 12
     * . . . . . . . . . . . . . . . .   row 13
     * . . . . . . . . . . . . . . . .   row 14
     * . . . . . . . . . . . . . . . .   row 15
     */
    0x00, 0x00,  0xF0, 0x0F,  0x10, 0x08,  0xD0, 0x0B,
    0xD0, 0x0B,  0xD0, 0x0B,  0xD0, 0x0B,  0xD0, 0x0B,
    0xD0, 0x0B,  0xD0, 0x0B,  0xD0, 0x0B,  0x10, 0x08,
    0xF0, 0x0F,  0xC0, 0x03,  0xC0, 0x03,  0x00, 0x00,

    /* '1' WIFI (4-bar signal strength)
     * . . . . . . . . . . . . . . . .   row 0
     * . . . . . . . . . . . . . . . .   row 1
     * . . . . . . . . . . . . . . . .   row 2
     * . . . . . . . . . . . . . # # .   row 3
     * . . . . . . . . . . # # . # # .   row 4
     * . . . . . . . . . . # # . # # .   row 5
     * . . . . . . . # # . # # . # # .   row 6
     * . . . . . . . # # . # # . # # .   row 7
     * . . . . # # . # # . # # . # # .   row 8
     * . . . . # # . # # . # # . # # .   row 9
     * . # # . # # . # # . # # . # # .   row 10
     * . # # . # # . # # . # # . # # .   row 11
     * . # # . # # . # # . # # . # # .   row 12
     * . . . . . . . . . . . . . . . .   row 13
     * . . . . . . . . . . . . . . . .   row 14
     * . . . . . . . . . . . . . . . .   row 15
     */
    0x00, 0x00,  0x00, 0x1C,  0x00, 0x1C,  0x00, 0x00,
    0x00, 0x1F,  0x00, 0x1F,  0x00, 0x00,  0xC0, 0x1F,
    0xC0, 0x1F,  0x00, 0x00,  0xF0, 0x1F,  0xF0, 0x1F,
    0x00, 0x00,  0xF8, 0x1F,  0xF8, 0x1F,  0x00, 0x00,

    /* '2' CHECK
     * . . . . . . . . . . . . . . . .   row 0
     * . . . . . . . . . . . . . . . .   row 1
     * . . . . . . . . . . . . . . . .   row 2
     * . . . . . . . . . . . . # # . .   row 3
     * . . . . . . . . . . . # # # . .   row 4
     * . . . . . . . . . . # # # . . .   row 5
     * . . . . . . . . . # # # . . . .   row 6
     * . . # # . . . . # # # . . . . .   row 7
     * . # # # . . . # # # . . . . . .   row 8
     * . # # # # . # # # . . . . . . .   row 9
     * . . # # # # # # . . . . . . . .   row 10
     * . . . # # # # . . . . . . . . .   row 11
     * . . . . # # . . . . . . . . . .   row 12
     * . . . . . . . . . . . . . . . .   row 13
     * . . . . . . . . . . . . . . . .   row 14
     * . . . . . . . . . . . . . . . .   row 15
     */
    0x00, 0x00,  0x00, 0x03,  0x80, 0x07,  0x80, 0x0F,
    0x00, 0x1E,  0x00, 0x1C,  0x00, 0x0E,  0x00, 0x07,
    0x80, 0x07,  0xC0, 0x01,  0xE0, 0x00,  0x70, 0x00,
    0x38, 0x00,  0x18, 0x00,  0x00, 0x00,  0x00, 0x00,

    /* '3' CROSS
     * . . . . . . . . . . . . . . . .   row 0
     * . . . . . . . . . . . . . . . .   row 1
     * . # # . . . . . . . . . . # # .   row 2
     * . . # # . . . . . . . . # # . .   row 3
     * . . . # # . . . . . . # # . . .   row 4
     * . . . . # # . . . . # # . . . .   row 5
     * . . . . . # # . . # # . . . . .   row 6
     * . . . . . . # # # # . . . . . .   row 7
     * . . . . . . . # # . . . . . . .   row 8
     * . . . . . . # # # # . . . . . .   row 9
     * . . . . . # # . . # # . . . . .   row 10
     * . . . . # # . . . . # # . . . .   row 11
     * . . . # # . . . . . . # # . . .   row 12
     * . . # # . . . . . . . . # # . .   row 13
     * . # # . . . . . . . . . . # # .   row 14
     * . . . . . . . . . . . . . . . .   row 15
     */
    0x00, 0x00,  0x04, 0x40,  0x0C, 0x60,  0x18, 0x30,
    0x30, 0x18,  0x60, 0x0C,  0xC0, 0x06,  0x80, 0x03,
    0x80, 0x03,  0xC0, 0x06,  0x60, 0x0C,  0x30, 0x18,
    0x18, 0x30,  0x0C, 0x60,  0x04, 0x40,  0x00, 0x00,
};

/*---------------------------------------------------------------------------*/
/* FONT DESCRIPTOR                                                           */
/*---------------------------------------------------------------------------*/

const tiku_kits_gfx_font_t tiku_kits_gfx_font_symbols = {
    .width            = 16,
    .height           = 16,
    .first            = 0x30,   /* '0' */
    .last             = 0x33,   /* '3' */
    .bytes_per_column = 2,
    .glyphs           = symbol_glyphs,
    .widths           = NULL,    /* monospaced */
    .ascent           = 13,
    .descent          = 3,
    .line_height      = 18,
};
