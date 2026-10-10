/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_ccm.c - AES-128-CCM* with a 13-byte nonce (RFC 3610)
 *
 * CBC-MAC over B_0, the length-prefixed additional data and the message,
 * each padded to a block; CTR from counter 1 for the data, and counter 0
 * masks the tag.  Encrypt MACs before it encrypts, decrypt after.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_crypto_ccm.h"
#include "../aes128/tiku_kits_crypto_aes128.h"
#include <string.h>

#define CCM_BLOCK 16U

/** @brief The counter block A_i: flags L-1, the nonce, i big-endian. */
static void ccm_counter(uint8_t a[CCM_BLOCK], const uint8_t nonce[13],
                        uint16_t i) {
    a[0] = 1U;
    memcpy(&a[1], nonce, 13U);
    a[14] = (uint8_t)(i >> 8);
    a[15] = (uint8_t)i;
}

/** @brief Encrypt or decrypt @p len bytes from counter 1: out = in ^ E(A_i). */
static void ccm_ctr(const tiku_kits_crypto_aes128_ctx_t *ctx,
                    const uint8_t nonce[13], const uint8_t *in, size_t len,
                    uint8_t *out) {
    uint8_t a[CCM_BLOCK], s[CCM_BLOCK];
    size_t off, i;

    for (off = 0U; off < len; off += CCM_BLOCK) {
        size_t n = (len - off < CCM_BLOCK) ? (len - off) : CCM_BLOCK;
        ccm_counter(a, nonce, (uint16_t)(off / CCM_BLOCK + 1U));
        (void)tiku_kits_crypto_aes128_encrypt(ctx, a, s);
        for (i = 0U; i < n; i++) {
            out[off + i] = in[off + i] ^ s[i];
        }
    }
}

/** @brief Fold @p len bytes into the CBC-MAC state @p x, the last block padded
 *         with zeros; @p first holds bytes already placed in the first block. */
static void ccm_mac_run(const tiku_kits_crypto_aes128_ctx_t *ctx,
                        uint8_t x[CCM_BLOCK], const uint8_t *p, size_t len,
                        const uint8_t *first, size_t first_len) {
    uint8_t blk[CCM_BLOCK];
    size_t pos = 0U, j, i;

    do {
        memset(blk, 0, CCM_BLOCK);
        j = 0U;
        if (first_len != 0U) {
            memcpy(blk, first, first_len);
            j = first_len;
            first_len = 0U;
        }
        for (; j < CCM_BLOCK && pos < len; j++) {
            blk[j] = p[pos++];
        }
        for (i = 0U; i < CCM_BLOCK; i++) {
            x[i] ^= blk[i];
        }
        (void)tiku_kits_crypto_aes128_encrypt(ctx, x, x);
    } while (pos < len);
}

/** @brief The tag T = CBC-MAC(B_0 | l(a) | a | m), masked by E(A_0). */
static void ccm_tag(const tiku_kits_crypto_aes128_ctx_t *ctx,
                    const uint8_t nonce[13], const uint8_t *aad,
                    size_t aad_len, const uint8_t *m, size_t m_len,
                    uint8_t mic_len, uint8_t *mic) {
    uint8_t x[CCM_BLOCK], a[CCM_BLOCK], s[CCM_BLOCK], len2[2];
    uint8_t i;

    x[0] = (uint8_t)(((aad_len != 0U) ? 0x40U : 0U) |
                     (uint8_t)(((mic_len - 2U) / 2U) << 3) | 1U);
    memcpy(&x[1], nonce, 13U);
    x[14] = (uint8_t)(m_len >> 8);
    x[15] = (uint8_t)m_len;
    (void)tiku_kits_crypto_aes128_encrypt(ctx, x, x);
    if (aad_len != 0U) {
        len2[0] = (uint8_t)(aad_len >> 8);
        len2[1] = (uint8_t)aad_len;
        ccm_mac_run(ctx, x, aad, aad_len, len2, 2U);
    }
    if (m_len != 0U) {
        ccm_mac_run(ctx, x, m, m_len, NULL, 0U);
    }
    ccm_counter(a, nonce, 0U);
    (void)tiku_kits_crypto_aes128_encrypt(ctx, a, s);
    for (i = 0U; i < mic_len; i++) {
        mic[i] = x[i] ^ s[i];
    }
}

int tiku_kits_crypto_ccm_star(int decrypt, const uint8_t key[16],
                              const uint8_t nonce[13], const uint8_t *aad,
                              size_t aad_len, const uint8_t *m, size_t m_len,
                              uint8_t mic_len, uint8_t *out, uint8_t *mic) {
    tiku_kits_crypto_aes128_ctx_t ctx;

    if (key == NULL || nonce == NULL || mic == NULL ||
        (aad == NULL && aad_len != 0U) ||
        ((m == NULL || out == NULL) && m_len != 0U)) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }
    if (mic_len != 4U && mic_len != 8U && mic_len != 16U) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    if ((uint32_t)m_len > 0xFFFFUL || (uint32_t)aad_len > 0xFEFFUL) {
        return TIKU_KITS_CRYPTO_ERR_SIZE;
    }
    if (tiku_kits_crypto_aes128_init(&ctx, key) != TIKU_KITS_CRYPTO_OK) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    if (decrypt) {
        ccm_ctr(&ctx, nonce, m, m_len, out);
        ccm_tag(&ctx, nonce, aad, aad_len, out, m_len, mic_len, mic);
    } else {
        ccm_tag(&ctx, nonce, aad, aad_len, m, m_len, mic_len, mic);
        ccm_ctr(&ctx, nonce, m, m_len, out);
    }
    memset(&ctx, 0, sizeof(ctx));
    return TIKU_KITS_CRYPTO_OK;
}
