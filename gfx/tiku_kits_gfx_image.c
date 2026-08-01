/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_image.c - Multi-format image rendering
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Two access patterns coexist in this file:
 *
 * 1. Sequential iterator (`seq_iter_*`)
 * For row-major blit() walks. RLE images decode incrementally
 * so the worst-case cost is O(W * H), even though random access
 * on RLE would be O(N) per pixel.
 *
 * 2. Random-access (`tiku_kits_gfx_image_pixel`)
 * For scaled / rotated / 9-slice blits where pixels are read
 * in a non-row-major order. RLE images pay O(N) per access; for
 * that reason RLE is strongly discouraged for scaled/rotated
 * use, although it still works correctly.
 */

#include "tiku_kits_gfx_image.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* PIXEL VALUE -> SURFACE COLOUR RESOLUTION                                  */
/*---------------------------------------------------------------------------*/

/* Returns 1 if the pixel should be painted (with *out_color filled
 * in), 0 if the pixel is transparent. */
static int
resolve_pixel(uint8_t raw, tiku_kits_gfx_img_format_t fmt,
               uint8_t fg_color, uint8_t mask_value,
               uint8_t *out_color)
{
    if (raw == mask_value) return 0;

    switch (fmt) {
    case TIKU_KITS_GFX_IMG_1BPP_ROW_MSB:
    case TIKU_KITS_GFX_IMG_1BPP_ROW_LSB:
    case TIKU_KITS_GFX_IMG_1BPP_RLE:
        if (raw == 0u) return 0;
        *out_color = fg_color;
        return 1;
    case TIKU_KITS_GFX_IMG_2BPP_BWR:
        switch (raw) {
        case 0: *out_color = TIKU_KITS_GFX_WHITE; return 1;
        case 1: *out_color = TIKU_KITS_GFX_BLACK; return 1;
        case 2: *out_color = TIKU_KITS_GFX_RED;   return 1;
        default: return 0;
        }
    case TIKU_KITS_GFX_IMG_4BPP_GRAY:
        if (raw < 8u) return 0;
        *out_color = fg_color;
        return 1;
    default:
        return 0;
    }
}

/* Default mask_value used by the un-masked blit() calls per format. */
static uint8_t
default_mask(tiku_kits_gfx_img_format_t fmt)
{
    return (fmt == TIKU_KITS_GFX_IMG_2BPP_BWR) ? 3u : 0u;
}

/*---------------------------------------------------------------------------*/
/* RANDOM-ACCESS PIXEL READER                                                */
/*---------------------------------------------------------------------------*/

uint8_t
tiku_kits_gfx_image_pixel(const tiku_kits_gfx_image_t *img,
                           uint16_t x, uint16_t y)
{
    if (img == NULL || img->data == NULL) return 0;
    if (x >= img->width || y >= img->height) return 0;

    switch (img->format) {
    case TIKU_KITS_GFX_IMG_1BPP_ROW_MSB: {
        uint16_t bytes_per_row = (uint16_t)((img->width + 7u) / 8u);
        uint8_t  b = img->data[(uint32_t)y * bytes_per_row + (x >> 3)];
        return (uint8_t)((b >> (7u - (x & 7u))) & 1u);
    }
    case TIKU_KITS_GFX_IMG_1BPP_ROW_LSB: {
        uint16_t bytes_per_row = (uint16_t)((img->width + 7u) / 8u);
        uint8_t  b = img->data[(uint32_t)y * bytes_per_row + (x >> 3)];
        return (uint8_t)((b >> (x & 7u)) & 1u);
    }
    case TIKU_KITS_GFX_IMG_1BPP_RLE: {
        uint32_t target  = (uint32_t)y * img->width + x;
        uint32_t current = 0;
        uint32_t i = 0;
        while (i < img->data_len) {
            uint8_t  b     = img->data[i++];
            uint8_t  c     = (uint8_t)(b >> 7);
            uint16_t run   = (uint16_t)((b & 0x7Fu) + 1u);
            if (target < current + run) return c;
            current += run;
        }
        return 0;
    }
    case TIKU_KITS_GFX_IMG_2BPP_BWR: {
        uint32_t bits_per_row  = (uint32_t)img->width * 2u;
        uint32_t bytes_per_row = (bits_per_row + 7u) / 8u;
        uint8_t  b = img->data[(uint32_t)y * bytes_per_row + (x >> 2)];
        uint8_t  shift = (uint8_t)(6u - (uint8_t)((x & 3u) * 2u));
        return (uint8_t)((b >> shift) & 0x3u);
    }
    case TIKU_KITS_GFX_IMG_4BPP_GRAY: {
        uint32_t bytes_per_row = ((uint32_t)img->width + 1u) / 2u;
        uint8_t  b = img->data[(uint32_t)y * bytes_per_row + (x >> 1)];
        uint8_t  shift = (uint8_t)((x & 1u) ? 0u : 4u);
        return (uint8_t)((b >> shift) & 0x0Fu);
    }
    default:
        return 0;
    }
}

