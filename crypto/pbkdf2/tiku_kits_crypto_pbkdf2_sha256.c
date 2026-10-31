/*
 * Tiku Operating System
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 * tiku_kits_crypto_pbkdf2_sha256.c - PBKDF2-HMAC-SHA256 with bounded work slices.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tiku_kits_crypto_pbkdf2.h"
#include "../sha256/tiku_kits_crypto_sha256.h"
#include <string.h>
/** @brief Erase temporary key material through volatile stores. */
static void wipe(void *p, size_t n)
{ volatile uint8_t *b = p; while (n--) *b++ = 0; }
int tiku_kits_crypto_pbkdf2_sha256(const uint8_t *pass, size_t pass_len,
    const uint8_t *salt, size_t salt_len, uint32_t rounds, uint8_t out[32],
    void (*progress)(void *), void *context)
{
    if (!pass || !salt || !out) return TIKU_KITS_CRYPTO_ERR_NULL;
    if (!rounds) return TIKU_KITS_CRYPTO_ERR_PARAM;
    tiku_kits_crypto_sha256_ctx_t inner, outer, hash;
    uint8_t block[64] = {0}, u[32], acc[32], index[4] = {0,0,0,1};
    if (pass_len > 64) {
        tiku_kits_crypto_sha256_init(&hash);
        tiku_kits_crypto_sha256_update(&hash, pass, pass_len);
        tiku_kits_crypto_sha256_final(&hash, block);
    } else memcpy(block, pass, pass_len);
    for (unsigned i = 0; i < 64; i++) block[i] ^= 0x36;
    tiku_kits_crypto_sha256_init(&inner);
    tiku_kits_crypto_sha256_update(&inner, block, 64);
    for (unsigned i = 0; i < 64; i++) block[i] ^= 0x36 ^ 0x5c;
    tiku_kits_crypto_sha256_init(&outer);
    tiku_kits_crypto_sha256_update(&outer, block, 64);
    for (uint32_t r = 0; r < rounds; r++) {
        hash = inner;
        if (!r) {
            tiku_kits_crypto_sha256_update(&hash, salt, salt_len);
            tiku_kits_crypto_sha256_update(&hash, index, 4);
        } else tiku_kits_crypto_sha256_update(&hash, u, 32);
        tiku_kits_crypto_sha256_final(&hash, u);
        hash = outer;
        tiku_kits_crypto_sha256_update(&hash, u, 32);
        tiku_kits_crypto_sha256_final(&hash, u);
        if (!r) memcpy(acc, u, 32);
        else for (unsigned i = 0; i < 32; i++) acc[i] ^= u[i];
        if (progress && !(r & 255u)) progress(context);
    }
    memcpy(out, acc, 32);
    wipe(&inner, sizeof inner); wipe(&outer, sizeof outer); wipe(&hash, sizeof hash);
    wipe(block, sizeof block); wipe(u, sizeof u); wipe(acc, sizeof acc);
    return TIKU_KITS_CRYPTO_OK;
}
