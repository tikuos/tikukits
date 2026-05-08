/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_epaper_itc_spectra4.c - Pervasive iTC Q-film driver
 *
 * Implements the Pervasive Displays "Spectra-4" protocol for
 * Q-film (BWRY) iTC panels.  Verified on E2417QS0A3 (4.17",
 * chip ID 0x0605).  The protocol is meaningfully different from
 * the J-film small-CJ driver; see the header for an overview.
 *
 * Protocol summary (per PDLS_EXT3_Basic_BWRY reference):
 *
 *   reset      -> 5 sequenced delays around RESET; then BUSY wait
 *                  (Q-film holds BUSY low much longer than J-film).
 *   OTP read   -> 3-wire SPI bit-bang over SCK + MOSI:
 *                  1. Send 0x70, read 2 bytes -> chip ID match
 *                  2. Per-chip-ID command sequence to position
 *                     the OTP read pointer
 *                  3. Read N bytes (panel-specific) into RAM
 *   initial    -> Per-panel sequence using OTP-derived bytes
 *                  for opcodes 0x00, 0x01, 0x03, 0x06, 0x30,
 *                  0x50, 0x60, 0x61, 0x65, 0xE3, 0xE7, 0xE9.
 *                  Closes with 0x04 power-on + BUSY wait.
 *   sendImage  -> Single packed 2bpp buffer via 0x10 (DTM1).
 *                  No DTM2 -- yellow is encoded inline.
 *   update     -> 0x12 0x00 + BUSY wait.
 *   powerOff   -> 0x02 0x00 + BUSY wait, then panel-specific tail
 *                  (5 s settle + PSR re-write for 4.17", etc.).
 *
 * BWRY frame encoding (per Pervasive app note):
 *
 *      bits | colour
 *      -----|-------
 *      00   | BLACK
 *      01   | RED
 *      10   | YELLOW
 *      11   | WHITE
 *
 * The driver packs the kit's two-plane framebuffer into this
 * format on its way to the controller.  Application code keeps
 * using set_pixel() with TIKU_KITS_EPAPER_{WHITE,BLACK,RED,YELLOW}.
 *
 * Memory note: a 4.17" frame is 30 KB.  We do NOT allocate a
 * second buffer for the packed output -- packing is streamed
 * row by row (100-byte row scratch on the stack).  The OTP
 * cache is static (128 bytes).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_epaper_itc_spectra4.h"
#include "tiku.h"
#include <interfaces/bus/tiku_spi_bus.h>
#include <interfaces/gpio/tiku_gpio.h>
#include <kernel/cpu/tiku_common.h>

#if defined(__MSP430__)
#include <msp430.h>
#endif

/* Optional UART trace for hardware bring-up.  Enabled by default
 * when the shell is available; the driver only emits a few lines
 * per refresh so it's cheap to leave on while debugging. */
#if defined(TIKU_SHELL_ENABLE) && (TIKU_SHELL_ENABLE == 1)
#include <kernel/shell/tiku_shell_io.h>
#define EPD_DBG(...) SHELL_PRINTF(__VA_ARGS__)
#else
#define EPD_DBG(...) do {} while (0)
#endif

/* Panel-power gate is now a generic kit feature: applications
 * supply power_port/power_pin in tiku_kits_epaper_pins_t and the
 * kit's tiku_kits_epaper_panel_power_on/off helpers do the right
 * thing (no-op when port=0).  See tiku_kits_epaper.h. */

/*---------------------------------------------------------------------------*/
/* PROTOCOL CONSTANTS                                                        */
/*---------------------------------------------------------------------------*/

#define EPD_OP_PSR              0x00u
#define EPD_OP_POWER_OFF        0x02u
#define EPD_OP_POWER_ON         0x04u
#define EPD_OP_DTM1             0x10u
#define EPD_OP_DISPLAY_REFRESH  0x12u
#define EPD_OP_TEMP_ACTIVE      0xE0u
#define EPD_OP_TEMP_INPUT       0xE6u

#define EPD_CS_HOLD_US          50u

/* Q-film panels can hold BUSY low for tens of seconds during a
 * full update (especially below 25 C).  At 20 ms per poll, 1500
 * iterations gives a 30 s ceiling. */
#define EPD_BUSY_POLL_MAX       1500u

/* OTP scratch.  Per-process singleton; matches Pervasive's
 * u_flagOTP cache.  Sized to the largest panel we support. */
#define EPD_OTP_MAX             128u
static uint8_t  s_otp[EPD_OTP_MAX];
static uint8_t  s_otp_valid;

/*---------------------------------------------------------------------------*/
/* 3-WIRE SPI BIT-BANG (OTP read)                                            */
/*---------------------------------------------------------------------------*/

/* OTP read uses a half-duplex 3-wire SPI dialect: SCK as clock,
 * MOSI as a bidirectional data line, CS pulsed around each byte.
 * We bit-bang on the same physical pins the regular 4-wire SPI
 * uses, briefly switching them out of alternate-function mode.
 *
 * The pin selection is currently hardcoded for the FR5994
 * LaunchPad (P5.2 = SCK, P5.0 = MOSI -- USCI_B1).  Override at
 * compile time for other boards. */

#ifndef TIKU_KITS_EPAPER_SPECTRA4_SCK_PORT
#define TIKU_KITS_EPAPER_SPECTRA4_SCK_PORT   5u
#endif
#ifndef TIKU_KITS_EPAPER_SPECTRA4_SCK_PIN
#define TIKU_KITS_EPAPER_SPECTRA4_SCK_PIN    2u
#endif
#ifndef TIKU_KITS_EPAPER_SPECTRA4_DATA_PORT
#define TIKU_KITS_EPAPER_SPECTRA4_DATA_PORT  5u
#endif
#ifndef TIKU_KITS_EPAPER_SPECTRA4_DATA_PIN
#define TIKU_KITS_EPAPER_SPECTRA4_DATA_PIN   0u
#endif
/* Optional separate read line.  PDLS's reference uses one pin
 * (MOSI) for both directions in 3-wire mode, but on some EXT3
 * variants the panel returns OTP on MISO instead.  Default = same
 * pin as DATA (matches PDLS); override with
 * -DTIKU_KITS_EPAPER_SPECTRA4_RX_PORT=5 -DTIKU_KITS_EPAPER_SPECTRA4_RX_PIN=1
 * to read on MISO (P5.1 on FR5994). */
#ifndef TIKU_KITS_EPAPER_SPECTRA4_RX_PORT
#define TIKU_KITS_EPAPER_SPECTRA4_RX_PORT   TIKU_KITS_EPAPER_SPECTRA4_DATA_PORT
#endif
#ifndef TIKU_KITS_EPAPER_SPECTRA4_RX_PIN
#define TIKU_KITS_EPAPER_SPECTRA4_RX_PIN    TIKU_KITS_EPAPER_SPECTRA4_DATA_PIN
#endif

#define SPI3_SCK_PORT   TIKU_KITS_EPAPER_SPECTRA4_SCK_PORT
#define SPI3_SCK_PIN    TIKU_KITS_EPAPER_SPECTRA4_SCK_PIN
#define SPI3_DAT_PORT   TIKU_KITS_EPAPER_SPECTRA4_DATA_PORT
#define SPI3_DAT_PIN    TIKU_KITS_EPAPER_SPECTRA4_DATA_PIN
#define SPI3_RX_PORT    TIKU_KITS_EPAPER_SPECTRA4_RX_PORT
#define SPI3_RX_PIN     TIKU_KITS_EPAPER_SPECTRA4_RX_PIN
#define SPI3_RX_DIFFERS (SPI3_RX_PORT != SPI3_DAT_PORT || \
                          SPI3_RX_PIN  != SPI3_DAT_PIN)

