# tikukits/gfx -- 2D graphics + text rendering

Display-agnostic 2D graphics library for TikuOS. Provides shape
primitives, bitmap-font text rendering, coordinate types, and
subsurface composition on any framebuffer-backed display.

Pairs with `tikukits/epaper` for e-paper displays today; designed
to compose with future LCD / OLED kits the same way, and to serve
as the foundation for a future UI kit (`tikukits/ui`).

## Layout

```
tikukits/gfx/
├── tiku_kits_gfx.h         Surface, coordinate types, drawing primitives
├── tiku_kits_gfx.c
├── tiku_kits_gfx_text.h    Bitmap-font text rendering + word-wrap
├── tiku_kits_gfx_text.c
└── fonts/
    └── tiku_kits_gfx_font_5x7.{h,c}   Built-in 5x7 ASCII font (~480 B)
```

No dependency on `tikukits/epaper`. The two kits compose via a
4-line application-level adapter (see "Wiring it to a display").

## Surface concept

The kit's only output requirement is a per-pixel set callback.
Anything that can paint a pixel can be a surface:

```c
typedef struct {
    uint16_t width, height;
    void (*set_pixel)(void *ctx, uint16_t x, uint16_t y, uint8_t color);
    void *ctx;
    int16_t origin_x, origin_y;   /* translation; zero for top-level */
} tiku_kits_gfx_surface_t;
```

Drawing coordinates are `int16_t`, allowing shapes that partially
extend off-screen (clipped against the surface bounds) or that
originate at negative coordinates (used by scrolling subsurfaces).

## Coordinate types

```c
tiku_kits_gfx_point_t   { x, y }                        /* int16 */
tiku_kits_gfx_size_t    { w, h }                        /* uint16 */
tiku_kits_gfx_rect_t    { x, y, w, h }                  /* int16, uint16 */

tiku_kits_gfx_rect_empty(&r)
tiku_kits_gfx_rect_contains(&r, x, y)
tiku_kits_gfx_rect_intersect(&a, &b, &out)
```

## Subsurfaces (clipping + translation)

Create a child surface that maps onto a sub-rectangle of a parent.
Drawing on the child uses local (0, 0)-based coordinates;
translation and clipping are automatic.

```c
tiku_kits_gfx_surface_t parent = { ... };
tiku_kits_gfx_surface_t card;

tiku_kits_gfx_subsurface(&card, &parent,
                          /*x=*/30, /*y=*/100,
                          /*w=*/180, /*h=*/120);

/* Now `card` is a 180x120 surface anchored at (30, 100) on parent.
 * Drawing at card-local (0, 0) lands at parent (30, 100). */
tiku_kits_gfx_fill_round_rect(&card, 0, 0, 180, 120, 8,
                               TIKU_KITS_GFX_BLACK);
```

Subsurfaces nest correctly -- creating a subsurface of a subsurface
accumulates the translation. UI widgets can hand each child widget
its own subsurface and have the child draw at (0, 0) without
worrying about its position on screen.

## Drawing API

### Lines and pixels

```c
tiku_kits_gfx_pixel(s, x, y, color);
tiku_kits_gfx_hline(s, x, y, w, color);
tiku_kits_gfx_vline(s, x, y, h, color);
tiku_kits_gfx_line(s, x0, y0, x1, y1, color);
tiku_kits_gfx_line_thick(s, x0, y0, x1, y1, thickness, color);
```

### Rectangles

```c
tiku_kits_gfx_rect(s, x, y, w, h, color);              /* outline */
tiku_kits_gfx_fill_rect(s, x, y, w, h, color);          /* filled */
tiku_kits_gfx_round_rect(s, x, y, w, h, r, color);      /* rounded outline */
tiku_kits_gfx_fill_round_rect(s, x, y, w, h, r, color); /* rounded filled */
```

### Triangles

```c
tiku_kits_gfx_triangle(s,      x0, y0, x1, y1, x2, y2, color);
tiku_kits_gfx_fill_triangle(s, x0, y0, x1, y1, x2, y2, color);
```

### Circles

```c
tiku_kits_gfx_circle(s,      cx, cy, r, color);
tiku_kits_gfx_fill_circle(s, cx, cy, r, color);
```

### Bitmaps

```c
tiku_kits_gfx_bitmap(s, x, y, bmp, w, h, color);
```

All shape primitives self-clip against the surface's
`(width, height)` -- out-of-range coordinates are silently dropped.

## Text rendering

### Single-line

```c
tiku_kits_gfx_draw_char(s,   x, y, c,     &font, color, scale);
tiku_kits_gfx_draw_string(s, x, y, str,   &font, color, scale);
tiku_kits_gfx_text_width(str, &font, scale);    /* pixels */
tiku_kits_gfx_line_height(&font, scale);        /* pixels */
```

### Aligned single-line in a rectangle

```c
tiku_kits_gfx_rect_t r = { 10, 50, 200, 30 };
tiku_kits_gfx_draw_string_in_rect(s, &r, "Hello", &font5x7,
                                    TIKU_KITS_GFX_BLACK, 2,
                                    TIKU_KITS_GFX_ALIGN_CENTER);
```

Alignment options: `LEFT`, `CENTER`, `RIGHT`.

