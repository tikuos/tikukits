/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_x509.h - minimal X.509 (DER) certificate parser
 *
 * Parses the fields a TLS 1.3 client needs to authenticate a server:
 * the signed TBSCertificate, the subject public key (RSA or EC P-256),
 * validity window, issuer/subject DNs (for chain linking), the
 * subjectAltName DNS list (for hostname matching), and the certificate
 * signature + algorithm.  Zero-copy: parsed fields point into the caller's
 * DER buffer, which must outlive the parsed struct.
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

#ifndef TIKU_KITS_CRYPTO_X509_H_
#define TIKU_KITS_CRYPTO_X509_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIKU_KITS_CRYPTO_X509_OK     0
#define TIKU_KITS_CRYPTO_X509_BAD  (-1)

/** Public-key algorithm of the subject. */
enum {
    TIKU_X509_PK_UNKNOWN = 0,
    TIKU_X509_PK_RSA,         /**< rsaEncryption: rsa_n / rsa_e valid     */
    TIKU_X509_PK_EC_P256,     /**< id-ecPublicKey prime256v1: ec_point    */
    TIKU_X509_PK_EC_P384      /**< id-ecPublicKey secp384r1: ec_point     */
};

/** Signature algorithm over the TBSCertificate. */
enum {
    TIKU_X509_SIG_UNKNOWN = 0,
    TIKU_X509_SIG_RSA_PKCS1_SHA256, /**< sha256WithRSAEncryption */
    TIKU_X509_SIG_RSA_PSS_SHA256,   /**< rsassaPss / SHA-256     */
    TIKU_X509_SIG_ECDSA_SHA256,     /**< ecdsa-with-SHA256       */
    TIKU_X509_SIG_ECDSA_SHA384      /**< ecdsa-with-SHA384       */
};

/**
 * @struct tiku_kits_crypto_x509_t
 * @brief  Parsed view over a DER certificate (pointers alias the input).
 */
typedef struct {
    const uint8_t *tbs;      size_t tbs_len;    /**< signed region (with hdr) */

    int            pk_alg;
    const uint8_t *rsa_n;    size_t rsa_n_len;  /**< RSA modulus (no sign 00) */
    const uint8_t *rsa_e;    size_t rsa_e_len;  /**< RSA exponent             */
    const uint8_t *ec_point; size_t ec_point_len;/**< EC point 04||X||Y       */

    const uint8_t *issuer;   size_t issuer_len; /**< raw DER Name             */
    const uint8_t *subject;  size_t subject_len;/**< raw DER Name             */

    const uint8_t *not_before; size_t not_before_len; /**< raw time bytes     */
    const uint8_t *not_after;  size_t not_after_len;
    uint8_t        nb_tag, na_tag;              /**< 0x17 UTCTime/0x18 GenTime*/

    const uint8_t *san;      size_t san_len;    /**< raw SAN GeneralNames     */

    int            sig_alg;
    const uint8_t *sig;      size_t sig_len;    /**< signature value (no BIT0)*/

    /* path-validation extensions (RFC 5280) */
    uint8_t        has_bc;      /**< basicConstraints present              */
    uint8_t        is_ca;       /**< basicConstraints cA = TRUE            */
    int            path_len;    /**< pathLenConstraint, -1 = absent        */
    uint8_t        has_ku;      /**< keyUsage present                      */
    uint8_t        key_usage;   /**< keyUsage first octet (see KU_* below) */
    uint8_t        has_eku;     /**< extendedKeyUsage present              */
    uint8_t        eku_server;  /**< EKU has serverAuth or anyExtendedKeyUsage */
} tiku_kits_crypto_x509_t;

/** keyUsage bits, as they sit in the BIT STRING's first content octet. */
#define TIKU_X509_KU_DIGITAL_SIG   0x80   /**< digitalSignature (bit 0) */
#define TIKU_X509_KU_KEY_CERT_SIGN 0x04   /**< keyCertSign      (bit 5) */

/**
 * @brief Parse a single DER certificate.
 * @param der   Certificate bytes (DER).
 * @param len   Length of @p der.
 * @param out   Filled on success; fields alias @p der.
 * @return TIKU_KITS_CRYPTO_X509_OK or _BAD.
 */
int tiku_kits_crypto_x509_parse(const uint8_t *der, size_t len,
                                tiku_kits_crypto_x509_t *out);

/**
 * @brief Check a hostname against the certificate's subjectAltName DNS list
 *        (case-insensitive, supports a leading "*." wildcard label).
 * @return TIKU_KITS_CRYPTO_X509_OK on match, else _BAD.
 */
int tiku_kits_crypto_x509_match_host(const tiku_kits_crypto_x509_t *c,
                                     const char *host);

/**
 * @brief Verify that certificate @p c was signed by @p issuer.
 *
 * Hashes c->tbs with SHA-256 and checks c->sig under issuer's public key,
 * dispatching on the signature algorithm (RSA PKCS#1 / RSA-PSS / ECDSA).
 * Does NOT check names, validity, or basic constraints -- pure signature.
 *
 * @return TIKU_KITS_CRYPTO_X509_OK if the signature verifies, else _BAD.
 */
int tiku_kits_crypto_x509_verify_signed_by(const tiku_kits_crypto_x509_t *c,
                                           const tiku_kits_crypto_x509_t *issuer);

/**
 * @struct tiku_kits_crypto_x509_root_t
 * @brief  One entry in a baked-in trust store: a root cert's DER plus a
 *         precomputed pointer to its subject DN (for fast anchor matching
 *         without parsing every root).  Both pointers alias persistent data.
 */
typedef struct {
    const uint8_t *der;      size_t der_len;
    const uint8_t *subject;  size_t subject_len;   /* subject DN, inside der */
} tiku_kits_crypto_x509_root_t;

/**
 * @brief Validate a chain against a baked-in trust store (lazy: parses only
 *        the root that matches the chain's top issuer).
 *
 * Same checks as tiku_kits_crypto_x509_verify_chain(), but the trust anchor
 * is found by matching the topmost cert's issuer DN against each store entry's
 * subject DN (a byte compare), then parsing + verifying only that root.
 *
 * @return TIKU_KITS_CRYPTO_X509_OK if trusted, else _BAD.
 */
int tiku_kits_crypto_x509_verify_chain_store(
    const tiku_kits_crypto_x509_t *chain, int n,
    const tiku_kits_crypto_x509_root_t *store, int nstore,
    const char *host, uint64_t now_unix);

/**
 * @brief Validate a certificate chain to a trusted root.
 *
 * @param chain   Parsed chain, chain[0] = leaf .. chain[n-1] = topmost sent.
 * @param n       Number of certs in @p chain (>= 1).
 * @param roots   Parsed trusted root certificates (the trust store).
 * @param nroots  Number of roots.
 * @param host    Hostname to match against the leaf SAN (NULL to skip).
 * @param now_unix Current time, Unix seconds, for validity checks (0 to skip).
 *
 * Checks, in order: leaf hostname; every cert's validity window; each link
 * (chain[i] signed by chain[i+1] and issuer==subject); and that the topmost
 * cert is signed by some trusted root (matched by subject==issuer).
 *
 * @return TIKU_KITS_CRYPTO_X509_OK if the chain is trusted, else _BAD.
 */
int tiku_kits_crypto_x509_verify_chain(const tiku_kits_crypto_x509_t *chain, int n,
                                       const tiku_kits_crypto_x509_t *roots, int nroots,
                                       const char *host, uint64_t now_unix);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_CRYPTO_X509_H_ */
