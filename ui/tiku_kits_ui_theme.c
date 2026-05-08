/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_theme.c - Theme registry + built-in themes
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

/*---------------------------------------------------------------------------*/
/* BUILT-IN THEMES                                                           */
/*---------------------------------------------------------------------------*/

const tiku_kits_ui_theme_t tiku_kits_ui_theme_beos = {
    .color_fg      = TIKU_KITS_GFX_BLACK,
    .color_bg      = TIKU_KITS_GFX_WHITE,
    .color_accent  = TIKU_KITS_GFX_RED,
    .color_focus   = TIKU_KITS_GFX_RED,
    .color_muted   = TIKU_KITS_GFX_BLACK,
    .color_danger  = TIKU_KITS_GFX_RED,
    .color_warn    = TIKU_KITS_GFX_RED,
    .color_success = TIKU_KITS_GFX_BLACK,

    .font_primary  = &tiku_kits_gfx_font_5x7,
    .font_headline = &tiku_kits_gfx_font_5x7,
    .font_mono     = &tiku_kits_gfx_font_5x7,
    .font_symbol   = NULL,

    .pad_x    = 4,
    .pad_y    = 4,
    .spacing  = 4,
    .frame_w  = 2,
    .corner_r = 4,
    .focus_w  = 1,
    .line_w   = 1,

    .flags = TIKU_KITS_UI_THEME_FLAG_BEVEL_BUTTONS
           | TIKU_KITS_UI_THEME_FLAG_TAB_TITLE,
};

const tiku_kits_ui_theme_t tiku_kits_ui_theme_flat = {
    .color_fg      = TIKU_KITS_GFX_BLACK,
    .color_bg      = TIKU_KITS_GFX_WHITE,
    .color_accent  = TIKU_KITS_GFX_RED,
    .color_focus   = TIKU_KITS_GFX_BLACK,
    .color_muted   = TIKU_KITS_GFX_BLACK,
    .color_danger  = TIKU_KITS_GFX_RED,
    .color_warn    = TIKU_KITS_GFX_RED,
    .color_success = TIKU_KITS_GFX_BLACK,

    .font_primary  = &tiku_kits_gfx_font_5x7,
    .font_headline = &tiku_kits_gfx_font_5x7,
    .font_mono     = &tiku_kits_gfx_font_5x7,
    .font_symbol   = NULL,

    .pad_x    = 6,
    .pad_y    = 4,
    .spacing  = 4,
    .frame_w  = 1,
    .corner_r = 0,
    .focus_w  = 1,
    .line_w   = 1,

    .flags = TIKU_KITS_UI_THEME_FLAG_DOTTED_FOCUS,
};

const tiku_kits_ui_theme_t tiku_kits_ui_theme_hud = {
    .color_fg      = TIKU_KITS_GFX_WHITE,
    .color_bg      = TIKU_KITS_GFX_BLACK,
    .color_accent  = TIKU_KITS_GFX_RED,
    .color_focus   = TIKU_KITS_GFX_WHITE,
    .color_muted   = TIKU_KITS_GFX_WHITE,
    .color_danger  = TIKU_KITS_GFX_RED,
    .color_warn    = TIKU_KITS_GFX_RED,
    .color_success = TIKU_KITS_GFX_WHITE,

    .font_primary  = &tiku_kits_gfx_font_5x7,
    .font_headline = &tiku_kits_gfx_font_5x7,
    .font_mono     = &tiku_kits_gfx_font_5x7,
    .font_symbol   = NULL,

    .pad_x    = 4,
    .pad_y    = 4,
    .spacing  = 4,
    .frame_w  = 1,
    .corner_r = 0,
    .focus_w  = 1,
    .line_w   = 1,

    .flags = TIKU_KITS_UI_THEME_FLAG_DOTTED_FOCUS,
};

const tiku_kits_ui_theme_t tiku_kits_ui_theme_mono = {
    .color_fg      = TIKU_KITS_GFX_BLACK,
    .color_bg      = TIKU_KITS_GFX_WHITE,
    .color_accent  = TIKU_KITS_GFX_BLACK,
    .color_focus   = TIKU_KITS_GFX_BLACK,
    .color_muted   = TIKU_KITS_GFX_BLACK,
    .color_danger  = TIKU_KITS_GFX_BLACK,
    .color_warn    = TIKU_KITS_GFX_BLACK,
    .color_success = TIKU_KITS_GFX_BLACK,

    .font_primary  = &tiku_kits_gfx_font_5x7,
    .font_headline = &tiku_kits_gfx_font_5x7,
    .font_mono     = &tiku_kits_gfx_font_5x7,
    .font_symbol   = NULL,

    .pad_x    = 4,
    .pad_y    = 4,
    .spacing  = 4,
    .frame_w  = 1,
    .corner_r = 0,
    .focus_w  = 1,
    .line_w   = 1,

    .flags = TIKU_KITS_UI_THEME_FLAG_DOTTED_FOCUS,
};

/*---------------------------------------------------------------------------*/
/* CURRENT THEME REGISTRY                                                    */
/*---------------------------------------------------------------------------*/

static const tiku_kits_ui_theme_t *current_theme = &tiku_kits_ui_theme_beos;

const tiku_kits_ui_theme_t *
tiku_kits_ui_theme_current(void)
{
    return (current_theme != NULL) ? current_theme
                                    : &tiku_kits_ui_theme_beos;
}

void
tiku_kits_ui_theme_set(const tiku_kits_ui_theme_t *theme)
{
    current_theme = (theme != NULL) ? theme : &tiku_kits_ui_theme_beos;
}
