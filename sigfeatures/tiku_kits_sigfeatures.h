/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_sigfeatures.h - Common signal feature extraction types
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Provides shared return codes, element type, and helper definitions
 * used across all TikuKits signal feature extraction libraries:
 *
 * - Zero-crossing rate  : sign-change counting per window
 * - Peak detector       : local maxima with configurable hysteresis
 * - Histogram / binning : fixed-width bin accumulation
 * - First-order delta   : x[n] - x[n-1] rate-of-change
 * - Goertzel            : single-frequency energy via DFT bin
 * - Z-score             : fixed-point (x - mean) / stddev normalization
 * - Min-max scale       : map [min, max] to [0, out_max] with clamping
 *
 * All storage is statically allocated; no heap required.
 */

#ifndef TIKU_KITS_SIGFEATURES_H_
#define TIKU_KITS_SIGFEATURES_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/*
 * Defaults to int32_t, which covers the full dynamic range of a
 * 16-bit ADC with headroom for accumulation and difference
 * operations.  On targets with very tight RAM, int16_t is
 * sufficient for raw ADC samples and halves buffer sizes.
 * Override before including this header to change the type:
 */

/**
 * @brief Element type used for all signal feature sample values.
 *
 * @code
 * #define TIKU_KITS_SIGFEATURES_ELEM_TYPE int16_t
 * #include "tiku_kits_sigfeatures.h"
 * @endcode
 */
#ifndef TIKU_KITS_SIGFEATURES_ELEM_TYPE
#define TIKU_KITS_SIGFEATURES_ELEM_TYPE int32_t
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/*
 * This typedef resolves to TIKU_KITS_SIGFEATURES_ELEM_TYPE (default
 * int32_t).  Every sub-module -- ZCR, peak, histogram, delta,
 * Goertzel, z-score, and min-max scale -- uses this type for input
 * samples, thresholds, and per-sample outputs so that a single
 * compile-time switch changes the precision globally.
 */

/**
 * @brief Scalar element type used for all signal sample values.
 *
 * @typedef tiku_kits_sigfeatures_elem_t
 */
typedef TIKU_KITS_SIGFEATURES_ELEM_TYPE tiku_kits_sigfeatures_elem_t;

/*---------------------------------------------------------------------------*/
/* RETURN CODES                                                              */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_SIGFEATURES_STATUS Signal Features Status Codes
 * @{ */
#define TIKU_KITS_SIGFEATURES_OK            0   /**< Operation succeeded */
#define TIKU_KITS_SIGFEATURES_ERR_NULL    (-1)  /**< NULL pointer argument */
#define TIKU_KITS_SIGFEATURES_ERR_SIZE    (-2)  /**< Buffer/window size error */
#define TIKU_KITS_SIGFEATURES_ERR_PARAM   (-3)  /**< Invalid parameter */
#define TIKU_KITS_SIGFEATURES_ERR_NODATA  (-4)  /**< No data available yet */
/** @} */

#endif /* TIKU_KITS_SIGFEATURES_H_ */
