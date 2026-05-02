/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper_itc_smallcj.c - Pervasive iTC small-CJ implementation
 *
 * Implements the Pervasive Displays "small CJ" controller protocol
 * for monochrome iTC panels up to 4.37". Verified on the 2.66"
 * E2266KS0C1.
 *
 * Protocol summary (per Pervasive's iTC small-CJ application note):
 *
 *   reset      -> 5 ms / RESET high / 5 ms / RESET low / 10 ms /
 *                 RESET high / 5 ms / CS high / 5 ms
 *   initial    -> 0x00 + 0x0E (soft reset)
 *                 0xE5 + temperature
 *                 0xE0 + 0x02 (engage temperature)
 *                 0x00 + {0xCF, 0x8D} (PSR, "OTP" hardcoded)
 *   sendImage  -> 0x10 + new frame (DTM1, with mid-frame CS pulse)
 *                 0x13 + previous frame (DTM2, same framing)
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
 *   Pervasive iTC drives BUSY HIGH when the panel is READY and LOW
 *   when busy. The library's internal helper inverts this so the
 *   high-level code can read "wait while !ready".
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
#include <arch/msp430/tiku_gpio_arch.h>
#include <kernel/cpu/tiku_common.h>

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/* Small-CJ opcodes */
#define EPD_OP_PSR              0x00u
#define EPD_OP_POWER_OFF        0x02u
#define EPD_OP_POWER_ON         0x04u
#define EPD_OP_DTM1             0x10u
#define EPD_OP_DTM2             0x13u
#define EPD_OP_DISPLAY_REFRESH  0x12u
#define EPD_OP_TEMP_INPUT       0xE5u
#define EPD_OP_TEMP_ACTIVE      0xE0u

/* Inter-byte CS hold time used by the iTC small-family framing.
 * The reference driver sets this to 50 us on every supported MCU. */
#define EPD_CS_HOLD_US          50u

/* Maximum number of busy-wait poll iterations before giving up.
 * One iteration is roughly tiku_common_delay_ms(20), so 750 caps
 * the safety window at ~15 seconds -- long enough for a global
 * update at 0 C, short enough that a wiring fault gets reported
 * promptly. */
#define EPD_BUSY_POLL_MAX       750u

/*---------------------------------------------------------------------------*/
/* PANEL DESCRIPTOR -- 2.66" E2266KS0C1                                      */
/*---------------------------------------------------------------------------*/

/* Panel-specific PSR bytes. Hardcoded per Pervasive's small-CJ
 * "fake OTP" table for sizes <= 2.90". Larger small-CJ panels
 * (3.70/4.17/4.37") would need {0x0F, 0x89} instead. */
static const uint8_t e2266ks0c1_psr[2] = { 0xCFu, 0x8Du };

const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2266ks0c1 = {
    .width   = 152u,
    .height  = 296u,
    .family  = TIKU_KITS_EPAPER_FAMILY_ITC_SMALL_CJ,
    .panel_specific = (const void *)e2266ks0c1_psr,
    .name    = "E2266KS0C1 (2.66\" iTC)",
};

/*---------------------------------------------------------------------------*/
/* GPIO HELPERS                                                              */
/*---------------------------------------------------------------------------*/

static inline void cs_low(const tiku_kits_epaper_pins_t *p)
{ tiku_gpio_arch_write(p->cs_port, p->cs_pin, 0); }

static inline void cs_high(const tiku_kits_epaper_pins_t *p)
{ tiku_gpio_arch_write(p->cs_port, p->cs_pin, 1); }

static inline void dc_command(const tiku_kits_epaper_pins_t *p)
{ tiku_gpio_arch_write(p->dc_port, p->dc_pin, 0); }

static inline void dc_data(const tiku_kits_epaper_pins_t *p)
{ tiku_gpio_arch_write(p->dc_port, p->dc_pin, 1); }

static inline void reset_assert(const tiku_kits_epaper_pins_t *p)
{ tiku_gpio_arch_write(p->reset_port, p->reset_pin, 0); }

static inline void reset_release(const tiku_kits_epaper_pins_t *p)
{ tiku_gpio_arch_write(p->reset_port, p->reset_pin, 1); }

/* BUSY: panel drives HIGH=ready, LOW=busy. Returns 1 while busy
 * so wait sites read naturally as "while busy_asserted()". */
static inline uint8_t busy_asserted(const tiku_kits_epaper_pins_t *p)
{
    return tiku_gpio_arch_read(p->busy_port, p->busy_pin) ? 0u : 1u;
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

/* Bounded BUSY-poll. Returns TIKU_KITS_EPAPER_OK once the panel
 * deasserts BUSY, or TIKU_KITS_EPAPER_ERR_BUSY if it never does
 * within EPD_BUSY_POLL_MAX iterations. */
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
    const uint8_t *psr = (const uint8_t *)epd->panel->panel_specific;

    send_cmd_data8(p, EPD_OP_PSR, 0x0Eu);    /* soft reset */
    tiku_common_delay_ms(5);

    send_cmd_data8(p, EPD_OP_TEMP_INPUT,  epd->temperature);
    send_cmd_data8(p, EPD_OP_TEMP_ACTIVE, 0x02u);

    send_index_data(p, EPD_OP_PSR, psr, 2);
}

static void
push_frames(const tiku_kits_epaper_t *epd)
{
    const uint16_t bytes =
        (uint16_t)tiku_kits_epaper_framebuffer_size(epd->panel);

    send_index_data(&epd->pins, EPD_OP_DTM1, epd->framebuffer,      bytes);
    send_index_data(&epd->pins, EPD_OP_DTM2, epd->framebuffer_prev, bytes);
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int
tiku_kits_epaper_itc_smallcj_init(tiku_kits_epaper_t *epd)
{
    if (epd == NULL || epd->panel == NULL ||
        epd->framebuffer == NULL || epd->framebuffer_prev == NULL) {
        return TIKU_KITS_EPAPER_ERR_PARAM;
    }
    if (epd->panel->family != TIKU_KITS_EPAPER_FAMILY_ITC_SMALL_CJ) {
        return TIKU_KITS_EPAPER_ERR_UNSUPPORTED;
    }

    tiku_gpio_arch_set_output(epd->pins.cs_port,    epd->pins.cs_pin);
    tiku_gpio_arch_set_output(epd->pins.dc_port,    epd->pins.dc_pin);
    tiku_gpio_arch_set_output(epd->pins.reset_port, epd->pins.reset_pin);
    tiku_gpio_arch_set_input (epd->pins.busy_port,  epd->pins.busy_pin);

    cs_high(&epd->pins);
    dc_data(&epd->pins);
    reset_release(&epd->pins);

    return TIKU_KITS_EPAPER_OK;
}

int
tiku_kits_epaper_itc_smallcj_refresh(tiku_kits_epaper_t *epd)
{
    int rc;
    size_t bytes;

    if (epd == NULL) {
        return TIKU_KITS_EPAPER_ERR_PARAM;
    }
    bytes = tiku_kits_epaper_framebuffer_size(epd->panel);

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

    rc = wait_ready(&epd->pins);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;

    /* Snapshot the just-pushed frame so the next refresh has the
     * correct (old, new) waveform context. */
    {
        size_t i;
        for (i = 0; i < bytes; i++) {
            epd->framebuffer_prev[i] = epd->framebuffer[i];
        }
    }

    return TIKU_KITS_EPAPER_OK;
}

int
tiku_kits_epaper_itc_smallcj_sleep(tiku_kits_epaper_t *epd)
{
    if (epd == NULL) {
        return TIKU_KITS_EPAPER_ERR_PARAM;
    }
    send_cmd8(&epd->pins, EPD_OP_POWER_OFF);
    return wait_ready(&epd->pins);
}
