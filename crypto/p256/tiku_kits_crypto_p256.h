/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_p256.h - NIST P-256 (secp256r1) ECDSA and ECDH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * ECDSA over the NIST P-256 curve: verification for TLS server
 * CertificateVerify messages and X.509 chains, signing for a TLS server's
 * own CertificateVerify, and ECDH for key exchange.  A private key or a
 * nonce is multiplied in constant time: no branch, no memory address and
 * no loop count depends on it.  Verification works on public data and is
 * not constant time.  Not reentrant: the arithmetic keeps its scratch in
 * file-scope storage.
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

/*
 * Derives the private scalar d from @p seed (d = seed mod n) and computes the
 * public point Q = d*G in uncompressed form.  Intended for ephemeral ECDHE:
 * pass 32 fresh random bytes as @p seed.
 */

/**
 * @brief Generate an (ephemeral) ECDH key pair.
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

/**
 * @brief The public point of a private key.
 * @param priv  The private scalar d (32 bytes, big-endian), 0 < d < n.
 * @param pub   Output: d*G as 0x04 || X || Y (65 bytes).
 * @return _OK, or _BAD if d is out of range.
 */
int tiku_kits_crypto_p256_public(
    const uint8_t priv[TIKU_KITS_CRYPTO_P256_LEN],
    uint8_t pub[TIKU_KITS_CRYPTO_P256_PUB_LEN]);

/**
 * @brief A nonce source: fills @p k with a candidate nonce for the hash
 *        @p h1 (bits2octets(h1), 32 bytes).
 * @param again  0 on the first call for a signature, 1 after the previous
 *               candidate was refused
 */
typedef void (*tiku_kits_crypto_p256_nonce_fn)(
    void *ctx, const uint8_t h1[TIKU_KITS_CRYPTO_P256_LEN], int again,
    uint8_t k[TIKU_KITS_CRYPTO_P256_LEN]);

/**
 * @brief Sign with nonces from @p nonce.  A nonce must be secret, uniform
 *        and never used twice: tiku_kits_crypto_p256_ecdsa_sign() supplies
 *        such nonces, and callers outside the tests use it.
 * @param hash  Message hash; its leftmost 256 bits are signed.
 * @return _OK, or _BAD if the key is out of range or 64 candidates were
 *         refused.
 */
int tiku_kits_crypto_p256_ecdsa_sign_nonce(
    const uint8_t priv[TIKU_KITS_CRYPTO_P256_LEN],
    const uint8_t *hash, size_t hashlen,
    tiku_kits_crypto_p256_nonce_fn nonce, void *ctx,
    uint8_t r[TIKU_KITS_CRYPTO_P256_LEN],
    uint8_t s[TIKU_KITS_CRYPTO_P256_LEN]);

/**
 * @brief ECDSA signature (r, s) of @p hash under @p priv, the nonce derived
 *        by RFC 6979 with @p extra as additional data (section 3.6).
 *
 * Without @p extra the signature is RFC 6979's deterministic one; random
 * @p extra keeps the nonce secret if the random source fails.  Defined in
 * tiku_kits_crypto_p256_sign.c, which needs the HMAC kit.
 *
 * @param extra     Additional data, at most 32 bytes, or NULL
 * @param extralen  Its length
 * @return _OK, or _BAD if the key is out of range or @p extra too long.
 */
int tiku_kits_crypto_p256_ecdsa_sign(
    const uint8_t priv[TIKU_KITS_CRYPTO_P256_LEN],
    const uint8_t *hash, size_t hashlen,
    const uint8_t *extra, size_t extralen,
    uint8_t r[TIKU_KITS_CRYPTO_P256_LEN],
    uint8_t s[TIKU_KITS_CRYPTO_P256_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_P256_H_ */
