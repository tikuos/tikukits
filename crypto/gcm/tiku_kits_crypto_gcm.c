/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_gcm.c - AES-128-GCM authenticated encryption
 *
 * NIST SP 800-38D compliant AES-128-GCM implementation optimised
 * for embedded targets with no heap allocation.  Uses bit-by-bit
 * schoolbook multiplication in GF(2^128) to keep code size small.
 * The 12-byte nonce format matches TLS 1.3 requirements.
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

#include "tiku_kits_crypto_gcm.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL CONSTANTS                                                        */
/*---------------------------------------------------------------------------*/

/** @brief AES block size, used throughout CTR and GHASH. */
#define BLOCK_SIZE  16

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Increment the last 4 bytes of a 16-byte block as a
 *        big-endian 32-bit integer.
 *
 * Used by CTR mode to step the counter portion of the IV block.
 * Only the rightmost 32 bits (bytes 12..15) are modified.
 *
 * @param block  16-byte counter block (modified in place).
 */
static void incr_counter(uint8_t *block)
{
    int8_t i;

    for (i = BLOCK_SIZE - 1; i >= 12; i--) {
        block[i]++;
        if (block[i] != 0) {
            break;
        }
    }
}

/**
 * @brief Multiply two 128-bit values in GF(2^128).
 *
 * Uses the GCM reduction polynomial
 *   R = x^128 + x^7 + x^2 + x + 1
 * with a bit-by-bit right-to-left schoolbook algorithm.
 * Each iteration examines the MSB of @p x (bit 0 of byte 0)
 * and conditionally XORs the running copy of @p y into the
 * result, then shifts @p y right by one bit with reduction.
 *
 * @param[in]  x       First 16-byte operand.
 * @param[in]  y       Second 16-byte operand.
 * @param[out] result  16-byte product in GF(2^128).
 */
static void ghash_multiply(const uint8_t *x,
                            const uint8_t *y,
                            uint8_t *result)
{
    uint8_t v[BLOCK_SIZE];
    uint8_t bit;
    uint8_t byte_idx;
    uint8_t lsb;
    uint8_t i;
    uint8_t j;

    memset(result, 0, BLOCK_SIZE);
    memcpy(v, y, BLOCK_SIZE);

    for (i = 0; i < BLOCK_SIZE; i++) {
        for (j = 0; j < 8; j++) {
            /* Test current bit of x (MSB first within each byte) */
            bit = (x[i] >> (7 - j)) & 1;

            if (bit) {
                for (byte_idx = 0; byte_idx < BLOCK_SIZE;
                     byte_idx++) {
                    result[byte_idx] ^= v[byte_idx];
                }
            }

            /* Shift v right by one bit with GCM reduction */
            lsb = v[BLOCK_SIZE - 1] & 1;

            for (byte_idx = BLOCK_SIZE - 1; byte_idx > 0;
                 byte_idx--) {
                v[byte_idx] = (v[byte_idx] >> 1)
                            | (v[byte_idx - 1] << 7);
            }
            v[0] >>= 1;

            /* If LSB was set, XOR with R = 0xE1 << 120 */
            if (lsb) {
                v[0] ^= 0xE1;
            }
        }
    }
}

/**
 * @brief Compute GHASH over an arbitrary-length byte string.
 *
 * Processes @p data in 16-byte blocks:
 *   Y_0 = 0^128
 *   Y_i = ghash_multiply(Y_{i-1} XOR X_i, H)
 *
 * If @p data_len is not a multiple of 16 the last partial block
 * is zero-padded before processing.  The caller is responsible
 * for providing the correctly formatted GHASH input (AAD ||
 * pad || CT || pad || len_block).
 *
 * @param[in]  h         16-byte GHASH subkey.
 * @param[in]  data      Input byte string.
 * @param[in]  data_len  Length of data in bytes.
 * @param[out] out       16-byte GHASH output.
 */
static void ghash(const uint8_t *h,
                   const uint8_t *data,
                   uint16_t data_len,
                   uint8_t *out)
{
    uint8_t block[BLOCK_SIZE];
    uint16_t offset;
    uint16_t chunk;
    uint8_t i;

    memset(out, 0, BLOCK_SIZE);

    for (offset = 0; offset < data_len; offset += BLOCK_SIZE) {
        /* Determine how many bytes remain in this block */
        chunk = data_len - offset;
        if (chunk > BLOCK_SIZE) {
            chunk = BLOCK_SIZE;
        }

        /* Copy and zero-pad the current block */
        memcpy(block, data + offset, chunk);
        if (chunk < BLOCK_SIZE) {
            memset(block + chunk, 0, BLOCK_SIZE - chunk);
        }

        /* XOR block into the running hash */
        for (i = 0; i < BLOCK_SIZE; i++) {
            out[i] ^= block[i];
        }

        /* Multiply by H */
        ghash_multiply(out, h, block);
        memcpy(out, block, BLOCK_SIZE);
    }
}

