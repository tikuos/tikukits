/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_sha256.c - SHA-256 cryptographic hash (FIPS 180-4)
 *
 * Platform-independent SHA-256 implementation.  All storage is
 * statically allocated; zero heap usage.  Follows FIPS 180-4 for
 * round constants, initial hash values, and padding rules.
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_crypto_sha256.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* ROUND CONSTANTS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief SHA-256 round constants K[0..63] (FIPS 180-4 section 4.2.2)
 *
 * First 32 bits of the fractional parts of the cube roots of the
 * first 64 primes (2..311).
 */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/*---------------------------------------------------------------------------*/
/* HELPER MACROS                                                             */
/*---------------------------------------------------------------------------*/

/** @brief Right-rotate a 32-bit word by n bits. */
#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))

/** @brief Right-shift a 32-bit word by n bits. */
#define SHR(x, n)   ((x) >> (n))

/** @brief SHA-256 Ch(x,y,z) = (x AND y) XOR (NOT x AND z). */
#define CH(x, y, z)   (((x) & (y)) ^ (~(x) & (z)))

/** @brief SHA-256 Maj(x,y,z) = (x AND y) XOR (x AND z) XOR (y AND z). */
#define MAJ(x, y, z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/** @brief SHA-256 big Sigma0(x) = ROTR(2) XOR ROTR(13) XOR ROTR(22). */
#define SIGMA0(x)  (ROTR(x, 2)  ^ ROTR(x, 13) ^ ROTR(x, 22))

/** @brief SHA-256 big Sigma1(x) = ROTR(6) XOR ROTR(11) XOR ROTR(25). */
#define SIGMA1(x)  (ROTR(x, 6)  ^ ROTR(x, 11) ^ ROTR(x, 25))

/** @brief SHA-256 small sigma0(x) = ROTR(7) XOR ROTR(18) XOR SHR(3). */
#define sigma0(x)  (ROTR(x, 7)  ^ ROTR(x, 18) ^ SHR(x, 3))

/** @brief SHA-256 small sigma1(x) = ROTR(17) XOR ROTR(19) XOR SHR(10). */
#define sigma1(x)  (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))

/*---------------------------------------------------------------------------*/
/* BIG-ENDIAN ENCODING / DECODING                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Load a 32-bit word from a big-endian byte array
 */
