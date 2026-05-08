/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_image.h - Multi-format bitmap images
 *
 * Generalises the simple `tiku_kits_gfx_bitmap()` primitive to a
 * tagged image type that supports several common bit-packings:
 *
 *   1BPP_ROW_MSB  1 bit/pixel, row-major, MSB = leftmost
 *                 Same format as tiku_kits_gfx_bitmap; default for
 *                 hand-rolled assets and font_bake.py output.
 *
 *   1BPP_ROW_LSB  1 bit/pixel, row-major, LSB = leftmost
 *                 X-Bitmap (XBM) format -- output of common
 *                 conversion tools.
 *
 *   1BPP_RLE      1 bit/pixel run-length encoded.
 *                 Each byte: bit 7 = color, bits 0..6 = run length
 *                 minus 1 (so runs of 1..128 pixels). Pixels are
 *                 emitted row-major; runs may cross row boundaries.
 *                 Best for icons with large solid regions.
 *
 *   2BPP_BWR      2 bits/pixel, native to 3-color e-paper.
 *                 Values: 00 = WHITE, 01 = BLACK, 10 = RED,
 *                 11 = transparent (skip). Row-major, packed
 *                 4 pixels per byte (high two bits = leftmost).
 *
 *   4BPP_GRAY     4 bits/pixel, 0..15 grayscale. Row-major, packed
 *                 2 pixels per byte (high nibble = leftmost). Used
 *                 to ship antialiased icons; on 1-bit panels the
 *                 blit thresholds at 8.
 *
 * All blit ops self-clip against the surface bounds; out-of-range
 * pixels are silently dropped.
 *
 * Memory model: caller-allocates. Image structs and their data
 * arrays are expected to be `const`-qualified and live in FRAM /
 * flash; tag with TIKU_HIFRAM_RODATA (or equivalent) for large
 * assets in HIFRAM-aware projects.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_GFX_IMAGE_H_
#define TIKU_KITS_GFX_IMAGE_H_

#include "tiku_kits_gfx.h"

/*---------------------------------------------------------------------------*/
/* IMAGE FORMAT                                                              */
/*---------------------------------------------------------------------------*/

typedef enum {
    TIKU_KITS_GFX_IMG_1BPP_ROW_MSB = 0,
    TIKU_KITS_GFX_IMG_1BPP_ROW_LSB = 1,
    TIKU_KITS_GFX_IMG_1BPP_RLE     = 2,
    TIKU_KITS_GFX_IMG_2BPP_BWR     = 3,
    TIKU_KITS_GFX_IMG_4BPP_GRAY    = 4,
} tiku_kits_gfx_img_format_t;

/*---------------------------------------------------------------------------*/
/* IMAGE DESCRIPTOR                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Static description of a bitmap image.
 *
 * Field meanings:
 *   - width, height  Pixel dimensions.
 *   - format         One of TIKU_KITS_GFX_IMG_*.
 *   - data           Pointer to the pixel byte stream.
 *   - data_len       Length of the byte stream in bytes. Used for
 *                    bounds-checking RLE traversal; for fixed-size
 *                    formats may be set to ceil(W * H * bpp / 8).
 */
typedef struct {
    uint16_t                    width;
    uint16_t                    height;
    tiku_kits_gfx_img_format_t  format;
    const uint8_t              *data;
    uint32_t                    data_len;
} tiku_kits_gfx_image_t;

/*---------------------------------------------------------------------------*/
/* ROTATION ENUM                                                             */
/*---------------------------------------------------------------------------*/

typedef enum {
    TIKU_KITS_GFX_ROT_0   = 0,
    TIKU_KITS_GFX_ROT_90  = 1,
    TIKU_KITS_GFX_ROT_180 = 2,
    TIKU_KITS_GFX_ROT_270 = 3,
} tiku_kits_gfx_rotation_t;

