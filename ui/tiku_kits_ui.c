/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui.c - Common UI helpers
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui.h"

/*---------------------------------------------------------------------------*/
/* SPATIAL FOCUS WALK                                                        */
/*---------------------------------------------------------------------------*/

static int
abs_int32(int32_t v) { return v < 0 ? (int)-v : (int)v; }

static void
widget_centre(const tiku_kits_ui_widget_t *w, int32_t *cx, int32_t *cy)
{
    *cx = (int32_t)w->x + (int32_t)(w->w / 2u);
    *cy = (int32_t)w->y + (int32_t)(w->h / 2u);
}

tiku_kits_ui_widget_t *
tiku_kits_ui_focus_walk(tiku_kits_ui_widget_t * const *children,
                         uint8_t n_children,
                         const tiku_kits_ui_widget_t *current,
                         int dx, int dy)
{
    tiku_kits_ui_widget_t *best = NULL;
    int32_t best_score = INT32_MAX;
    int32_t cur_cx = 0, cur_cy = 0;
    uint8_t i;

    if (children == NULL || n_children == 0u) return NULL;
    if (dx == 0 && dy == 0) return NULL;

    if (current != NULL) {
        widget_centre(current, &cur_cx, &cur_cy);
    } else {
        /* No current focus: start from the opposite corner so the
         * first focus naturally lands on the side closest to the
         * direction. */
        cur_cx = (dx >= 0) ? -1 : 0x7FFF;
        cur_cy = (dy >= 0) ? -1 : 0x7FFF;
    }

    for (i = 0; i < n_children; i++) {
        tiku_kits_ui_widget_t *c = children[i];
        int32_t cx, cy, ddx, ddy;
        int32_t score, primary, perp;

        if (c == NULL || c == current) continue;
        if (!tiku_kits_ui_widget_is_focusable(c)) continue;

        widget_centre(c, &cx, &cy);
        ddx = cx - cur_cx;
        ddy = cy - cur_cy;

        /* Reject candidates not in the requested direction. */
        if (dx ==  1 && ddx <= 0) continue;
        if (dx == -1 && ddx >= 0) continue;
        if (dy ==  1 && ddy <= 0) continue;
        if (dy == -1 && ddy >= 0) continue;

        if (dx != 0) {
            primary = abs_int32(ddx);
            perp    = abs_int32(ddy);
        } else {
            primary = abs_int32(ddy);
            perp    = abs_int32(ddx);
        }
        score = primary + 4 * perp;

        if (score < best_score) {
            best_score = score;
            best = c;
        }
    }

    return best;
}
