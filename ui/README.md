# tikukits/ui -- Retained-mode UI library

A small, retained-mode user interface kit built on top of
`tikukits/gfx`. Designed for monochrome and BWR e-paper displays,
where IMGUI-style "redraw every frame" is impossible (refresh
takes 10-25 s) but a static widget tree fits naturally.

Visual style is BeOS-inspired: hairline frames, red accent tabs,
outset bevels on buttons, focus indicated by a colour swap on
the widget border.

## Layout

```
tikukits/ui/
├── tiku_kits_ui.h            Base widget type, ops vtable,
│                             event types, common helpers
├── tiku_kits_ui.c            (currently empty -- placeholder)
└── widgets/
    ├── tiku_kits_ui_window.{h,c}    Container + BeOS title tab
    ├── tiku_kits_ui_label.{h,c}     Static text
    ├── tiku_kits_ui_button.{h,c}    Clickable button with bevel
    └── tiku_kits_ui_icon.{h,c}      1bpp bitmap, optional click
```

No dependency on `tikukits/epaper`. The UI talks to a
`tiku_kits_gfx_surface_t`; the application wires that surface to
whichever display kit is in use (today: e-paper; tomorrow: LCD,
OLED, anything).

## Architecture

```
application
    |
    v   (build widget tree, feed events)
+----------------------+
| tiku_kits_ui (this)  |   widget base + focus + dispatch
| widgets/             |   window / label / button / icon
+----------------------+
    |
    v   (renders via tiku_kits_gfx_surface_t)
tikukits/gfx
    |
    v
display kit (tikukits/epaper) -> physical panel
```

Three things the kit owns:
- **Widget tree walking** -- the window's `render` recurses into
  children, with each child drawing into a subsurface so it sees
  local `(0, 0)`-based coordinates.
- **Focus management** -- which child is currently focused, plus
  next/prev navigation that wraps and skips non-focusable widgets.
- **Event dispatch** -- ACTIVATE goes to the focused widget's
  callback.

Three things it deliberately does NOT own:
- **Refresh / display I/O** -- the application calls the display
  kit's `refresh()` itself, on its own schedule.
- **Memory allocation** -- every widget is a static struct the
  caller owns. No malloc.
- **Input source** -- the application reads physical buttons (or
  a serial command, or whatever) and feeds events in.

## Quick start

```c
#include <tikukits/ui/tiku_kits_ui.h>
#include <tikukits/ui/widgets/tiku_kits_ui_window.h>
#include <tikukits/ui/widgets/tiku_kits_ui_label.h>
#include <tikukits/ui/widgets/tiku_kits_ui_button.h>

static tiku_kits_ui_window_t win;
static tiku_kits_ui_label_t  hdr;
static tiku_kits_ui_button_t ok_btn;

static void on_ok(void *user_data) { /* ... */ }

void build_ui(void) {
    tiku_kits_ui_window_init(&win, 4, 4, 232, 408,
                              "Settings",
                              &tiku_kits_gfx_font_5x7, 2);

    tiku_kits_ui_label_init(&hdr, 0, 16, 224, 24,
                             "Press OK to continue",
                             &tiku_kits_gfx_font_5x7,
                             TIKU_KITS_GFX_BLACK, 2,
                             TIKU_KITS_GFX_ALIGN_CENTER);

    tiku_kits_ui_button_init(&ok_btn, 70, 60, 90, 40,
                              "OK", &tiku_kits_gfx_font_5x7, 3,
                              on_ok, NULL);

    tiku_kits_ui_window_add(&win, &hdr.base);
    tiku_kits_ui_window_add(&win, &ok_btn.base);
}

void render_and_refresh(tiku_kits_gfx_surface_t *surface,
                         tiku_kits_epaper_t *epd) {
    tiku_kits_epaper_clear(epd, TIKU_KITS_EPAPER_WHITE);
    tiku_kits_ui_window_render(&win, surface);
    tiku_kits_epaper_refresh(epd);
    tiku_kits_epaper_sleep(epd);
}

void on_button_press(void) {
    tiku_kits_ui_window_event(&win, TIKU_KITS_UI_EVT_FOCUS_NEXT);
    /* re-render + refresh after state changes */
}
```

## Widgets

### `tiku_kits_ui_window_t`

BeOS-styled container. Renders:
- a red rounded title tab projecting above the frame
- a 2-px black hairline border around the content area
- all visible children, each into a subsurface clipped to the
  content area