/* Switch a SPI pin between alt-function (SPI peripheral) and GPIO.
 * On the FR5994 LaunchPad SPI uses PxSEL0=1, PxSEL1=0 (primary).
 * Restoring that bit pattern hands the pin back to the SPI
 * peripheral; clearing both makes it a plain GPIO. */
#if defined(__MSP430__)
static volatile uint8_t *p5sel0_reg(void) { return &P5SEL0; }
static volatile uint8_t *p5sel1_reg(void) { return &P5SEL1; }

static void
spi3_pin_to_gpio(uint8_t pin)
{
    *p5sel0_reg() &= (uint8_t)~(1u << pin);
    *p5sel1_reg() &= (uint8_t)~(1u << pin);
}

static void
spi3_pin_to_alt(uint8_t pin)
{
    *p5sel0_reg() |=  (uint8_t)(1u << pin);
    *p5sel1_reg() &= (uint8_t)~(1u << pin);
}

/* Configure the data line as a pulled-up input.  PDLS uses
 * Arduino's pinMode(p, INPUT) which is high-Z, but Arduino MOSI
 * pins typically have a board-level pull-up that biases the line
 * HIGH at idle.  The FR5994 LaunchPad has no such pull on P5.0,
 * so a true high-Z input drifts LOW from parasitic capacitance
 * and OTP reads come back as 0x0000.  Engaging the MCU's
 * internal pull-up (~35 kohm) gives us the same idle-HIGH bias.
 * The panel's push-pull driver easily overrides 35 kohm, so reads
 * for both 0 and 1 bits remain correct.
 *
 * Override at compile time with -DTIKU_KITS_EPAPER_SPECTRA4_READ_PULLUP=0
 * if a panel doesn't tolerate the pull-up (e.g., open-drain output
 * that's too weak to win against it). */
#ifndef TIKU_KITS_EPAPER_SPECTRA4_READ_PULLUP
#define TIKU_KITS_EPAPER_SPECTRA4_READ_PULLUP 1
#endif

static void
spi3_dat_high_z_input(void)
{
    /* Configure the RX pin for input (with optional pull-up) and,
     * if RX is the same pin as DATA, that's the line we read.
     * If RX is a different pin (MISO theory), we still need to
     * make sure the DATA pin isn't fighting -- release it to
     * input as well so it floats. */
#if SPI3_RX_DIFFERS
    /* Release DATA (write line) to input so it doesn't drive. */
    P5DIR &= (uint8_t)~(1u << SPI3_DAT_PIN);
    P5REN &= (uint8_t)~(1u << SPI3_DAT_PIN);
#endif
    /* Configure RX pin as the read input. */
    P5DIR &= (uint8_t)~(1u << SPI3_RX_PIN);
#if TIKU_KITS_EPAPER_SPECTRA4_READ_PULLUP
    P5OUT |=  (uint8_t)(1u << SPI3_RX_PIN);
    P5REN |=  (uint8_t)(1u << SPI3_RX_PIN);
#else
    P5REN &= (uint8_t)~(1u << SPI3_RX_PIN);
#endif
}

static void
spi3_dat_push_pull_output(void)
{
    P5REN &= (uint8_t)~(1u << SPI3_DAT_PIN);
    P5DIR |=  (uint8_t)(1u << SPI3_DAT_PIN);
}

static uint8_t
spi3_dat_read(void)
{
    return (uint8_t)((P5IN >> SPI3_RX_PIN) & 1u);
}

#else /* not MSP430 -- fall back to interface helpers (works only
       * if the platform's dir_in is high-Z by default). */
static void spi3_pin_to_gpio(uint8_t pin) { (void)pin; }
static void spi3_pin_to_alt(uint8_t pin)  { (void)pin; }
static void spi3_dat_high_z_input(void)
{
    (void)tiku_gpio_dir_in(SPI3_DAT_PORT, SPI3_DAT_PIN);
}
static void spi3_dat_push_pull_output(void)
{
    (void)tiku_gpio_dir_out(SPI3_DAT_PORT, SPI3_DAT_PIN);
}
static uint8_t spi3_dat_read(void)
{
    return (uint8_t)tiku_gpio_read(SPI3_DAT_PORT, SPI3_DAT_PIN);
}
#endif

/* Per-phase delay for the OTP bit-bang.  Pervasive's Arduino
 * reference uses delayMicroseconds(1) but Arduino's digitalWrite
 * is itself ~3-5 us, so their effective clock is much slower than
 * a 1 us phase suggests.  At MSP430's faster GPIO we need to add
 * the missing time explicitly -- 5 us per phase gives a ~15 us
 * bit period, comfortably within Q-film tolerance.
 * Override at compile time if a panel needs even slower. */
#ifndef TIKU_KITS_EPAPER_SPECTRA4_BIT_DELAY_US
#define TIKU_KITS_EPAPER_SPECTRA4_BIT_DELAY_US 50u
#endif
#define SPI3_BIT_DELAY_US   TIKU_KITS_EPAPER_SPECTRA4_BIT_DELAY_US

