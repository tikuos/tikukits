/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_record.h - TLS 1.3 record layer
 *
 * Handles TLS record framing, AEAD encryption/decryption, and
 * nonce construction per RFC 8446 Section 5.
 *
 * Record format (on the wire):
 *   [content_type(1)] [legacy_version(2)] [length(2)] [fragment(N)]
 *
 * For encrypted records:
 *   - Outer content_type is always application_data (23)
 *   - Fragment = AEAD-encrypted(inner_content || content_type)
 *   - The real content type is the last byte of the inner plaintext
 *   - Fragment includes the 16-byte AEAD authentication tag
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_TLS_RECORD_H_
#define TIKU_KITS_CRYPTO_TLS_RECORD_H_

#include "../tiku_kits_crypto.h"
#include "../gcm/tiku_kits_crypto_gcm.h"
#include "tiku_kits_crypto_tls_config.h"

/*---------------------------------------------------------------------------*/
/* RECORD OPERATIONS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build a plaintext TLS record in the output buffer.
 *
 * Used for the initial ClientHello (sent before encryption is
 * established).  Writes the 5-byte record header followed by
 * the content.
 *
 * @param out          Output buffer (must hold 5 + content_len)
 * @param content_type TLS content type (e.g., CT_HANDSHAKE)
 * @param content      Record payload
 * @param content_len  Payload length
 * @return Total record length (5 + content_len)
 */
uint16_t tiku_kits_crypto_tls_record_build_plain(
    uint8_t *out,
    uint8_t content_type,
    const uint8_t *content,
    uint16_t content_len);

/**
 * @brief Encrypt and build a TLS 1.3 encrypted record.
 *
 * Constructs an encrypted record:
 *   outer header: type=23, version=0x0303, length=N+tag+1
 *   fragment: AEAD(inner_content || real_content_type)
 *
 * The nonce is constructed as IV XOR padded_sequence_number
 * per RFC 8446 Section 5.3.  The sequence number is incremented
 * after each record.
 *
 * @param out          Output buffer (FRAM-backed)
 * @param ctx          GCM context for encryption
 * @param iv           12-byte nonce base
 * @param seq          Pointer to sequence number (incremented)
 * @param content_type Real content type (22=handshake, 23=app)
 * @param content      Plaintext to encrypt
 * @param content_len  Plaintext length
 * @return Total record length (5 + content_len + 1 + 16), or
 *         0 on error
 */
uint16_t tiku_kits_crypto_tls_record_encrypt(
    uint8_t *out,
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *iv,
    uint64_t *seq,
    uint8_t content_type,
    const uint8_t *content,
    uint16_t content_len);

/**
 * @brief Decrypt a TLS 1.3 encrypted record.
 *
 * Verifies the AEAD tag and extracts the real content type
 * (last byte of decrypted inner plaintext).
 *
 * @param record       Full TLS record (header + encrypted fragment)
 * @param record_len   Total record length
 * @param ctx          GCM context for decryption
 * @param iv           12-byte nonce base
 * @param seq          Pointer to sequence number (incremented)
 * @param out_content  Output: decrypted content (without type byte)
 * @param out_len      Output: decrypted content length
 * @param out_type     Output: real content type
 * @return TIKU_KITS_CRYPTO_OK on success,
 *         TIKU_KITS_CRYPTO_ERR_CORRUPT on tag verification failure,
 *         TIKU_KITS_CRYPTO_ERR_PARAM on malformed record
 */
int tiku_kits_crypto_tls_record_decrypt(
    const uint8_t *record,
    uint16_t record_len,
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *iv,
    uint64_t *seq,
    uint8_t *out_content,
    uint16_t *out_len,
    uint8_t *out_type);

/**
 * @brief Construct a GCM nonce from IV and sequence number.
 *
 * nonce = iv XOR (0x00000000 || sequence_number_be)
 * where the sequence number is zero-padded to 12 bytes on the left.
 *
 * @param iv    12-byte nonce base
 * @param seq   64-bit sequence number
 * @param nonce Output: 12-byte nonce
 */
void tiku_kits_crypto_tls_nonce(const uint8_t *iv,
                                  uint64_t seq,
                                  uint8_t *nonce);

#endif /* TIKU_KITS_CRYPTO_TLS_RECORD_H_ */
