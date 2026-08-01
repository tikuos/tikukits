/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_window.h - Window container widget
 *
 * BeOS-styled window: thin black hairline frame around the content
 * area, with a red title tab on the top-left projecting above the
 * frame. Holds N child widgets at fixed positions inside the
 * content area.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_WINDOW_H_
#define TIKU_KITS_UI_WINDOW_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

#ifndef TIKU_KITS_UI_WINDOW_MAX_CHILDREN
#define TIKU_KITS_UI_WINDOW_MAX_CHILDREN 16
#endif

/*
 * Children draw into the content area below the title tab;
 * the window establishes a subsurface so child coordinates are
 * relative to the inside of the frame.
 * Focus management: the window tracks which child is currently
 * focused (focus_idx). Focus events advance / retreat the index
 * to the next focusable child, wrapping at the ends.
 */

/**
 * @brief A window container with a BeOS-style title tab.
 */
typedef struct {
    tiku_kits_ui_widget_t base;
    const char *title;
    const tiku_kits_gfx_font_t *title_font;
    uint8_t title_scale;
    tiku_kits_ui_widget_t *children[TIKU_KITS_UI_WINDOW_MAX_CHILDREN];
    uint8_t  n_children;
    int8_t   focus_idx;       /* index into children[]; -1 = no focus */
} tiku_kits_ui_window_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_window_ops;

/**
 * @brief Initialize a window container.
 *
 * @param win        Window struct (caller-allocated)
 * @param x, y, w, h Position + size in parent coords
 * @param title      Title shown in the window's title tab (may be NULL)
 * @param title_font Font for the title (typically the built-in 5x7)
 * @param title_scale Font scale for the title (1 or 2 looks BeOS-ish)
 */
void tiku_kits_ui_window_init(tiku_kits_ui_window_t *win,
                               int16_t x, int16_t y,
                               uint16_t w, uint16_t h,
                               const char *title,
                               const tiku_kits_gfx_font_t *title_font,
                               uint8_t title_scale);

/**
 * @brief Add a child widget to the window.
 *
 * @return TIKU_KITS_UI_OK on success,
 *         TIKU_KITS_UI_ERR_FULL if the children array is full,
 *         TIKU_KITS_UI_ERR_PARAM if @p win or @p child is NULL.
 */
int tiku_kits_ui_window_add(tiku_kits_ui_window_t *win,
                             tiku_kits_ui_widget_t *child);

/**
 * @brief Render the window and all visible children into @p s.
 *
 * Convenience wrapper around the ops vtable -- equivalent to
 * tiku_kits_ui_widget_render(&win->base, s) but takes a typed
 * pointer for IDE friendliness.
 */
void tiku_kits_ui_window_render(const tiku_kits_ui_window_t *win,
                                 const tiku_kits_gfx_surface_t *s);

/*
 * FOCUS_NEXT / FOCUS_PREV walk the focusable children, wrapping.
 * ACTIVATE dispatches to the focused child's handle_event.
 * The window does NOT trigger a re-render -- the application is
 * responsible for calling render() and then refreshing the
 * display kit when ready.
 */

/**
 * @brief Feed an input event into the window.
 */
void tiku_kits_ui_window_event(tiku_kits_ui_window_t *win,
                                tiku_kits_ui_event_t evt);

/*
 * For each dirty child:
 * 1. Clears its rectangle on the content area with @p bg_color.
 * 2. Renders the child.
 * 3. Clears the child's dirty flag.
 * 4. Accumulates the child's screen-absolute rectangle into
 * The window's title tab and frame are NOT redrawn -- only the
 * content area inside the frame. If you need to repaint the
 * chrome (e.g. the title text changed), call window_render
 * instead.
 */

/**
 * @brief Render only the children whose `dirty` flag is set.
 *
 * @p out_dirty_rect (if non-NULL).
 * @return Number of children repainted. When 0, the surface was
 * not touched and @p out_dirty_rect is set to an empty
 * rectangle. Useful with epaper_refresh_rect() to skip
 * the refresh entirely when nothing changed.
 */
uint16_t tiku_kits_ui_window_render_dirty(
    tiku_kits_ui_window_t *win,
    const tiku_kits_gfx_surface_t *s,
    uint8_t bg_color,
    tiku_kits_gfx_rect_t *out_dirty_rect);

#endif /* TIKU_KITS_UI_WINDOW_H_ */
