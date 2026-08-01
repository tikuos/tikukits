/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_theme.h - Theme abstraction for the UI kit
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Bundles palette, fonts, spacing and style switches that widgets
 * read at render time. Apps can either use one of the built-in
 * themes (BeOS, flat, mono) or define their own.
 *
 * The kit keeps a "current" theme pointer used by every widget that
 * doesn't own its own colours / fonts. Set it once at boot via
 * `tiku_kits_ui_theme_set()`; widgets created before or after the
 * call all pick up the new theme on their next render.
 */

#ifndef TIKU_KITS_UI_THEME_H_
#define TIKU_KITS_UI_THEME_H_

#include <stdint.h>
#include <tikukits/gfx/tiku_kits_gfx.h>
#include <tikukits/gfx/tiku_kits_gfx_text.h>

/*---------------------------------------------------------------------------*/
/* STYLE FLAGS                                                               */
/*---------------------------------------------------------------------------*/

/* OR-combined into theme.flags. */
#define TIKU_KITS_UI_THEME_FLAG_BEVEL_BUTTONS  (1u << 0)
                                       /* Outset highlight/shadow on
                                        * buttons (BeOS look).        */
#define TIKU_KITS_UI_THEME_FLAG_TAB_TITLE      (1u << 1)
                                       /* Window title rendered as a
                                        * projecting tab; otherwise a
                                        * full-width title bar.       */
#define TIKU_KITS_UI_THEME_FLAG_DOTTED_FOCUS   (1u << 2)
                                       /* Focus drawn as a dotted
                                        * outline; otherwise the
                                        * widget's border colour
                                        * swaps to color_focus.       */

/*---------------------------------------------------------------------------*/
/* THEME STRUCT                                                              */
/*---------------------------------------------------------------------------*/

typedef struct {
    /* Palette (raw pixel-format colours, passed verbatim to gfx). */
    uint8_t color_fg;
    uint8_t color_bg;
    uint8_t color_accent;
    uint8_t color_focus;
    uint8_t color_muted;
    uint8_t color_danger;
    uint8_t color_warn;
    uint8_t color_success;

    /* Default fonts. NULL is allowed; widgets fall back to the
     * built-in 5x7 font if the slot is unset. */
    const tiku_kits_gfx_font_t *font_primary;
    const tiku_kits_gfx_font_t *font_headline;
    const tiku_kits_gfx_font_t *font_mono;
    const tiku_kits_gfx_font_t *font_symbol;

    /* Layout metrics (in pixels). */
    uint8_t pad_x;
    uint8_t pad_y;
    uint8_t spacing;
    uint8_t frame_w;
    uint8_t corner_r;
    uint8_t focus_w;
    uint8_t line_w;

    /* Style switches (OR of TIKU_KITS_UI_THEME_FLAG_*). */
    uint8_t flags;
} tiku_kits_ui_theme_t;

/*---------------------------------------------------------------------------*/
/* GLOBAL CURRENT-THEME ACCESSORS                                            */
/*---------------------------------------------------------------------------*/

/** Return the current theme pointer; never NULL. */
const tiku_kits_ui_theme_t *tiku_kits_ui_theme_current(void);

/** Set the current theme. Passing NULL resets to the default
 *  (BeOS) theme. The pointer is stored verbatim -- the theme
 *  struct must outlive every widget that may read it. */
void tiku_kits_ui_theme_set(const tiku_kits_ui_theme_t *theme);

/*---------------------------------------------------------------------------*/
/* BUILT-IN THEMES                                                           */
/*---------------------------------------------------------------------------*/

/* BeOS-styled (the original look): hairline frame, projecting red
 * title tab, button bevel, color-swap focus. Default theme. */
extern const tiku_kits_ui_theme_t tiku_kits_ui_theme_beos;

/* Flat: no bevel, full title bar, dotted focus, accent uses BLACK
 * to remain monochrome-friendly while still distinguishing status. */
extern const tiku_kits_ui_theme_t tiku_kits_ui_theme_flat;

/* Monochrome: no RED at all; everything in BLACK on WHITE. Useful
 * for B/W e-paper or LCD where there is no second colour plane. */
extern const tiku_kits_ui_theme_t tiku_kits_ui_theme_mono;

/* HUD: high-contrast inverted -- WHITE foreground on BLACK
 * background. RED accent for status hot spots. Dotted focus.
 * Sharp 1-px borders, no corner rounding. Fits aircraft-cockpit /
 * dashboard aesthetics; also great for OLED where black pixels
 * cost no power. */
extern const tiku_kits_ui_theme_t tiku_kits_ui_theme_hud;

#endif /* TIKU_KITS_UI_THEME_H_ */
