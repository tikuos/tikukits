/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui.h - Retained-mode user interface library
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A static-allocation, retained-mode UI kit for TikuOS, built on
 * top of tikukits/gfx. The application declares a tree of widgets
 * (window with children: labels, buttons, icons), the kit walks
 * the tree to render into any gfx_surface, and the application
 * feeds keyboard-style focus / activate events from physical
 * buttons.
 *
 * Architecture
 * ------------
 *
 * application
 * |
 * v   (build widget tree statically, feed events)
 * +----------------------+
 * | tiku_kits_ui.h/.c    |   widget base + focus + dispatch
 * | widgets/             |   window, label, button, icon
 * +----------------------+
 * |
 * v   (renders via tiku_kits_gfx_surface_t)
 * tikukits/gfx
 * |
 * v
 * display kit (tikukits/epaper) -> physical panel
 *
 * Visual style: BeOS-inspired hairline frames with red accent
 * tabs and outset bevels on buttons. Renders well on monochrome
 * and BWR e-paper because everything is 1-pixel-edge work.
 *
 * Memory model: caller-allocates. Every widget is a static struct
 * the application owns. No malloc, no widget pool, no garbage
 * collection. Apps that switch screens just swap which top-level
 * window is active.
 *
 * Refresh model: the kit never triggers display refresh. The app
 * calls tiku_kits_ui_window_render(window, surface) to paint into
 * the framebuffer, then triggers refresh through the display kit
 * separately. This keeps the UI display-agnostic and lets the app
 * decide refresh policy (e.g. only re-render after state changes).
 *
 * Adding new widget types
 * -----------------------
 * 1. Define a struct that embeds tiku_kits_ui_widget_t as its
 * first member.
 * 2. Implement render() and (optionally) handle_event() and
 * is_focusable().
 * 3. Export an ops vtable + an init function.
 * 4. Drop the .c file under widgets/ -- the Makefile picks up
 * `widgets/<file>.c` automatically.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef TIKU_KITS_UI_H_
#define TIKU_KITS_UI_H_

#include <stdint.h>
#include <tikukits/gfx/tiku_kits_gfx.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_UI_OK             0
#define TIKU_KITS_UI_ERR_PARAM    (-1)
#define TIKU_KITS_UI_ERR_FULL     (-2)   /**< Container has no room for more children */

/*---------------------------------------------------------------------------*/
/* INPUT EVENTS                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Events fed into the UI kit by the application.
 *
 * Apps wire physical buttons (e.g. LaunchPad S1/S2) or any other
 * input source to these events. The UI walks its focusable widgets
 * and dispatches accordingly.
 */
typedef enum {
    TIKU_KITS_UI_EVT_FOCUS_NEXT  = 0,   /**< Move focus to next focusable widget */
    TIKU_KITS_UI_EVT_FOCUS_PREV  = 1,   /**< Move focus to previous focusable widget */
    TIKU_KITS_UI_EVT_ACTIVATE    = 2,   /**< Activate the focused widget (click) */
    TIKU_KITS_UI_EVT_INC         = 3,   /**< Increment value of focused widget */
    TIKU_KITS_UI_EVT_DEC         = 4,   /**< Decrement value of focused widget */
    TIKU_KITS_UI_EVT_BACK        = 5,   /**< Cancel / pop screen / dismiss dialog */
    TIKU_KITS_UI_EVT_LONG_PRESS  = 6,   /**< Long-press on focused widget */
    TIKU_KITS_UI_EVT_MENU        = 7,   /**< Open contextual menu */
    /* Spatial / D-pad navigation. Layout containers consume these
     * to move focus geometrically; widgets without spatial peers
     * fall back to NEXT / PREV semantics. */
    TIKU_KITS_UI_EVT_FOCUS_UP    = 8,
    TIKU_KITS_UI_EVT_FOCUS_DOWN  = 9,
    TIKU_KITS_UI_EVT_FOCUS_LEFT  = 10,
    TIKU_KITS_UI_EVT_FOCUS_RIGHT = 11,
    /* Numeric input. Widgets that accept digit input (textbox,
     * stepper) handle these; others ignore them. The encoding
     * `BASE + digit` keeps the mapping branch-free. */
    TIKU_KITS_UI_EVT_NUMERIC_BASE = 12,
    TIKU_KITS_UI_EVT_NUMERIC_0    = 12,
    TIKU_KITS_UI_EVT_NUMERIC_9    = 21,
} tiku_kits_ui_event_t;

/** Convenience: event for digit `d` in 0..9. */
#define TIKU_KITS_UI_EVT_NUMERIC(d) \
    ((tiku_kits_ui_event_t)(TIKU_KITS_UI_EVT_NUMERIC_BASE + (d)))

/*---------------------------------------------------------------------------*/
/* WIDGET BASE                                                               */
/*---------------------------------------------------------------------------*/

/* Forward declaration so ops can take a pointer to it. */
typedef struct tiku_kits_ui_widget tiku_kits_ui_widget_t;

/*
 * - render():         draw the widget into the given surface using
 * the widget's own coordinates (relative to
 * the parent's content origin).
 * - handle_event():   optional. Called when the widget is focused
 * and an event arrives. Returns 1 if handled,
 * 0 otherwise.
 * - is_focusable():   returns 1 if this widget can receive focus
 * (buttons yes; static labels no).
 * - intrinsic_size(): optional. Layouts call this to ask the
 * widget for its natural size given an
 * available width. NULL = use the widget's
 * current (w, h) verbatim. Used by vbox / hbox
 * / grid for "auto"-sized children.
 */

