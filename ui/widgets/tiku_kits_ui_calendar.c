/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_calendar.c - Calendar / date picker impl
 *
 * Rendering:
 *   row 0: month + year banner ("Mar 2026")
 *   row 1: weekday header letters ("S M T W T F S")
 *   rows 2..7: up to 6 weeks of day numbers
 *
 * The selected day inverts colours (bg-on-fg square) for visibility.
 *
 * Date math:
 *   - Leap-year detection per the proleptic Gregorian rule.
 *   - Day-of-week-of-month-1st via Zeller's congruence (returns
 *     0 = Sunday in the calendar's column convention).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_ui_calendar.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/* DATE HELPERS                                                              */
/*---------------------------------------------------------------------------*/

static int
is_leap(uint16_t y)
{
    return ((y % 4u) == 0u && (y % 100u) != 0u) || ((y % 400u) == 0u);
}

static uint8_t
days_in_month(uint16_t y, uint8_t m)
{
    static const uint8_t dim[] =
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1u || m > 12u) return 30u;
    if (m == 2u) return is_leap(y) ? 29u : 28u;
    return dim[m - 1u];
}

/* Zeller's congruence: returns 0..6 with 0 = Sunday. */
static uint8_t
dow_first_of_month(uint16_t y, uint8_t m)
{
    int q = 1;
    int M = m;
    int Y = y;
    int K, J, h;
    if (M < 3) { M += 12; Y -= 1; }
    K = Y % 100;
    J = Y / 100;
    h = (q + (13 * (M + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
    /* h: 0 = Saturday. Convert to 0 = Sunday. */
    return (uint8_t)((h + 6) % 7);
}

static const char *
month_short_name(uint8_t m)
{
    static const char names[12][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    if (m < 1u || m > 12u) return "???";
    return names[m - 1u];
}

static void
add_days(uint16_t *y, uint8_t *m, uint8_t *d, int delta)
{
    int dd = (int)*d + delta;
    while (dd < 1) {
        if (*m == 1u) { *m = 12u; *y = (uint16_t)(*y - 1u); }
        else          { *m = (uint8_t)(*m - 1u); }
        dd += days_in_month(*y, *m);
    }
    while (dd > (int)days_in_month(*y, *m)) {
        dd -= (int)days_in_month(*y, *m);
        if (*m == 12u) { *m = 1u; *y = (uint16_t)(*y + 1u); }
        else           { *m = (uint8_t)(*m + 1u); }
    }
    *d = (uint8_t)dd;
}

static void
shift_month(uint16_t *y, uint8_t *m, uint8_t *d, int delta)
{
    int mm = (int)*m + delta;
    while (mm < 1)  { mm += 12; *y = (uint16_t)(*y - 1u); }
    while (mm > 12) { mm -= 12; *y = (uint16_t)(*y + 1u); }
    *m = (uint8_t)mm;
    {
        uint8_t dim = days_in_month(*y, *m);
        if (*d > dim) *d = dim;
    }
}

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static void
calendar_render(const tiku_kits_ui_widget_t *base,
                 const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_calendar_t *cal =
        (const tiku_kits_ui_calendar_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    char     banner[16];
    uint16_t cell_w, cell_h;
    int16_t  banner_h, header_h, grid_y;
    uint8_t  glyph_h;
    uint8_t  first_dow;
    uint8_t  ndays;
    uint8_t  i, day;
    uint8_t  border_color;

    border_color = base->focused ? t->color_focus : t->color_fg;
    tiku_kits_gfx_rect(s, base->x, base->y, base->w, base->h, border_color);

    glyph_h  = (uint8_t)(cal->font->height * cal->scale);
    banner_h = (int16_t)(glyph_h + 4);
    header_h = (int16_t)(glyph_h + 4);
    grid_y   = (int16_t)(base->y + banner_h + header_h);
    cell_w   = (uint16_t)((base->w - 2u) / 7u);
    cell_h   = (uint16_t)((base->h - banner_h - header_h - 2) / 6u);
    if (cell_h == 0u) cell_h = 1u;

    /* --- Banner: "Mar 2026" centred --- */
    snprintf(banner, sizeof(banner), "%s %u",
              month_short_name(cal->month), (unsigned)cal->year);
    {
        tiku_kits_gfx_rect_t r = { base->x, base->y, base->w, (uint16_t)banner_h };
        tiku_kits_gfx_draw_string_in_rect(s, &r, banner,
            cal->font, t->color_fg, cal->scale,
            TIKU_KITS_GFX_ALIGN_CENTER);
    }

    /* --- Weekday letters --- */
    {
        static const char *labels[7] = { "S","M","T","W","T","F","S" };
        for (i = 0; i < 7; i++) {
            tiku_kits_gfx_rect_t r = {
                (int16_t)(base->x + 1 + i * cell_w),
                (int16_t)(base->y + banner_h),
                cell_w,
                (uint16_t)header_h
            };
            tiku_kits_gfx_draw_string_in_rect(s, &r, labels[i],
                cal->font, t->color_muted, cal->scale,
                TIKU_KITS_GFX_ALIGN_CENTER);
        }
    }

    /* --- Day grid --- */
    first_dow = dow_first_of_month(cal->year, cal->month);
    ndays     = days_in_month(cal->year, cal->month);

    for (day = 1; day <= ndays; day++) {
        uint8_t cell_idx = (uint8_t)(first_dow + day - 1u);
        uint8_t row = (uint8_t)(cell_idx / 7u);
        uint8_t col = (uint8_t)(cell_idx % 7u);
        int16_t cx  = (int16_t)(base->x + 1 + col * cell_w);
        int16_t cy  = (int16_t)(grid_y + row * cell_h);
        char    txt[4];

        snprintf(txt, sizeof(txt), "%u", (unsigned)day);

        if (day == cal->selected_day) {
            /* Inverse highlight square. */
            tiku_kits_gfx_fill_rect(s,
                cx, cy, cell_w, cell_h, t->color_fg);
            {
                tiku_kits_gfx_rect_t r = { cx, cy, cell_w, cell_h };
                tiku_kits_gfx_draw_string_in_box(s, &r, txt,
                    cal->font, t->color_bg, cal->scale,
                    TIKU_KITS_GFX_ALIGN_CENTER,
                    TIKU_KITS_GFX_VALIGN_MIDDLE);
            }
        } else {
            tiku_kits_gfx_rect_t r = { cx, cy, cell_w, cell_h };
            tiku_kits_gfx_draw_string_in_box(s, &r, txt,
                cal->font, t->color_fg, cal->scale,
                TIKU_KITS_GFX_ALIGN_CENTER,
                TIKU_KITS_GFX_VALIGN_MIDDLE);
        }
    }
}

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

static int
calendar_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_calendar_t *cal = (tiku_kits_ui_calendar_t *)base;
    int delta_days = 0;
    int delta_months = 0;

    switch (evt) {
    case TIKU_KITS_UI_EVT_FOCUS_LEFT:
    case TIKU_KITS_UI_EVT_DEC:        delta_days = -1; break;
    case TIKU_KITS_UI_EVT_FOCUS_RIGHT:
    case TIKU_KITS_UI_EVT_INC:        delta_days =  1; break;
    case TIKU_KITS_UI_EVT_FOCUS_UP:   delta_days = -7; break;
    case TIKU_KITS_UI_EVT_FOCUS_DOWN: delta_days =  7; break;
    case TIKU_KITS_UI_EVT_BACK:       delta_months = -1; break;
    case TIKU_KITS_UI_EVT_MENU:       delta_months =  1; break;
    case TIKU_KITS_UI_EVT_ACTIVATE:
        if (cal->on_select != NULL) {
            cal->on_select(cal->year, cal->month, cal->selected_day,
                            base->user_data);
        }
        return 1;
    default:
        return 0;
    }

    if (delta_days != 0) {
        add_days(&cal->year, &cal->month, &cal->selected_day, delta_days);
        return 1;
    }
    if (delta_months != 0) {
        shift_month(&cal->year, &cal->month, &cal->selected_day,
                     delta_months);
        return 1;
    }
    return 0;
}

static int
calendar_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_calendar_ops = {
    .render         = calendar_render,
    .handle_event   = calendar_handle_event,
    .is_focusable   = calendar_is_focusable,
    .intrinsic_size = NULL,
};

/*---------------------------------------------------------------------------*/
/* INIT + STATE                                                              */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_calendar_init(tiku_kits_ui_calendar_t *cal,
                            int16_t x, int16_t y,
                            uint16_t w, uint16_t h,
                            uint16_t year, uint8_t month, uint8_t day,
                            const tiku_kits_gfx_font_t *font,
                            uint8_t scale,
                            tiku_kits_ui_calendar_cb_t on_select,
                            void *user_data)
{
    if (cal == NULL) return;
    cal->base.ops      = &tiku_kits_ui_calendar_ops;
    cal->base.x        = x;
    cal->base.y        = y;
    cal->base.w        = w;
    cal->base.h        = h;
    cal->base.visible  = 1;
    cal->base.focused  = 0;
    cal->base.dirty    = 0;
    cal->base.user_data = user_data;
    cal->year      = year;
    cal->month     = (month >= 1u && month <= 12u) ? month : 1u;
    cal->selected_day = (day >= 1u && day <= days_in_month(cal->year, cal->month))
                          ? day : 1u;
    cal->font  = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    cal->scale = (scale > 0u) ? scale : 1u;
    cal->on_select = on_select;
}

void
tiku_kits_ui_calendar_set_date(tiku_kits_ui_calendar_t *cal,
                                 uint16_t year, uint8_t month, uint8_t day)
{
    if (cal == NULL) return;
    cal->year  = year;
    cal->month = (month >= 1u && month <= 12u) ? month : 1u;
    cal->selected_day = (day >= 1u && day <= days_in_month(cal->year, cal->month))
                          ? day : 1u;
}
