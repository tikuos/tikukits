/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper.h - Common e-paper display types and definitions
 *
 * Provides shared types, return codes, colour constants, and
 * framebuffer helpers used across all TikuKits e-paper drivers.
 * Each driver (Pervasive iTC, Waveshare SSD16xx, etc.) includes
 * this header for its common types and uses the shared return
 * codes -- drivers must never define their own.
 *
 * Memory model:
 *   The kit is "caller-allocates". Applications size and provide
 *   two framebuffers (current + previous frame) themselves, using
 *   tiku_kits_epaper_framebuffer_size() to compute the byte count
 *   for a chosen panel. This lets the application place the buffers
 *   wherever its memory budget allows -- typically HIFRAM via
 *   tiku_tier_arena_create() for parts that have it, or static
 *   allocation otherwise.
 *
 * API shape:
 *   Driver functions are blocking. tiku_kits_epaper_<family>_refresh()
 *   does not return until the panel has finished latching the new
 *   frame (typically 6-15 s for a global update). Apps that need
 *   non-blocking refreshes should run the driver inside a dedicated
 *   process; the kernel will continue to service other processes
 *   between BUSY polls inside the driver.
 *
 * Panel selection:
 *   Panels are described by a tiku_kits_epaper_panel_t descriptor,
 *   provided as a const extern by each driver module. The driver
 *   itself is selected at compile time (one panel per build).
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
 * Every public driver function returns one of these codes. Zero
 * indicates success; negative values indicate distinct error
 * classes. Drivers must never define their own return codes.
 * @{
 */
#define TIKU_KITS_EPAPER_OK              0    /**< Operation succeeded */
#define TIKU_KITS_EPAPER_ERR_PARAM     (-1)   /**< NULL pointer or invalid argument */
#define TIKU_KITS_EPAPER_ERR_BUSY      (-2)   /**< Panel reported BUSY beyond timeout */
#define TIKU_KITS_EPAPER_ERR_TIMEOUT   (-3)   /**< SPI / GPIO timeout */
#define TIKU_KITS_EPAPER_ERR_UNSUPPORTED (-4) /**< Panel family not supported by this build */
/** @} */

/*---------------------------------------------------------------------------*/
/* COLOURS                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @defgroup TIKU_KITS_EPAPER_COLOUR E-paper Colours
 * @brief Logical colour codes for monochrome panels.
 *
 * Stored in the framebuffer as one bit per pixel: 0 = white,
 * 1 = black. Applications never need to know this -- always pass
 * one of the named constants below to set_pixel / clear.
 * @{
 */
#define TIKU_KITS_EPAPER_WHITE     0u
#define TIKU_KITS_EPAPER_BLACK     1u
/** @} */

/*---------------------------------------------------------------------------*/
/* PANEL DESCRIPTOR                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Panel family identifier.
 *
 * Tags the controller protocol family so the kit can dispatch to
 * the correct driver. Each driver module declares its own family
 * code here; the descriptor stored in tiku_kits_epaper_panel_t
 * carries this tag for runtime sanity checks.
 */
typedef enum {
    TIKU_KITS_EPAPER_FAMILY_NONE        = 0,
    TIKU_KITS_EPAPER_FAMILY_ITC_SMALL_CJ = 1,  /**< Pervasive iTC small CJ
                                                *  (panels up to 4.37") */
    /* Future: ITC_MEDIUM_CJ, ITC_LARGE_CJ, SSD16XX, IL3897, ... */
} tiku_kits_epaper_family_t;

/**
 * @brief Static description of an e-paper panel.
 *
 * One descriptor per supported panel model, declared as a const
 * extern by the driver module. Carries everything the driver
 * needs to talk to the panel: physical geometry, controller
 * family, and any panel-specific calibration constants.
 *
 * Drivers may extend this with family-specific state by embedding
 * an opaque pointer (panel_specific) -- the common kit treats it
 * as opaque.
 */
typedef struct {
    /** Pixels along the panel's short axis (e.g. 152 for the 2.66"). */
    uint16_t width;
    /** Pixels along the panel's long axis (e.g. 296 for the 2.66"). */
    uint16_t height;
    /** Controller protocol family. */
    tiku_kits_epaper_family_t family;
    /** Driver-specific calibration / OTP data. Cast inside the driver. */
    const void *panel_specific;
    /** Human-readable panel name, for logging / diagnostics. */
    const char *name;
} tiku_kits_epaper_panel_t;

/*---------------------------------------------------------------------------*/
/* PIN CONFIGURATION                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Discrete control GPIOs the driver toggles.
 *
 * SPI clock / data lines are configured separately by the
 * tiku_spi_init() call before passing the kit context to the
 * driver. Only the per-panel discrete signals are described here.
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
 * Caller allocates this structure (typically static or on the
 * process stack) and fills the panel / pins / framebuffer pointers
 * before calling the driver init function. The driver may stash
 * its own state inside an opaque slot in future versions; today
 * the struct is fully transparent.
 */
typedef struct {
    /** Panel descriptor (compile-time constant). */
    const tiku_kits_epaper_panel_t *panel;
    /** Discrete control pins. */
    tiku_kits_epaper_pins_t pins;
    /** New-frame buffer (sized via tiku_kits_epaper_framebuffer_size). */
    uint8_t *framebuffer;
    /** Previous-frame buffer (same size, used by global-update waveform). */
    uint8_t *framebuffer_prev;
    /** Ambient temperature in panel units (0x19 = 25 C, 0x00 = 0 C). */
    uint8_t temperature;
} tiku_kits_epaper_t;

/*---------------------------------------------------------------------------*/
/* FRAMEBUFFER HELPERS                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute the byte size of one framebuffer for a panel.
 *
 * Each pixel occupies one bit; the byte size is
 * (width * height) / 8 bytes per buffer. Apps must allocate two
 * buffers of this size (current + previous frame).
 *
 * @param panel  Pointer to the panel descriptor (must not be NULL)
 * @return Number of bytes required for one framebuffer, or 0 if
 *         @p panel is NULL.
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
 * @param colour  TIKU_KITS_EPAPER_WHITE or TIKU_KITS_EPAPER_BLACK
 */
void tiku_kits_epaper_clear(tiku_kits_epaper_t *epd, uint8_t colour);

/**
 * @brief Set or clear one pixel in the framebuffer.
 *
 * In-memory only; coordinates outside the panel are silently
 * clipped. Call refresh() to push the modified buffer to the
 * panel.
 *
 * @param epd     Driver context with framebuffer assigned
 * @param x       Column, 0 .. width-1 (along the short axis)
 * @param y       Row, 0 .. height-1 (along the long axis)
 * @param colour  TIKU_KITS_EPAPER_WHITE or TIKU_KITS_EPAPER_BLACK
 */
void tiku_kits_epaper_set_pixel(tiku_kits_epaper_t *epd,
                                 uint16_t x, uint16_t y, uint8_t colour);

#endif /* TIKU_KITS_EPAPER_H_ */
