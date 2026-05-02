/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper.c - Generic e-paper API: dispatch + framebuffer helpers
 *
 * Two responsibilities:
 *
 *   1. Generic API dispatch -- tiku_kits_epaper_init / refresh / sleep
 *      forward to the family driver via the ops vtable embedded in
 *      the panel descriptor. No driver code lives here.
 *
 *   2. Driver-agnostic framebuffer helpers -- set_pixel / clear /
 *      framebuffer_size operate purely on the in-memory buffers
 *      and never touch SPI. Layout matches the column-major
 *      MSB-first scan order used by every monochrome SPI EPD
 *      controller this kit supports.
 *
 * Bit layout:
 *     byte_index = row * (width / 8) + (column / 8)
 *     bit_mask   = 0x80 >> (column & 7)
 *
 * Bit polarity (black plane and red plane independently):
 *     1 = pixel ON (black on the black plane, red on the red plane)
 *     0 = pixel OFF (white on the black plane, not-red on the red plane)
 *
 * Plane encoding (combined):
 *     WHITE  : black=0, red=0
 *     BLACK  : black=1, red=0
 *     RED    : black=0, red=1
 *     YELLOW : black=1, red=1   (BWRY only)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_epaper.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* GENERIC DISPATCH                                                          */
/*---------------------------------------------------------------------------*/

/* Validate the context has the minimum required fields and that
 * the panel's ops vtable supplies the operation we are about to
 * dispatch.  Returns the negative error code if invalid, or
 * TIKU_KITS_EPAPER_OK if the call may proceed. */
static int
validate_context(const tiku_kits_epaper_t *epd)
{
    if (epd == NULL || epd->panel == NULL ||
        epd->framebuffer == NULL || epd->panel->ops == NULL) {
        return TIKU_KITS_EPAPER_ERR_PARAM;
    }
    if (epd->panel->colour_planes >= 2u && epd->framebuffer_red == NULL) {
        return TIKU_KITS_EPAPER_ERR_PARAM;
    }
    return TIKU_KITS_EPAPER_OK;
}

int
tiku_kits_epaper_init(tiku_kits_epaper_t *epd)
{
    int rc = validate_context(epd);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;
    if (epd->panel->ops->init == NULL) {
        return TIKU_KITS_EPAPER_ERR_UNSUPPORTED;
    }
    return epd->panel->ops->init(epd);
}

int
tiku_kits_epaper_refresh(tiku_kits_epaper_t *epd)
{
    int rc = validate_context(epd);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;
    if (epd->panel->ops->refresh == NULL) {
        return TIKU_KITS_EPAPER_ERR_UNSUPPORTED;
    }
    return epd->panel->ops->refresh(epd);
}

int
tiku_kits_epaper_sleep(tiku_kits_epaper_t *epd)
{
    int rc = validate_context(epd);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;
    if (epd->panel->ops->sleep == NULL) {
        return TIKU_KITS_EPAPER_ERR_UNSUPPORTED;
    }
    return epd->panel->ops->sleep(epd);
}

/*---------------------------------------------------------------------------*/
/* FRAMEBUFFER HELPERS                                                       */
/*---------------------------------------------------------------------------*/

size_t
tiku_kits_epaper_framebuffer_size(const tiku_kits_epaper_panel_t *panel)
{
    if (panel == NULL) {
        return 0;
    }
    return (size_t)(((uint32_t)panel->width * panel->height + 7u) / 8u);
}

/* Decompose a logical colour into per-plane bit values. On a
 * monochrome panel, RED and YELLOW collapse to BLACK so set_pixel
 * remains forgiving across panel types. */
static void
decode_colour(uint8_t colour, uint8_t panel_planes,
               uint8_t *black_bit, uint8_t *red_bit)
{
    switch (colour) {
    case TIKU_KITS_EPAPER_RED:    *black_bit = 0; *red_bit = 1; break;
    case TIKU_KITS_EPAPER_YELLOW: *black_bit = 1; *red_bit = 1; break;
    case TIKU_KITS_EPAPER_BLACK:  *black_bit = 1; *red_bit = 0; break;
    case TIKU_KITS_EPAPER_WHITE:
    default:                       *black_bit = 0; *red_bit = 0; break;
    }
    if (panel_planes < 2u &&
        (colour == TIKU_KITS_EPAPER_RED ||
         colour == TIKU_KITS_EPAPER_YELLOW)) {
        *black_bit = 1;
        *red_bit   = 0;
    }
}

void
tiku_kits_epaper_clear(tiku_kits_epaper_t *epd, uint8_t colour)
{
    size_t bytes;
    uint8_t black_bit, red_bit;

    if (epd == NULL || epd->framebuffer == NULL || epd->panel == NULL) {
        return;
    }
    bytes = tiku_kits_epaper_framebuffer_size(epd->panel);
    decode_colour(colour, epd->panel->colour_planes, &black_bit, &red_bit);

    memset(epd->framebuffer, black_bit ? 0xFFu : 0x00u, bytes);
    if (epd->framebuffer_red != NULL) {
        memset(epd->framebuffer_red, red_bit ? 0xFFu : 0x00u, bytes);
    }
}

void
tiku_kits_epaper_set_pixel(tiku_kits_epaper_t *epd,
                            uint16_t x, uint16_t y, uint8_t colour)
{
    uint16_t bytes_per_row;
    uint32_t byte_idx;
    uint8_t  bit_mask;
    uint8_t  black_bit, red_bit;

    if (epd == NULL || epd->framebuffer == NULL || epd->panel == NULL) {
        return;
    }
    if (x >= epd->panel->width || y >= epd->panel->height) {
        return;
    }

    decode_colour(colour, epd->panel->colour_planes, &black_bit, &red_bit);

    bytes_per_row = (uint16_t)((epd->panel->width + 7u) / 8u);
    byte_idx = (uint32_t)y * bytes_per_row + (x >> 3);
    bit_mask = (uint8_t)(0x80u >> (x & 7u));

    if (black_bit) epd->framebuffer[byte_idx] |= bit_mask;
    else           epd->framebuffer[byte_idx] &= (uint8_t)~bit_mask;

    if (epd->framebuffer_red != NULL) {
        if (red_bit) epd->framebuffer_red[byte_idx] |= bit_mask;
        else         epd->framebuffer_red[byte_idx] &= (uint8_t)~bit_mask;
    }
}
