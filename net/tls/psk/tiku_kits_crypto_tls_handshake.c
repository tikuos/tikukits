/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_handshake.c - TLS 1.3 handshake messages
 *
 * Builds and parses TLS 1.3 handshake messages for PSK-only mode:
 * ClientHello construction with pre_shared_key extension, ServerHello
 * parsing, EncryptedExtensions validation, and Finished message
 * handling.  All buffers are caller-provided; no heap allocation.
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

#include "tiku_kits_crypto_tls_handshake.h"
#include "tiku_kits_crypto_tls_config.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* CLIENT HELLO                                                              */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build a TLS 1.3 ClientHello handshake message.
 *
 * Constructs a minimal ClientHello for PSK-only mode with
 * supported_versions, psk_key_exchange_modes, and pre_shared_key
 * extensions.  The binder field is zeroed; the caller patches it
 * after computing the transcript-based HMAC.
 */
uint16_t tiku_kits_crypto_tls_build_client_hello(
    uint8_t *out,
    const uint8_t *client_random,
    const uint8_t *psk_identity,
    uint16_t psk_id_len,
    uint16_t *binder_offset)
{
    uint16_t pos;
    uint16_t ext_start;
    uint16_t ext_total_len;
    uint16_t psk_ext_len;
    uint16_t identities_len;
    uint16_t binders_len;
    uint16_t body_len;

    pos = 0;

    /*---------------------------------------------------------------*/
    /* Handshake header (4 bytes, length filled later)               */
    /*---------------------------------------------------------------*/
    out[pos++] = TIKU_KITS_CRYPTO_TLS_HT_CLIENT_HELLO;
    out[pos++] = 0x00; /* length byte 0 (24-bit, filled later) */
    out[pos++] = 0x00; /* length byte 1 */
    out[pos++] = 0x00; /* length byte 2 */

    /*---------------------------------------------------------------*/
    /* ClientHello body                                              */
    /*---------------------------------------------------------------*/

    /* legacy_version = 0x0303 */
    out[pos++] = 0x03;
    out[pos++] = 0x03;

    /* client_random (32 bytes) */
    memcpy(out + pos, client_random, 32);
    pos += 32;

    /* legacy_session_id = empty */
    out[pos++] = 0x00;

    /* cipher_suites: length=2, TLS_AES_128_GCM_SHA256=0x1301 */
    out[pos++] = 0x00;
    out[pos++] = 0x02;
    out[pos++] = 0x13;
    out[pos++] = 0x01;

    /* compression_methods: length=1, null=0x00 */
    out[pos++] = 0x01;
    out[pos++] = 0x00;

    /*---------------------------------------------------------------*/
    /* Extensions                                                    */
    /*---------------------------------------------------------------*/

    /* extensions_total_len placeholder (2 bytes, filled later) */
    ext_start = pos;
    out[pos++] = 0x00;
    out[pos++] = 0x00;

    /*-- Extension 1: supported_versions (type=43) --*/
    out[pos++] = 0x00;
    out[pos++] = 0x2B; /* type = 43 */
    out[pos++] = 0x00;
    out[pos++] = 0x03; /* extension data length */
    out[pos++] = 0x02; /* list length */
    out[pos++] = 0x03;
    out[pos++] = 0x04; /* TLS 1.3 = 0x0304 */

    /*-- Extension 2: psk_key_exchange_modes (type=45) --*/
    out[pos++] = 0x00;
    out[pos++] = 0x2D; /* type = 45 */
    out[pos++] = 0x00;
    out[pos++] = 0x02; /* extension data length */
    out[pos++] = 0x01; /* list length */
    out[pos++] = 0x00; /* psk_ke mode */

    /*-- Extension 3: pre_shared_key (type=41) -- MUST BE LAST --*/
    /* Compute sub-field lengths */
    identities_len = 2 + psk_id_len + 4;
    binders_len    = 1 + TIKU_KITS_CRYPTO_TLS_HASH_SIZE;
    psk_ext_len    = 2 + identities_len + 2 + binders_len;

    out[pos++] = 0x00;
    out[pos++] = 0x29; /* type = 41 */
    out[pos++] = (uint8_t)(psk_ext_len >> 8);
    out[pos++] = (uint8_t)(psk_ext_len);

    /* identities list */
    out[pos++] = (uint8_t)(identities_len >> 8);
    out[pos++] = (uint8_t)(identities_len);

    /* identity length + identity */
    out[pos++] = (uint8_t)(psk_id_len >> 8);
    out[pos++] = (uint8_t)(psk_id_len);
    memcpy(out + pos, psk_identity, psk_id_len);
    pos += psk_id_len;

    /* obfuscated_ticket_age = 0x00000000 (external PSK) */
    out[pos++] = 0x00;
    out[pos++] = 0x00;
    out[pos++] = 0x00;
    out[pos++] = 0x00;

    /* binders list */
    out[pos++] = (uint8_t)(binders_len >> 8);
    out[pos++] = (uint8_t)(binders_len);

    /* binder length */
    out[pos++] = (uint8_t)TIKU_KITS_CRYPTO_TLS_HASH_SIZE;

    /* Record the binder offset for later patching */
    *binder_offset = pos;

    /* 32-byte binder placeholder (zeroed) */
    memset(out + pos, 0, TIKU_KITS_CRYPTO_TLS_HASH_SIZE);
    pos += TIKU_KITS_CRYPTO_TLS_HASH_SIZE;

    /*---------------------------------------------------------------*/
    /* Fill in deferred length fields                                */
    /*---------------------------------------------------------------*/

    /* extensions_total_len (everything after the 2-byte length) */
    ext_total_len = pos - ext_start - 2;
    out[ext_start]     = (uint8_t)(ext_total_len >> 8);
    out[ext_start + 1] = (uint8_t)(ext_total_len);

    /* handshake body length (24-bit, bytes 1-3) */
    body_len = pos - 4;
    out[1] = 0;  /* body_len < 256, high byte always 0 */
    out[2] = (uint8_t)(body_len >> 8);
    out[3] = (uint8_t)(body_len);

    return pos;
}

