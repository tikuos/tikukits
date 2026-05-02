/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper_itc_smallcj.h - Pervasive iTC small-CJ driver family
 *
 * Drives Pervasive Displays iTC family panels up to 4.37" using
 * the "small CJ" controller protocol (BW films C/H/K and colour
 * films J/Q). Supported on the EXT3-1 / EXT4 extension boards.
 *
 * Verified panels:
 *   - E2266KS0C1  (2.66" BW, K film, single plane)
 *   - E2370JS0C1  (3.70" BWR, J film "Spectra", two planes)
 *
 * Adding a new small-CJ panel:
 *   1. Pick the correct PSR pair from Pervasive's small-CJ
 *      "fake OTP" table:
 *        sizes <= 2.90" -> { 0xCF, 0x8D }
 *        sizes >= 3.70" -> { 0x0F, 0x89 }
 *   2. Define a static const tiku_kits_epaper_itc_smallcj_data_t
 *      with those PSR bytes.
 *   3. Define a tiku_kits_epaper_panel_t pointing to the data and
 *      to tiku_kits_epaper_itc_smallcj_ops; set width = SHORT axis,
 *      height = LONG axis (the spec sheet's WxH is often LONG x SHORT
 *      so swap if needed -- getting this wrong shears the image).
 *   4. Add an extern declaration here and use it from the application.
 *
 * Application usage (transport-agnostic via the generic API):
 * @code
 *   #include <tikukits/epaper/pervasive_itc/tiku_kits_epaper_itc_smallcj.h>
 *
 *   static uint8_t fb_black[12480], fb_red[12480];
 *   tiku_kits_epaper_t epd = {
 *       .panel = &tiku_kits_epaper_panel_e2370js0c1,
 *       .pins  = { .cs_port = 3, .cs_pin = 0, .dc_port = 3, .dc_pin = 1,
 *                  .reset_port = 3, .reset_pin = 2,
 *                  .busy_port  = 3, .busy_pin  = 3 },
 *       .framebuffer     = fb_black,
 *       .framebuffer_red = fb_red,
 *       .temperature     = 0x19,
 *   };
 *   tiku_kits_epaper_init(&epd);                    // generic API
 *   tiku_kits_epaper_set_pixel(&epd, 10, 10, TIKU_KITS_EPAPER_RED);
 *   tiku_kits_epaper_refresh(&epd);                 // blocks 6-25 s
 *   tiku_kits_epaper_sleep(&epd);
 * @endcode
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_EPAPER_ITC_SMALLCJ_H_
#define TIKU_KITS_EPAPER_ITC_SMALLCJ_H_

#include "../tiku_kits_epaper.h"

/*---------------------------------------------------------------------------*/
/* FAMILY DATA                                                               */
/*---------------------------------------------------------------------------*/

/**
 * @brief Per-panel calibration data for Pervasive iTC small-CJ.
 *
 * Pointed to from each panel descriptor's `family_data` slot. The
 * driver casts the opaque pointer back to this type internally.
 *
 * Fields:
 *   - psr_bytes: the two "Panel Setting Register" bytes Pervasive's
 *     small-CJ controllers expect at boot. Hardcoded per panel size
 *     (small-CJ does not actually read OTP from the panel for these
 *     sizes -- the host supplies them).
 */
typedef struct {
    uint8_t psr_bytes[2];
} tiku_kits_epaper_itc_smallcj_data_t;

/*---------------------------------------------------------------------------*/
/* OPS VTABLE                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Operations table for the Pervasive iTC small-CJ family.
 *
 * Panel descriptors reference this so the generic API in
 * tiku_kits_epaper.h can dispatch through it. Application code
 * never calls the driver functions directly.
 */
extern const tiku_kits_epaper_ops_t tiku_kits_epaper_itc_smallcj_ops;

/*---------------------------------------------------------------------------*/
/* SUPPORTED PANELS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Pervasive Displays 2.66" monochrome iTC panel.
 *
 * Reference E2266KS0C1, 152 x 296 pixels, K film (wide-temperature
 * monochrome), 0C driver. PSR { 0xCF, 0x8D }. Single colour plane;
 * `framebuffer_red` is unused.
 */
extern const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2266ks0c1;

/**
 * @brief Pervasive Displays 3.70" BWR ("Spectra") iTC panel.
 *
 * Reference E2370JS0C1, 240 x 416 pixels (short x long), J film
 * (Spectra BWR), 0C driver. PSR { 0x0F, 0x89 } (size >= 3.70"
 * variant). Two colour planes -- caller must allocate both
 * `framebuffer` (black) and `framebuffer_red` (red).
 */
extern const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2370js0c1;

#endif /* TIKU_KITS_EPAPER_ITC_SMALLCJ_H_ */
