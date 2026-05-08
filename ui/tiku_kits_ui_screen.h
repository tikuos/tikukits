/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_screen.h - Screen stack for navigation
 *
 * A simple LIFO stack of top-level windows. Push a new window when
 * the user enters a sub-screen (settings page, detail view, etc.);
 * pop on BACK. The application renders the top of the stack each
 * frame and feeds events into the stack's dispatcher.
 *
 * Memory model: caller-allocates everything. The stack stores
 * pointers, not copies; the windows themselves must outlive the
 * stack.
 *
 * Usage:
 *
 *   static tiku_kits_ui_screen_stack_t stack;
 *   static tiku_kits_ui_window_t       home_screen, settings_screen;
 *
 *   tiku_kits_ui_screen_stack_init(&stack);
 *   tiku_kits_ui_screen_stack_push(&stack, &home_screen);
 *
 *   // user opens settings:
 *   tiku_kits_ui_screen_stack_push(&stack, &settings_screen);
 *
 *   // each render:
 *   tiku_kits_ui_screen_stack_render(&stack, &surface);
 *
 *   // each input event:
 *   tiku_kits_ui_screen_stack_event(&stack, evt);
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_SCREEN_H_
#define TIKU_KITS_UI_SCREEN_H_

#include "tiku_kits_ui.h"
#include "widgets/tiku_kits_ui_window.h"

#ifndef TIKU_KITS_UI_SCREEN_STACK_DEPTH
#define TIKU_KITS_UI_SCREEN_STACK_DEPTH 6
#endif

typedef void (*tiku_kits_ui_screen_back_cb_t)(void *user_data);

typedef struct {
    tiku_kits_ui_window_t *windows[TIKU_KITS_UI_SCREEN_STACK_DEPTH];
    uint8_t                depth;
    /* Optional callback fired when BACK reaches the root and there
     * is nothing more to pop. Apps can use it to exit / sleep / etc. */
    tiku_kits_ui_screen_back_cb_t on_back_at_root;
    void                  *user_data;
} tiku_kits_ui_screen_stack_t;

void tiku_kits_ui_screen_stack_init(tiku_kits_ui_screen_stack_t *st);

void tiku_kits_ui_screen_stack_set_back_cb(tiku_kits_ui_screen_stack_t *st,
                                            tiku_kits_ui_screen_back_cb_t cb,
                                            void *user_data);

/** Push a new top-level window. Returns TIKU_KITS_UI_OK or
 *  TIKU_KITS_UI_ERR_FULL when the stack depth is exceeded. */
int tiku_kits_ui_screen_stack_push(tiku_kits_ui_screen_stack_t *st,
                                    tiku_kits_ui_window_t *win);

/** Pop the topmost window. Returns 1 if a window was popped, 0
 *  when the stack was already at one entry (root) or empty. */
int tiku_kits_ui_screen_stack_pop(tiku_kits_ui_screen_stack_t *st);

tiku_kits_ui_window_t *tiku_kits_ui_screen_stack_top(
    const tiku_kits_ui_screen_stack_t *st);

void tiku_kits_ui_screen_stack_render(
    const tiku_kits_ui_screen_stack_t *st,
    const tiku_kits_gfx_surface_t *s);

/**
 * @brief Dispatch an event to the active screen.
 *
 * - BACK pops the stack. If the stack is at the root, the
 *   optional on_back_at_root callback fires (or the event is
 *   silently dropped if no callback is registered).
 * - All other events are forwarded to the top window's
 *   handle_event via tiku_kits_ui_window_event.
 */
void tiku_kits_ui_screen_stack_event(tiku_kits_ui_screen_stack_t *st,
                                      tiku_kits_ui_event_t evt);

#endif /* TIKU_KITS_UI_SCREEN_H_ */