/*---------------------------------------------------------------------------*/
/* BINDER PATCHING                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Patch the PSK binder in a previously built ClientHello.
 *
 * Overwrites the 32-byte binder placeholder with the computed
 * HMAC value.
 */
void tiku_kits_crypto_tls_ch_patch_binder(
    uint8_t *ch_buf,
    uint16_t binder_offset,
    const uint8_t *binder)
{
    memcpy(ch_buf + binder_offset, binder,
           TIKU_KITS_CRYPTO_TLS_HASH_SIZE);
}

/*---------------------------------------------------------------------------*/
/* SERVER HELLO PARSING                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parse a ServerHello handshake message body.
 *
 * Extracts server_random, cipher_suite, and scans extensions for
 * supported_versions (must contain 0x0304) and pre_shared_key
 * (selected identity index).
 */
int tiku_kits_crypto_tls_parse_server_hello(
    const uint8_t *msg,
    uint16_t msg_len,
    tiku_kits_crypto_tls_sh_t *result)
{
    uint16_t pos;
    uint8_t  sid_len;
    uint16_t ext_len;
    uint16_t ext_end;
    uint16_t etype;
    uint16_t elen;

    /* Initialize result */
    memset(result, 0, sizeof(*result));

    /* Minimum: 2 (version) + 32 (random) + 1 (sid_len) +
     *          2 (cipher) + 1 (comp) = 38 */
    if (msg_len < 38) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    pos = 0;

    /* legacy_version: must be 0x0303 */
    if (msg[pos] != 0x03 || msg[pos + 1] != 0x03) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    pos += 2;

    /* server_random (32 bytes) */
    memcpy(result->server_random, msg + pos, 32);
    pos += 32;

    /* legacy_session_id */
    sid_len = msg[pos++];
    if (pos + sid_len > msg_len) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    pos += sid_len; /* skip session id */

    /* cipher_suite (2 bytes) */
    if (pos + 2 > msg_len) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    result->cipher_suite = ((uint16_t)msg[pos] << 8)
                           | (uint16_t)msg[pos + 1];
    pos += 2;

    /* compression_method (must be 0) */
    if (pos >= msg_len) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    if (msg[pos] != 0x00) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    pos++;

    /* Extensions */
    if (pos + 2 > msg_len) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    ext_len = ((uint16_t)msg[pos] << 8) | (uint16_t)msg[pos + 1];
    pos += 2;

    if (pos + ext_len > msg_len) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }
    ext_end = pos + ext_len;

    /* Scan extensions */
    while (pos + 4 <= ext_end) {
        etype = ((uint16_t)msg[pos] << 8)
                | (uint16_t)msg[pos + 1];
        pos += 2;
        elen = ((uint16_t)msg[pos] << 8)
               | (uint16_t)msg[pos + 1];
        pos += 2;

        if (pos + elen > ext_end) {
            return TIKU_KITS_CRYPTO_ERR_PARAM;
        }

        if (etype == TIKU_KITS_CRYPTO_TLS_EXT_SUPPORTED_VERSIONS) {
            /* supported_versions: 2-byte selected version */
            if (elen < 2) {
                return TIKU_KITS_CRYPTO_ERR_PARAM;
            }
            if (msg[pos] == 0x03 && msg[pos + 1] == 0x04) {
                result->version_ok = 1;
            }
        } else if (etype
                   == TIKU_KITS_CRYPTO_TLS_EXT_PRE_SHARED_KEY) {
            /* pre_shared_key: 2-byte selected identity index */
            if (elen < 2) {
                return TIKU_KITS_CRYPTO_ERR_PARAM;
            }
            result->psk_index =
                ((uint16_t)msg[pos] << 8)
                | (uint16_t)msg[pos + 1];
        }

        pos += elen;
    }

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* ENCRYPTED EXTENSIONS                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @brief Parse EncryptedExtensions (minimal validation).
 *
 * For PSK-only mode we do not require any extensions from EE.
 * Just validate the 2-byte extension list length field and
 * confirm it does not exceed the message body.
 */
