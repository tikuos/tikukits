/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_keysched.h - TLS 1.3 key schedule
 *
 * Implements the TLS 1.3 key derivation functions (RFC 8446
 * Section 7.1) for PSK-only mode using HKDF-SHA256.
 *
 * Key schedule flow (PSK-only, no ECDHE):
 *
 *   PSK --> HKDF-Extract --> Early Secret
 *     |-> binder_key (for PSK binder in ClientHello)
 *     |-> HKDF-Extract --> Handshake Secret
 *         |-> client_hs_traffic_secret (CH..SH transcript)
 *         |-> server_hs_traffic_secret (CH..SH transcript)
 *         |-> HKDF-Extract --> Master Secret
 *             |-> client_app_traffic_secret (CH..SF transcript)
 *             |-> server_app_traffic_secret (CH..SF transcript)
 *
 * Each traffic secret is expanded to key (16B) + IV (12B).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_TLS_KEYSCHED_H_
#define TIKU_KITS_CRYPTO_TLS_KEYSCHED_H_

#include "../tiku_kits_crypto.h"
#include "tiku_kits_crypto_tls_config.h"

/*---------------------------------------------------------------------------*/
/* KEY SCHEDULE FUNCTIONS                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute Early Secret from PSK.
 *
 * early_secret = HKDF-Extract(salt=0, IKM=PSK)
 *
 * @param psk         Pre-shared key bytes
 * @param psk_len     PSK length
 * @param early_secret  Output: 32-byte Early Secret
 */
int tiku_kits_crypto_tls_early_secret(
    const uint8_t *psk, uint16_t psk_len,
    uint8_t *early_secret);

/**
 * @brief Compute binder_key for PSK binder verification.
 *
 * binder_key = Derive-Secret(Early Secret, "ext binder", "")
 *
 * @param early_secret  32-byte Early Secret
 * @param binder_key    Output: 32-byte binder key
 */
int tiku_kits_crypto_tls_binder_key(
    const uint8_t *early_secret,
    uint8_t *binder_key);

/**
 * @brief Compute Handshake Secret.
 *
 * derived = Derive-Secret(Early Secret, "derived", "")
 * hs_secret = HKDF-Extract(salt=derived, IKM=0)
 *
 * @param early_secret   32-byte Early Secret
 * @param hs_secret      Output: 32-byte Handshake Secret
 */
int tiku_kits_crypto_tls_handshake_secret(
    const uint8_t *early_secret,
    uint8_t *hs_secret);

/**
 * @brief Derive client and server handshake traffic secrets.
 *
 * client = Derive-Secret(hs_secret, "c hs traffic", CH..SH)
 * server = Derive-Secret(hs_secret, "s hs traffic", CH..SH)
 *
 * @param hs_secret       32-byte Handshake Secret
 * @param transcript_hash 32-byte hash of ClientHello..ServerHello
 * @param client_secret   Output: 32-byte client handshake secret
 * @param server_secret   Output: 32-byte server handshake secret
 */
int tiku_kits_crypto_tls_hs_traffic_secrets(
    const uint8_t *hs_secret,
    const uint8_t *transcript_hash,
    uint8_t *client_secret,
    uint8_t *server_secret);

/**
 * @brief Compute Master Secret.
 *
 * derived = Derive-Secret(hs_secret, "derived", "")
 * master = HKDF-Extract(salt=derived, IKM=0)
 *
 * @param hs_secret     32-byte Handshake Secret
 * @param master_secret Output: 32-byte Master Secret
 */
int tiku_kits_crypto_tls_master_secret(
    const uint8_t *hs_secret,
    uint8_t *master_secret);

/**
 * @brief Derive client and server application traffic secrets.
 *
 * client = Derive-Secret(master, "c ap traffic", CH..SF)
 * server = Derive-Secret(master, "s ap traffic", CH..SF)
 *
 * @param master_secret   32-byte Master Secret
 * @param transcript_hash 32-byte hash of CH..server Finished
 * @param client_secret   Output: 32-byte client app secret
 * @param server_secret   Output: 32-byte server app secret
 */
int tiku_kits_crypto_tls_app_traffic_secrets(
    const uint8_t *master_secret,
    const uint8_t *transcript_hash,
    uint8_t *client_secret,
    uint8_t *server_secret);

/**
 * @brief Expand a traffic secret into AES key and GCM IV.
 *
 * key = HKDF-Expand-Label(secret, "key", "", 16)
 * iv  = HKDF-Expand-Label(secret, "iv",  "", 12)
 *
 * @param traffic_secret 32-byte traffic secret
 * @param key            Output: 16-byte AES-128 key
 * @param iv             Output: 12-byte GCM IV (nonce base)
 */
int tiku_kits_crypto_tls_traffic_keys(
    const uint8_t *traffic_secret,
    uint8_t *key,
    uint8_t *iv);

/**
 * @brief Compute Finished verify_data.
 *
 * finished_key = HKDF-Expand-Label(base_key, "finished", "", 32)
 * verify_data  = HMAC-SHA256(finished_key, transcript_hash)
 *
 * @param base_key        32-byte handshake traffic secret
 * @param transcript_hash 32-byte hash of handshake up to this point
 * @param verify_data     Output: 32-byte verify data
 */
int tiku_kits_crypto_tls_finished_verify(
    const uint8_t *base_key,
    const uint8_t *transcript_hash,
    uint8_t *verify_data);

/**
 * @brief Compute PSK binder value for ClientHello.
 *
 * binder = HMAC-SHA256(binder_key, transcript_hash_truncated)
 *
 * @param binder_key          32-byte binder key
 * @param transcript_hash     32-byte hash of truncated ClientHello
 * @param binder              Output: 32-byte binder value
 */
int tiku_kits_crypto_tls_compute_binder(
    const uint8_t *binder_key,
    const uint8_t *transcript_hash,
    uint8_t *binder);

#endif /* TIKU_KITS_CRYPTO_TLS_KEYSCHED_H_ */
