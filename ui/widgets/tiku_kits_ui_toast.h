/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_toast.h - Transient banner with auto-dismiss
 *
 * Renders a banner with a single-line message. Auto-hides itself
 * (sets `base.visible = 0`) after `lifetime_renders` frames have
 * been drawn. Display duration is therefore tied to how often the
 * UI tree is rendered -- on EPDs that's often only on user
 * interactions, so tune lifetime to your refresh cadence.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_TOAST_H_
#define TIKU_KITS_UI_TOAST_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

typedef struct {
    tiku_kits_ui_widget_t base;
    const char *message;
    const tiku_kits_gfx_font_t *font;
    uint8_t scale;
    uint16_t lifetime_renders;
    uint16_t renders_seen;
} tiku_kits_ui_toast_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_toast_ops;

void tiku_kits_ui_toast_init(tiku_kits_ui_toast_t *tt,
                              int16_t x, int16_t y,
                              uint16_t w, uint16_t h,
                              const tiku_kits_gfx_font_t *font,
                              uint8_t scale);

/** Show a message for @p lifetime_renders frames before auto-hide. */
void tiku_kits_ui_toast_show(tiku_kits_ui_toast_t *tt,
                              const char *message,
                              uint16_t lifetime_renders);

void tiku_kits_ui_toast_hide(tiku_kits_ui_toast_t *tt);

#endif /* TIKU_KITS_UI_TOAST_H_ */