int tiku_kits_crypto_tls_parse_encrypted_extensions(
    const uint8_t *msg,
    uint16_t msg_len)
{
    uint16_t list_len;

    /* Need at least the 2-byte extensions list length */
    if (msg_len < 2) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    list_len = ((uint16_t)msg[0] << 8) | (uint16_t)msg[1];

    /* List must fit within the remaining message body */
    if (list_len > msg_len - 2) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* FINISHED (VERIFY)                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Verify a Finished message using constant-time comparison.
 *
 * Compares the received verify_data against the expected value.
 * Uses a volatile accumulator to prevent the compiler from
 * short-circuiting the comparison.
 */
int tiku_kits_crypto_tls_verify_finished(
    const uint8_t *msg,
    uint16_t msg_len,
    const uint8_t *expected)
{
    volatile uint8_t diff;
    uint8_t i;

    if (msg_len != TIKU_KITS_CRYPTO_TLS_HASH_SIZE) {
        return TIKU_KITS_CRYPTO_ERR_CORRUPT;
    }

    /* Constant-time comparison */
    diff = 0;
    for (i = 0; i < TIKU_KITS_CRYPTO_TLS_HASH_SIZE; i++) {
        diff |= msg[i] ^ expected[i];
    }

    if (diff != 0) {
        return TIKU_KITS_CRYPTO_ERR_CORRUPT;
    }

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* FINISHED (BUILD)                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build a Finished handshake message.
 *
 * Writes a 4-byte handshake header (type=20, length=32) followed
 * by the 32-byte verify_data.
 */
uint16_t tiku_kits_crypto_tls_build_finished(
    uint8_t *out,
    const uint8_t *verify_data)
{
    /* Handshake header: type=Finished(20), length=0x000020 */
    out[0] = TIKU_KITS_CRYPTO_TLS_HT_FINISHED;
    out[1] = 0x00;
    out[2] = 0x00;
    out[3] = TIKU_KITS_CRYPTO_TLS_HASH_SIZE;

    /* Copy 32-byte verify_data */
    memcpy(out + 4, verify_data,
           TIKU_KITS_CRYPTO_TLS_HASH_SIZE);

    return (uint16_t)(4 + TIKU_KITS_CRYPTO_TLS_HASH_SIZE);
}
