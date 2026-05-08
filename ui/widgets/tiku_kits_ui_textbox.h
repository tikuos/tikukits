/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_textbox.h - Single-line text editor
 *
 * Caller-allocated char buffer plus a cursor index. The textbox
 * mutates the buffer in place; the application sees the latest
 * contents at any time and can wire @p on_submit to receive an
 * "enter" callback when the user activates the widget.
 *
 * Input is fed through the existing event vocabulary:
 *
 *   TIKU_KITS_UI_EVT_NUMERIC_*   inserts the digit at the cursor
 *                                 (consumed only when @p kind
 *                                  allows it).
 *   TIKU_KITS_UI_EVT_BACK         deletes the character to the
 *                                 left of the cursor; consumed
 *                                 only when there is something
 *                                 to delete (so a screen-stack
 *                                 BACK still works on an empty
 *                                 textbox).
 *   TIKU_KITS_UI_EVT_FOCUS_LEFT   moves cursor one position left.
 *   TIKU_KITS_UI_EVT_FOCUS_RIGHT  moves cursor one position right.
 *   TIKU_KITS_UI_EVT_ACTIVATE     fires on_submit(buffer).
 *
 * For TEXT mode the application is expected to wire an on-screen
 * keyboard widget (or any other input source) that calls
 * tiku_kits_ui_textbox_insert() / tiku_kits_ui_textbox_backspace()
 * directly.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_TEXTBOX_H_
#define TIKU_KITS_UI_TEXTBOX_H_

#include "../tiku_kits_ui.h"
#include <tikukits/gfx/tiku_kits_gfx_text.h>

typedef enum {
    TIKU_KITS_UI_TEXTBOX_NUMERIC = 0,
    TIKU_KITS_UI_TEXTBOX_TEXT    = 1,
} tiku_kits_ui_textbox_kind_t;

typedef void (*tiku_kits_ui_textbox_cb_t)(const char *text, void *user_data);

typedef struct {
    tiku_kits_ui_widget_t base;
    char    *buffer;       /* caller-allocated, holds the \0 */
    uint8_t  capacity;     /* sizeof(buffer), including \0 slot */
    uint8_t  length;       /* current strlen */
    uint8_t  cursor;       /* 0 .. length */
    tiku_kits_ui_textbox_kind_t kind;
    const tiku_kits_gfx_font_t *font;
    uint8_t  scale;
    tiku_kits_ui_textbox_cb_t on_submit;
} tiku_kits_ui_textbox_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_textbox_ops;

void tiku_kits_ui_textbox_init(tiku_kits_ui_textbox_t *tb,
                                int16_t x, int16_t y,
                                uint16_t w, uint16_t h,
                                char *buffer, uint8_t capacity,
                                tiku_kits_ui_textbox_kind_t kind,
                                const tiku_kits_gfx_font_t *font,
                                uint8_t scale,
                                tiku_kits_ui_textbox_cb_t on_submit,
                                void *user_data);

/** Insert a single character at the cursor (no-op if buffer is full). */
void tiku_kits_ui_textbox_insert(tiku_kits_ui_textbox_t *tb, char c);

/** Delete the character to the left of the cursor (no-op if cursor == 0). */
void tiku_kits_ui_textbox_backspace(tiku_kits_ui_textbox_t *tb);

/** Replace the text content. The buffer must be NUL-terminated and
 *  fit in the textbox capacity. */
void tiku_kits_ui_textbox_set_text(tiku_kits_ui_textbox_t *tb,
                                    const char *text);

#endif /* TIKU_KITS_UI_TEXTBOX_H_ */
