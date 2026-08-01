/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_offscreen.c - RAM-backed framebuffer surface impl
 *
 * The surface adapter uses a small `set_pixel` thunk that decodes
 * (x, y, color) into a byte/bit position based on the configured
 * format and writes into the caller's framebuffer. Read-back is
 * symmetric for the diff and get_pixel helpers.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_gfx_offscreen.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* SIZE / WRITE / READ HELPERS                                               */
/*---------------------------------------------------------------------------*/

uint32_t
tiku_kits_gfx_offscreen_buffer_size(uint16_t w, uint16_t h,
                                     tiku_kits_gfx_img_format_t format)
{
    uint32_t bits_per_row;
    uint32_t bytes_per_row;

    if (w == 0u || h == 0u) return 0;

    switch (format) {
    case TIKU_KITS_GFX_IMG_1BPP_ROW_MSB:
    case TIKU_KITS_GFX_IMG_1BPP_ROW_LSB:
        bits_per_row = (uint32_t)w;
        break;
    case TIKU_KITS_GFX_IMG_2BPP_BWR:
        bits_per_row = (uint32_t)w * 2u;
        break;
    case TIKU_KITS_GFX_IMG_4BPP_GRAY:
        bits_per_row = (uint32_t)w * 4u;
        break;
    case TIKU_KITS_GFX_IMG_1BPP_RLE:
    default:
        /* RLE has no fixed framebuffer layout; not supported as an
         * offscreen target. */
        return 0;
    }
    bytes_per_row = (bits_per_row + 7u) / 8u;
    return bytes_per_row * (uint32_t)h;
}

static void
write_pixel(tiku_kits_gfx_offscreen_t *fb,
             uint16_t x, uint16_t y, uint8_t color)
{
    if (fb == NULL || fb->data == NULL) return;
    if (x >= fb->width || y >= fb->height) return;

    switch (fb->format) {
    case TIKU_KITS_GFX_IMG_1BPP_ROW_MSB: {
        uint16_t bytes_per_row = (uint16_t)((fb->width + 7u) / 8u);
        uint32_t idx  = (uint32_t)y * bytes_per_row + (x >> 3);
        uint8_t  mask = (uint8_t)(0x80u >> (x & 7u));
        if (color != 0u) fb->data[idx] |= mask;
        else             fb->data[idx] &= (uint8_t)~mask;
        break;
    }
    case TIKU_KITS_GFX_IMG_1BPP_ROW_LSB: {
        uint16_t bytes_per_row = (uint16_t)((fb->width + 7u) / 8u);
        uint32_t idx  = (uint32_t)y * bytes_per_row + (x >> 3);
        uint8_t  mask = (uint8_t)(1u << (x & 7u));
        if (color != 0u) fb->data[idx] |= mask;
        else             fb->data[idx] &= (uint8_t)~mask;
        break;
    }
    case TIKU_KITS_GFX_IMG_2BPP_BWR: {
        uint32_t bits_per_row  = (uint32_t)fb->width * 2u;
        uint32_t bytes_per_row = (bits_per_row + 7u) / 8u;
        uint32_t idx   = (uint32_t)y * bytes_per_row + (x >> 2);
        uint8_t  shift = (uint8_t)(6u - (uint8_t)((x & 3u) * 2u));
        uint8_t  mask  = (uint8_t)(0x3u << shift);
        uint8_t  val   = (uint8_t)((color & 0x3u) << shift);
        fb->data[idx] = (uint8_t)((fb->data[idx] & ~mask) | val);
        break;
    }
    case TIKU_KITS_GFX_IMG_4BPP_GRAY: {
        uint32_t bytes_per_row = ((uint32_t)fb->width + 1u) / 2u;
        uint32_t idx   = (uint32_t)y * bytes_per_row + (x >> 1);
        uint8_t  shift = (uint8_t)((x & 1u) ? 0u : 4u);
        uint8_t  mask  = (uint8_t)(0xFu << shift);
        uint8_t  val   = (uint8_t)((color & 0xFu) << shift);
        fb->data[idx] = (uint8_t)((fb->data[idx] & ~mask) | val);
        break;
    }
    default:
        break;
    }
}

uint8_t
tiku_kits_gfx_offscreen_get_pixel(const tiku_kits_gfx_offscreen_t *fb,
                                    uint16_t x, uint16_t y)
{
    if (fb == NULL || fb->data == NULL) return 0;
    if (x >= fb->width || y >= fb->height) return 0;

    switch (fb->format) {
    case TIKU_KITS_GFX_IMG_1BPP_ROW_MSB: {
        uint16_t bytes_per_row = (uint16_t)((fb->width + 7u) / 8u);
        uint8_t  b = fb->data[(uint32_t)y * bytes_per_row + (x >> 3)];
        return (uint8_t)((b >> (7u - (x & 7u))) & 1u);
    }
    case TIKU_KITS_GFX_IMG_1BPP_ROW_LSB: {
        uint16_t bytes_per_row = (uint16_t)((fb->width + 7u) / 8u);
        uint8_t  b = fb->data[(uint32_t)y * bytes_per_row + (x >> 3)];
        return (uint8_t)((b >> (x & 7u)) & 1u);
    }
    case TIKU_KITS_GFX_IMG_2BPP_BWR: {
        uint32_t bits_per_row  = (uint32_t)fb->width * 2u;
        uint32_t bytes_per_row = (bits_per_row + 7u) / 8u;
        uint8_t  b = fb->data[(uint32_t)y * bytes_per_row + (x >> 2)];
        uint8_t  shift = (uint8_t)(6u - (uint8_t)((x & 3u) * 2u));
        return (uint8_t)((b >> shift) & 0x3u);
    }
    case TIKU_KITS_GFX_IMG_4BPP_GRAY: {
        uint32_t bytes_per_row = ((uint32_t)fb->width + 1u) / 2u;
        uint8_t  b = fb->data[(uint32_t)y * bytes_per_row + (x >> 1)];
        uint8_t  shift = (uint8_t)((x & 1u) ? 0u : 4u);
        return (uint8_t)((b >> shift) & 0x0Fu);
    }
    default:
        return 0;
    }
}

