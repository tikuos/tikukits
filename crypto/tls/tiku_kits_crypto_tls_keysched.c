/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_keysched.c - TLS 1.3 key schedule (RFC 8446)
 *
 * Implements the TLS 1.3 key derivation functions for PSK-only mode
 * (RFC 8446 Section 7.1) using HKDF-SHA256.  All storage is
 * statically allocated; zero heap usage.
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

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_crypto_tls_keysched.h"
#include "../hkdf/tiku_kits_crypto_hkdf.h"
#include "../hmac/tiku_kits_crypto_hmac.h"
#include "../sha256/tiku_kits_crypto_sha256.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* PRIVATE CONSTANTS                                                         */
/*---------------------------------------------------------------------------*/

/** @brief SHA-256 digest / HKDF PRK size in bytes. */
#define KS_HASH_LEN     TIKU_KITS_CRYPTO_TLS_HASH_SIZE

/** @brief AES-128 key size in bytes. */
#define KS_KEY_LEN      TIKU_KITS_CRYPTO_TLS_KEY_SIZE

/** @brief GCM IV (nonce) size in bytes. */
#define KS_IV_LEN       TIKU_KITS_CRYPTO_TLS_IV_SIZE

/**
 * @brief SHA-256 hash of the empty string.
 *
 * SHA-256("") = e3b0c442 98fc1c14 9afbf4c8 996fb924
 *               27ae41e4 649b934c a495991b 7852b855
 *
 * Hardcoded to avoid recomputing on every Derive-Secret("", ...).
 */
static const uint8_t sha256_empty[KS_HASH_LEN] = {
    0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
    0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
    0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
    0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};

/**
 * @brief 32-byte all-zero buffer used as IKM in PSK-only mode
 *        (no ECDHE shared secret).
 */
static const uint8_t zero_ikm[KS_HASH_LEN] = { 0 };

/*---------------------------------------------------------------------------*/
/* PRIVATE HELPERS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Derive-Secret helper (RFC 8446 Section 7.1).
 *
 * Derive-Secret(Secret, Label, Messages) =
 *     HKDF-Expand-Label(Secret, Label, Hash(Messages), 32)
 *
 * The caller provides the 32-byte SHA-256 digest of Messages
 * directly (or sha256_empty for the empty transcript).
 *
 * @param[in]  secret  32-byte secret (PRK)
 * @param[in]  label   ASCII label without "tls13 " prefix
 * @param[in]  hash    32-byte transcript hash (or hash of "")
 * @param[out] out     32-byte derived secret
 *
 * @return TIKU_KITS_CRYPTO_OK on success, negative on error
 */
