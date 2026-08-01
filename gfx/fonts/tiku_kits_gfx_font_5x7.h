/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_gfx_font_5x7.h - Built-in 5x7 ASCII font
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Classic 5-pixel-wide / 7-pixel-tall monospaced bitmap font
 * covering the printable ASCII range (0x20 .. 0x7E). Ideal for
 * small-display labels and headlines at scale 2-4 on EPDs.
 *
 * Total cost: ~480 bytes of FRAM.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef TIKU_KITS_GFX_FONT_5X7_H_
#define TIKU_KITS_GFX_FONT_5X7_H_

#include "../tiku_kits_gfx_text.h"

/**
 * @brief 5x7 ASCII font covering printable characters 0x20 .. 0x7E.
 *
 * Pass to any tiku_kits_gfx_draw_string / draw_char call.
 */
extern const tiku_kits_gfx_font_t tiku_kits_gfx_font_5x7;

#endif /* TIKU_KITS_GFX_FONT_5X7_H_ */
