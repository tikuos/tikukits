/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_p256.h - NIST P-256 (secp256r1) ECDSA signature verify
 *
 * Verification-only ECDSA over the NIST P-256 curve, for authenticating
 * TLS 1.3 server CertificateVerify messages and ECDSA links in an X.509
 * certificate chain.  Operates exclusively on public data (signature,
 * public key, message hash), so it is deliberately NOT constant-time --
 * there are no secrets to protect here.
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

#ifndef TIKU_KITS_CRYPTO_P256_H_
#define TIKU_KITS_CRYPTO_P256_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Length of a P-256 field element / coordinate / scalar, in bytes. */
#define TIKU_KITS_CRYPTO_P256_LEN   32

/** Verify result: signature is valid. */
#define TIKU_KITS_CRYPTO_P256_OK        0
/** Verify result: signature is invalid (or an input was malformed). */
#define TIKU_KITS_CRYPTO_P256_BAD     (-1)

/**
 * @brief Verify an ECDSA P-256 signature.
 *
 * All multi-byte values are big-endian (the wire/X.509 convention).
 *
 * @param qx     Public-key X coordinate (32 bytes, big-endian).
 * @param qy     Public-key Y coordinate (32 bytes, big-endian).
 * @param hash   Message hash (typically a SHA-256 digest).
 * @param hashlen Length of @p hash in bytes; the leftmost 256 bits are used.
 * @param r      Signature r component (32 bytes, big-endian).
 * @param s      Signature s component (32 bytes, big-endian).
 * @return TIKU_KITS_CRYPTO_P256_OK if the signature verifies, else
 *         TIKU_KITS_CRYPTO_P256_BAD.
 */
int tiku_kits_crypto_p256_ecdsa_verify(
    const uint8_t qx[TIKU_KITS_CRYPTO_P256_LEN],
    const uint8_t qy[TIKU_KITS_CRYPTO_P256_LEN],
    const uint8_t *hash, size_t hashlen,
    const uint8_t r[TIKU_KITS_CRYPTO_P256_LEN],
    const uint8_t s[TIKU_KITS_CRYPTO_P256_LEN]);

/** Length of an uncompressed P-256 public point: 0x04 || X(32) || Y(32). */
#define TIKU_KITS_CRYPTO_P256_PUB_LEN  65

/**
 * @brief Generate an (ephemeral) ECDH key pair.
 *
 * Derives the private scalar d from @p seed (d = seed mod n) and computes the
 * public point Q = d*G in uncompressed form.  Intended for ephemeral ECDHE:
 * pass 32 fresh random bytes as @p seed.  The scalar multiply is best-effort
 * constant-time (double-and-add-always).
 *
 * @param seed  32 random bytes (big-endian).
 * @param priv  Output: the private scalar d (32 bytes, big-endian).
 * @param pub   Output: Q as 0x04 || X || Y (65 bytes).
 * @return _OK, or _BAD if d reduced to zero (reseed and retry).
 */
int tiku_kits_crypto_p256_ecdh_keypair(
    const uint8_t seed[TIKU_KITS_CRYPTO_P256_LEN],
    uint8_t priv[TIKU_KITS_CRYPTO_P256_LEN],
    uint8_t pub[TIKU_KITS_CRYPTO_P256_PUB_LEN]);

/**
 * @brief Compute the ECDH shared secret d*Ppeer.
 *
 * Validates @p peer (uncompressed, coordinates < p, on the curve), multiplies
 * by the private scalar, and outputs the X coordinate of the result -- the
 * shared secret as used by TLS.
 *
 * @param priv     The private scalar (32 bytes, big-endian).
 * @param peer     Peer public point, 0x04 || X || Y (65 bytes).
 * @param out_x    Output: shared secret = (d*Ppeer).x (32 bytes, big-endian).
 * @return _OK, or _BAD if the peer point is invalid.
 */
int tiku_kits_crypto_p256_ecdh_shared(
    const uint8_t priv[TIKU_KITS_CRYPTO_P256_LEN],
    const uint8_t peer[TIKU_KITS_CRYPTO_P256_PUB_LEN],
    uint8_t out_x[TIKU_KITS_CRYPTO_P256_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_P256_H_ */
