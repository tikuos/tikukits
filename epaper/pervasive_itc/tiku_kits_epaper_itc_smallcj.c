/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper_itc_smallcj.c - Pervasive iTC small-CJ implementation
 *
 * Implements the Pervasive Displays "small CJ" controller protocol
 * for monochrome (FILM_C/H/K) and colour (FILM_J/Q) iTC panels up
 * to 4.37". Verified on E2266KS0C1 (BW) and E2370JS0C1 (BWR).
 *
 * Driver entry points are private (static) and exposed only via the
 * tiku_kits_epaper_itc_smallcj_ops vtable. Applications go through
 * the generic tiku_kits_epaper_init / refresh / sleep API.
 *
 * Protocol summary (Pervasive iTC small-CJ application note):
 *
 *   reset      -> 5 ms / RESET high / 5 ms / RESET low / 10 ms /
 *                 RESET high / 5 ms / CS high / 5 ms
 *   initial    -> 0x00 + 0x0E (soft reset)
 *                 0xE5 + temperature
 *                 0xE0 + 0x02 (engage temperature)
 *                 0x00 + {psr0, psr1} (panel-specific PSR)
 *   sendImage  -> 0x10 + black plane (DTM1, with mid-frame CS pulse)
 *                 0x13 + red plane   (DTM2, same framing) for colour;
 *                          for BW panels, DTM2 may be omitted
 *   update     -> wait BUSY, 0x04, wait BUSY, 0x12, delay 5 ms,
 *                 wait BUSY
 *   powerOff   -> 0x02, wait BUSY
 *
 * Critical SPI framing detail:
 *   The DTM1/DTM2 commands use "index_data" framing -- between the
 *   opcode byte and the data block, CS pulses HIGH then LOW with
 *   DC switched to data mode while CS is high. Without this pulse
 *   the controller silently rejects the frame data and the panel
 *   produces no visible change. Single-byte data commands (E5, E0)
 *   do NOT need this pulse.
 *
 * BUSY semantics:
 *   Pervasive iTC drives BUSY HIGH when ready, LOW when busy.
 *   Internal helper inverts this so wait sites can read naturally
 *   as "while busy()".
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_epaper_itc_smallcj.h"
#include "tiku.h"
#include <interfaces/bus/tiku_spi_bus.h>
#include <interfaces/gpio/tiku_gpio.h>
#include <kernel/cpu/tiku_common.h>

/*---------------------------------------------------------------------------*/
/* PROTOCOL CONSTANTS                                                        */
/*---------------------------------------------------------------------------*/

#define EPD_OP_PSR              0x00u
#define EPD_OP_POWER_OFF        0x02u
#define EPD_OP_POWER_ON         0x04u
#define EPD_OP_DTM1             0x10u
#define EPD_OP_DTM2             0x13u
#define EPD_OP_DISPLAY_REFRESH  0x12u
#define EPD_OP_TEMP_INPUT       0xE5u
#define EPD_OP_TEMP_ACTIVE      0xE0u

/* Inter-byte CS hold time used by the iTC small-family framing.
 * The PDLS reference driver sets this to 50 us on every supported
 * MCU. Shorter values risk the controller missing the data phase
 * boundary. */
#define EPD_CS_HOLD_US          50u

/* BUSY-poll safety bound. One iteration is roughly 20 ms, so 750
 * caps the total wait at ~15 s -- long enough for a global refresh
 * at 0 C, short enough that a wiring fault gets reported promptly. */
#define EPD_BUSY_POLL_MAX       750u

/*---------------------------------------------------------------------------*/
/* GPIO HELPERS                                                              */
/*---------------------------------------------------------------------------*/

static inline void cs_low(const tiku_kits_epaper_pins_t *p)
{ (void)tiku_gpio_clear(p->cs_port, p->cs_pin); }

static inline void cs_high(const tiku_kits_epaper_pins_t *p)
{ (void)tiku_gpio_set(p->cs_port, p->cs_pin); }

static inline void dc_command(const tiku_kits_epaper_pins_t *p)
{ (void)tiku_gpio_clear(p->dc_port, p->dc_pin); }

static inline void dc_data(const tiku_kits_epaper_pins_t *p)
{ (void)tiku_gpio_set(p->dc_port, p->dc_pin); }