/*---------------------------------------------------------------------------*/
/* RAW PIXEL ACCESS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the raw (format-encoded) pixel value at (x, y).
 *
 * Range:
 *   1BPP*  -> 0 or 1
 *   2BPP   -> 0..3
 *   4BPP   -> 0..15
 * Out-of-range coordinates return 0.
 *
 * For RLE images this is O(width * height) in the worst case (the
 * implementation walks from the start of the stream). Use blit
 * primitives for sequential access -- they iterate the stream once.
 */
uint8_t tiku_kits_gfx_image_pixel(const tiku_kits_gfx_image_t *img,
                                   uint16_t x, uint16_t y);

/*---------------------------------------------------------------------------*/
/* BLIT OPERATIONS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Blit @p img onto @p s with its top-left at (dst_x, dst_y).
 *
 * Color mapping per format:
 *   1BPP*  -- set pixels (raw 1) painted with @p fg_color;
 *             unset pixels (raw 0) are transparent.
 *   2BPP   -- 0 -> WHITE, 1 -> BLACK, 2 -> RED, 3 -> transparent
 *             (intrinsic colours; @p fg_color is ignored).
 *   4BPP   -- pixels with raw value >= 8 painted with @p fg_color;
 *             lighter pixels are transparent.
 */
void tiku_kits_gfx_image_blit(const tiku_kits_gfx_surface_t *s,
                               int16_t dst_x, int16_t dst_y,
                               const tiku_kits_gfx_image_t *img,
                               uint8_t fg_color);

/**
 * @brief Like blit() but with an explicit transparent value.
 *
 * Pixels whose raw format-encoded value equals @p mask_value are
 * skipped; all others are painted using the same colour mapping
 * as blit(). Use to override the default transparent value (e.g.
 * to make a 1bpp image's "0" pixels paint with bg_color instead
 * of being transparent, set mask_value = 255 -- never matches).
 */
void tiku_kits_gfx_image_blit_masked(const tiku_kits_gfx_surface_t *s,
                                      int16_t dst_x, int16_t dst_y,
                                      const tiku_kits_gfx_image_t *img,
                                      uint8_t fg_color,
                                      uint8_t mask_value);

/**
 * @brief Nearest-neighbour scaled blit into a (dst_w, dst_h)
 *        destination rectangle.
 */
void tiku_kits_gfx_image_blit_scaled(const tiku_kits_gfx_surface_t *s,
                                      int16_t dst_x, int16_t dst_y,
                                      uint16_t dst_w, uint16_t dst_h,
                                      const tiku_kits_gfx_image_t *img,
                                      uint8_t fg_color);

/**
 * @brief Blit rotated by 0 / 90 / 180 / 270 degrees CCW.
 *
 * Top-left of the rotated image is placed at (dst_x, dst_y). For
 * 90/270 rotations the rotated image's pixel dimensions are
 * (img->height, img->width) -- caller is responsible for the
 * coordinate math when laying out a rotated asset.
 */
void tiku_kits_gfx_image_blit_rotated(const tiku_kits_gfx_surface_t *s,
                                       int16_t dst_x, int16_t dst_y,
                                       const tiku_kits_gfx_image_t *img,
                                       tiku_kits_gfx_rotation_t rot,
                                       uint8_t fg_color);

/**
 * @brief 9-slice blit: render @p img stretched to fill @p dst,
 *        with the four corners drawn at native size and the four
 *        edges and centre stretched (nearest-neighbour).
 *
 * @p left, @p right, @p top, @p bottom are the corner inset widths
 * in source-image pixels. The middle slice is the source rect
 * (left, top, W-left-right, H-top-bottom).
 *
 * If @p dst is smaller than left+right (resp. top+bottom), the
 * corners are clipped against the destination rect.
 */
void tiku_kits_gfx_image_nine_slice(const tiku_kits_gfx_surface_t *s,
                                     const tiku_kits_gfx_rect_t *dst,
                                     const tiku_kits_gfx_image_t *img,
                                     uint16_t left, uint16_t right,
                                     uint16_t top,  uint16_t bottom,
                                     uint8_t fg_color);

#endif /* TIKU_KITS_GFX_IMAGE_H_ */
