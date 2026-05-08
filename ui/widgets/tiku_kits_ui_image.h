/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_image.h - Generic image widget
 *
 * Wraps tiku_kits_gfx_image_t in a UI widget. The image is centred
 * inside the widget rect by default; setting @p stretch != 0 makes
 * it scale (nearest) to fill the rect. Optional 1-px frame around
 * the image rect.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_IMAGE_H_
#define TIKU_KITS_UI_IMAGE_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_image.h>

typedef struct {
    tiku_kits_ui_widget_t base;
    const tiku_kits_gfx_image_t *image;
    uint8_t color;
    uint8_t bordered;
    uint8_t stretch;
} tiku_kits_ui_image_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_image_ops;

void tiku_kits_ui_image_init(tiku_kits_ui_image_t *iw,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h,
                              const tiku_kits_gfx_image_t *image,
                              uint8_t color,
                              uint8_t bordered,
                              uint8_t stretch);

void tiku_kits_ui_image_set_image(tiku_kits_ui_image_t *iw,
                                   const tiku_kits_gfx_image_t *image);

#endif /* TIKU_KITS_UI_IMAGE_H_ */