/*---------------------------------------------------------------------------*/
/* SEQUENTIAL ITERATOR (used by row-major blit)                              */
/*---------------------------------------------------------------------------*/

typedef struct {
    const tiku_kits_gfx_image_t *img;
    uint32_t idx;        /* next pixel index, row-major          */
    /* RLE-only: */
    uint32_t byte_idx;
    uint16_t run_left;
    uint8_t  run_color;
} seq_iter_t;

static void
seq_iter_init(seq_iter_t *it, const tiku_kits_gfx_image_t *img)
{
    it->img       = img;
    it->idx       = 0;
    it->byte_idx  = 0;
    it->run_left  = 0;
    it->run_color = 0;
}

static uint8_t
seq_iter_next(seq_iter_t *it)
{
    uint16_t row, col;
    uint8_t  raw = 0;

    switch (it->img->format) {
    case TIKU_KITS_GFX_IMG_1BPP_ROW_MSB: {
        uint16_t bytes_per_row = (uint16_t)((it->img->width + 7u) / 8u);
        row = (uint16_t)(it->idx / it->img->width);
        col = (uint16_t)(it->idx % it->img->width);
        {
            uint8_t b = it->img->data[(uint32_t)row * bytes_per_row + (col >> 3)];
            raw = (uint8_t)((b >> (7u - (col & 7u))) & 1u);
        }
        break;
    }
    case TIKU_KITS_GFX_IMG_1BPP_ROW_LSB: {
        uint16_t bytes_per_row = (uint16_t)((it->img->width + 7u) / 8u);
        row = (uint16_t)(it->idx / it->img->width);
        col = (uint16_t)(it->idx % it->img->width);
        {
            uint8_t b = it->img->data[(uint32_t)row * bytes_per_row + (col >> 3)];
            raw = (uint8_t)((b >> (col & 7u)) & 1u);
        }
        break;
    }
    case TIKU_KITS_GFX_IMG_1BPP_RLE:
        if (it->run_left == 0) {
            if (it->byte_idx >= it->img->data_len) {
                /* Out of data -- pad with 0. */
                raw = 0;
                break;
            }
            {
                uint8_t b = it->img->data[it->byte_idx++];
                it->run_color = (uint8_t)(b >> 7);
                it->run_left  = (uint16_t)((b & 0x7Fu) + 1u);
            }
        }
        it->run_left--;
        raw = it->run_color;
        break;
    case TIKU_KITS_GFX_IMG_2BPP_BWR: {
        uint32_t bits_per_row  = (uint32_t)it->img->width * 2u;
        uint32_t bytes_per_row = (bits_per_row + 7u) / 8u;
        row = (uint16_t)(it->idx / it->img->width);
        col = (uint16_t)(it->idx % it->img->width);
        {
            uint8_t b = it->img->data[(uint32_t)row * bytes_per_row + (col >> 2)];
            uint8_t shift = (uint8_t)(6u - (uint8_t)((col & 3u) * 2u));
            raw = (uint8_t)((b >> shift) & 0x3u);
        }
        break;
    }
    case TIKU_KITS_GFX_IMG_4BPP_GRAY: {
        uint32_t bytes_per_row = ((uint32_t)it->img->width + 1u) / 2u;
        row = (uint16_t)(it->idx / it->img->width);
        col = (uint16_t)(it->idx % it->img->width);
        {
            uint8_t b = it->img->data[(uint32_t)row * bytes_per_row + (col >> 1)];
            uint8_t shift = (uint8_t)((col & 1u) ? 0u : 4u);
            raw = (uint8_t)((b >> shift) & 0x0Fu);
        }
        break;
    }
    default:
        raw = 0;
        break;
    }
    it->idx++;
    return raw;
}

/*---------------------------------------------------------------------------*/
/* BLIT: SEQUENTIAL                                                          */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_image_blit_masked(const tiku_kits_gfx_surface_t *s,
                                 int16_t dst_x, int16_t dst_y,
                                 const tiku_kits_gfx_image_t *img,
                                 uint8_t fg_color,
                                 uint8_t mask_value)
{
    seq_iter_t it;
    uint16_t   x, y;

    if (s == NULL || s->set_pixel == NULL) return;
    if (img == NULL || img->data == NULL) return;
    if (img->width == 0u || img->height == 0u) return;

    seq_iter_init(&it, img);
    for (y = 0; y < img->height; y++) {
        for (x = 0; x < img->width; x++) {
            uint8_t raw = seq_iter_next(&it);
            uint8_t c;
            if (resolve_pixel(raw, img->format, fg_color, mask_value, &c)) {
                tiku_kits_gfx_pixel(s,
                    (int16_t)(dst_x + (int16_t)x),
                    (int16_t)(dst_y + (int16_t)y), c);
            }
        }
    }
}

