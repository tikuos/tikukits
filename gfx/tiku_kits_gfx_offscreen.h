/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_offscreen.h - RAM-backed framebuffer surface
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Wraps a caller-allocated framebuffer in a tiku_kits_gfx_surface_t,
 * so anything that draws on a surface can write into RAM instead
 * of an attached display. Use cases:
 *
 * 1. "Diff and push" partial refresh: rasterize a frame
 * offscreen, compare against the previous frame, and only
 * push changed regions to the panel via
 * tiku_kits_epaper_refresh_rect().
 * 2. Off-display compositing: build complex scenes with multiple
 * sources, then blit the final to the panel.
 * 3. Host simulation: a PNG-writing surface adapter for CI.
 *
 * Supported formats (matches tiku_kits_gfx_image_t):
 *
 * IMG_1BPP_ROW_MSB  1 bit/pixel, row-major MSB-first.
 * Default for monochrome panels.
 * IMG_2BPP_BWR      2 bits/pixel, native to BWR e-paper:
 * 0 = white, 1 = black, 2 = red, 3 = reserved.
 * IMG_4BPP_GRAY     4 bits/pixel, grayscale.
 *
 * Memory cost: caller-allocated buffer of
 * tiku_kits_gfx_offscreen_buffer_size(w, h, format) bytes plus
 * ~16 B of struct overhead.
 */

#ifndef TIKU_KITS_GFX_OFFSCREEN_H_
#define TIKU_KITS_GFX_OFFSCREEN_H_

#include "tiku_kits_gfx.h"
#include "tiku_kits_gfx_image.h"

/*---------------------------------------------------------------------------*/
/* OFFSCREEN BACKING                                                         */
/*---------------------------------------------------------------------------*/

typedef struct {
    uint8_t                    *data;
    uint16_t                    width;
    uint16_t                    height;
    tiku_kits_gfx_img_format_t  format;
} tiku_kits_gfx_offscreen_t;

/**
 * @brief Compute the byte size of a framebuffer for the given
 *        dimensions and format. Useful for sizing the caller's
 *        backing array at compile time.
 */
uint32_t tiku_kits_gfx_offscreen_buffer_size(
    uint16_t w, uint16_t h,
    tiku_kits_gfx_img_format_t format);

/**
 * @brief Initialise an offscreen with @p data as its backing
 *        buffer. The caller owns @p data and is responsible for
 *        ensuring it's at least @ref tiku_kits_gfx_offscreen_buffer_size
 *        bytes large. The buffer is NOT cleared by init -- call
 *        tiku_kits_gfx_offscreen_clear() if you want zero contents.
 */
void tiku_kits_gfx_offscreen_init(
    tiku_kits_gfx_offscreen_t *fb,
    uint8_t *data, uint16_t w, uint16_t h,
    tiku_kits_gfx_img_format_t format);

/**
 * @brief Wire @p surface to the offscreen so drawing primitives
 *        write into the framebuffer. After this returns the
 *        surface is valid for any tiku_kits_gfx_* call.
 */
void tiku_kits_gfx_offscreen_attach(
    tiku_kits_gfx_offscreen_t *fb,
    tiku_kits_gfx_surface_t *surface);

/** Fill the entire offscreen buffer with @p color. */
void tiku_kits_gfx_offscreen_clear(
    tiku_kits_gfx_offscreen_t *fb,
    uint8_t color);

/** Read back the raw pixel value at (x, y). Out-of-range returns 0. */
uint8_t tiku_kits_gfx_offscreen_get_pixel(
    const tiku_kits_gfx_offscreen_t *fb, uint16_t x, uint16_t y);

/*
 * Both buffers must be the same dimensions and format. Returns 1
 * with @p out_rect set to the smallest rectangle covering all
 * differing pixels; returns 0 with @p out_rect zeroed when the
 * buffers are identical.
 * Use this with tiku_kits_epaper_refresh_rect() to push only the
 * pixels that actually changed since the last frame.
 */

/**
 * @brief Compute the bounding rectangle of pixel differences
 * between two offscreen buffers @p a and @p b.
 */
int tiku_kits_gfx_offscreen_diff(
    const tiku_kits_gfx_offscreen_t *a,
    const tiku_kits_gfx_offscreen_t *b,
    tiku_kits_gfx_rect_t *out_rect);

#endif /* TIKU_KITS_GFX_OFFSCREEN_H_ */
