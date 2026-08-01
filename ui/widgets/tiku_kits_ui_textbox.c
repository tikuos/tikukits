/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_textbox.c - Single-line text editor impl
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Cursor is rendered as a 1-px underline at the column where the
 * next character would be inserted. The text area scrolls
 * horizontally only when length exceeds the visible width -- in
 * that case the displayed substring shifts so the cursor stays
 * inside the rect.
 */

#include "tiku_kits_ui_textbox.h"
#include "../tiku_kits_ui_theme.h"
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>

/*---------------------------------------------------------------------------*/
/* HELPERS                                                                   */
/*---------------------------------------------------------------------------*/

static uint16_t
prefix_width(const tiku_kits_ui_textbox_t *tb, uint8_t n)
{
    char tmp;
    uint16_t w;
    if (n >= tb->length) {
        return tiku_kits_gfx_text_width(tb->buffer, tb->font, tb->scale);
    }
    tmp = tb->buffer[n];
    tb->buffer[n] = '\0';
    w = tiku_kits_gfx_text_width(tb->buffer, tb->font, tb->scale);
    tb->buffer[n] = tmp;
    return w;
}

/*---------------------------------------------------------------------------*/
/* RENDER                                                                    */
/*---------------------------------------------------------------------------*/

static void
textbox_render(const tiku_kits_ui_widget_t *base,
                const tiku_kits_gfx_surface_t *s)
{
    const tiku_kits_ui_textbox_t *tb = (const tiku_kits_ui_textbox_t *)base;
    const tiku_kits_ui_theme_t *t = tiku_kits_ui_theme_current();
    uint8_t  edge = base->focused ? t->color_focus : t->color_fg;
    int16_t  x = base->x;
    int16_t  y = base->y;
    uint16_t w = base->w;
    uint16_t h = base->h;
    int16_t  text_x;
    int16_t  text_y;
    uint16_t glyph_h;

    /* Border. */
    tiku_kits_gfx_rect(s, x, y, w, h, edge);

    glyph_h = (uint16_t)(tb->font->height * tb->scale);
    text_x  = (int16_t)(x + 4);
    text_y  = (int16_t)(y + (int16_t)((h > glyph_h) ? (h - glyph_h) / 2 : 0));

    /* Render the text. (No horizontal clipping yet -- callers
     * should size the textbox wide enough for the maximum content.) */
    if (tb->buffer != NULL && tb->length > 0u) {
        tiku_kits_gfx_draw_string(s, text_x, text_y,
            tb->buffer, tb->font, t->color_fg, tb->scale);
    }

    /* Cursor: only visible when focused. Drawn as a solid 2-px-tall
     * underline beneath the next-insert position. */
    if (base->focused) {
        uint16_t cx = (uint16_t)(text_x + (int16_t)prefix_width(tb,
                                                                  tb->cursor));
        int16_t cy = (int16_t)(text_y + (int16_t)glyph_h);
        tiku_kits_gfx_hline(s, (int16_t)cx, cy,
            (uint16_t)(tb->font->width * tb->scale), t->color_fg);
    }
}

/*---------------------------------------------------------------------------*/
/* MUTATORS                                                                  */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_textbox_insert(tiku_kits_ui_textbox_t *tb, char c)
{
    uint8_t i;
    if (tb == NULL || tb->buffer == NULL) return;
    if (tb->length + 1u >= tb->capacity) return;   /* leaves room for \0 */

    /* Shift right from end down to cursor. */
    for (i = tb->length; i > tb->cursor; i--) {
        tb->buffer[i] = tb->buffer[i - 1];
    }
    tb->buffer[tb->cursor] = c;
    tb->length++;
    tb->cursor++;
    tb->buffer[tb->length] = '\0';
}

void
tiku_kits_ui_textbox_backspace(tiku_kits_ui_textbox_t *tb)
{
    uint8_t i;
    if (tb == NULL || tb->buffer == NULL || tb->cursor == 0u) return;
    for (i = tb->cursor; i < tb->length; i++) {
        tb->buffer[i - 1] = tb->buffer[i];
    }
    tb->cursor--;
    tb->length--;
    tb->buffer[tb->length] = '\0';
}

