/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper.h - Generic e-paper display library, common API
 *
 * This kit provides a manufacturer-agnostic API for SPI-attached
 * monochrome and colour e-paper displays. The application picks a
 * panel descriptor (e.g. tiku_kits_epaper_panel_e2370js0c1, which
 * lives in a manufacturer-specific driver module), passes it to
 * the generic init/refresh/sleep functions defined here, and never
 * needs to know which controller family is underneath.
 *
 * Architecture
 * ------------
 *
 *     application
 *         |
 *         v
 *     +---------------------+
 *     | tiku_kits_epaper.h  |    <- this header (generic API)
 *     | tiku_kits_epaper.c  |       set_pixel / clear / dispatch
 *     +---------------------+
 *         |
 *         v   (vtable in panel descriptor)
 *     +---------------------+
 *     | family driver       |    <- e.g. pervasive_itc/, waveshare_ssd16xx/
 *     | (per-panel logic)   |       implements ops + panel descriptors
 *     +---------------------+
 *         |
 *         v
 *     interfaces/bus/spi  +  arch/.../gpio
 *
 * Each panel descriptor carries:
 *   - common geometry  (width, height, colour-plane count)
 *   - a family-specific ops vtable  (init/refresh/sleep dispatch)
 *   - opaque family-specific data    (PSRs, LUTs, OTP, etc. -- cast
 *                                     to the driver's own struct)
 *
 * The application code is therefore the same regardless of which
 * panel is wired:
 *
 *     epd.panel = &tiku_kits_epaper_panel_<model>;
 *     epd.pins  = ...;
 *     epd.framebuffer     = ...;
 *     epd.framebuffer_red = ...;   // colour panels only
 *     tiku_kits_epaper_init(&epd);
 *     tiku_kits_epaper_set_pixel(&epd, x, y, TIKU_KITS_EPAPER_BLACK);
 *     tiku_kits_epaper_refresh(&epd);
 *     tiku_kits_epaper_sleep(&epd);
 *
 * Memory model
 * ------------
 * Caller-allocates. Apps size and provide one or two framebuffers
 * (current frame + red plane for colour panels) themselves, using
 * tiku_kits_epaper_framebuffer_size() to compute byte counts. This
 * lets the application place the buffers wherever its memory budget
 * allows -- HIFRAM via the tier API on FRAM parts, static SRAM
 * elsewhere.
 *
 * Adding new panels
 * -----------------
 *  1. If the controller family already has a driver under
 *     tikukits/epaper/<family>/, add a new
 *     extern const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_<model>;
 *     to its header and the corresponding constant definition (with
 *     family-specific calibration data) to its .c file.
 *  2. If the controller family is new, create a new subdirectory
 *     under tikukits/epaper/, define a tiku_kits_epaper_ops_t
 *     vtable backed by that family's protocol, and emit one panel
 *     descriptor per supported model pointing to the vtable.
 *
 * No changes to the common header / .c file are required for
 * either case -- the dispatch is fully data-driven.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_EPAPER_H_
#define TIKU_KITS_EPAPER_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_EPAPER_STATUS E-paper Status Codes
 * @brief Return codes shared by all TikuKits e-paper drivers.
 *
 * Every public function returns one of these codes. Zero indicates
 * success; negative values indicate distinct error classes. Driver
 * implementations must never define their own return codes.
 * @{
 */
#define TIKU_KITS_EPAPER_OK              0    /**< Operation succeeded */
#define TIKU_KITS_EPAPER_ERR_PARAM     (-1)   /**< NULL pointer or invalid argument */
#define TIKU_KITS_EPAPER_ERR_BUSY      (-2)   /**< Panel reported BUSY beyond timeout */
#define TIKU_KITS_EPAPER_ERR_TIMEOUT   (-3)   /**< SPI / GPIO timeout */
#define TIKU_KITS_EPAPER_ERR_UNSUPPORTED (-4) /**< Operation not implemented for this family */
/** @} */

/*---------------------------------------------------------------------------*/
/* COLOURS                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_EPAPER_COLOUR E-paper Colours
 * @brief Logical colour codes accepted by set_pixel / clear.
 *
 * Encoded internally as a 2-plane bit pattern (black plane, red
 * plane). Monochrome panels (colour_planes == 1) accept only WHITE
 * and BLACK; passing RED or YELLOW falls back to BLACK so calls
 * are forgiving across panel types. BWR panels accept WHITE / BLACK
 * / RED. BWRY panels accept all four.
 *
 * Plane encoding:
 *   WHITE  : black=0, red=0
 *   BLACK  : black=1, red=0
 *   RED    : black=0, red=1
 *   YELLOW : black=1, red=1   (BWRY only)
 * @{
 */
#define TIKU_KITS_EPAPER_WHITE     0u
#define TIKU_KITS_EPAPER_BLACK     1u
#define TIKU_KITS_EPAPER_RED       2u
#define TIKU_KITS_EPAPER_YELLOW    3u
/** @} */

/*---------------------------------------------------------------------------*/
/* FORWARD DECLARATIONS                                                      */
/*---------------------------------------------------------------------------*/

/* Forward-declared so the ops vtable can take a pointer to it. */
typedef struct tiku_kits_epaper tiku_kits_epaper_t;

/*---------------------------------------------------------------------------*/
/* DRIVER OPS (VTABLE)                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Family-driver operations table.
 *
 * Each driver family implements this struct once and exports it
 * via its header. Panel descriptors point to the appropriate ops
 * via their `ops` field, and the generic API in this kit dispatches
 * through it.
 *
 * Conventions:
 *   - All ops take a pointer to the application's tiku_kits_epaper_t.
 *   - All ops return one of TIKU_KITS_EPAPER_OK / ERR_*.
 *   - `init` is called once before any other op. It configures the
 *     control GPIOs and brings the panel out of reset; it does NOT
 *     init the SPI bus (that is the application's responsibility).
 *   - `refresh` blocks until the panel has finished latching the
 *     new frame.
 *   - `sleep` places the panel in low-power state. Calling refresh
 *     again should wake it.
 */
typedef struct {
    int (*init)(tiku_kits_epaper_t *epd);
    int (*refresh)(tiku_kits_epaper_t *epd);
    int (*sleep)(tiku_kits_epaper_t *epd);
} tiku_kits_epaper_ops_t;

/*---------------------------------------------------------------------------*/
/* PANEL DESCRIPTOR                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Static description of an e-paper panel.
 *
 * One descriptor per supported panel model, declared as
 * `extern const` by the appropriate driver module. Carries the
 * common geometry, the family-driver vtable, and an opaque pointer
 * to family-specific calibration data (PSR bytes for Pervasive iTC,
 * LUT data for SSD16xx, etc.) that the driver casts internally.
 *
 * @note Geometry convention: `width` is the panel's SHORT axis,
 *       `height` is the LONG axis. This matches the controller's
 *       native scan-line stride; getting it backwards shears the
 *       image into apparent dots. For example the 3.7" panel
 *       advertised as "416x240" has width=240, height=416.
 */
typedef struct {
    /** Pixels along the panel's short axis. */
    uint16_t width;
    /** Pixels along the panel's long axis. */
    uint16_t height;
    /** Number of colour planes the panel accepts.
     *  1 = monochrome (BW film, e.g. K, C, H);
     *  2 = colour (BWR / BWRY film, e.g. J, Q). */
    uint8_t colour_planes;
    /** Human-readable panel name, for logging / diagnostics. */
    const char *name;
    /** Family driver vtable (shared across all panels of a family). */
    const tiku_kits_epaper_ops_t *ops;
    /** Opaque pointer to family-specific calibration / OTP / LUT data.
     *  Cast inside the driver to the family's own data struct. */
    const void *family_data;
} tiku_kits_epaper_panel_t;

/*---------------------------------------------------------------------------*/
/* PIN CONFIGURATION                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Discrete control GPIOs the driver toggles.
 *
 * SPI clock / data lines are configured separately by the
 * application's tiku_spi_init() call before invoking the driver.
 * Only the per-panel discrete signals are described here.
 *
 * All four lines are required for SPI EPDs. Some panels also
 * have an external power-gate MOSFET; if your custom board uses
 * one, gate it from the application code (the kit does not own it).
 */
typedef struct {
    uint8_t cs_port,    cs_pin;     /**< Chip select (active low) */
    uint8_t dc_port,    dc_pin;     /**< Data / command select */
    uint8_t reset_port, reset_pin;  /**< Hardware reset (active low) */
    uint8_t busy_port,  busy_pin;   /**< BUSY input from panel */
} tiku_kits_epaper_pins_t;

/*---------------------------------------------------------------------------*/
/* DEVICE CONTEXT                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Per-instance e-paper state.
 *
 * Caller allocates this structure (typically static) and fills its
 * fields before calling tiku_kits_epaper_init(). The struct is
 * fully transparent today; future kit versions may add an opaque
 * private slot for driver state at the bottom.
 */
struct tiku_kits_epaper {
    /** Panel descriptor (compile-time constant). */
    const tiku_kits_epaper_panel_t *panel;
    /** Discrete control pins. */
    tiku_kits_epaper_pins_t pins;
    /** Black plane (DTM1 / black pixel data). Always required;
     *  size = tiku_kits_epaper_framebuffer_size(panel). */
    uint8_t *framebuffer;
    /** Red plane (DTM2 / second-colour pixel data). Required for
     *  colour panels (panel->colour_planes == 2); may be NULL for
     *  monochrome panels (some drivers may still send a fixed fill
     *  to DTM2 internally for protocol compliance). */
    uint8_t *framebuffer_red;
    /** Ambient temperature in panel units, used by drivers that
     *  vary their waveform with temperature. For Pervasive iTC:
     *  0x19 = 25 C, 0x00 = 0 C. */
    uint8_t temperature;
};

/*---------------------------------------------------------------------------*/
/* GENERIC API (dispatches through panel->ops)                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize the panel and bring it out of reset.
 *
 * Configures GPIOs, validates that the framebuffer pointers are
 * appropriate for the panel's colour-plane count, and dispatches
 * to the family driver. The SPI bus must already be initialized
 * by the application.
 *
 * @param epd  Driver context with panel, pins, and framebuffers
 *             assigned.
 * @return TIKU_KITS_EPAPER_OK on success, ERR_PARAM on missing
 *         fields, or any error code returned by the family driver.
 */
int tiku_kits_epaper_init(tiku_kits_epaper_t *epd);

/**
 * @brief Push the framebuffer(s) to the panel and trigger an update.
 *
 * Blocks until the panel finishes latching the new frame
 * (typically 6-15 s for monochrome global update at 25 C, longer
 * for colour panels and at lower temperatures).
 *
 * @param epd  Driver context previously passed to init()
 * @return TIKU_KITS_EPAPER_OK on success, ERR_BUSY on BUSY-line
 *         timeout, or any error code returned by the family driver.
 */
int tiku_kits_epaper_refresh(tiku_kits_epaper_t *epd);

/**
 * @brief Place the panel in low-power sleep.
 *
 * Panel retains the last displayed image without consuming power.
 * Call refresh() again to wake.
 *
 * @param epd  Driver context
 * @return TIKU_KITS_EPAPER_OK on success, or any error code
 *         returned by the family driver.
 */
int tiku_kits_epaper_sleep(tiku_kits_epaper_t *epd);

/*---------------------------------------------------------------------------*/
/* FRAMEBUFFER HELPERS (driver-agnostic)                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the byte size of one framebuffer plane for a panel.
 *
 * Each pixel occupies one bit per plane. Apps allocating a colour
 * panel's two planes must allocate 2 * this size in total.
 *
 * @param panel  Pointer to the panel descriptor (must not be NULL)
 * @return Number of bytes per plane, or 0 if @p panel is NULL.
 */
size_t tiku_kits_epaper_framebuffer_size(
    const tiku_kits_epaper_panel_t *panel);

/**
 * @brief Fill the entire framebuffer with a single colour.
 *
 * In-memory only; no SPI traffic. Call refresh() to push the
 * cleared buffer to the panel.
 *
 * @param epd     Driver context with framebuffer assigned
 * @param colour  TIKU_KITS_EPAPER_WHITE / BLACK / RED / YELLOW
 *                (RED/YELLOW collapse to BLACK on monochrome panels)
 */
void tiku_kits_epaper_clear(tiku_kits_epaper_t *epd, uint8_t colour);

/**
 * @brief Set or clear one pixel in the framebuffer.
 *
 * In-memory only; coordinates outside the panel are silently
 * clipped. Call refresh() to push the modified buffer to the panel.
 *
 * @param epd     Driver context with framebuffer assigned
 * @param x       Column, 0 .. width-1 (along the short axis)
 * @param y       Row, 0 .. height-1 (along the long axis)
 * @param colour  TIKU_KITS_EPAPER_WHITE / BLACK / RED / YELLOW
 */
void tiku_kits_epaper_set_pixel(tiku_kits_epaper_t *epd,
                                 uint16_t x, uint16_t y, uint8_t colour);

#endif /* TIKU_KITS_EPAPER_H_ */