static void
spi3_begin(void)
{
    /* MSP430 trap: tiku_cpu_freq_boot_arch sets P5DIR=0xFF,
     * P5OUT=0x00 at boot for power savings.  When we just clear
     * PSEL on the SPI pins, they snap from peripheral-driven to
     * GPIO-driven LOW -- the host clamps MOSI low at the very
     * moment the panel might be transitioning to drive it for
     * any read protocol.  PDLS on Arduino doesn't trigger this
     * because Arduino's default DDR=0 means the GPIO state is
     * INPUT (high-Z) after pinMode resets.
     *
     * To match Arduino's behaviour: set DIR=0 (input) on the
     * SPI3 pins FIRST so when PSEL clears they go peripheral ->
     * high-Z, not peripheral -> hard LOW.  Only then close the
     * SPI module and configure SCK as bit-bang output.
     */
#if defined(__MSP430__)
    P5REN &= (uint8_t)~((1u << SPI3_SCK_PIN) | (1u << SPI3_DAT_PIN));
    P5DIR &= (uint8_t)~((1u << SPI3_SCK_PIN) | (1u << SPI3_DAT_PIN));
#endif
    spi3_pin_to_gpio(SPI3_SCK_PIN);
    spi3_pin_to_gpio(SPI3_DAT_PIN);
    tiku_spi_close();
    /* Settle: let pin capacitance bleed off the peripheral's
     * residual state and let any panel-side state machine
     * notice that the host has released the bus. */
    tiku_common_delay_ms(5);

    /* Now drive SCK as the bit-bang clock (idle LOW for Mode 0).
     * DAT stays input until spi3_dat_push_pull_output is called
     * inside spi3_write_byte -- so the data line stays high-Z
     * until we deliberately start sending the first command. */
    (void)tiku_gpio_dir_out(SPI3_SCK_PORT, SPI3_SCK_PIN);
    (void)tiku_gpio_clear(SPI3_SCK_PORT, SPI3_SCK_PIN);
#if defined(__MSP430__)
    EPD_DBG("epd: P5SEL0=0x%02x P5SEL1=0x%02x P5DIR=0x%02x "
            "P5OUT=0x%02x P5IN=0x%02x\r\n",
            (unsigned)P5SEL0, (unsigned)P5SEL1,
            (unsigned)P5DIR,  (unsigned)P5OUT, (unsigned)P5IN);
#endif
}

static void
spi3_end(void)
{
    /* Hand the pins back to the SPI peripheral.  The application
     * is expected to call tiku_spi_init() again before driving
     * the panel via 4-wire; we just restore PxSEL bits here. */
    spi3_pin_to_alt(SPI3_SCK_PIN);
    spi3_pin_to_alt(SPI3_DAT_PIN);
}

static void
spi3_write_byte(uint8_t value)
{
    int8_t i;
    spi3_dat_push_pull_output();
    for (i = 7; i >= 0; i--) {
        if ((value >> i) & 1u) {
            (void)tiku_gpio_set(SPI3_DAT_PORT, SPI3_DAT_PIN);
        } else {
            (void)tiku_gpio_clear(SPI3_DAT_PORT, SPI3_DAT_PIN);
        }
        tiku_common_delay_us(SPI3_BIT_DELAY_US);
        (void)tiku_gpio_set(SPI3_SCK_PORT, SPI3_SCK_PIN);
        tiku_common_delay_us(SPI3_BIT_DELAY_US);
        (void)tiku_gpio_clear(SPI3_SCK_PORT, SPI3_SCK_PIN);
        tiku_common_delay_us(SPI3_BIT_DELAY_US);
    }
}

static uint8_t
spi3_read_byte(void)
{
    uint8_t value = 0u;
    int8_t  i;
    /* Caller is responsible for switching DAT to high-Z input
     * BEFORE the surrounding CS-low window opens.  Calling
     * spi3_dat_high_z_input() here would re-introduce the
     * MCU-vs-panel contention bug for inter-byte windows. */
    for (i = 0; i < 8; i++) {
        (void)tiku_gpio_set(SPI3_SCK_PORT, SPI3_SCK_PIN);
        tiku_common_delay_us(SPI3_BIT_DELAY_US);
        value = (uint8_t)(value << 1);
        if (spi3_dat_read()) {
            value |= 1u;
        }
        (void)tiku_gpio_clear(SPI3_SCK_PORT, SPI3_SCK_PIN);
        tiku_common_delay_us(SPI3_BIT_DELAY_US);
    }
    return value;
}

/*---------------------------------------------------------------------------*/
/* 4-WIRE GPIO HELPERS (shared shape with itc_smallcj)                       */
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

/* Q-film BUSY semantics: HIGH = ready, LOW = busy (same as
 * J-film, just the wait-window is much longer). */
static inline uint8_t busy_asserted(const tiku_kits_epaper_pins_t *p)
{
    return tiku_gpio_read(p->busy_port, p->busy_pin) ? 0u : 1u;
}

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
/* 4-WIRE FRAMING (matches small-CJ shape -- the wire format is identical    */
/*                  for index_data / cmd / cmd_data, only the opcodes and    */
/*                  per-byte semantics differ)                               */
/*---------------------------------------------------------------------------*/

static void
send_cmd8(const tiku_kits_epaper_pins_t *p, uint8_t opcode)
{
    dc_command(p);
    cs_low(p);
    (void)tiku_spi_transfer(opcode);
    cs_high(p);
}

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
send_index_data_open(const tiku_kits_epaper_pins_t *p, uint8_t opcode)
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
}

static void
send_index_data_close(const tiku_kits_epaper_pins_t *p)
{
    tiku_common_delay_us(EPD_CS_HOLD_US);
    cs_high(p);
    tiku_common_delay_us(EPD_CS_HOLD_US);
}

static void
send_index_data(const tiku_kits_epaper_pins_t *p,
                 uint8_t opcode, const uint8_t *data, uint16_t size)
{
    send_index_data_open(p, opcode);
    (void)tiku_spi_write(data, size);
    send_index_data_close(p);
}

/*---------------------------------------------------------------------------*/
/* RESET PULSE (Q-film cadence)                                              */
/*---------------------------------------------------------------------------*/