/*---------------------------------------------------------------------------*/
/* SURFACE THUNK                                                             */
/*---------------------------------------------------------------------------*/

static void
offscreen_set_pixel(void *ctx, uint16_t x, uint16_t y, uint8_t color)
{
    write_pixel((tiku_kits_gfx_offscreen_t *)ctx, x, y, color);
}

void
tiku_kits_gfx_offscreen_init(tiku_kits_gfx_offscreen_t *fb,
                              uint8_t *data, uint16_t w, uint16_t h,
                              tiku_kits_gfx_img_format_t format)
{
    if (fb == NULL) return;
    fb->data   = data;
    fb->width  = w;
    fb->height = h;
    fb->format = format;
}

void
tiku_kits_gfx_offscreen_attach(tiku_kits_gfx_offscreen_t *fb,
                                tiku_kits_gfx_surface_t *surface)
{
    if (fb == NULL || surface == NULL) return;
    surface->width      = fb->width;
    surface->height     = fb->height;
    surface->set_pixel  = offscreen_set_pixel;
    surface->ctx        = fb;
    surface->origin_x   = 0;
    surface->origin_y   = 0;
    surface->clip_depth = 0;
}

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_offscreen_clear(tiku_kits_gfx_offscreen_t *fb,
                               uint8_t color)
{
    uint32_t size;
    if (fb == NULL || fb->data == NULL) return;
    size = tiku_kits_gfx_offscreen_buffer_size(fb->width, fb->height,
                                                fb->format);
    if (size == 0u) return;

    /* For 1bpp formats a byte can be splatted; the multi-bit
     * formats need the value replicated into a byte first. */
    {
        uint8_t  fill_byte;
        uint32_t i;
        switch (fb->format) {
        case TIKU_KITS_GFX_IMG_1BPP_ROW_MSB:
        case TIKU_KITS_GFX_IMG_1BPP_ROW_LSB:
            fill_byte = (color != 0u) ? 0xFFu : 0x00u;
            break;
        case TIKU_KITS_GFX_IMG_2BPP_BWR: {
            uint8_t v = (uint8_t)(color & 0x3u);
            fill_byte = (uint8_t)((v << 6) | (v << 4) | (v << 2) | v);
            break;
        }
        case TIKU_KITS_GFX_IMG_4BPP_GRAY: {
            uint8_t v = (uint8_t)(color & 0xFu);
            fill_byte = (uint8_t)((v << 4) | v);
            break;
        }
        default:
            fill_byte = 0;
            break;
        }
        for (i = 0; i < size; i++) fb->data[i] = fill_byte;
    }
}

/*---------------------------------------------------------------------------*/
/* DIFF (bounding-box of pixel changes)                                      */
/*---------------------------------------------------------------------------*/

int
tiku_kits_gfx_offscreen_diff(const tiku_kits_gfx_offscreen_t *a,
                              const tiku_kits_gfx_offscreen_t *b,
                              tiku_kits_gfx_rect_t *out_rect)
{
    uint16_t y, x;
    int      have_diff = 0;
    int16_t  x0 = 0, y0 = 0, x1 = 0, y1 = 0;

    if (out_rect != NULL) {
        out_rect->x = 0; out_rect->y = 0;
        out_rect->w = 0; out_rect->h = 0;
    }
    if (a == NULL || b == NULL || a->data == NULL || b->data == NULL) return 0;
    if (a->width != b->width || a->height != b->height) return 0;
    if (a->format != b->format) return 0;

    for (y = 0; y < a->height; y++) {
        for (x = 0; x < a->width; x++) {
            uint8_t pa = tiku_kits_gfx_offscreen_get_pixel(a, x, y);
            uint8_t pb = tiku_kits_gfx_offscreen_get_pixel(b, x, y);
            if (pa == pb) continue;
            if (!have_diff) {
                x0 = x1 = (int16_t)x;
                y0 = y1 = (int16_t)y;
                have_diff = 1;
            } else {
                if ((int16_t)x < x0) x0 = (int16_t)x;
                if ((int16_t)x > x1) x1 = (int16_t)x;
                if ((int16_t)y < y0) y0 = (int16_t)y;
                if ((int16_t)y > y1) y1 = (int16_t)y;
            }
        }
    }

    if (!have_diff) return 0;
    if (out_rect != NULL) {
        out_rect->x = x0;
        out_rect->y = y0;
        out_rect->w = (uint16_t)(x1 - x0 + 1);
        out_rect->h = (uint16_t)(y1 - y0 + 1);
    }
    return 1;
}