```c
tiku_kits_ui_window_init(win, x, y, w, h, title, font, scale);
tiku_kits_ui_window_add(win, child_widget);
tiku_kits_ui_window_render(win, surface);
tiku_kits_ui_window_event(win, TIKU_KITS_UI_EVT_*);
```

Children are added via `tiku_kits_ui_window_add` up to
`TIKU_KITS_UI_WINDOW_MAX_CHILDREN` (default 16, override at
compile time).

### `tiku_kits_ui_label_t`

Static text inside a rectangle. Not focusable. Honours
`TIKU_KITS_GFX_ALIGN_LEFT` / `CENTER` / `RIGHT` via the gfx kit's
`draw_string_in_rect`.

```c
tiku_kits_ui_label_init(lbl, x, y, w, h, text, font, color,
                         scale, align);
```

### `tiku_kits_ui_button_t`

Clickable button. Visual: 1-px outer border (BLACK normally, RED
when focused) + outset bevel + centered text. On `ACTIVATE` event
when focused, invokes `on_click(user_data)`.

```c
tiku_kits_ui_button_init(btn, x, y, w, h, text, font, scale,
                          on_click_cb, user_data);
```

### `tiku_kits_ui_icon_t`

1-bit-per-pixel bitmap, optionally clickable, optionally framed.
If `on_click` is NULL the icon is purely decorative (not
focusable). Bitmap format matches `tiku_kits_gfx_bitmap`:
row-major, MSB = leftmost pixel.

```c
tiku_kits_ui_icon_init(ico, x, y, w, h,
                        bitmap, bw, bh,
                        color, bordered,
                        on_click_cb, user_data);
```

## Events

```c
TIKU_KITS_UI_EVT_FOCUS_NEXT   /* wrap to next focusable child */
TIKU_KITS_UI_EVT_FOCUS_PREV   /* wrap to previous focusable child */
TIKU_KITS_UI_EVT_ACTIVATE     /* fire focused widget's callback */
```

Wire physical buttons (LaunchPad S1/S2, GPIOs on a custom board,
etc.) to whichever events match your input convention. The demo
example uses S1=ACTIVATE, S2=FOCUS_NEXT (single-direction wrap).

## Refresh model

The UI kit does NOT call refresh. After feeding an event or
mutating widget state (e.g. updating a label's text), the
application is responsible for:

1. Clearing the framebuffer (`tiku_kits_epaper_clear`).
2. Calling `tiku_kits_ui_window_render(win, surface)` to repaint.
3. Calling the display kit's refresh (`tiku_kits_epaper_refresh`).

This separation keeps the UI display-agnostic and lets the app
batch multiple state changes into a single refresh -- important
on EPDs where each refresh costs 10-25 s of wall-clock time.

## Memory cost

| Module                          | Approx FRAM cost |
|---------------------------------|------------------|
| Base + window                   | ~1 KB code       |
| + label                         | +~200 B code     |
| + button                        | +~400 B code     |
| + icon                          | +~300 B code     |

Per-widget runtime cost: ~16-32 bytes per widget instance,
plus the application's own glue. A typical screen of 1 window +
6 widgets = under 200 bytes RAM, all caller-allocated.

## Adding a new widget type

1. Define a struct that embeds `tiku_kits_ui_widget_t base` as
   its first member.
2. Implement at minimum `render(widget, surface)`.
3. Optionally implement `handle_event(widget, evt)` and
   `is_focusable(widget)` if the widget is interactive.
4. Export an `extern const tiku_kits_ui_widget_ops_t my_ops` and
   an `init` function setting the base fields + your own.
5. Drop `.h` and `.c` under `widgets/`. The Makefile globs the
   directory, so adding the file is enough.

## Reference example

`examples/28_ui_demo/ui_demo.c` -- BeOS-styled window with title
tab, a status label, two counter buttons, and a heart icon, all
driven by LaunchPad buttons S1 (activate) and S2 (focus-next).

## Out of scope (today)

- Auto-layout (flexbox / grid). Absolute positioning only.
- Dynamic widgets (no malloc). Apps switch screens by swapping
  the active top-level window.
- Modal dialogs. Implementable as a second window the app
  renders on top.
- Scroll views, list / menu widgets, virtual keyboard. Add when
  a concrete app needs them.
- Animation, transitions. EPD refresh rate kills the use case.
- Dirty-rect partial refresh. Possible later via a flag on each
  widget; today every render redraws the whole tree.