/**
 * @brief Per-widget-type vtable.
 */
typedef struct {
    void (*render)(const tiku_kits_ui_widget_t *w,
                    const tiku_kits_gfx_surface_t *s);
    int  (*handle_event)(tiku_kits_ui_widget_t *w,
                          tiku_kits_ui_event_t evt);
    int  (*is_focusable)(const tiku_kits_ui_widget_t *w);
    void (*intrinsic_size)(const tiku_kits_ui_widget_t *w,
                            uint16_t avail_w,
                            tiku_kits_gfx_size_t *out);
} tiku_kits_ui_widget_ops_t;

/*
 * Geometry: (x, y) is the top-left of the widget in its parent's
 * content-area coordinates. (w, h) is the widget size in pixels.
 * Visibility / focus state are mutated by the kit; apps generally
 * shouldn't poke them directly.
 * Dirty bit: set whenever the widget's visible state changes
 * (mutator helpers like *_set_text / *_set_value / *_set_checked
 * flip it). Cleared by the dirty-aware render walk after the
 * widget is repainted. tiku_kits_ui_window_render_dirty() uses
 * this to skip widgets whose pixels have not changed since the
 * last full paint -- the big win on EPDs that support partial
 * refresh.
 */

/**
 * @brief Common header embedded as the first member of every
 * widget type. Lets the kit operate on widgets generically
 * through the base pointer.
 */
struct tiku_kits_ui_widget {
    const tiku_kits_ui_widget_ops_t *ops;
    int16_t  x;
    int16_t  y;
    uint16_t w;
    uint16_t h;
    uint8_t  visible;        /**< 1 = drawn during render */
    uint8_t  focused;        /**< 1 = currently focused */
    uint8_t  dirty;          /**< 1 = needs repaint on next dirty pass */
    uint8_t  reserved;       /**< reserved for future flags */
    void    *user_data;      /**< Application-defined */
};

/** Mark @p w as needing a repaint on the next render_dirty pass. */
static inline void
tiku_kits_ui_widget_mark_dirty(tiku_kits_ui_widget_t *w)
{
    if (w != NULL) w->dirty = 1u;
}

/** Clear @p w 's dirty bit (called by the dirty-aware render after
 *  a successful repaint). */
static inline void
tiku_kits_ui_widget_clear_dirty(tiku_kits_ui_widget_t *w)
{
    if (w != NULL) w->dirty = 0u;
}

/*---------------------------------------------------------------------------*/
/* GENERIC API                                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Convenience: dispatch render via the widget's vtable.
 * Apps almost never call this directly -- the window's render
 * walks its children for you. Useful when implementing custom
 * container widgets.
 */
static inline void
tiku_kits_ui_widget_render(const tiku_kits_ui_widget_t *w,
                            const tiku_kits_gfx_surface_t *s)
{
    if (w == NULL || w->ops == NULL || w->ops->render == NULL) return;
    if (!w->visible) return;
    w->ops->render(w, s);
}

static inline int
tiku_kits_ui_widget_is_focusable(const tiku_kits_ui_widget_t *w)
{
    if (w == NULL || w->ops == NULL || w->ops->is_focusable == NULL) return 0;
    if (!w->visible) return 0;
    return w->ops->is_focusable(w);
}

/*
 * Used by layout containers (vbox / hbox / grid) when sizing
 * auto children. Caller widgets that want to be auto-sized
 * implement the ops slot; static widgets leave it NULL and pass
 * their explicit size through.
 */

/**
 * @brief Ask @p w for its natural size given @p avail_w of horizontal
 * space. Falls back to the widget's current (w, h) if its
 * ops vtable doesn't define intrinsic_size.
 */
static inline void
tiku_kits_ui_widget_intrinsic_size(const tiku_kits_ui_widget_t *w,
                                    uint16_t avail_w,
                                    tiku_kits_gfx_size_t *out)
{
    if (out == NULL) return;
    if (w == NULL) { out->w = 0; out->h = 0; return; }
    if (w->ops != NULL && w->ops->intrinsic_size != NULL) {
        w->ops->intrinsic_size(w, avail_w, out);
        return;
    }
    out->w = w->w;
    out->h = w->h;
}

/*---------------------------------------------------------------------------*/
/* SPATIAL FOCUS WALK                                                        */
/*---------------------------------------------------------------------------*/

/*
 * Implements the "next focus in direction" walk used by D-pad
 * navigation. Each candidate is scored by (a) its offset along
 * the direction axis (must be positive in the requested
 * direction) and (b) its perpendicular offset (penalised at 4x).
 * The widget with the lowest score wins; if no child lies in the
 * requested direction the function returns NULL and the caller
 * can fall back to linear navigation.
 */

/**
 * @brief Pick the focusable child whose centre best matches a
 * direction vector starting from @p current.
 *
 * @param children    Caller-owned array of widget pointers.
 * @param n_children  Length of @p children.
 * @param current     Currently focused widget, or NULL to start
 * from the corner opposite the direction.
 * @param dx          1 for right, -1 for left, 0 for vertical-only.
 * @param dy          1 for down, -1 for up, 0 for horizontal-only.
 * @return The best candidate, or NULL if none exists.
 */
tiku_kits_ui_widget_t *
tiku_kits_ui_focus_walk(tiku_kits_ui_widget_t * const *children,
                         uint8_t n_children,
                         const tiku_kits_ui_widget_t *current,
                         int dx, int dy);

#endif /* TIKU_KITS_UI_H_ */