### Word-wrapped multi-line

```c
tiku_kits_gfx_rect_t card = { 20, 100, 200, 200 };
tiku_kits_gfx_draw_text_wrapped(s, &card,
    "This is a longer message that will be wrapped at word "
    "boundaries to fit inside the rectangle.",
    &font5x7, TIKU_KITS_GFX_BLACK, 2,
    TIKU_KITS_GFX_ALIGN_LEFT);
```

Breaks at whitespace; over-long words break character-wise.
Honours embedded `\n` for forced line breaks. Lines that fall
outside the rect are not drawn.

## Fonts

A font is a `const tiku_kits_gfx_font_t` plus its glyph data:

```c
typedef struct {
    uint8_t width;             /* monospace width / max width */
    uint8_t height;
    uint8_t first;             /* first character code */
    uint8_t last;              /* last character code (inclusive) */
    uint8_t bytes_per_column;  /* ceil(height / 8) */
    const uint8_t *glyphs;
    const uint8_t *widths;     /* per-glyph widths, NULL = monospace */
    uint8_t ascent;            /* metric: pixels above baseline */
    uint8_t descent;           /* metric: pixels below baseline */
    uint8_t line_height;       /* metric: vertical advance, 0 -> height+1 */
} tiku_kits_gfx_font_t;
```

Glyph format: column-major, 1 bit per pixel, LSB = top row of each
column. For glyphs taller than 8 pixels, multiple bytes per column
are stacked top-to-bottom (`bytes_per_column = 2` for 9..16 px).

For proportional fonts, supply a per-glyph `widths` array
(`(last - first + 1)` bytes). Glyph storage is still
`width * bytes_per_column` per glyph (some columns are unused);
only the rendering advance shrinks.

To add a new font: drop a new `.h`/`.c` pair under `fonts/`,
exporting `extern const tiku_kits_gfx_font_t my_font`. The Makefile
globs `fonts/*.c` so just adding the file is enough.

## Wiring it to a display (epaper example)

```c
#include <tikukits/gfx/tiku_kits_gfx.h>
#include <tikukits/gfx/tiku_kits_gfx_text.h>
#include <tikukits/gfx/fonts/tiku_kits_gfx_font_5x7.h>
#include <tikukits/epaper/pervasive_itc/tiku_kits_epaper_itc_smallcj.h>

static tiku_kits_epaper_t epd = { ... };

static void epd_set_pixel_thunk(void *ctx, uint16_t x, uint16_t y,
                                 uint8_t color) {
    tiku_kits_epaper_set_pixel((tiku_kits_epaper_t *)ctx, x, y, color);
}

tiku_kits_gfx_surface_t surface = {
    .width      = epd.panel->width,
    .height     = epd.panel->height,
    .set_pixel  = epd_set_pixel_thunk,
    .ctx        = &epd,
};

/* Draw and refresh: */
tiku_kits_gfx_fill_round_rect(&surface, 10, 10, 200, 80, 12,
                               TIKU_KITS_GFX_BLACK);
tiku_kits_gfx_draw_string_in_rect(&surface,
    &(tiku_kits_gfx_rect_t){ 10, 30, 200, 20 },
    "Hello", &tiku_kits_gfx_font_5x7,
    TIKU_KITS_GFX_RED, 3, TIKU_KITS_GFX_ALIGN_CENTER);
tiku_kits_epaper_refresh(&epd);
```

## Memory cost

| Module                           | Approx FRAM cost |
|----------------------------------|------------------|
| All drawing primitives           | ~3.5 KB code     |
| + text rendering + word-wrap     | +~1 KB code      |
| + 5x7 ASCII font                 | +~480 B data     |

Per-glyph cost: `width * bytes_per_column` bytes (5x7 = 5 bytes;
8x16 = 16 bytes). A 95-character font costs ~`95 * width * bytes_per_column`.
Build with `MEMORY_MODEL=large` and large fonts land in HIFRAM
automatically, so adding multiple fonts costs almost no SRAM.

## Reference example

`examples/26_gfx_demo/` cycles through scenes that exercise the
drawing primitives + text rendering on the BWR e-paper.

## Future: UI kit (`tikukits/ui`)

This kit is designed to be the substrate for a future UI kit:

- **Subsurface** gives every widget its own coordinate system.
- **rect_t** types match the natural language of layout.
- **draw_text_wrapped** + **alignment** cover label / paragraph
  rendering.
- **rounded rectangles** + **thick lines** cover button styling.
- **clipping** (via subsurface bounds) keeps widgets isolated.

When the UI kit lands, it will live at `tikukits/ui/`, take a
`tiku_kits_gfx_surface_t *` at construction, and never touch the
underlying display kit directly.

## Out of scope (today)

- Anti-aliasing -- no benefit on 2-color EPD.
- Variable-width fonts ship-ready but no proportional font yet
  in `fonts/` -- add when needed.
- Image decoders (PNG / BMP) -- ship images as `const uint8_t[]`
  bitmaps and use `tiku_kits_gfx_bitmap`.
- Vector graphics / TrueType -- too heavy for FRAM-class MCUs.
- Animation primitives -- EPD refresh rate kills the use case.