/* Pervasive Spectra-4 reset is parameterised: b_reset(a, b, c, d, e)
 * with idle / RESET-high / RESET-low / RESET-high / CS-high spacings.
 * 1.54" and 2.13" use (10, 10, 20, 40, 10) -- note the 40 ms hold
 * after the LOW pulse, vs the 10 ms used by other sizes. */
static void
hw_reset_154(const tiku_kits_epaper_pins_t *p)
{
    tiku_common_delay_ms(10);
    reset_release(p);
    tiku_common_delay_ms(10);
    reset_assert(p);
    {
        uint8_t busy_low_seen = 0u;
        uint8_t i;
        for (i = 0u; i < 20u; i++) {
            if (!tiku_gpio_read(p->busy_port, p->busy_pin)) {
                busy_low_seen = 1u;
            }
            tiku_common_delay_ms(1);
        }
        EPD_DBG("epd: during reset LOW pulse: BUSY went low? %s\r\n",
                busy_low_seen ? "YES" : "NO (panel not seated/powered?)");
    }
    reset_release(p);
    tiku_common_delay_ms(40);   /* longer hold for 1.54"/2.13" */
    cs_high(p);
    tiku_common_delay_ms(10);
}

/* 4.17" / 2.66" / 2.06" / 4.37" use (20, 10, 20, 10, 10). */
static void
hw_reset_417(const tiku_kits_epaper_pins_t *p)
{
    tiku_common_delay_ms(20);
    reset_release(p);
    tiku_common_delay_ms(10);
    reset_assert(p);
    /* Sample BUSY across the LOW pulse.  A live panel pulls BUSY
     * low during reset; a panel that's not connected (or not
     * powered) leaves BUSY at the board pull-up level.  If we
     * never see a 0 here the panel isn't actually responding,
     * regardless of what comes next. */
    {
        uint8_t busy_low_seen = 0u;
        uint8_t i;
        for (i = 0u; i < 20u; i++) {
            if (!tiku_gpio_read(p->busy_port, p->busy_pin)) {
                busy_low_seen = 1u;
            }
            tiku_common_delay_ms(1);
        }
        EPD_DBG("epd: during reset LOW pulse: BUSY went low? %s\r\n",
                busy_low_seen ? "YES" : "NO (panel not seated/powered?)");
    }
    reset_release(p);
    tiku_common_delay_ms(10);
    cs_high(p);
    tiku_common_delay_ms(10);
}

/*---------------------------------------------------------------------------*/
/* OTP READ (chip ID 0x0605 -- 4.17")                                        */
/*---------------------------------------------------------------------------*/

/* Step 1: chip-ID handshake.  The panel returns a 16-bit ID after
 * a 0x70 probe; bytes are sent MSB then LSB.  Returns 0 on match,
 * negative on mismatch / timeout. */
static int
otp_check_chip_id(const tiku_kits_epaper_pins_t *p, uint16_t expected)
{
    uint16_t id;

    /* Sanity sample: with the line idle and high-Z, what does
     * P5IN read?  If it's stuck at 0 even before any panel
     * activity, the line is being held LOW by something other
     * than the panel (residual MCU drive, wiring fault, etc.). */
    spi3_dat_high_z_input();
    tiku_common_delay_us(20);
    EPD_DBG("epd: idle DAT=%u (high-Z; floats on pull, "
            "if always 0 panel/wiring issue)\r\n",
            (unsigned)spi3_dat_read());

    /* Probe: command 0x70, no data. */
    dc_command(p);
    cs_low(p);
    spi3_write_byte(0x70u);
    cs_high(p);
    /* Release MOSI to high-Z immediately so the MCU stops driving
     * it LOW (boot code sets P5DIR=0xFF P5OUT=0 for power savings;
     * after CS deassert that's an MCU drive against the panel's
     * pending OTP response and the panel never wins). */
    spi3_dat_high_z_input();
    tiku_common_delay_ms(8);

    /* Read two bytes (MSB then LSB), CS-pulsed per byte.  DAT
     * is already high-Z from above; do NOT re-toggle it inside
     * spi3_read_byte during a multi-byte read, that re-introduces
     * the contention window. */
    dc_data(p);
    cs_low(p);
    id = (uint16_t)spi3_read_byte() << 8;
    cs_high(p);
    cs_low(p);
    id = (uint16_t)(id | spi3_read_byte());
    cs_high(p);


    /* Pervasive's library accepts 0x8302 as an alias for 0x0302
     * (a known panel-firmware quirk).  We honour the same alias. */
    if (id == 0x8302u) {
        id = 0x0302u;
    }
    EPD_DBG("epd: OTP chip ID read 0x%04x (expected 0x%04x)\r\n",
            (unsigned)id, (unsigned)expected);
    return (id == expected) ?
        TIKU_KITS_EPAPER_OK : TIKU_KITS_EPAPER_ERR_TIMEOUT;
}

/* Alternative OTP probe via the 4-wire SPI peripheral.
 *
 * If 3-wire bit-bang on MOSI doesn't get a chip ID back (some
 * EXT3-1 hardware revisions appear to be one-way on MOSI -- they
 * pass commands MCU->panel just fine but the panel's OTP-read
 * driver can't pull MOSI back the other direction), we fall back
 * to the same probe done over real 4-wire SPI: MCU drives MOSI as
 * normal, the peripheral clocks SCK, and the panel's response
 * comes back on MISO.  Pervasive's PDLS doesn't ship this path
 * because Arduino's MOSI is always bidirectional; we need it
 * because ours apparently isn't.
 *
 * Returns the chip ID actually read (0xFFFF if MISO is also
 * silent).  Caller decides whether to accept the value. */
