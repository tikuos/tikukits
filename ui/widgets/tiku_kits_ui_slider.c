/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_slider.c - Horizontal slider impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_slider.h"
#include "../tiku_kits_ui_theme.h"

/*---------------------------------------------------------------------------*/
/* HELPERS                                                                   */
/*---------------------------------------------------------------------------*/

static int16_t
clamp(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* x-coordinate (within the widget's width) of the thumb's centre. */
static int16_t
thumb_cx(const tiku_kits_ui_slider_t *sl)
{
    int32_t span = (int32_t)sl->max - (int32_t)sl->min;
    int32_t pos;
    int16_t margin = 2;
    int32_t track_w = (int32_t)sl->base.w - 2 * margin;
    if (track_w < 1) track_w = 1;
    if (span <= 0) {
        pos = 0;
    } else {
        pos = ((int32_t)(sl->value - sl->min) * track_w) / span;
    }
    return (int16_t)(sl->base.x + margin + pos);
}

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static void
slider_render(const tiku_kits_ui_widget_t *base,
               const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_slider_t *sl = (const tiku_kits_ui_slider_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    const int16_t  x = base->x;
    const int16_t  y = base->y;
    const uint16_t w = base->w;
    const uint16_t h = base->h;
    int16_t        track_y = (int16_t)(y + h / 2 - 1);
    int16_t        thumb_y = (int16_t)(y + 1);
    uint16_t       thumb_w = 6;
    uint16_t       thumb_h = (uint16_t)(h - 2);
    int16_t        cx;

    /* Track: 2-px tall horizontal bar across the widget. */
    tiku_kits_gfx_fill_rect(s, x, track_y, w, 2u, t->color_muted);

    /* Filled portion to the left of the thumb. */
    cx = thumb_cx(sl);
    if (cx > x) {
        tiku_kits_gfx_fill_rect(s,
            x, track_y, (uint16_t)(cx - x), 2u, t->color_accent);
    }

    /* Thumb: square centred on cx. */
    {
        int16_t tx = (int16_t)(cx - (int16_t)thumb_w / 2);
        uint8_t color = base->focused ? t->color_focus : t->color_fg;
        tiku_kits_gfx_fill_rect(s, tx, thumb_y, thumb_w, thumb_h, color);
    }
}

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

static int
slider_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_slider_t *sl = (tiku_kits_ui_slider_t *)base;
    int16_t prev = sl->value;
    int16_t step = (sl->step > 0) ? sl->step : 1;

    switch (evt) {
    case TIKU_KITS_UI_EVT_INC:
        sl->value = clamp((int16_t)(sl->value + step), sl->min, sl->max);
        break;
    case TIKU_KITS_UI_EVT_DEC:
        sl->value = clamp((int16_t)(sl->value - step), sl->min, sl->max);
        break;
    case TIKU_KITS_UI_EVT_ACTIVATE:
        /* Wrap at max so a single-button input can cycle. */
        if ((int32_t)sl->value + step > (int32_t)sl->max) {
            sl->value = sl->min;
        } else {
            sl->value = (int16_t)(sl->value + step);
        }
        break;
    default:
        return 0;
    }

    if (sl->value != prev && sl->on_change != NULL) {
        sl->on_change(sl->value, base->user_data);
    }
    return 1;
}

static int
slider_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_slider_ops = {
    .render        = slider_render,
    .handle_event  = slider_handle_event,
    .is_focusable  = slider_is_focusable,
};

/*---------------------------------------------------------------------------*/
/* INIT + STATE                                                              */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_slider_init(tiku_kits_ui_slider_t *sl,
                          int16_t x, int16_t y,
                          uint16_t w, uint16_t h,
                          int16_t min, int16_t max,
                          int16_t step, int16_t value,
                          tiku_kits_ui_slider_cb_t on_change,
                          void *user_data)
{
    if (sl == NULL) return;
    if (max < min) { int16_t t = min; min = max; max = t; }
    sl->base.ops      = &tiku_kits_ui_slider_ops;
    sl->base.x        = x;
    sl->base.y        = y;
    sl->base.w        = w;
    sl->base.h        = h;
    sl->base.visible  = 1;
    sl->base.focused  = 0;
    sl->base.dirty  = 0;
    sl->base.user_data = user_data;
    sl->min           = min;
    sl->max           = max;
    sl->step          = (step > 0) ? step : 1;
    sl->value         = clamp(value, min, max);
    sl->on_change     = on_change;
}

void
tiku_kits_ui_slider_set_value(tiku_kits_ui_slider_t *sl, int16_t value)
{
    if (sl == NULL) return;
    sl->value = clamp(value, sl->min, sl->max);
}
