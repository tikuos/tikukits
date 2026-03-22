/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_handshake.h - TLS 1.3 handshake messages
 *
 * Functions to construct and parse TLS 1.3 handshake messages
 * for PSK-only mode.  Messages handled:
 *   - ClientHello (build)
 *   - ServerHello (parse)
 *   - EncryptedExtensions (parse)
 *   - Finished (build and parse)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_TLS_HANDSHAKE_H_
#define TIKU_KITS_CRYPTO_TLS_HANDSHAKE_H_

#include "../tiku_kits_crypto.h"
#include "tiku_kits_crypto_tls_config.h"

/*---------------------------------------------------------------------------*/
/* CLIENT HELLO                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build a TLS 1.3 ClientHello message.
 *
 * Constructs a minimal ClientHello with:
 *   - legacy_version = 0x0303
 *   - random (32 bytes, caller-provided)
 *   - legacy_session_id = empty
 *   - cipher_suites = { TLS_AES_128_GCM_SHA256 }
 *   - compression_methods = { null }
 *   - Extensions:
 *     - supported_versions: { 0x0304 }
 *     - psk_key_exchange_modes: { psk_ke (0) }
 *     - pre_shared_key: identity + binder placeholder
 *
 * The binder field is filled with zeros; the caller must compute
 * the actual binder value over the transcript hash of the
 * truncated ClientHello (up to but excluding the binder) and
 * patch it in using tiku_kits_crypto_tls_ch_patch_binder().
 *
 * @param out             Output buffer (FRAM-backed, >= 200 bytes)
 * @param client_random   32-byte random value
 * @param psk_identity    PSK identity bytes
 * @param psk_id_len      Identity length
 * @param binder_offset   Output: byte offset of the binder value
 *                        in the output buffer (for patching)
 * @return Total ClientHello message length (incl. handshake header),
 *         or 0 on error
 */
uint16_t tiku_kits_crypto_tls_build_client_hello(
    uint8_t *out,
    const uint8_t *client_random,
    const uint8_t *psk_identity,
    uint16_t psk_id_len,
    uint16_t *binder_offset);

/**
 * @brief Patch the PSK binder value in a ClientHello.
 *
 * After computing the binder HMAC over the truncated ClientHello
 * transcript hash, write the 32-byte binder value at the offset
 * returned by build_client_hello().
 *
 * @param ch_buf         ClientHello buffer
 * @param binder_offset  Offset from build_client_hello()
 * @param binder         32-byte binder value
 */
void tiku_kits_crypto_tls_ch_patch_binder(
    uint8_t *ch_buf,
    uint16_t binder_offset,
    const uint8_t *binder);

/*---------------------------------------------------------------------------*/
/* SERVER HELLO                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parsed ServerHello result.
 */
typedef struct {
    uint8_t  server_random[32]; /**< Server random value */
    uint16_t cipher_suite;      /**< Selected cipher suite */
    uint16_t psk_index;         /**< Selected PSK index (must be 0) */
    uint8_t  version_ok;        /**< 1 if supported_versions has 0x0304 */
} tiku_kits_crypto_tls_sh_t;

/**
 * @brief Parse a ServerHello handshake message.
 *
 * Extracts server_random, cipher_suite, selected PSK index,
 * and verifies the supported_versions extension.
 *
 * @param msg       ServerHello message (after handshake header)
 * @param msg_len   Message length
 * @param result    Output: parsed fields
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_PARAM on parse failure
 */
int tiku_kits_crypto_tls_parse_server_hello(
    const uint8_t *msg,
    uint16_t msg_len,
    tiku_kits_crypto_tls_sh_t *result);

/*---------------------------------------------------------------------------*/
/* ENCRYPTED EXTENSIONS                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parse EncryptedExtensions (minimal validation).
 *
 * For PSK-only mode, we don't need any extensions from EE.
 * Just validate the structure (2-byte extension list length)
 * and skip the contents.
 *
 * @param msg       EE message body (after handshake header)
 * @param msg_len   Message length
 * @return TIKU_KITS_CRYPTO_OK if structurally valid,
 *         TIKU_KITS_CRYPTO_ERR_PARAM on parse failure
 */
int tiku_kits_crypto_tls_parse_encrypted_extensions(
    const uint8_t *msg,
    uint16_t msg_len);

/*---------------------------------------------------------------------------*/
/* FINISHED                                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parse and verify a Finished message.
 *
 * Compares the received verify_data against the expected value.
 * Uses constant-time comparison.
 *
 * @param msg            Finished message (after handshake header)
 * @param msg_len        Message length (must be 32)
 * @param expected       32-byte expected verify_data
 * @return TIKU_KITS_CRYPTO_OK if valid,
 *         TIKU_KITS_CRYPTO_ERR_CORRUPT if mismatch
 */
int tiku_kits_crypto_tls_verify_finished(
    const uint8_t *msg,
    uint16_t msg_len,
    const uint8_t *expected);

/**
 * @brief Build a Finished handshake message.
 *
 * @param out           Output buffer (must hold 4 + 32 = 36 bytes)
 * @param verify_data   32-byte verify data
 * @return Message length (36)
 */
uint16_t tiku_kits_crypto_tls_build_finished(
    uint8_t *out,
    const uint8_t *verify_data);

#endif /* TIKU_KITS_CRYPTO_TLS_HANDSHAKE_H_ */