static inline void reset_assert(const tiku_kits_epaper_pins_t *p)
{ (void)tiku_gpio_clear(p->reset_port, p->reset_pin); }

static inline void reset_release(const tiku_kits_epaper_pins_t *p)
{ (void)tiku_gpio_set(p->reset_port, p->reset_pin); }

/* Panel drives BUSY HIGH=ready, LOW=busy. Returns 1 while busy. */
static inline uint8_t busy_asserted(const tiku_kits_epaper_pins_t *p)
{
    return tiku_gpio_read(p->busy_port, p->busy_pin) ? 0u : 1u;
}

/*---------------------------------------------------------------------------*/
/* SPI FRAMING                                                               */
/*---------------------------------------------------------------------------*/

static void
send_cmd_data8(const tiku_kits_epaper_pins_t *p,
                uint8_t opcode, uint8_t data)
{
    dc_command(p);
    cs_low(p);
    (void)tiku_spi_transfer(opcode);
    dc_data(p);
    (void)tiku_spi_transfer(data);
    cs_high(p);
}

static void
send_cmd8(const tiku_kits_epaper_pins_t *p, uint8_t opcode)
{
    dc_command(p);
    cs_low(p);
    (void)tiku_spi_transfer(opcode);
    cs_high(p);
}

/* Multi-byte data variant. Performs the CS HIGH/LOW pulse between
 * opcode and data block that the small-CJ controller requires to
 * latch the data block. Single-byte sends (above) do not need this. */
static void
send_index_data(const tiku_kits_epaper_pins_t *p,
                 uint8_t opcode, const uint8_t *data, uint16_t size)
{
    dc_command(p);
    cs_low(p);
    tiku_common_delay_us(EPD_CS_HOLD_US);
    (void)tiku_spi_transfer(opcode);
    tiku_common_delay_us(EPD_CS_HOLD_US);

    cs_high(p);
    dc_data(p);
    cs_low(p);

    tiku_common_delay_us(EPD_CS_HOLD_US);
    (void)tiku_spi_write(data, size);
    tiku_common_delay_us(EPD_CS_HOLD_US);

    cs_high(p);
    tiku_common_delay_us(EPD_CS_HOLD_US);
}

/* Bounded BUSY-poll. Returns OK once panel deasserts BUSY, or
 * ERR_BUSY if it never does within EPD_BUSY_POLL_MAX iterations. */
static int
wait_ready(const tiku_kits_epaper_pins_t *p)
{
    uint16_t i;
    for (i = 0; i < EPD_BUSY_POLL_MAX; i++) {
        if (!busy_asserted(p)) {
            return TIKU_KITS_EPAPER_OK;
        }
        tiku_common_delay_ms(20);
    }
    return TIKU_KITS_EPAPER_ERR_BUSY;
}

/*---------------------------------------------------------------------------*/
/* HIGH-LEVEL SEQUENCES                                                      */
/*---------------------------------------------------------------------------*/

static void
hw_reset(const tiku_kits_epaper_pins_t *p)
{
    tiku_common_delay_ms(5);
    reset_release(p);
    tiku_common_delay_ms(5);
    reset_assert(p);
    tiku_common_delay_ms(10);
    reset_release(p);
    tiku_common_delay_ms(5);
    cs_high(p);
    tiku_common_delay_ms(5);
}

static void
configure_panel(const tiku_kits_epaper_t *epd)
{
    const tiku_kits_epaper_pins_t *p = &epd->pins;
    const tiku_kits_epaper_itc_smallcj_data_t *fd =
        (const tiku_kits_epaper_itc_smallcj_data_t *)epd->panel->family_data;

    send_cmd_data8(p, EPD_OP_PSR, 0x0Eu);    /* soft reset */
    tiku_common_delay_ms(5);

    send_cmd_data8(p, EPD_OP_TEMP_INPUT,  epd->temperature);
    send_cmd_data8(p, EPD_OP_TEMP_ACTIVE, 0x02u);

    send_index_data(p, EPD_OP_PSR, fd->psr_bytes, 2);
}

