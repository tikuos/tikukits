/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper.c - Common e-paper framebuffer helpers
 *
 * Pure software framebuffer operations shared across all e-paper
 * controller families. No SPI traffic happens here -- these
 * functions only mutate the in-memory bitmap. The driver-specific
 * refresh() function pushes the buffer to the panel.
 *
 * Bit layout:
 *   The framebuffer is stored row-major, MSB = leftmost pixel:
 *     byte_index = row * (width / 8) + (column / 8)
 *     bit_mask   = 0x80 >> (column & 7)
 *   This matches the native scan order of the Pervasive iTC and
 *   most other monochrome EPD controllers, so refresh() can hand
 *   the buffer straight to the panel without an intermediate copy.
 *
 * Bit polarity:
 *   1 = black (pixel ON), 0 = white (pixel OFF). Matches the
 *   library convention used by Pervasive's PDLS and most other
 *   EPD vendors for monochrome panels.
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

size_t
tiku_kits_epaper_framebuffer_size(const tiku_kits_epaper_panel_t *panel)
{
    if (panel == NULL) {
        return 0;
    }
    return (size_t)(((uint32_t)panel->width * panel->height + 7u) / 8u);
}

void
tiku_kits_epaper_clear(tiku_kits_epaper_t *epd, uint8_t colour)
{
    size_t bytes;

    if (epd == NULL || epd->framebuffer == NULL || epd->panel == NULL) {
        return;
    }
    bytes = tiku_kits_epaper_framebuffer_size(epd->panel);
    memset(epd->framebuffer, colour ? 0xFFu : 0x00u, bytes);
}

void
tiku_kits_epaper_set_pixel(tiku_kits_epaper_t *epd,
                            uint16_t x, uint16_t y, uint8_t colour)
{
    uint16_t bytes_per_row;
    uint32_t byte_idx;
    uint8_t  bit_mask;

    if (epd == NULL || epd->framebuffer == NULL || epd->panel == NULL) {
        return;
    }
    if (x >= epd->panel->width || y >= epd->panel->height) {
        return;
    }

    bytes_per_row = (uint16_t)((epd->panel->width + 7u) / 8u);
    byte_idx = (uint32_t)y * bytes_per_row + (x >> 3);
    bit_mask = (uint8_t)(0x80u >> (x & 7u));

    if (colour) {
        epd->framebuffer[byte_idx] |= bit_mask;
    } else {
        epd->framebuffer[byte_idx] &= (uint8_t)~bit_mask;
    }
}