void
tiku_kits_ui_textbox_set_text(tiku_kits_ui_textbox_t *tb, const char *text)
{
    uint8_t i;
    if (tb == NULL || tb->buffer == NULL) return;
    if (text == NULL) {
        tb->buffer[0] = '\0';
        tb->length = 0;
        tb->cursor = 0;
        return;
    }
    for (i = 0; i + 1u < tb->capacity && text[i] != '\0'; i++) {
        tb->buffer[i] = text[i];
    }
    tb->buffer[i] = '\0';
    tb->length = i;
    tb->cursor = i;
}

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

static int
textbox_handle_event(tiku_kits_ui_widget_t *base, tiku_kits_ui_event_t evt)
{
    tiku_kits_ui_textbox_t *tb = (tiku_kits_ui_textbox_t *)base;

    /* Numeric events: only consumed in NUMERIC mode. */
    if (evt >= TIKU_KITS_UI_EVT_NUMERIC_0 &&
        evt <= TIKU_KITS_UI_EVT_NUMERIC_9) {
        if (tb->kind != TIKU_KITS_UI_TEXTBOX_NUMERIC) return 0;
        tiku_kits_ui_textbox_insert(tb,
            (char)('0' + (evt - TIKU_KITS_UI_EVT_NUMERIC_0)));
        return 1;
    }

    switch (evt) {
    case TIKU_KITS_UI_EVT_BACK:
        if (tb->cursor == 0u) return 0;     /* let parent handle */
        tiku_kits_ui_textbox_backspace(tb);
        return 1;
    case TIKU_KITS_UI_EVT_FOCUS_LEFT:
        if (tb->cursor == 0u) return 0;
        tb->cursor--;
        return 1;
    case TIKU_KITS_UI_EVT_FOCUS_RIGHT:
        if (tb->cursor >= tb->length) return 0;
        tb->cursor++;
        return 1;
    case TIKU_KITS_UI_EVT_ACTIVATE:
        if (tb->on_submit != NULL) {
            tb->on_submit(tb->buffer, base->user_data);
        }
        return 1;
    default:
        return 0;
    }
}

static int
textbox_is_focusable(const tiku_kits_ui_widget_t *base)
{
    (void)base;
    return 1;
}

const tiku_kits_ui_widget_ops_t tiku_kits_ui_textbox_ops = {
    .render         = textbox_render,
    .handle_event   = textbox_handle_event,
    .is_focusable   = textbox_is_focusable,
    .intrinsic_size = NULL,
};

/*---------------------------------------------------------------------------*/
/* INIT                                                                      */
/*---------------------------------------------------------------------------*/

void
tiku_kits_ui_textbox_init(tiku_kits_ui_textbox_t *tb,
                           int16_t x, int16_t y,
                           uint16_t w, uint16_t h,
                           char *buffer, uint8_t capacity,
                           tiku_kits_ui_textbox_kind_t kind,
                           const tiku_kits_gfx_font_t *font,
                           uint8_t scale,
                           tiku_kits_ui_textbox_cb_t on_submit,
                           void *user_data)
{
    if (tb == NULL) return;
    tb->base.ops      = &tiku_kits_ui_textbox_ops;
    tb->base.x        = x;
    tb->base.y        = y;
    tb->base.w        = w;
    tb->base.h        = h;
    tb->base.visible  = 1;
    tb->base.focused  = 0;
    tb->base.dirty  = 0;
    tb->base.user_data = user_data;
    tb->buffer    = buffer;
    tb->capacity  = capacity;
    tb->length    = 0;
    tb->cursor    = 0;
    tb->kind      = kind;
    tb->font      = (font != NULL) ? font : &tiku_kits_gfx_font_5x7;
    tb->scale     = (scale > 0) ? scale : 1;
    tb->on_submit = on_submit;
    if (buffer != NULL && capacity > 0) {
        buffer[0] = '\0';
    }
}