/**
 * @brief AES-CTR mode encryption / decryption.
 *
 * Encrypts (or decrypts, since CTR is symmetric) @p len bytes by
 * generating a keystream from AES(K, nonce || counter_BE) and
 * XOR-ing it with the input.  The counter is a 32-bit big-endian
 * value occupying bytes 12..15 of the 16-byte block.
 *
 * @param[in]  aes            Initialized AES-128 context.
 * @param[in]  nonce          12-byte nonce.
 * @param[in]  counter_start  Initial counter value (big-endian).
 * @param[in]  in             Input bytes.
 * @param[in]  len            Number of bytes to process.
 * @param[out] out            Output bytes (may alias @p in).
 */
static void gcm_ctr(const tiku_kits_crypto_aes128_ctx_t *aes,
                     const uint8_t *nonce,
                     uint32_t counter_start,
                     const uint8_t *in,
                     uint16_t len,
                     uint8_t *out)
{
    uint8_t cb[BLOCK_SIZE];      /* counter block            */
    uint8_t ks[BLOCK_SIZE];      /* keystream block          */
    uint16_t offset;
    uint16_t chunk;
    uint16_t i;

    /* Build the initial counter block: nonce || counter_BE */
    memcpy(cb, nonce, TIKU_KITS_CRYPTO_GCM_NONCE_SIZE);
    cb[12] = (uint8_t)(counter_start >> 24);
    cb[13] = (uint8_t)(counter_start >> 16);
    cb[14] = (uint8_t)(counter_start >> 8);
    cb[15] = (uint8_t)(counter_start);

    for (offset = 0; offset < len; offset += BLOCK_SIZE) {
        /* Encrypt the counter block to produce keystream */
        tiku_kits_crypto_aes128_encrypt(aes, cb, ks);

        /* XOR keystream with input (handle partial last block) */
        chunk = len - offset;
        if (chunk > BLOCK_SIZE) {
            chunk = BLOCK_SIZE;
        }

        for (i = 0; i < chunk; i++) {
            out[offset + i] = in[offset + i] ^ ks[i];
        }

        /* Increment counter for next block */
        incr_counter(cb);
    }
}

/**
 * @brief Constant-time comparison of two byte arrays.
 *
 * XORs all bytes into an accumulator to avoid short-circuit
 * branching.  Uses volatile to prevent compiler optimisation.
 *
 * @param[in] a    First byte array.
 * @param[in] b    Second byte array.
 * @param[in] len  Number of bytes to compare.
 * @return 0 if arrays are equal, non-zero otherwise.
 */
