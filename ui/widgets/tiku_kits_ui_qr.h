/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ui_qr.h - QR-code rendering widget
 *
 * Renders pre-encoded QR module data. The widget itself is a pure
 * renderer; the caller supplies the module bits using any QR
 * encoder (e.g. nayuki/QR-Code-generator port, picoqr, or a
 * future tikukits/codec/qr).
 *
 * Module storage:
 *   Row-major, MSB-first, 1 bit per module. Each row is
 *   ceil(size / 8) bytes wide. A 21x21 QR (version 1) needs
 *   21 * 3 = 63 bytes; a 33x33 QR (version 4) needs 33 * 5 = 165
 *   bytes.
 *
 * Display sizing:
 *   Each QR module is rendered as a `module_px` x `module_px`
 *   pixel square. A `quiet_modules`-wide padding is drawn around
 *   the code (the QR spec recommends 4 modules; barcode scanners
 *   tolerate 2 in practice).
 *
 * Total rendered side length = (size + 2 * quiet_modules) * module_px.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_UI_QR_H_
#define TIKU_KITS_UI_QR_H_

#include "../tiku_kits_ui.h"

typedef struct {
    tiku_kits_ui_widget_t base;
    const uint8_t *modules;
    uint16_t       size;            /* modules per side */
    uint8_t        module_px;       /* pixels per module */
    uint8_t        quiet_modules;   /* padding in modules (0..8) */
} tiku_kits_ui_qr_t;

extern const tiku_kits_ui_widget_ops_t tiku_kits_ui_qr_ops;

/**
 * @brief Initialise a QR widget.
 *
 * The widget rect (w, h) is informational; the actual rendered
 * size is (size + 2*quiet) * module_px on each axis. Pass w, h
 * the same value for layout placement; the widget will not
 * exceed those bounds (clipping handled by the surface).
 */
void tiku_kits_ui_qr_init(tiku_kits_ui_qr_t *q,
                            int16_t x, int16_t y,
                            uint16_t w, uint16_t h,
                            const uint8_t *modules,
                            uint16_t size,
                            uint8_t module_px,
                            uint8_t quiet_modules);

/** Replace the module data without changing geometry. */
void tiku_kits_ui_qr_set_modules(tiku_kits_ui_qr_t *q,
                                   const uint8_t *modules,
                                   uint16_t size);

#endif /* TIKU_KITS_UI_QR_H_ */
