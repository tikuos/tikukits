/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_screen.c - Screen stack impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_screen.h"

void
tiku_kits_ui_screen_stack_init(tiku_kits_ui_screen_stack_t *st)
{
    if (st == NULL) return;
    st->depth = 0;
    st->on_back_at_root = NULL;
    st->user_data = NULL;
}

void
tiku_kits_ui_screen_stack_set_back_cb(tiku_kits_ui_screen_stack_t *st,
                                       tiku_kits_ui_screen_back_cb_t cb,
                                       void *user_data)
{
    if (st == NULL) return;
    st->on_back_at_root = cb;
    st->user_data = user_data;
}

int
tiku_kits_ui_screen_stack_push(tiku_kits_ui_screen_stack_t *st,
                                tiku_kits_ui_window_t *win)
{
    if (st == NULL || win == NULL) return TIKU_KITS_UI_ERR_PARAM;
    if (st->depth >= TIKU_KITS_UI_SCREEN_STACK_DEPTH) {
        return TIKU_KITS_UI_ERR_FULL;
    }
    st->windows[st->depth++] = win;
    return TIKU_KITS_UI_OK;
}

int
tiku_kits_ui_screen_stack_pop(tiku_kits_ui_screen_stack_t *st)
{
    if (st == NULL || st->depth <= 1u) return 0;
    st->depth--;
    return 1;
}

tiku_kits_ui_window_t *
tiku_kits_ui_screen_stack_top(const tiku_kits_ui_screen_stack_t *st)
{
    if (st == NULL || st->depth == 0u) return NULL;
    return st->windows[st->depth - 1u];
}

void
tiku_kits_ui_screen_stack_render(const tiku_kits_ui_screen_stack_t *st,
                                  const tiku_kits_gfx_surface_t *s)
{
    tiku_kits_ui_window_t *top;
    if (st == NULL || s == NULL) return;
    top = tiku_kits_ui_screen_stack_top(st);
    if (top != NULL) tiku_kits_ui_window_render(top, s);
}

void
tiku_kits_ui_screen_stack_event(tiku_kits_ui_screen_stack_t *st,
                                 tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_window_t *top;
    if (st == NULL) return;

    if (evt == TIKU_KITS_UI_EVT_BACK) {
        if (st->depth > 1u) {
            st->depth--;
            return;
        }
        /* Root screen -- defer to callback, if any. */
        if (st->on_back_at_root != NULL) {
            st->on_back_at_root(st->user_data);
        }
        return;
    }

    top = tiku_kits_ui_screen_stack_top(st);
    if (top != NULL) tiku_kits_ui_window_event(top, evt);
}
