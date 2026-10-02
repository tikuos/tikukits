/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_pbkdf2.h - PBKDF2 with HMAC-SHA1 (RFC 8018, 5.2)
 *
 * A key from a password: WPA2-PSK's PMK is PBKDF2(passphrase, SSID, 4096
 * iterations, 32 bytes).  The work is iterations x output blocks x two
 * SHA-1 blocks; no heap, a few hundred bytes of stack.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_PBKDF2_H_
#define TIKU_KITS_CRYPTO_PBKDF2_H_

#include <stddef.h>
#include "../tiku_kits_crypto.h"

/**
 * @brief Derive @p out_len bytes from @p pass and @p salt over @p iterations
 *        rounds of HMAC-SHA1.
 * @return TIKU_KITS_CRYPTO_OK, _ERR_NULL for a missing buffer, or
 *         _ERR_PARAM for zero iterations
 */
int tiku_kits_crypto_pbkdf2_hmac_sha1(const uint8_t *pass, size_t pass_len,
                                      const uint8_t *salt, size_t salt_len,
                                      uint32_t iterations,
                                      uint8_t *out, size_t out_len);

#endif /* TIKU_KITS_CRYPTO_PBKDF2_H_ */
