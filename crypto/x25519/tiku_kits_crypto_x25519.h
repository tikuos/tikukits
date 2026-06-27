/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_x25519.h - X25519 (Curve25519 ECDH) key agreement
 *
 * Implements the X25519 function of RFC 7748: a Montgomery-ladder scalar
 * multiplication on Curve25519, used as the ephemeral key-exchange (ECDHE)
 * primitive for TLS 1.3.  Constant-time with respect to the scalar.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_X25519_H_
#define TIKU_KITS_CRYPTO_X25519_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Length in bytes of an X25519 scalar, u-coordinate, and shared secret. */
#define TIKU_KITS_CRYPTO_X25519_LEN     32

/**
 * @brief X25519 scalar multiplication: out = scalar * point.
 *
 * Computes the Curve25519 Montgomery-ladder product of a 32-byte scalar
 * and a 32-byte u-coordinate (little-endian, per RFC 7748).  The scalar is
 * clamped internally (bits set/cleared as the spec requires), so the caller
 * may pass raw random bytes.  Constant-time in the scalar.
 *
 * @param out    32-byte output u-coordinate / shared secret.
 * @param scalar 32-byte secret scalar.
 * @param point  32-byte input u-coordinate (peer public key).
 */
void tiku_kits_crypto_x25519_scalarmult(
    uint8_t out[TIKU_KITS_CRYPTO_X25519_LEN],
    const uint8_t scalar[TIKU_KITS_CRYPTO_X25519_LEN],
    const uint8_t point[TIKU_KITS_CRYPTO_X25519_LEN]);

/**
 * @brief X25519 public-key derivation: out = scalar * basepoint (u=9).
 *
 * Convenience wrapper that multiplies the secret scalar by the Curve25519
 * base point to produce the corresponding public key.
 *
 * @param out    32-byte public key.
 * @param scalar 32-byte secret scalar.
 */
void tiku_kits_crypto_x25519_base(
    uint8_t out[TIKU_KITS_CRYPTO_X25519_LEN],
    const uint8_t scalar[TIKU_KITS_CRYPTO_X25519_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_X25519_H_ */