static uint16_t
otp_probe_chip_id_4wire(const tiku_kits_epaper_pins_t *p)
{
    uint8_t  bytes[8];
    uint8_t  k;

    /* Slow SPI for OTP probe -- the panel's response drive is
     * weak; 4 MHz is too fast for it to settle. */
    {
        tiku_spi_config_t cfg;
        cfg.mode      = TIKU_SPI_MODE_0;
        cfg.bit_order = TIKU_SPI_MSB_FIRST;
        cfg.prescaler = TIKU_BOARD_SPI_BRW_500KHZ;   /* 500 kHz */
        (void)tiku_spi_init(&cfg);
    }

    /* Single transaction with CS held LOW: send 0x70 (command),
     * switch to data mode, clock 8 dummy bytes and capture MISO.
     * Some Pervasive panels expect the entire OTP probe inside a
     * single CS-low window rather than as separate pulses. */
    dc_command(p);
    cs_low(p);
    (void)tiku_spi_transfer(0x70u);
    /* DC switches to data mid-transaction without CS toggling --
     * panel keeps clocking but interprets subsequent bytes as
     * data, not command. */
    dc_data(p);
    for (k = 0; k < 8; k++) {
        bytes[k] = tiku_spi_transfer(0x00u);
    }
    cs_high(p);

    EPD_DBG("epd: 4-wire stream: %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
            bytes[0], bytes[1], bytes[2], bytes[3],
            bytes[4], bytes[5], bytes[6], bytes[7]);

    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

/* OTP read for chip ID 0x0302 (1.54", 2.13", 2.66" -- the small
 * Q-film panels share this controller).  The sequence is shorter
 * than 0x0605 and uses different opcodes (0xA4 to position the
 * read pointer, 0xA1 to trigger the read).  After a 1-byte dummy
 * we expect 0xA5 as the first OTP byte; mismatch means panel
 * doesn't have a valid OTP table at offset 0. */
static int
otp_read_0x0302(const tiku_kits_epaper_pins_t *p,
                 uint8_t *buf, uint16_t bytes)
{
    uint8_t  dummy;
    uint16_t i;

    /* Position read pointer: cmd 0xA4, data {0x15, 0x00, 0x01}. */
    dc_command(p);
    cs_low(p);  spi3_write_byte(0xA4u);                cs_high(p);
    dc_data(p);
    cs_low(p);  spi3_write_byte(0x15u);                cs_high(p);
    cs_low(p);  spi3_write_byte(0x00u);                cs_high(p);
    cs_low(p);  spi3_write_byte(0x01u);                cs_high(p);

    /* Wait for the panel to be ready before triggering the read.
     * Pervasive's library inserts b_waitBusy() here. */
    {
        uint16_t k;
        for (k = 0; k < EPD_BUSY_POLL_MAX; k++) {
            if (!busy_asserted(p)) break;
            tiku_common_delay_ms(20);
        }
        if (k >= EPD_BUSY_POLL_MAX) return TIKU_KITS_EPAPER_ERR_BUSY;
    }

    /* cmd 0xA1 -> trigger read. */
    dc_command(p);
    cs_low(p);  spi3_write_byte(0xA1u);                cs_high(p);

    /* Release the line BEFORE asserting CS for the data read --
     * MCU's residual LOW drive (P5DIR=0xFF P5OUT=0 from boot
     * defaults) would otherwise short out the panel's response. */
    spi3_dat_high_z_input();

    /* Dummy byte. */
    dc_data(p);
    cs_low(p);  dummy = spi3_read_byte(); (void)dummy; cs_high(p);

    /* First real byte -- must equal 0xA5. */
    cs_low(p);  buf[0] = spi3_read_byte();             cs_high(p);
    if (buf[0] != 0xA5u) {
        EPD_DBG("epd: OTP[0]=0x%02x, expected 0xa5 (no valid table)\r\n",
                (unsigned)buf[0]);
        return TIKU_KITS_EPAPER_ERR_TIMEOUT;
    }
    /* Read remaining bytes. */
    for (i = 1u; i < bytes; i++) {
        cs_low(p); buf[i] = spi3_read_byte(); cs_high(p);
    }
    return TIKU_KITS_EPAPER_OK;
}

/* Step 2: read the OTP table for chip ID 0x0605 (4.17").  The
 * sequence below positions the OTP read pointer, then streams
 * `bytes` bytes into `buf`.  `buf[0]` must equal 0xA5 for the
 * table to be considered valid; if not we retry at offset 0x70
 * (some panel revisions ship a backup table at that offset). */
static int
otp_read_0x0605(const tiku_kits_epaper_pins_t *p,
                 uint8_t *buf, uint16_t bytes)
{
    uint8_t  dummy;
    uint16_t offset;
    uint16_t i;

    /* Position read pointer: cmd 0xA2, data {0x00, 0x15, 0x00}. */
    dc_command(p);
    cs_low(p);  spi3_write_byte(0xA2u);                cs_high(p);
    dc_data(p);
    cs_low(p);  spi3_write_byte(0x00u);                cs_high(p);
    cs_low(p);  spi3_write_byte(0x15u);                cs_high(p);
    cs_low(p);  spi3_write_byte(0x00u);                cs_high(p);

    /* cmd 0xA0 -> read mode. */
    dc_command(p);
    cs_low(p);  spi3_write_byte(0xA0u);                cs_high(p);

    /* cmd 0x92 -> trigger read. */
    dc_command(p);
    cs_low(p);  spi3_write_byte(0x92u);                cs_high(p);

    /* Release the line BEFORE asserting CS for the data read --
     * otherwise the MCU's residual LOW drive (P5DIR=0xFF P5OUT=0
     * from the boot defaults) shorts out the panel's response. */
    spi3_dat_high_z_input();

    /* Dummy data byte (per app note). */
    dc_data(p);
    cs_low(p);  dummy = spi3_read_byte(); (void)dummy; cs_high(p);

    /* First real byte. */
    cs_low(p);  buf[0] = spi3_read_byte();             cs_high(p);

    if (buf[0] == 0xA5u) {
        offset = 0u;
    } else {
        /* Backup table at 0x70.  Skip 0x70 - 1 already-read byte. */
        offset = 0x70u;
        for (i = 1u; i < offset; i++) {
            cs_low(p); (void)spi3_read_byte(); cs_high(p);
        }
        cs_low(p); buf[0] = spi3_read_byte(); cs_high(p);
        if (buf[0] != 0xA5u) {
            return TIKU_KITS_EPAPER_ERR_TIMEOUT;
        }
    }
    /* Read remaining bytes. */
    for (i = 1u; i < bytes; i++) {
        cs_low(p); buf[i] = spi3_read_byte(); cs_high(p);
    }
    return TIKU_KITS_EPAPER_OK;
}

/*---------------------------------------------------------------------------*/
/* PER-PANEL INIT SEQUENCES                                                  */
/*---------------------------------------------------------------------------*/

/* 1.54" / 2.13" / 2.66" Q-film (chip ID 0x0302) init.  Mirrors
 * Pervasive's Pervasive_BWRY_Small::COG_initial() for
 * eScreen_EPD_154_QS_0F / 213_QS_0F / 266_QS_0F.  The OTP
 * positions are slightly different from the 4.17". */
static int
init_154_family(tiku_kits_epaper_t *epd, const uint8_t *otp)
{
    const tiku_kits_epaper_pins_t *p = &epd->pins;

    /* Temperature first. */
    send_cmd_data8(p, EPD_OP_TEMP_ACTIVE, 0x02u);
    send_cmd_data8(p, EPD_OP_TEMP_INPUT,  epd->temperature);

    /* Soft-reset / wakeup, then BUSY wait. */
    send_cmd8(p, 0xA5u);
    if (wait_ready(p) != TIKU_KITS_EPAPER_OK) {
        return TIKU_KITS_EPAPER_ERR_BUSY;
    }

    send_index_data(p, 0x01u, &otp[16], 1);
    send_index_data(p, 0x00u, &otp[17], 2);
    send_index_data(p, 0x03u, &otp[30], 3);
    send_index_data(p, 0x06u, &otp[23], 7);

    send_cmd_data8(p, 0x50u, otp[39]);
    send_index_data(p, 0x60u, &otp[40], 2);
    send_index_data(p, 0x61u, &otp[19], 4);
    send_cmd_data8(p, 0xE7u, otp[33]);
    send_cmd_data8(p, 0xE3u, otp[42]);

    send_cmd_data8(p, 0x4Du, otp[43]);
    send_cmd_data8(p, 0xB4u, otp[44]);
    send_cmd_data8(p, 0xB5u, otp[45]);

    send_cmd_data8(p, 0xE9u, 0x01u);
    send_cmd_data8(p, 0x30u, 0x08u);   /* PLL */

    return TIKU_KITS_EPAPER_OK;
}

/* 4.17" E2417QS0A3 init.  Mirrors Pervasive's
 * Pervasive_BWRY_Small::COG_initial() case eScreen_EPD_417_QS_0A,
 * including the documented fall-through into the 437 sequence. */
static int
init_417(tiku_kits_epaper_t *epd, const uint8_t *otp)
{
    const tiku_kits_epaper_pins_t *p = &epd->pins;

    send_cmd_data8(p, EPD_OP_TEMP_ACTIVE, 0x02u);
    send_cmd_data8(p, EPD_OP_TEMP_INPUT,  epd->temperature);

    /* 417-specific block. */
    send_index_data(p, 0x01u, &otp[16], 1);
    send_index_data(p, 0x00u, &otp[17], 2);
    send_index_data(p, 0x03u, &otp[30], 3);
    send_index_data(p, 0x06u, &otp[23], 3);
    send_cmd_data8(p, 0x50u, otp[39]);
    send_index_data(p, 0x60u, &otp[40], 2);
    send_index_data(p, 0x61u, &otp[19], 4);
    send_cmd_data8(p, 0xE3u, otp[42]);
    send_cmd_data8(p, 0xE7u, otp[33]);
    send_index_data(p, 0x65u, &otp[34], 4);
    send_cmd_data8(p, 0x30u, otp[38]);
    send_cmd_data8(p, 0xE9u, 0x01u);

    send_cmd8(p, EPD_OP_POWER_ON);
    if (wait_ready(p) != TIKU_KITS_EPAPER_OK) {
        return TIKU_KITS_EPAPER_ERR_BUSY;
    }

    /* Pervasive's library deliberately falls through into the
     * 437 init block here.  Reproduce that without the fall-
     * through goto chain. */
    send_cmd8(p, 0xA5u);
    if (wait_ready(p) != TIKU_KITS_EPAPER_OK) {
        return TIKU_KITS_EPAPER_ERR_BUSY;
    }
    send_index_data(p, 0x00u, &otp[17], 2);
    send_index_data(p, 0x01u, &otp[16], 1);
    send_index_data(p, 0x03u, &otp[30], 3);
    send_index_data(p, 0x06u, &otp[23], 3);
    send_index_data(p, 0x30u, &otp[38], 1);
    send_index_data(p, 0x50u, &otp[39], 1);
    send_index_data(p, 0x60u, &otp[40], 2);
    send_index_data(p, 0x61u, &otp[19], 4);
    send_index_data(p, 0x65u, &otp[34], 4);
    send_index_data(p, 0xE7u, &otp[33], 1);
    send_index_data(p, 0xE3u, &otp[42], 1);
    send_cmd_data8(p, 0xE9u, 0x01u);

    return TIKU_KITS_EPAPER_OK;
}

/* 4.17" power-off tail.  After 0x02 / BUSY wait the application
 * note says: 5 s settle, re-write PSR from OTP[26..27], 100 ms. */
static int
power_off_tail_417(tiku_kits_epaper_t *epd, const uint8_t *otp)
{
    const tiku_kits_epaper_pins_t *p = &epd->pins;

    tiku_common_delay_ms(5000);
    send_index_data(p, EPD_OP_PSR, &otp[26], 2);
    tiku_common_delay_ms(100);
    return TIKU_KITS_EPAPER_OK;
}

/*---------------------------------------------------------------------------*/
/* FRAMEBUFFER PACKING (kit two-plane -> Spectra-4 single 2bpp buffer)       */
/*---------------------------------------------------------------------------*/

/* Map (black_bit << 1 | red_bit) onto the 2-bit BWRY code the
 * controller expects. The kit's decode_colour() guarantees:
 *
 *     WHITE  -> (0, 0)   BWRY 0b11
 *     RED    -> (0, 1)   BWRY 0b01
 *     BLACK  -> (1, 0)   BWRY 0b00
 *     YELLOW -> (1, 1)   BWRY 0b10
 *
 * Indexing by ((b<<1) | r) lets us LUT the conversion. */
static const uint8_t bwry_code_lut[4] = {
    /* (0,0) WHITE  */ 0x3u,
    /* (0,1) RED    */ 0x1u,
    /* (1,0) BLACK  */ 0x0u,
    /* (1,1) YELLOW */ 0x2u,
};

/* Pack 4 source pixels (one nibble's worth) from a byte-pair of
 * the kit's two planes into one packed 2bpp output byte.
 *
 *   bit_offset = 4 -> source bits 7..4 (leftmost 4 pixels of the
 *                     source byte)
 *   bit_offset = 0 -> source bits 3..0 (rightmost 4 pixels)
 *
 * Output: MSB = leftmost-pixel code (matches controller scan
 * order: MSB = first pixel out the gate). */
static inline uint8_t
pack_quad(uint8_t black_byte, uint8_t red_byte, uint8_t bit_offset)
{
    uint8_t out = 0u;
    int8_t  i;
    /* Emit leftmost source pixel first so it lands in output MSB. */
    for (i = 3; i >= 0; i--) {
        uint8_t shift = (uint8_t)(bit_offset + (uint8_t)i);
        uint8_t bb    = (uint8_t)((black_byte >> shift) & 1u);
        uint8_t rb    = (uint8_t)((red_byte   >> shift) & 1u);
        uint8_t code  = bwry_code_lut[(bb << 1) | rb];
        out = (uint8_t)((out << 2) | code);
    }
    return out;
}

/* Stream the entire frame.  CS / DC framing already opened by
 * the caller.  Walks the kit's two planes row by row, packs
 * each row into a stack scratch, and pushes it via spi_write.
 *
 * Buffer orientation note: unlike the J-film small-CJ driver
 * (which uses width = SHORT axis), the Spectra-4 controller
 * scans along the panel's LONG axis -- so our Q-film panel
 * descriptors set width = LONG, height = SHORT.  Pervasive's
 * own test images confirm this layout (100 bytes/row * 300
 * rows = 30,000 bytes for the 4.17"). */
static void
stream_packed_frame(tiku_kits_epaper_t *epd)
{
    const uint16_t w             = epd->panel->width;          /* 400 */
    const uint16_t h             = epd->panel->height;         /* 300 */
    const uint16_t bytes_per_row = (uint16_t)((w + 7u) / 8u);  /* 50  */

    /* Largest supported row = 100 packed bytes (400 px at 2bpp).
     * 4.37" Q-film also fits comfortably under this ceiling. */
    uint8_t  row_scratch[100];
    uint16_t y, i;

    for (y = 0u; y < h; y++) {
        const uint8_t *blk = epd->framebuffer +
                             (uint32_t)y * bytes_per_row;
        const uint8_t *red = (epd->framebuffer_red != NULL) ?
                              epd->framebuffer_red +
                              (uint32_t)y * bytes_per_row : NULL;
        for (i = 0u; i < bytes_per_row; i++) {
            uint8_t bb = blk[i];
            uint8_t rb = (red != NULL) ? red[i] : 0u;
            row_scratch[2u * i]     = pack_quad(bb, rb, 4u);
            row_scratch[2u * i + 1] = pack_quad(bb, rb, 0u);
        }
        (void)tiku_spi_write(row_scratch,
                              (uint16_t)(2u * bytes_per_row));
    }
}

/*---------------------------------------------------------------------------*/
/* DRIVER ENTRY POINTS                                                       */
/*---------------------------------------------------------------------------*/

static int
spectra4_init(tiku_kits_epaper_t *epd)
{
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
spectra4_refresh(tiku_kits_epaper_t *epd)
{
    const tiku_kits_epaper_itc_spectra4_data_t *fd =
        (const tiku_kits_epaper_itc_spectra4_data_t *)epd->panel->family_data;
    const tiku_kits_epaper_pins_t *p = &epd->pins;
    int rc;

    if (fd->otp_bytes > EPD_OTP_MAX) {
        return TIKU_KITS_EPAPER_ERR_PARAM;
    }

    /* Optional panel-power cycle for Q-film OTP reset.  Only
     * runs on boards with a panel-power gate wired (EXT4); on
     * EXT3-1 the kit's panel_power_on/off are no-ops.
     *
     * IMPORTANT: release SPI and discrete-control pins to true
     * high-Z (DIR=0, REN=0) before dropping panel Vcc -- driving
     * signal lines into an unpowered IC clamps its protection
     * diodes and can leave the controller refusing to re-enable
     * its OTP output. */
    if (epd->pins.power_port != 0u) {
        EPD_DBG("epd: panel power cycle\r\n");
#if defined(__MSP430__)
        P5REN &= (uint8_t)~((1u << SPI3_SCK_PIN) | (1u << SPI3_DAT_PIN));
        P5DIR &= (uint8_t)~((1u << SPI3_SCK_PIN) | (1u << SPI3_DAT_PIN));
        P3REN &= (uint8_t)~((1u << epd->pins.cs_pin) |
                             (1u << epd->pins.dc_pin) |
                             (1u << epd->pins.reset_pin));
        P3DIR &= (uint8_t)~((1u << epd->pins.cs_pin) |
                             (1u << epd->pins.dc_pin) |
                             (1u << epd->pins.reset_pin));
#endif
        tiku_kits_epaper_panel_power_off(epd);
        tiku_common_delay_ms(300);          /* full discharge */
        tiku_kits_epaper_panel_power_on(epd); /* includes 50 ms settle */
        /* Re-drive control lines to the state b_resume() leaves
         * them in (CS=HIGH, DC=HIGH, RESET=HIGH all output). */
        (void)tiku_gpio_dir_out(epd->pins.cs_port,    epd->pins.cs_pin);
        (void)tiku_gpio_set(epd->pins.cs_port,        epd->pins.cs_pin);
        (void)tiku_gpio_dir_out(epd->pins.dc_port,    epd->pins.dc_pin);
        (void)tiku_gpio_set(epd->pins.dc_port,        epd->pins.dc_pin);
        (void)tiku_gpio_dir_out(epd->pins.reset_port, epd->pins.reset_pin);
        (void)tiku_gpio_set(epd->pins.reset_port,     epd->pins.reset_pin);
        tiku_common_delay_ms(30);           /* panel boot */
    }

    /* Reset + initial BUSY wait (Q-film holds BUSY low after
     * reset, unlike J-film).  Timing pulse is panel-specific. */
    EPD_DBG("epd: reset begin, BUSY=%u\r\n",
            (unsigned)tiku_gpio_read(p->busy_port, p->busy_pin));
    if (fd->hw_reset != NULL) {
        fd->hw_reset(p);
    } else {
        hw_reset_417(p);
    }
    EPD_DBG("epd: reset done,  BUSY=%u\r\n",
            (unsigned)tiku_gpio_read(p->busy_port, p->busy_pin));
    rc = wait_ready(p);
    if (rc != TIKU_KITS_EPAPER_OK) {
        EPD_DBG("epd: BUSY never released after reset\r\n");
        return rc;
    }
    EPD_DBG("epd: BUSY released, BUSY=%u\r\n",
            (unsigned)tiku_gpio_read(p->busy_port, p->busy_pin));

    /* OTP read, once per process.  3-wire SPI bit-bang on the
     * SPI peripheral pins; suspend the SPI module while we
     * borrow the lines. */
    if (!s_otp_valid) {
        EPD_DBG("epd: spi3 begin (bit_delay=%u us)\r\n",
                (unsigned)SPI3_BIT_DELAY_US);
        spi3_begin();
        rc = otp_check_chip_id(p, fd->chip_id);
        if (rc == TIKU_KITS_EPAPER_OK && fd->read_otp != NULL) {
            rc = fd->read_otp(p, s_otp, fd->otp_bytes);
        }
        spi3_end();

        /* If the 3-wire probe failed (no MOSI response from the
         * panel), retry the chip-ID read over 4-wire SPI with
         * the response on MISO.  Some EXT3-1 hardware paths are
         * effectively one-way on MOSI; the panel can still drive
         * MISO during a normal 4-wire SPI clocking. */
        if (rc != TIKU_KITS_EPAPER_OK) {
            uint16_t alt_id;
            /* hw_reset_417 may need to run again before the alt
             * probe so the panel is in a known state. */
            if (fd->hw_reset != NULL) fd->hw_reset(p);
            else                     hw_reset_417(p);
            (void)wait_ready(p);
            alt_id = otp_probe_chip_id_4wire(p);
            EPD_DBG("epd: 4-wire OTP probe via MISO: 0x%04x\r\n",
                    (unsigned)alt_id);
        }
        /* Reinstate 4-wire SPI before any further panel writes. */
        {
            tiku_spi_config_t cfg;
            cfg.mode      = TIKU_SPI_MODE_0;
            cfg.bit_order = TIKU_SPI_MSB_FIRST;
            cfg.prescaler = TIKU_BOARD_SPI_BRW_4MHZ;
            (void)tiku_spi_init(&cfg);
        }
        if (rc != TIKU_KITS_EPAPER_OK) {
            return rc;
        }
        s_otp_valid = 1u;
        /* Pervasive's library performs another COG_reset after
         * a fresh OTP read.  Repeat for safety. */
        hw_reset_417(p);
        if (wait_ready(p) != TIKU_KITS_EPAPER_OK) {
            return TIKU_KITS_EPAPER_ERR_BUSY;
        }
    }

    /* Run the panel-specific init sequence. */
    rc = fd->run_init(epd, s_otp);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;

    /* Push the packed frame as DTM1 (single buffer, 2bpp). */
    send_index_data_open(p, EPD_OP_DTM1);
    stream_packed_frame(epd);
    send_index_data_close(p);

    /* Update + BUSY wait. */
    send_cmd_data8(p, EPD_OP_DISPLAY_REFRESH, 0x00u);
    rc = wait_ready(p);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;

    return TIKU_KITS_EPAPER_OK;
}

static int
spectra4_sleep(tiku_kits_epaper_t *epd)
{
    const tiku_kits_epaper_itc_spectra4_data_t *fd =
        (const tiku_kits_epaper_itc_spectra4_data_t *)epd->panel->family_data;
    const tiku_kits_epaper_pins_t *p = &epd->pins;
    int rc;

    send_cmd_data8(p, EPD_OP_POWER_OFF, 0x00u);
    rc = wait_ready(p);
    if (rc != TIKU_KITS_EPAPER_OK) return rc;

    if (fd->run_power_off_tail != NULL && s_otp_valid) {
        (void)fd->run_power_off_tail(epd, s_otp);
    }
    return TIKU_KITS_EPAPER_OK;
}

/*---------------------------------------------------------------------------*/
/* OPS VTABLE                                                                */
/*---------------------------------------------------------------------------*/

const tiku_kits_epaper_ops_t tiku_kits_epaper_itc_spectra4_ops = {
    .init    = spectra4_init,
    .refresh = spectra4_refresh,
    .sleep   = spectra4_sleep,
};

/*---------------------------------------------------------------------------*/
/* PANEL DESCRIPTORS                                                         */
/*---------------------------------------------------------------------------*/

/* 1.54" E2154QS0F: square 152 x 152 panel, chip ID 0x0302,
 * 48-byte OTP.  Shares its protocol with the 2.13" and 2.66"
 * Q-film panels (PDLS_BWRY's 154/213/266_QS_0F group). */
static const tiku_kits_epaper_itc_spectra4_data_t e2154qs0f_data = {
    .chip_id            = 0x0302u,
    .otp_bytes          = 48u,
    .hw_reset           = hw_reset_154,
    .read_otp           = otp_read_0x0302,
    .run_init           = init_154_family,
    .run_power_off_tail = NULL,     /* 154 uses the default (none) */
};

const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2154qs0f = {
    .width         = 152u,
    .height        = 152u,
    .colour_planes = 2u,        /* kit-side: black + red planes;
                                   driver packs to 2bpp internally */
    .name          = "E2154QS0F (1.54\" iTC BWRY Spectra-4)",
    .ops           = &tiku_kits_epaper_itc_spectra4_ops,
    .family_data   = &e2154qs0f_data,
};

static const tiku_kits_epaper_itc_spectra4_data_t e2417qs0a3_data = {
    .chip_id            = 0x0605u,
    .otp_bytes          = 112u,
    .hw_reset           = hw_reset_417,
    .read_otp           = otp_read_0x0605,
    .run_init           = init_417,
    .run_power_off_tail = power_off_tail_417,
};

/* Geometry note: Spectra-4 panels deviate from the kit's
 * "width = SHORT axis" convention.  The Q-film controller scans
 * along the panel's LONG axis, so we set width = 400 (long) and
 * height = 300 (short).  This matches Pervasive's own framebuffer
 * layout (100 bytes per row * 300 rows = 30,000 bytes for 4.17"). */
const tiku_kits_epaper_panel_t tiku_kits_epaper_panel_e2417qs0a3 = {
    .width         = 400u,
    .height        = 300u,
    .colour_planes = 2u,        /* kit-side: black + red planes;
                                   driver packs to 2bpp internally */
    .name          = "E2417QS0A3 (4.17\" iTC BWRY Spectra-4)",
    .ops           = &tiku_kits_epaper_itc_spectra4_ops,
    .family_data   = &e2417qs0a3_data,
};