void
tiku_kits_gfx_image_blit(const tiku_kits_gfx_surface_t *s,
                          int16_t dst_x, int16_t dst_y,
                          const tiku_kits_gfx_image_t *img,
                          uint8_t fg_color)
{
    if (img == NULL) return;
    tiku_kits_gfx_image_blit_masked(s, dst_x, dst_y, img,
                                     fg_color, default_mask(img->format));
}

/*---------------------------------------------------------------------------*/
/* BLIT: SUB-RECT SCALED (private helper, used by scaled + nine-slice)       */
/*---------------------------------------------------------------------------*/

static void
blit_subrect_scaled(const tiku_kits_gfx_surface_t *s,
                     int16_t dst_x, int16_t dst_y,
                     uint16_t dst_w, uint16_t dst_h,
                     const tiku_kits_gfx_image_t *img,
                     uint16_t src_x, uint16_t src_y,
                     uint16_t src_w, uint16_t src_h,
                     uint8_t fg_color, uint8_t mask_value)
{
    uint16_t i, j;
    if (dst_w == 0 || dst_h == 0 || src_w == 0 || src_h == 0) return;

    for (j = 0; j < dst_h; j++) {
        uint16_t sy = (uint16_t)(src_y +
            (uint16_t)((uint32_t)j * src_h / dst_h));
        for (i = 0; i < dst_w; i++) {
            uint16_t sx = (uint16_t)(src_x +
                (uint16_t)((uint32_t)i * src_w / dst_w));
            uint8_t raw = tiku_kits_gfx_image_pixel(img, sx, sy);
            uint8_t c;
            if (resolve_pixel(raw, img->format, fg_color, mask_value, &c)) {
                tiku_kits_gfx_pixel(s,
                    (int16_t)(dst_x + (int16_t)i),
                    (int16_t)(dst_y + (int16_t)j), c);
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
/* BLIT: SCALED                                                              */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_image_blit_scaled(const tiku_kits_gfx_surface_t *s,
                                 int16_t dst_x, int16_t dst_y,
                                 uint16_t dst_w, uint16_t dst_h,
                                 const tiku_kits_gfx_image_t *img,
                                 uint8_t fg_color)
{
    if (s == NULL || s->set_pixel == NULL) return;
    if (img == NULL || img->data == NULL) return;
    if (dst_w == 0u || dst_h == 0u) return;
    if (img->width == 0u || img->height == 0u) return;

    blit_subrect_scaled(s, dst_x, dst_y, dst_w, dst_h, img,
        0, 0, img->width, img->height,
        fg_color, default_mask(img->format));
}

/*---------------------------------------------------------------------------*/
/* BLIT: ROTATED                                                             */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_image_blit_rotated(const tiku_kits_gfx_surface_t *s,
                                  int16_t dst_x, int16_t dst_y,
                                  const tiku_kits_gfx_image_t *img,
                                  tiku_kits_gfx_rotation_t rot,
                                  uint8_t fg_color)
{
    uint16_t out_w, out_h;
    uint16_t i, j;
    uint8_t  mask;

    if (s == NULL || s->set_pixel == NULL) return;
    if (img == NULL || img->data == NULL) return;
    if (img->width == 0u || img->height == 0u) return;

    if (rot == TIKU_KITS_GFX_ROT_0 || rot == TIKU_KITS_GFX_ROT_180) {
        out_w = img->width;
        out_h = img->height;
    } else {
        out_w = img->height;
        out_h = img->width;
    }

    mask = default_mask(img->format);

    for (j = 0; j < out_h; j++) {
        for (i = 0; i < out_w; i++) {
            uint16_t sx = 0, sy = 0;
            uint8_t  raw, c;
            switch (rot) {
            case TIKU_KITS_GFX_ROT_0:
                sx = i;            sy = j;            break;
            case TIKU_KITS_GFX_ROT_90:
                sx = (uint16_t)(out_h - 1u - j);
                sy = i;
                break;
            case TIKU_KITS_GFX_ROT_180:
                sx = (uint16_t)(out_w - 1u - i);
                sy = (uint16_t)(out_h - 1u - j);
                break;
            case TIKU_KITS_GFX_ROT_270:
                sx = j;
                sy = (uint16_t)(out_w - 1u - i);
                break;
            default: continue;
            }
            raw = tiku_kits_gfx_image_pixel(img, sx, sy);
            if (resolve_pixel(raw, img->format, fg_color, mask, &c)) {
                tiku_kits_gfx_pixel(s,
                    (int16_t)(dst_x + (int16_t)i),
                    (int16_t)(dst_y + (int16_t)j), c);
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
/* BLIT: 9-SLICE                                                             */
/*---------------------------------------------------------------------------*/

void
tiku_kits_gfx_image_nine_slice(const tiku_kits_gfx_surface_t *s,
                                const tiku_kits_gfx_rect_t *dst,
                                const tiku_kits_gfx_image_t *img,
                                uint16_t left, uint16_t right,
                                uint16_t top,  uint16_t bottom,
                                uint8_t fg_color)
{
    uint16_t src_w, src_h;
    uint16_t mid_src_w, mid_src_h;
    uint16_t mid_dst_w, mid_dst_h;
    int16_t  dx, dy;
    uint16_t dw, dh;
    uint8_t  mask;

    if (s == NULL || s->set_pixel == NULL) return;
    if (dst == NULL || img == NULL || img->data == NULL) return;
    if (dst->w == 0u || dst->h == 0u) return;
    if (img->width == 0u || img->height == 0u) return;

    src_w = img->width;
    src_h = img->height;

    /* Clamp insets so they fit inside the source image. */
    if ((uint32_t)left + right >= src_w) {
        left  = (uint16_t)(src_w / 2u);
        right = (uint16_t)(src_w - left);
        if (right > 0u) right = (uint16_t)(right - 1u); /* keep mid >= 1 */
    }
    if ((uint32_t)top + bottom >= src_h) {
        top    = (uint16_t)(src_h / 2u);
        bottom = (uint16_t)(src_h - top);
        if (bottom > 0u) bottom = (uint16_t)(bottom - 1u);
    }

    dx = dst->x;
    dy = dst->y;
    dw = dst->w;
    dh = dst->h;

    mid_src_w = (uint16_t)(src_w - left - right);
    mid_src_h = (uint16_t)(src_h - top - bottom);
    mid_dst_w = (dw > (uint32_t)left + right)
                  ? (uint16_t)(dw - left - right) : 0u;
    mid_dst_h = (dh > (uint32_t)top + bottom)
                  ? (uint16_t)(dh - top - bottom) : 0u;

    mask = default_mask(img->format);

    /* Top-left, top, top-right. */
    blit_subrect_scaled(s, dx, dy, left, top, img,
        0, 0, left, top, fg_color, mask);
    if (mid_dst_w > 0u) {
        blit_subrect_scaled(s, (int16_t)(dx + (int16_t)left), dy,
            mid_dst_w, top, img,
            left, 0, mid_src_w, top, fg_color, mask);
    }
    blit_subrect_scaled(s, (int16_t)(dx + (int16_t)dw - (int16_t)right), dy,
        right, top, img,
        (uint16_t)(src_w - right), 0, right, top, fg_color, mask);

    /* Left, centre, right. */
    if (mid_dst_h > 0u) {
        blit_subrect_scaled(s, dx, (int16_t)(dy + (int16_t)top),
            left, mid_dst_h, img,
            0, top, left, mid_src_h, fg_color, mask);
        if (mid_dst_w > 0u) {
            blit_subrect_scaled(s,
                (int16_t)(dx + (int16_t)left),
                (int16_t)(dy + (int16_t)top),
                mid_dst_w, mid_dst_h, img,
                left, top, mid_src_w, mid_src_h, fg_color, mask);
        }
        blit_subrect_scaled(s,
            (int16_t)(dx + (int16_t)dw - (int16_t)right),
            (int16_t)(dy + (int16_t)top),
            right, mid_dst_h, img,
            (uint16_t)(src_w - right), top, right, mid_src_h,
            fg_color, mask);
    }

    /* Bottom-left, bottom, bottom-right. */
    blit_subrect_scaled(s, dx,
        (int16_t)(dy + (int16_t)dh - (int16_t)bottom),
        left, bottom, img,
        0, (uint16_t)(src_h - bottom), left, bottom, fg_color, mask);
    if (mid_dst_w > 0u) {
        blit_subrect_scaled(s,
            (int16_t)(dx + (int16_t)left),
            (int16_t)(dy + (int16_t)dh - (int16_t)bottom),
            mid_dst_w, bottom, img,
            left, (uint16_t)(src_h - bottom),
            mid_src_w, bottom, fg_color, mask);
    }
    blit_subrect_scaled(s,
        (int16_t)(dx + (int16_t)dw - (int16_t)right),
        (int16_t)(dy + (int16_t)dh - (int16_t)bottom),
        right, bottom, img,
        (uint16_t)(src_w - right), (uint16_t)(src_h - bottom),
        right, bottom, fg_color, mask);
}
