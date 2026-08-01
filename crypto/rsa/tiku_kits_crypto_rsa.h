/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_rsa.h - RSA signature verification (PKCS#1 v1.5 + PSS)
 *
 * Verification-only RSA over moduli up to 4096 bits, for authenticating
 * RSA links in an X.509 certificate chain (PKCS#1 v1.5, sha256WithRSA-
 * Encryption) and TLS 1.3 server CertificateVerify (RSASSA-PSS, MGF1-
 * SHA-256).  Public-key operation with a small public exponent, so the
 * modexp is cheap; nothing here is secret, so it is not constant-time.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_RSA_H_
#define TIKU_KITS_CRYPTO_RSA_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Largest supported modulus, in bytes (4096-bit). */
#define TIKU_KITS_CRYPTO_RSA_MAX_BYTES   512

#define TIKU_KITS_CRYPTO_RSA_OK     0
#define TIKU_KITS_CRYPTO_RSA_BAD  (-1)

/**
 * @brief Verify an RSASSA-PKCS1-v1_5 signature over a SHA-256 digest.
 *
 * Used for X.509 certificate-chain signatures (sha256WithRSAEncryption).
 * All big integers are big-endian.
 *
 * @param n       Modulus bytes (big-endian).
 * @param nlen    Length of @p n (<= 512).
 * @param e       Public exponent bytes (big-endian, e.g. {0x01,0x00,0x01}).
 * @param elen    Length of @p e.
 * @param sig     Signature bytes (big-endian, length should equal nlen).
 * @param siglen  Length of @p sig.
 * @param hash32  The 32-byte SHA-256 digest that was signed.
 * @return TIKU_KITS_CRYPTO_RSA_OK if valid, else TIKU_KITS_CRYPTO_RSA_BAD.
 */
int tiku_kits_crypto_rsa_pkcs1_sha256_verify(
    const uint8_t *n, size_t nlen,
    const uint8_t *e, size_t elen,
    const uint8_t *sig, size_t siglen,
    const uint8_t hash32[32]);

/**
 * @brief Verify an RSASSA-PKCS1-v1_5 signature over a SHA-384 digest.
 *
 * Used for X.509 certificate-chain signatures (sha384WithRSAEncryption),
 * common on Microsoft / Azure RSA chains.  All big integers are big-endian.
 *
 * @param hash48  The 48-byte SHA-384 digest that was signed.
 * @return TIKU_KITS_CRYPTO_RSA_OK if valid, else TIKU_KITS_CRYPTO_RSA_BAD.
 */
int tiku_kits_crypto_rsa_pkcs1_sha384_verify(
    const uint8_t *n, size_t nlen,
    const uint8_t *e, size_t elen,
    const uint8_t *sig, size_t siglen,
    const uint8_t hash48[48]);

/**
 * @brief Verify an RSASSA-PSS signature (MGF1-SHA-256) over a SHA-256 digest.
 *
 * Used for TLS 1.3 CertificateVerify (rsa_pss_rsae_sha256).  The salt length
 * is recovered from the encoding, so any standard salt length is accepted.
 *
 * @param n       Modulus bytes (big-endian).
 * @param nlen    Length of @p n (<= 512).
 * @param e       Public exponent bytes (big-endian).
 * @param elen    Length of @p e.
 * @param sig     Signature bytes (big-endian).
 * @param siglen  Length of @p sig.
 * @param mhash32 The 32-byte SHA-256 digest of the signed message.
 * @return TIKU_KITS_CRYPTO_RSA_OK if valid, else TIKU_KITS_CRYPTO_RSA_BAD.
 */
int tiku_kits_crypto_rsa_pss_sha256_verify(
    const uint8_t *n, size_t nlen,
    const uint8_t *e, size_t elen,
    const uint8_t *sig, size_t siglen,
    const uint8_t mhash32[32]);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_RSA_H_ */
