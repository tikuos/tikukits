/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper_itc_smallcj.h - Pervasive Displays iTC small-CJ driver
 *
 * Drives Pervasive Displays iTC family monochrome panels up to
 * 4.37" using the "small CJ" controller protocol via the EXT3 /
 * EXT3-1 extension board. Tested on the 2.66" E2266KS0C1 panel
 * (152 x 296 pixels, K film, C/0C driver).
 *
 * Prerequisites:
 *   - SPI bus initialized via tiku_spi_init() before driver init.
 *     The driver does not own the SPI bus; multiple SPI devices
 *     may share it as long as each manages its own CS.
 *   - The board's SPI CLK / SIMO pins routed to the panel's CLK /
 *     SDI pins. SOMI is not used by this driver.
 *
 * Wiring (one example, on FR5994 LaunchPad):
 *   Panel CS     -> any GPIO (caller picks)
 *   DC           -> any GPIO
 *   RESET        -> any GPIO
 *   BUSY         -> any GPIO (input)
 *   SCK          -> P5.2 (UCB1CLK)
 *   SDI / MOSI   -> P5.0 (UCB1SIMO)
 *   GND, 3V3     -> any
 *
 * Typical usage:
 * @code
 *   #include <tikukits/epaper/pervasive_itc/tiku_kits_epaper_itc_smallcj.h>
 *
 *   static uint8_t fb_new[5624];
 *   static uint8_t fb_old[5624];
 *
 *   tiku_kits_epaper_t epd = {
 *       .panel = &tiku_kits_epaper_panel_e2266ks0c1,
 *       .pins  = { .cs_port = 3, .cs_pin = 0,
 *                  .dc_port = 3, .dc_pin = 1,
 *                  .reset_port = 3, .reset_pin = 2,
 *                  .busy_port = 3, .busy_pin = 3 },
 *       .framebuffer      = fb_new,
 *       .framebuffer_prev = fb_old,
 *       .temperature      = 0x19,
 *   };
 *
 *   tiku_kits_epaper_itc_smallcj_init(&epd);
 *   tiku_kits_epaper_clear(&epd, TIKU_KITS_EPAPER_BLACK);
 *   tiku_kits_epaper_itc_smallcj_refresh(&epd);
 *   tiku_kits_epaper_itc_smallcj_sleep(&epd);
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
/* SUPPORTED PANELS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Pervasive Displays 2.66" monochrome iTC panel.
 *
 * Reference E2266KS0C1, 152 x 296 pixels, K film (wide-temperature
 * monochrome), 0C driver (small-CJ). The KS-0C variant uses the
 * default Small-CJ "OTP" panel-setting bytes (0xCF, 0x8D).
 */
extern const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2266ks0c1;

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Configure GPIOs and bring the panel out of reset.
 *
 * Sets the discrete control pins as outputs, BUSY as input, drives
 * the controller through its required reset pulse pattern, and
 * leaves the panel ready to receive a refresh().  Does NOT init
 * the SPI bus -- the caller is responsible for tiku_spi_init().
 *
 * @param epd  Driver context with panel, pins, and framebuffers
 *             assigned. The framebuffers may be uninitialized;
 *             they will be sent to the panel only when refresh()
 *             is called.
 * @return TIKU_KITS_EPAPER_OK on success,
 *         TIKU_KITS_EPAPER_ERR_PARAM if @p epd or required fields
 *             are NULL,
 *         TIKU_KITS_EPAPER_ERR_UNSUPPORTED if the panel descriptor
 *             belongs to a different family.
 */
int tiku_kits_epaper_itc_smallcj_init(tiku_kits_epaper_t *epd);

/**
 * @brief Push the framebuffer to the panel and trigger a global update.
 *
 * Sends the soft-reset / temperature / PSR sequence, transmits the
 * current and previous framebuffers via DTM1 / DTM2, then runs the
 * power-on / refresh / power-off cycle. Blocks for the duration of
 * the panel's BUSY signal (typically 6-15 s for a global update at
 * 25 C).
 *
 * After return, the just-pushed frame is copied into
 * framebuffer_prev so the next refresh has correct waveform
 * context for the controller.
 *
 * @param epd  Driver context previously passed to init()
 * @return TIKU_KITS_EPAPER_OK on success,
 *         TIKU_KITS_EPAPER_ERR_PARAM if @p epd is NULL,
 *         TIKU_KITS_EPAPER_ERR_BUSY if the panel never released
 *             BUSY within the safety timeout (likely wiring or
 *             power issue).
 */
int tiku_kits_epaper_itc_smallcj_refresh(tiku_kits_epaper_t *epd);

/**
 * @brief Place the panel in low-power sleep.
 *
 * Sends the DC/DC power-off command. The panel retains the last
 * displayed image without consuming power. Wake by calling
 * refresh() again.
 *
 * @param epd  Driver context
 * @return TIKU_KITS_EPAPER_OK on success,
 *         TIKU_KITS_EPAPER_ERR_PARAM if @p epd is NULL.
 */
int tiku_kits_epaper_itc_smallcj_sleep(tiku_kits_epaper_t *epd);

#endif /* TIKU_KITS_EPAPER_ITC_SMALLCJ_H_ */
