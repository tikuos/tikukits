/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_sha256.h - SHA-256 cryptographic hash (FIPS 180-4)
 *
 * Platform-independent SHA-256 implementation for ultra-low-power
 * embedded systems.  All storage is statically allocated; zero heap
 * usage.  Designed for MSP430 MCUs but portable to any C99 target.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_SHA256_H_
#define TIKU_KITS_CRYPTO_SHA256_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_crypto.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* CONSTANTS                                                                 */
/*---------------------------------------------------------------------------*/

/** @brief SHA-256 internal block size in bytes (512 bits). */
#define TIKU_KITS_CRYPTO_SHA256_BLOCK_SIZE   64

/** @brief SHA-256 digest size in bytes (256 bits). */
#define TIKU_KITS_CRYPTO_SHA256_DIGEST_SIZE  32

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/*
 * Holds all state needed to compute a SHA-256 digest incrementally.
 * Declare as a static or stack variable -- no heap allocation is
 * required.  Call init() once, update() zero or more times, then
 * final() to produce the 32-byte digest.
 * Example:
 * tiku_kits_crypto_sha256_init(&ctx);
 * tiku_kits_crypto_sha256_update(&ctx, data, len);
 * tiku_kits_crypto_sha256_final(&ctx, digest);
 */

/**
 * @brief SHA-256 incremental hashing context
 *
 * @struct tiku_kits_crypto_sha256_ctx
 * @note After final() the context is left in an undefined state.
 * Call init() again to reuse it for a new message.
 * @code
 * tiku_kits_crypto_sha256_ctx_t ctx;
 * uint8_t digest[TIKU_KITS_CRYPTO_SHA256_DIGEST_SIZE];
 * @endcode
 */
typedef struct tiku_kits_crypto_sha256_ctx {
    uint32_t state[8];  /**< Intermediate hash value (H0..H7) */
    uint8_t  buffer[TIKU_KITS_CRYPTO_SHA256_BLOCK_SIZE];
                        /**< Partial block accumulation buffer */
    uint32_t count_lo;  /**< Total message length in bits (low 32) */
    uint32_t count_hi;  /**< Total message length in bits (high 32) */
    uint8_t  buf_len;   /**< Bytes currently buffered (0..63) */
} tiku_kits_crypto_sha256_ctx_t;

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a SHA-256 context with the standard IV
 *
 * Resets the context to its initial state per FIPS 180-4 section
 * 5.3.3.  Must be called before the first update().
 *
 * @param ctx  Context to initialize (must not be NULL)
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if ctx is NULL
 */
int tiku_kits_crypto_sha256_init(
    tiku_kits_crypto_sha256_ctx_t *ctx);

/*---------------------------------------------------------------------------*/
/* INCREMENTAL HASHING                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Feed data into the SHA-256 computation
 *
 * Accumulates @p len bytes from @p data into the running hash.  May
 * be called repeatedly to hash data that arrives in chunks.  Internal
 * buffering ensures correct block alignment.
 *
 * @param ctx   Context previously initialized with init()
 *              (must not be NULL)
 * @param data  Input data to hash (must not be NULL if len > 0)
 * @param len   Number of bytes in @p data (0 is a valid no-op)
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if ctx or data is NULL
 */
int tiku_kits_crypto_sha256_update(
    tiku_kits_crypto_sha256_ctx_t *ctx,
    const uint8_t *data,
    size_t len);

/*---------------------------------------------------------------------------*/
/* FINALIZATION                                                              */
/*---------------------------------------------------------------------------*/

/*
 * Applies FIPS 180-4 padding (a 1-bit, zero padding, and the 64-bit
 * big-endian message length), compresses the final block(s), and
 * writes the 32-byte digest in big-endian byte order.
 * After this call the context is consumed and must not be used for
 * further update() calls.  Call init() to reuse the context.
 */

/**
 * @brief Finalize the hash and write the 32-byte digest
 *
 * @param ctx     Context to finalize (must not be NULL)
 * @param digest  Output buffer, at least
 * TIKU_KITS_CRYPTO_SHA256_DIGEST_SIZE bytes
 * (must not be NULL)
 * @return TIKU_KITS_CRYPTO_OK on success,
 * TIKU_KITS_CRYPTO_ERR_NULL if ctx or digest is NULL
 */
int tiku_kits_crypto_sha256_final(
    tiku_kits_crypto_sha256_ctx_t *ctx,
    uint8_t *digest);

/*---------------------------------------------------------------------------*/
/* ONE-SHOT CONVENIENCE                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute SHA-256 of a complete message in one call
 *
 * Convenience wrapper that calls init(), update(), final() using a
 * stack-allocated context.  Equivalent to the three-step sequence but
 * simpler when the entire message is available up front.
 *
 * @param data    Input data to hash (must not be NULL if len > 0)
 * @param len     Number of bytes in @p data
 * @param digest  Output buffer, at least
 *                TIKU_KITS_CRYPTO_SHA256_DIGEST_SIZE bytes
 *                (must not be NULL)
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_NULL if data or digest is NULL
 */
int tiku_kits_crypto_sha256_hash(
    const uint8_t *data,
    size_t len,
    uint8_t *digest);

#endif /* TIKU_KITS_CRYPTO_SHA256_H_ */