static uint8_t constant_time_compare(const uint8_t *a,
                                      const uint8_t *b,
                                      uint16_t len)
{
    volatile uint8_t diff = 0;
    uint16_t i;

    for (i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff;
}

/**
 * @brief Build the GHASH input and compute the tag.
 *
 * Constructs the GHASH input per NIST SP 800-38D:
 *   AAD || pad_to_16 || CT || pad_to_16 || len(AAD)*8 || len(CT)*8
 * where the lengths are 64-bit big-endian bit counts.  Then XORs
 * the GHASH output with the encrypted J0 block (tag mask) to
 * produce the final authentication tag.
 *
 * @param[in]  ctx      GCM context (provides AES and H).
 * @param[in]  nonce    12-byte nonce.
 * @param[in]  aad      Associated data (may be NULL if aad_len 0).
 * @param[in]  aad_len  AAD length in bytes.
 * @param[in]  ct       Ciphertext.
 * @param[in]  ct_len   Ciphertext length in bytes.
 * @param[out] tag      16-byte authentication tag output.
 */
static void compute_tag(
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *nonce,
    const uint8_t *aad,
    uint16_t aad_len,
    const uint8_t *ct,
    uint16_t ct_len,
    uint8_t *tag)
{
    uint8_t j0[BLOCK_SIZE];      /* initial counter block    */
    uint8_t s[BLOCK_SIZE];       /* AES(K, J0) -- tag mask   */
    uint8_t ghash_out[BLOCK_SIZE];
    uint8_t len_block[BLOCK_SIZE];
    uint8_t pad_block[BLOCK_SIZE];
    uint32_t bits;
    uint8_t i;

    /* ---- Compute J0 = nonce || 0x00000001 ---- */
    memcpy(j0, nonce, TIKU_KITS_CRYPTO_GCM_NONCE_SIZE);
    j0[12] = 0x00;
    j0[13] = 0x00;
    j0[14] = 0x00;
    j0[15] = 0x01;

    /* ---- Tag mask S = AES(K, J0) ---- */
    tiku_kits_crypto_aes128_encrypt(&ctx->aes, j0, s);

    /* ---- GHASH(H, AAD || pad || CT || pad || len_block) ---- */

    /* Start with Y = 0 */
    memset(ghash_out, 0, BLOCK_SIZE);

    /* Process AAD blocks */
    if (aad_len > 0) {
        ghash(ctx->h, aad, aad_len, ghash_out);
    }

    /*
     * If AAD did not end on a 16-byte boundary, the ghash()
     * function already zero-padded the last partial block.
     * However we need to continue hashing CT into the same
     * running state.  Re-enter GHASH manually so the running
     * hash value (ghash_out) is preserved.
     */

    /* Process CT blocks into the running hash */
    if (ct_len > 0) {
        uint16_t offset;
        uint16_t chunk;
        uint8_t block[BLOCK_SIZE];

        for (offset = 0; offset < ct_len;
             offset += BLOCK_SIZE) {
            chunk = ct_len - offset;
            if (chunk > BLOCK_SIZE) {
                chunk = BLOCK_SIZE;
            }

            memcpy(block, ct + offset, chunk);
            if (chunk < BLOCK_SIZE) {
                memset(block + chunk, 0,
                       BLOCK_SIZE - chunk);
            }

            for (i = 0; i < BLOCK_SIZE; i++) {
                ghash_out[i] ^= block[i];
            }

            ghash_multiply(ghash_out, ctx->h, block);
            memcpy(ghash_out, block, BLOCK_SIZE);
        }
    }

    /* Build the 16-byte length block: len(AAD)*8 || len(CT)*8 */
    memset(len_block, 0, BLOCK_SIZE);

    /* AAD bit length as 64-bit big-endian (upper 32 bits zero
     * since aad_len is uint16_t) */
    bits = (uint32_t)aad_len * 8;
    len_block[4] = (uint8_t)(bits >> 24);
    len_block[5] = (uint8_t)(bits >> 16);
    len_block[6] = (uint8_t)(bits >> 8);
    len_block[7] = (uint8_t)(bits);

    /* CT bit length as 64-bit big-endian */
    bits = (uint32_t)ct_len * 8;
    len_block[12] = (uint8_t)(bits >> 24);
    len_block[13] = (uint8_t)(bits >> 16);
    len_block[14] = (uint8_t)(bits >> 8);
    len_block[15] = (uint8_t)(bits);

    /* Hash the length block into the running state */
    for (i = 0; i < BLOCK_SIZE; i++) {
        ghash_out[i] ^= len_block[i];
    }
    ghash_multiply(ghash_out, ctx->h, pad_block);
    memcpy(ghash_out, pad_block, BLOCK_SIZE);

    /* ---- tag = GHASH_result XOR S ---- */
    for (i = 0; i < BLOCK_SIZE; i++) {
        tag[i] = ghash_out[i] ^ s[i];
    }
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_gcm_init(tiku_kits_crypto_gcm_ctx_t *ctx,
                               const uint8_t *key)
{
    uint8_t zero_block[BLOCK_SIZE];
    int rc;

    if (ctx == NULL || key == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* Expand the AES-128 key schedule */
    rc = tiku_kits_crypto_aes128_init(&ctx->aes, key);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    /* Compute GHASH subkey H = AES(K, 0^128) */
    memset(zero_block, 0, BLOCK_SIZE);
    tiku_kits_crypto_aes128_encrypt(&ctx->aes, zero_block,
                                     ctx->h);

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_gcm_encrypt(
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *nonce,
    const uint8_t *aad,
    uint16_t aad_len,
    const uint8_t *pt,
    uint16_t pt_len,
    uint8_t *ct,
    uint8_t *tag)
{
    if (ctx == NULL || nonce == NULL || tag == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (pt_len > 0 && (pt == NULL || ct == NULL)) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (aad_len > 0 && aad == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* CTR-mode encrypt starting at counter = 2 */
    if (pt_len > 0) {
        gcm_ctr(&ctx->aes, nonce, 2, pt, pt_len, ct);
    }

    /* Compute authentication tag over AAD and ciphertext */
    compute_tag(ctx, nonce, aad, aad_len, ct, pt_len, tag);

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_gcm_decrypt(
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *nonce,
    const uint8_t *aad,
    uint16_t aad_len,
    const uint8_t *ct,
    uint16_t ct_len,
    const uint8_t *tag,
    uint8_t *pt)
{
    uint8_t computed_tag[TIKU_KITS_CRYPTO_GCM_TAG_SIZE];

    if (ctx == NULL || nonce == NULL || tag == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (ct_len > 0 && (ct == NULL || pt == NULL)) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (aad_len > 0 && aad == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* Compute the expected tag over AAD and ciphertext */
    compute_tag(ctx, nonce, aad, aad_len, ct, ct_len,
                computed_tag);

    /* Constant-time tag verification */
    if (constant_time_compare(computed_tag, tag,
                              TIKU_KITS_CRYPTO_GCM_TAG_SIZE)
        != 0) {
        /* Authentication failed -- zero output and return */
        if (ct_len > 0) {
            memset(pt, 0, ct_len);
        }
        return TIKU_KITS_CRYPTO_ERR_CORRUPT;
    }

    /* Tag verified -- CTR-mode decrypt (symmetric with encrypt) */
    if (ct_len > 0) {
        gcm_ctr(&ctx->aes, nonce, 2, ct, ct_len, pt);
    }

    return TIKU_KITS_CRYPTO_OK;
}
