# tikukits/epaper -- E-paper display library

Manufacturer-agnostic SPI e-paper driver kit for TikuOS. Designed
to grow into a wide-coverage library across Pervasive Displays,
Waveshare, GoodDisplay, and any other SPI-attached EPD.

## Layout

```
tikukits/epaper/
├── tiku_kits_epaper.h         Common types, return codes,
│                              colour constants, ops vtable, and
│                              the generic init/refresh/sleep API.
├── tiku_kits_epaper.c         Driver-agnostic dispatch +
│                              framebuffer helpers (set_pixel,
│                              clear, framebuffer_size).
└── <family>/                  One subdirectory per controller
    ├── tiku_kits_epaper_<...>.h    family. Ships an ops vtable
    └── tiku_kits_epaper_<...>.c    + one panel descriptor per
                                    supported model.
```

Currently shipped families:
- `pervasive_itc/` -- Pervasive iTC small-CJ controller (BW + BWR + BWRY,
  panels up to 4.37").

## Application code is family-agnostic

Apps include the family header (only to reach the panel
descriptor's name) but call only the generic API:

```c
#include <tikukits/epaper/pervasive_itc/tiku_kits_epaper_itc_smallcj.h>

static uint8_t fb_black[12480], fb_red[12480];
static tiku_kits_epaper_t epd = {
    .panel = &tiku_kits_epaper_panel_e2370js0c1,
    .pins  = { /* CS / DC / RESET / BUSY GPIOs */ },
    .framebuffer     = fb_black,
    .framebuffer_red = fb_red,
    .temperature     = 0x19,   /* 25 C */
};

tiku_kits_epaper_init(&epd);
tiku_kits_epaper_clear(&epd, TIKU_KITS_EPAPER_WHITE);
tiku_kits_epaper_set_pixel(&epd, 50, 50, TIKU_KITS_EPAPER_RED);
tiku_kits_epaper_refresh(&epd);
tiku_kits_epaper_sleep(&epd);
```

Switching panels = changing the `epd.panel` pointer. Switching to
a panel from a different controller family also = changing only
the `epd.panel` pointer, plus rebuilding with the corresponding
driver subdirectory included.

## Adding a new panel within an existing family

Pure data work, no driver edits:

1. Look up the panel-specific calibration in the family's
   datasheet / library (PSR bytes for Pervasive iTC, LUT bytes
   for Waveshare, etc.).
2. Add a `static const <family>_data_t` constant in the family's
   `.c` file with that calibration data.
3. Add a `const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_<model>`
   pointing to the data, the family ops vtable, and the right
   geometry (width = SHORT axis, height = LONG axis -- see below).
4. Add a matching `extern const ...` declaration in the family
   header so apps can reference it.

Example: adding a 4.17" iTC BWRY panel to `pervasive_itc/`:

```c
static const tiku_kits_epaper_itc_smallcj_data_t e2417qs0a3_data = {
    .psr_bytes = { 0x0Fu, 0x89u },   /* size >= 3.70" */
};
const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2417qs0a3 = {
    .width = 300, .height = 400,
    .colour_planes = 2,              /* BWRY */
    .name = "E2417QS0A3 (4.17\" iTC BWRY)",
    .ops = &tiku_kits_epaper_itc_smallcj_ops,
    .family_data = &e2417qs0a3_data,
};
```

## Adding a new controller family

Slightly more work, still localised to the new subdirectory:

1. Create `tikukits/epaper/<family>/`.
2. Define a private family-data struct holding whatever this
   controller needs per panel (PSR / LUT / source voltage / etc.).
3. Implement static `init`, `refresh`, `sleep` functions matching
   the `tiku_kits_epaper_ops_t` signature.
4. Export one `extern const tiku_kits_epaper_ops_t <family>_ops`.
5. Define one `const tiku_kits_epaper_panel_t` per supported model,
   pointing to your ops vtable and the per-panel data struct.
6. Add `SRCS += $(wildcard tikukits/epaper/<family>/*.c)` to the
   project Makefile.

The common header / `.c` file does NOT need to change. The ops
dispatch is purely data-driven.

## Geometry convention (read this before adding a panel)

The kit's framebuffer layout is row-major where each "row" of the
buffer matches one native scan line of the controller. The
controller's scan-line stride is `short_axis_pixels / 8` bytes.

This means **the panel descriptor's `width` field must always be
the panel's SHORT axis, and `height` must be the LONG axis**,
regardless of how the spec sheet labels the dimensions.

Example: Pervasive's spec sheet for the 3.7" panel says
"416 x 240" (long x short, landscape). The kit descriptor uses
`.width = 240, .height = 416`. Putting it the other way round
makes the row stride wrong and the image shears into apparent
dots -- looks like a font bug; isn't.

If the spec sheet's W x H is already short x long (portrait), use
those numbers directly.

## Memory model

Caller-allocated framebuffers. The kit never calls `malloc`. Apps
size their buffers via `tiku_kits_epaper_framebuffer_size(panel)`
(returns bytes per plane) and place them wherever fits the memory
budget:
- FRAM parts (e.g. MSP430FR5994): allocate from HIFRAM via the
  tier API (`tiku_tier_arena_create` / `tiku_arena_alloc`).
- Larger SRAM parts (nRF54L, RISC-V): static or stack-allocated.

Per plane: `width * height / 8` bytes. Colour panels need TWO
planes (`framebuffer` for the black plane, `framebuffer_red` for
the red plane). Monochrome panels need only `framebuffer` and may
leave `framebuffer_red` as `NULL`.

## Refresh timing

E-paper updates are slow by physics, not by software:
- Pervasive iTC monochrome global update: 6-15 s at 25 C.
- Pervasive iTC BWR / BWRY global update:  20-30 s at 25 C.
- Larger / colder panels: longer.

`tiku_kits_epaper_refresh()` blocks for the duration. Apps that
want non-blocking refreshes should run the kit calls inside a
dedicated process; the kernel scheduler runs other processes
between BUSY polls inside the driver.

## Verified hardware

| Panel         | Family        | Verified on            |
|---------------|---------------|------------------------|
| E2266KS0C1    | iTC small-CJ  | MSP430FR5994 LP + EXT3-1 |
| E2370JS0C1    | iTC small-CJ  | MSP430FR5994 LP + EXT3-1 |