static void
push_frames(const tiku_kits_epaper_t *epd)
{
    const uint16_t bytes =
        (uint16_t)tiku_kits_epaper_framebuffer_size(epd->panel);

    /* DTM1 = black plane, always sent.
     * DTM2 = red plane for colour panels. For BW panels DTM2 is
     *        omitted; the controller does not require it for
     *        single-plane image data on this family. */
    send_index_data(&epd->pins, EPD_OP_DTM1, epd->framebuffer, bytes);
    if (epd->framebuffer_red != NULL) {
        send_index_data(&epd->pins, EPD_OP_DTM2,
                         epd->framebuffer_red, bytes);
    }
}

/*---------------------------------------------------------------------------*/
/* DRIVER ENTRY POINTS (referenced from the ops vtable below)                */
/*---------------------------------------------------------------------------*/

static int
itc_smallcj_init(tiku_kits_epaper_t *epd)
{
    /* validate_context() in the generic dispatcher already ensured
     * epd, panel, framebuffer, and (for colour panels) framebuffer_red
     * are non-NULL. We still need the family_data sanity check. */
    if (epd->panel->family_data == NULL) {
        return TIKU_KITS_EPAPER_ERR_PARAM;
    }

    (void)tiku_gpio_dir_out(epd->pins.cs_port,    epd->pins.cs_pin);
    (void)tiku_gpio_dir_out(epd->pins.dc_port,    epd->pins.dc_pin);
    (void)tiku_gpio_dir_out(epd->pins.reset_port, epd->pins.reset_pin);
    (void)tiku_gpio_dir_in (epd->pins.busy_port,  epd->pins.busy_pin);

    cs_high(&epd->pins);
    dc_data(&epd->pins);
    reset_release(&epd->pins);

    return TIKU_KITS_EPAPER_OK;
}

static int
itc_smallcj_refresh(tiku_kits_epaper_t *epd)
{
    int rc;

    hw_reset(&epd->pins);
    configure_panel(epd);
    push_frames(epd);

    rc = wait_ready(&epd->pins);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;
    send_cmd8(&epd->pins, EPD_OP_POWER_ON);

    rc = wait_ready(&epd->pins);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;
    send_cmd8(&epd->pins, EPD_OP_DISPLAY_REFRESH);
    tiku_common_delay_ms(5);

    return wait_ready(&epd->pins);
}

static int
itc_smallcj_sleep(tiku_kits_epaper_t *epd)
{
    send_cmd8(&epd->pins, EPD_OP_POWER_OFF);
    return wait_ready(&epd->pins);
}

/*---------------------------------------------------------------------------*/
/* OPS VTABLE                                                                */
/*---------------------------------------------------------------------------*/

const tiku_kits_epaper_ops_t tiku_kits_epaper_itc_smallcj_ops = {
    .init    = itc_smallcj_init,
    .refresh = itc_smallcj_refresh,
    .sleep   = itc_smallcj_sleep,
};

/*---------------------------------------------------------------------------*/
/* PANEL DESCRIPTORS                                                         */
/*---------------------------------------------------------------------------*/

/* 2.66" E2266KS0C1 -- BW, K film, single plane. */
static const tiku_kits_epaper_itc_smallcj_data_t e2266ks0c1_data = {
    .psr_bytes = { 0xCFu, 0x8Du },
};
const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2266ks0c1 = {
    .width         = 152u,
    .height        = 296u,
    .colour_planes = 1u,
    .name          = "E2266KS0C1 (2.66\" iTC BW)",
    .ops           = &tiku_kits_epaper_itc_smallcj_ops,
    .family_data   = &e2266ks0c1_data,
};

/* 3.70" E2370JS0C1 -- BWR, J film, two planes. Larger small-CJ
 * panels (>= 3.70") use the alternate PSR pair. */
static const tiku_kits_epaper_itc_smallcj_data_t e2370js0c1_data = {
    .psr_bytes = { 0x0Fu, 0x89u },
};
const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2370js0c1 = {
    .width         = 240u,
    .height        = 416u,
    .colour_planes = 2u,
    .name          = "E2370JS0C1 (3.70\" iTC BWR)",
    .ops           = &tiku_kits_epaper_itc_smallcj_ops,
    .family_data   = &e2370js0c1_data,
};