static uint32_t be32_load(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           ((uint32_t)p[3]);
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Store a 32-bit word into a big-endian byte array
 */
static void be32_store(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
}

/*---------------------------------------------------------------------------*/
/* COMPRESSION FUNCTION                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief SHA-256 compression: process one 64-byte block
 *
 * Implements the SHA-256 block processing per FIPS 180-4 section
 * 6.2.2.  Expands the 16-word message schedule to 64 words, then
 * performs 64 rounds of mixing using the working variables a..h.
 *
 * @param state  Current hash state H0..H7 (updated in place)
 * @param block  Pointer to 64 bytes of input data
 */
static void sha256_compress(uint32_t state[8],
                            const uint8_t block[64])
{
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    uint8_t t;

    /* Prepare the message schedule (FIPS 180-4 section 6.2.2 step 1).
     * First 16 words come directly from the block in big-endian
     * order; words 16..63 are derived from earlier words. */
    for (t = 0; t < 16; t++) {
        W[t] = be32_load(block + t * 4);
    }
    for (t = 16; t < 64; t++) {
        W[t] = sigma1(W[t - 2]) + W[t - 7]
             + sigma0(W[t - 15]) + W[t - 16];
    }

    /* Initialize working variables (step 2). */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    /* 64 rounds of compression (step 3). */
    for (t = 0; t < 64; t++) {
        T1 = h + SIGMA1(e) + CH(e, f, g) + K[t] + W[t];
        T2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    /* Compute the intermediate hash value (step 4). */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a SHA-256 context with the standard IV
 *
 * Sets the eight state words to the initial hash values defined in
 * FIPS 180-4 section 5.3.3 (first 32 bits of the fractional parts
 * of the square roots of the first eight primes).
 */
int tiku_kits_crypto_sha256_init(
    tiku_kits_crypto_sha256_ctx_t *ctx)
{
    if (ctx == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;

    ctx->count_lo = 0;
    ctx->count_hi = 0;
    ctx->buf_len  = 0;

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* INCREMENTAL HASHING                                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Feed data into the SHA-256 computation
 *
 * Buffers incoming bytes until a full 64-byte block is available,
 * then compresses.  Handles arbitrary input lengths including zero.
 * The bit-length counter is maintained as a split 32+32-bit pair to
 * avoid requiring 64-bit arithmetic on 16-bit targets.
 */
int tiku_kits_crypto_sha256_update(
    tiku_kits_crypto_sha256_ctx_t *ctx,
    const uint8_t *data,
    size_t len)
{
    uint32_t bits;

    if (ctx == NULL || data == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* Update the 64-bit bit counter (split across two uint32_t
     * fields so that we avoid 64-bit types on MSP430). */
    bits = (uint32_t)len << 3;
    ctx->count_lo += bits;
    if (ctx->count_lo < bits) {
        ctx->count_hi++;    /* carry */
    }
    ctx->count_hi += (uint32_t)((uint32_t)len >> 29);

    while (len > 0) {
        /* Fill the internal buffer up to BLOCK_SIZE bytes. */
        size_t space = TIKU_KITS_CRYPTO_SHA256_BLOCK_SIZE
                     - ctx->buf_len;
        size_t copy  = (len < space) ? len : space;

        memcpy(ctx->buffer + ctx->buf_len, data, copy);
        ctx->buf_len += (uint8_t)copy;
        data += copy;
        len  -= copy;

        /* Compress when a full block is available. */
        if (ctx->buf_len == TIKU_KITS_CRYPTO_SHA256_BLOCK_SIZE) {
            sha256_compress(ctx->state, ctx->buffer);
            ctx->buf_len = 0;
        }
    }

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* FINALIZATION                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Finalize the hash and write the 32-byte digest
 *
 * Applies FIPS 180-4 padding: append bit '1', then zeros, then the
 * 64-bit message length in big-endian.  If the current partial block
 * has 56 or more bytes, an extra block is needed for the length
 * field.  The final digest is written in big-endian byte order.
 */
int tiku_kits_crypto_sha256_final(
    tiku_kits_crypto_sha256_ctx_t *ctx,
    uint8_t *digest)
{
    uint8_t i;

    if (ctx == NULL || digest == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* Append the mandatory 0x80 byte (bit '1' followed by 7 zero
     * bits).  There is always room because buf_len < 64. */
    ctx->buffer[ctx->buf_len++] = 0x80;

    /* If the current partial block cannot hold the 8-byte length
     * field (need at least 8 free bytes), pad it with zeros and
     * compress, then start a fresh padding block. */
    if (ctx->buf_len > 56) {
        memset(ctx->buffer + ctx->buf_len, 0,
               TIKU_KITS_CRYPTO_SHA256_BLOCK_SIZE
               - ctx->buf_len);
        sha256_compress(ctx->state, ctx->buffer);
        ctx->buf_len = 0;
    }

    /* Zero-pad up to byte 56 (leaving 8 bytes for length). */
    memset(ctx->buffer + ctx->buf_len, 0,
           56 - ctx->buf_len);

    /* Append the 64-bit message length in big-endian. */
    be32_store(ctx->buffer + 56, ctx->count_hi);
    be32_store(ctx->buffer + 60, ctx->count_lo);

    /* Compress the final padded block. */
    sha256_compress(ctx->state, ctx->buffer);

    /* Write the digest in big-endian byte order. */
    for (i = 0; i < 8; i++) {
        be32_store(digest + i * 4, ctx->state[i]);
    }

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* ONE-SHOT CONVENIENCE                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute SHA-256 of a complete message in one call
 *
 * Allocates the context on the stack, calls init/update/final, and
 * returns.  The context is automatically reclaimed when the function
 * returns.
 */
int tiku_kits_crypto_sha256_hash(
    const uint8_t *data,
    size_t len,
    uint8_t *digest)
{
    tiku_kits_crypto_sha256_ctx_t ctx;
    int rc;

    if (data == NULL || digest == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    rc = tiku_kits_crypto_sha256_init(&ctx);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    rc = tiku_kits_crypto_sha256_update(&ctx, data, len);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    return tiku_kits_crypto_sha256_final(&ctx, digest);
}
