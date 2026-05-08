/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper_itc_spectra4.h - Pervasive iTC Spectra-4 (BWRY) driver
 *
 * Drives Pervasive Displays Q-film "Spectra-4" panels: black,
 * white, red, yellow on a single iTC controller.  Q-film panels
 * use a different protocol from the J-film small-CJ driver:
 *
 *   - Per-panel calibration is not hardcoded.  The host reads
 *     OTP memory from the panel via 3-wire SPI on power-up, and
 *     sprinkles those bytes across a panel-specific init
 *     sequence.  The driver caches the OTP after the first read.
 *   - Frame data is pushed as ONE packed 2-bits-per-pixel buffer,
 *     not two separate (black, red) planes.  The driver packs
 *     the kit's two-plane framebuffer into the BWRY layout on
 *     its way to the controller.
 *   - Power-off is a multi-step sequence with size-specific tail
 *     timing (5 s settle + PSR re-write for 4.17", etc.).
 *
 * Application code is unchanged: continue using the generic
 * tiku_kits_epaper_init / set_pixel / refresh / sleep API and
 * the kit's two-plane framebuffer.  TIKU_KITS_EPAPER_YELLOW now
 * does what its name implies on a Spectra-4 panel.
 *
 * Verified panels:
 *   - E2154QS0F   (1.54", chip ID 0x0302, driver revision 0F)
 *   - E2417QS0A   (4.17", chip ID 0x0605, driver revision 0A)
 *
 * Adding a new Spectra-4 panel:
 *   1. Look up the panel's chip ID and OTP byte count from
 *      Pervasive's PDLS_EXT3_Basic_BWRY reference.
 *   2. Add a `tiku_kits_epaper_itc_spectra4_data_t` constant
 *      with those values plus a function pointer that emits the
 *      panel-specific init sequence given the cached OTP buffer.
 *   3. Add a `tiku_kits_epaper_panel_t` pointing to the data and
 *      to tiku_kits_epaper_itc_spectra4_ops.
 *
 * Hardware: this driver currently bit-bangs the OTP read on the
 * SCK + MOSI pins of the same SPI module the application uses
 * for the regular 4-wire push (matching what Pervasive's PDLS
 * does on Arduino).  That makes the OTP read MSP430-specific.
 * Other architectures will need to add a `spi3` bit-bang shim.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_EPAPER_ITC_SPECTRA4_H_
#define TIKU_KITS_EPAPER_ITC_SPECTRA4_H_

#include "../tiku_kits_epaper.h"

/*---------------------------------------------------------------------------*/
/* FAMILY DATA                                                               */
/*---------------------------------------------------------------------------*/

/* Forward struct types so the function-pointer typedefs below can
 * reference them without the implementation header. */
struct tiku_kits_epaper_pins;

/* OTP read function: per-chip-id sequence that pulls panel
 * calibration into the supplied buffer over 3-wire SPI. */
typedef int (*tiku_kits_epaper_itc_spectra4_otp_fn_t)(
    const struct tiku_kits_epaper_pins *p, uint8_t *buf, uint16_t bytes);

/* Init function: panel-specific opcode sequence using the OTP
 * buffer just read. */
typedef int (*tiku_kits_epaper_itc_spectra4_init_fn_t)(
    tiku_kits_epaper_t *epd, const uint8_t *otp);

/**
 * @brief Per-panel calibration descriptor for Pervasive iTC
 *        Spectra-4 (Q-film) panels.
 *
 * Pointed to from each panel's `family_data` slot; cast back to
 * this type internally by the driver.
 *
 * Fields:
 *   - chip_id:    16-bit chip ID returned by the OTP probe
 *                 (0x0302 for 1.54"/2.13"/2.66"; 0x0605 for 4.17";
 *                 0x0B04 for 4.37"; 0xC901 for 2.06").
 *   - otp_bytes:  number of OTP bytes to read after the chip-ID
 *                 check (48 for chipId 0x0302; 112 for 0x0605).
 *   - read_otp:   chip-id-specific OTP read sequence -- the
 *                 actual SPI3 dialect differs across chip IDs.
 *   - run_init:   panel-specific init sequence.  Receives the
 *                 cached OTP buffer and emits the right
 *                 b_sendIndexData / b_sendCommandData8 ops.
 *   - run_power_off_tail: optional panel-specific tail after
 *                 the DC/DC off command.  May be NULL.
 */
typedef struct {
    uint16_t                                   chip_id;
    uint16_t                                   otp_bytes;
    /* Hardware reset pulse pattern -- timing differs across
     * panel sizes (1.54"/2.13" need 40 ms post-pulse hold). */
    void  (*hw_reset)(const struct tiku_kits_epaper_pins *p);
    tiku_kits_epaper_itc_spectra4_otp_fn_t     read_otp;
    tiku_kits_epaper_itc_spectra4_init_fn_t    run_init;
    int (*run_power_off_tail)(tiku_kits_epaper_t *epd,
                               const uint8_t *otp);
} tiku_kits_epaper_itc_spectra4_data_t;

/*---------------------------------------------------------------------------*/
/* OPS VTABLE                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Operations table for the Pervasive iTC Spectra-4 family.
 *
 * Panel descriptors reference this so the generic API in
 * tiku_kits_epaper.h can dispatch through it.  Application code
 * never calls the driver functions directly.
 */
extern const tiku_kits_epaper_ops_t tiku_kits_epaper_itc_spectra4_ops;

/*---------------------------------------------------------------------------*/
/* SUPPORTED PANELS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Pervasive Displays 1.54" BWRY ("Spectra-4") iTC panel.
 *
 * Reference E2154QS0F, 152 x 152 pixels (square), Q film,
 * chip ID 0x0302, 48-byte OTP, driver revision 0F.  Uses the
 * kit's standard two-plane (black + red) framebuffer; the driver
 * packs it into a single 2bpp buffer for the controller.
 */
extern const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2154qs0f;

/**
 * @brief Pervasive Displays 4.17" BWRY ("Spectra-4") iTC panel.
 *
 * Reference E2417QS0A3, 400 x 300 pixels (long x short), Q film,
 * chip ID 0x0605, 112-byte OTP, driver revision 0A.  Uses the
 * kit's standard two-plane (black + red) framebuffer; the driver
 * packs it into a single 2bpp buffer for the controller.
 */
extern const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2417qs0a3;

#endif /* TIKU_KITS_EPAPER_ITC_SPECTRA4_H_ */
