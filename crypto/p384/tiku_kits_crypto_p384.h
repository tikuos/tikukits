/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_p384.h - NIST P-384 (secp384r1) ECDSA signature verify
 *
 * Verify-only ECDSA over NIST P-384, for authenticating P-384 links in an
 * X.509 chain (ecdsa-with-SHA384) and TLS 1.3 ecdsa_secp384r1_sha384
 * CertificateVerify.  Same design as the P-256 kit -- generic Montgomery
 * (runtime constants), Jacobian a=-3 -- only the limb count (12) and curve
 * constants differ.  Public-data only, so deliberately NOT constant-time.
 *
 * TODO: P-256 and P-384 are byte-identical except NL + constants; unify them
 * into one curve-parameterized module.
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

#ifndef TIKU_KITS_CRYPTO_P384_H_
#define TIKU_KITS_CRYPTO_P384_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Length of a P-384 coordinate / scalar, in bytes. */
#define TIKU_KITS_CRYPTO_P384_LEN   48

#define TIKU_KITS_CRYPTO_P384_OK        0
#define TIKU_KITS_CRYPTO_P384_BAD     (-1)

/**
 * @brief Verify an ECDSA P-384 signature.  All values big-endian.
 *
 * @param qx     Public-key X (48 bytes).
 * @param qy     Public-key Y (48 bytes).
 * @param hash   Message hash (typically a 48-byte SHA-384 digest).
 * @param hashlen Length of @p hash; the leftmost 384 bits are used.
 * @param r      Signature r (48 bytes).
 * @param s      Signature s (48 bytes).
 * @return TIKU_KITS_CRYPTO_P384_OK if valid, else _BAD.
 */
int tiku_kits_crypto_p384_ecdsa_verify(
    const uint8_t qx[TIKU_KITS_CRYPTO_P384_LEN],
    const uint8_t qy[TIKU_KITS_CRYPTO_P384_LEN],
    const uint8_t *hash, size_t hashlen,
    const uint8_t r[TIKU_KITS_CRYPTO_P384_LEN],
    const uint8_t s[TIKU_KITS_CRYPTO_P384_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_P384_H_ */