static int derive_secret(const uint8_t *secret, const char *label,
                          const uint8_t *hash, uint8_t *out)
{
    return tiku_kits_crypto_hkdf_expand_label(
        secret, label, hash, KS_HASH_LEN, out, KS_HASH_LEN);
}

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_early_secret(
    const uint8_t *psk, uint16_t psk_len,
    uint8_t *early_secret)
{
    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (psk == NULL || early_secret == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* Early Secret = HKDF-Extract(salt=0, IKM=PSK)                   */
    /*   NULL salt triggers the 32-byte zero salt per hkdf_extract.   */
    /* ---------------------------------------------------------------*/

    return tiku_kits_crypto_hkdf_extract(NULL, 0,
                                          psk, psk_len,
                                          early_secret);
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_binder_key(
    const uint8_t *early_secret,
    uint8_t *binder_key)
{
    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (early_secret == NULL || binder_key == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* binder_key = Derive-Secret(Early Secret, "ext binder", "")     */
    /*   Hash of empty transcript = sha256_empty (hardcoded).         */
    /* ---------------------------------------------------------------*/

    return derive_secret(early_secret, "ext binder",
                          sha256_empty, binder_key);
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_handshake_secret(
    const uint8_t *early_secret,
    uint8_t *hs_secret)
{
    uint8_t derived[KS_HASH_LEN];
    int rc;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (early_secret == NULL || hs_secret == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* derived = Derive-Secret(Early Secret, "derived", "")           */
    /* ---------------------------------------------------------------*/

    rc = derive_secret(early_secret, "derived",
                        sha256_empty, derived);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    /* ---------------------------------------------------------------*/
    /* hs_secret = HKDF-Extract(salt=derived, IKM=0^32)              */
    /*   PSK-only mode: no ECDHE shared secret, so IKM is zeros.     */
    /* ---------------------------------------------------------------*/

    rc = tiku_kits_crypto_hkdf_extract(derived, KS_HASH_LEN,
                                        zero_ikm, KS_HASH_LEN,
                                        hs_secret);

    /* Scrub intermediate secret */
    memset(derived, 0, KS_HASH_LEN);

    return rc;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_hs_traffic_secrets(
    const uint8_t *hs_secret,
    const uint8_t *transcript_hash,
    uint8_t *client_secret,
    uint8_t *server_secret)
{
    int rc;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (hs_secret == NULL || transcript_hash == NULL
        || client_secret == NULL || server_secret == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* client = Derive-Secret(hs_secret, "c hs traffic", CH..SH)     */
    /* ---------------------------------------------------------------*/

    rc = derive_secret(hs_secret, "c hs traffic",
                        transcript_hash, client_secret);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    /* ---------------------------------------------------------------*/
    /* server = Derive-Secret(hs_secret, "s hs traffic", CH..SH)     */
    /* ---------------------------------------------------------------*/

    return derive_secret(hs_secret, "s hs traffic",
                          transcript_hash, server_secret);
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_master_secret(
    const uint8_t *hs_secret,
    uint8_t *master_secret)
{
    uint8_t derived[KS_HASH_LEN];
    int rc;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (hs_secret == NULL || master_secret == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* derived = Derive-Secret(hs_secret, "derived", "")              */
    /* ---------------------------------------------------------------*/

    rc = derive_secret(hs_secret, "derived",
                        sha256_empty, derived);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    /* ---------------------------------------------------------------*/
    /* master = HKDF-Extract(salt=derived, IKM=0^32)                  */
    /*   PSK-only mode: no ECDHE, so IKM is zeros.                   */
    /* ---------------------------------------------------------------*/

    rc = tiku_kits_crypto_hkdf_extract(derived, KS_HASH_LEN,
                                        zero_ikm, KS_HASH_LEN,
                                        master_secret);

    /* Scrub intermediate secret */
    memset(derived, 0, KS_HASH_LEN);

    return rc;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_app_traffic_secrets(
    const uint8_t *master_secret,
    const uint8_t *transcript_hash,
    uint8_t *client_secret,
    uint8_t *server_secret)
{
    int rc;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (master_secret == NULL || transcript_hash == NULL
        || client_secret == NULL || server_secret == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* client = Derive-Secret(master, "c ap traffic", CH..SF)         */
    /* ---------------------------------------------------------------*/

    rc = derive_secret(master_secret, "c ap traffic",
                        transcript_hash, client_secret);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    /* ---------------------------------------------------------------*/
    /* server = Derive-Secret(master, "s ap traffic", CH..SF)         */
    /* ---------------------------------------------------------------*/

    return derive_secret(master_secret, "s ap traffic",
                          transcript_hash, server_secret);
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_traffic_keys(
    const uint8_t *traffic_secret,
    uint8_t *key,
    uint8_t *iv)
{
    int rc;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (traffic_secret == NULL || key == NULL || iv == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* key = HKDF-Expand-Label(secret, "key", "", 16)                 */
    /* ---------------------------------------------------------------*/

    rc = tiku_kits_crypto_hkdf_expand_label(
        traffic_secret, "key", NULL, 0, key, KS_KEY_LEN);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    /* ---------------------------------------------------------------*/
    /* iv = HKDF-Expand-Label(secret, "iv", "", 12)                   */
    /* ---------------------------------------------------------------*/

    return tiku_kits_crypto_hkdf_expand_label(
        traffic_secret, "iv", NULL, 0, iv, KS_IV_LEN);
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_finished_verify(
    const uint8_t *base_key,
    const uint8_t *transcript_hash,
    uint8_t *verify_data)
{
    uint8_t finished_key[KS_HASH_LEN];
    int rc;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (base_key == NULL || transcript_hash == NULL
        || verify_data == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* finished_key = HKDF-Expand-Label(base_key, "finished", "", 32) */
    /* ---------------------------------------------------------------*/

    rc = tiku_kits_crypto_hkdf_expand_label(
        base_key, "finished", NULL, 0,
        finished_key, KS_HASH_LEN);
    if (rc != TIKU_KITS_CRYPTO_OK) {
        return rc;
    }

    /* ---------------------------------------------------------------*/
    /* verify_data = HMAC-SHA256(finished_key, transcript_hash)       */
    /* ---------------------------------------------------------------*/

    rc = tiku_kits_crypto_hmac_sha256(
        finished_key, KS_HASH_LEN,
        transcript_hash, KS_HASH_LEN,
        verify_data);

    /* Scrub finished_key */
    memset(finished_key, 0, KS_HASH_LEN);

    return rc;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_tls_compute_binder(
    const uint8_t *binder_key,
    const uint8_t *transcript_hash,
    uint8_t *binder)
{
    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (binder_key == NULL || transcript_hash == NULL
        || binder == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* binder = HMAC-SHA256(binder_key, transcript_hash)              */
    /* ---------------------------------------------------------------*/

    return tiku_kits_crypto_hmac_sha256(
        binder_key, KS_HASH_LEN,
        transcript_hash, KS_HASH_LEN,
        binder);
}
