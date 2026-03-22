/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_config.h - TLS 1.3 compile-time configuration
 *
 * All buffer sizes and limits for the TLS 1.3 PSK-only client.
 * Override any value by defining it before including this header
 * or via EXTRA_CFLAGS.
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

#ifndef TIKU_KITS_CRYPTO_TLS_CONFIG_H_
#define TIKU_KITS_CRYPTO_TLS_CONFIG_H_

/*---------------------------------------------------------------------------*/
/* PSK CONFIGURATION                                                         */
/*---------------------------------------------------------------------------*/

/** Maximum pre-shared key length in bytes. */
#ifndef TIKU_KITS_CRYPTO_TLS_MAX_PSK_LEN
#define TIKU_KITS_CRYPTO_TLS_MAX_PSK_LEN       32
#endif

/** Maximum PSK identity length in bytes. */
#ifndef TIKU_KITS_CRYPTO_TLS_MAX_PSK_ID_LEN
#define TIKU_KITS_CRYPTO_TLS_MAX_PSK_ID_LEN    32
#endif

/*---------------------------------------------------------------------------*/
/* BUFFER SIZES (FRAM-backed)                                                */
/*---------------------------------------------------------------------------*/

/**
 * Handshake transcript buffer size.
 *
 * Must hold the raw bytes of all handshake messages for transcript
 * hashing: ClientHello (~150B) + ServerHello (~100B) +
 * EncryptedExtensions (~50B) + Finished (~36B) = ~336B typical.
 * 512B provides headroom for server extensions.
 */
#ifndef TIKU_KITS_CRYPTO_TLS_TRANSCRIPT_SIZE
#define TIKU_KITS_CRYPTO_TLS_TRANSCRIPT_SIZE    512
#endif

/**
 * TLS record buffer size (used for both RX and TX).
 *
 * A TLS record can carry up to 2^14 bytes of payload, but we limit
 * to a value that fits the SLIP MTU.  The 5-byte record header plus
 * up to 256 bytes of content plus 16-byte AEAD tag plus 1-byte
 * inner content type = 278 bytes.  300 provides margin.
 */
#ifndef TIKU_KITS_CRYPTO_TLS_RECORD_BUF_SIZE
#define TIKU_KITS_CRYPTO_TLS_RECORD_BUF_SIZE    300
#endif

/**
 * Maximum application data fragment size.
 *
 * Upper bound on plaintext payload per TLS record.  Must satisfy:
 * FRAG + 16 (tag) + 1 (content type) + 5 (record header) <= RECORD_BUF.
 */
#ifndef TIKU_KITS_CRYPTO_TLS_MAX_FRAG_LEN
#define TIKU_KITS_CRYPTO_TLS_MAX_FRAG_LEN      256
#endif

/*---------------------------------------------------------------------------*/
/* PROTOCOL CONSTANTS (not configurable)                                     */
/*---------------------------------------------------------------------------*/

/** TLS 1.3 version in ProtocolVersion format. */
#define TIKU_KITS_CRYPTO_TLS_VERSION_13         0x0304

/** TLS record legacy version (always 0x0303 per RFC 8446). */
#define TIKU_KITS_CRYPTO_TLS_LEGACY_VERSION     0x0303

/** TLS_AES_128_GCM_SHA256 cipher suite value. */
#define TIKU_KITS_CRYPTO_TLS_CS_AES128_GCM     0x1301

/** TLS record header size. */
#define TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE    5

/** AEAD tag size (AES-128-GCM). */
#define TIKU_KITS_CRYPTO_TLS_TAG_SIZE           16

/** GCM nonce (IV) size. */
#define TIKU_KITS_CRYPTO_TLS_IV_SIZE            12

/** AES-128 key size. */
#define TIKU_KITS_CRYPTO_TLS_KEY_SIZE           16

/** SHA-256 digest / secret size. */
#define TIKU_KITS_CRYPTO_TLS_HASH_SIZE          32

/*---------------------------------------------------------------------------*/
/* TLS CONTENT TYPES                                                         */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_CRYPTO_TLS_CT_CHANGE_CIPHER   20
#define TIKU_KITS_CRYPTO_TLS_CT_ALERT           21
#define TIKU_KITS_CRYPTO_TLS_CT_HANDSHAKE       22
#define TIKU_KITS_CRYPTO_TLS_CT_APPLICATION     23

/*---------------------------------------------------------------------------*/
/* TLS HANDSHAKE MESSAGE TYPES                                               */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_CRYPTO_TLS_HT_CLIENT_HELLO     1
#define TIKU_KITS_CRYPTO_TLS_HT_SERVER_HELLO      2
#define TIKU_KITS_CRYPTO_TLS_HT_ENC_EXTENSIONS    8
#define TIKU_KITS_CRYPTO_TLS_HT_FINISHED          20

/*---------------------------------------------------------------------------*/
/* TLS EXTENSION TYPES                                                       */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_CRYPTO_TLS_EXT_SUPPORTED_VERSIONS  43
#define TIKU_KITS_CRYPTO_TLS_EXT_PSK_KEY_MODES       45
#define TIKU_KITS_CRYPTO_TLS_EXT_PRE_SHARED_KEY      41

/*---------------------------------------------------------------------------*/
/* TLS ALERT DESCRIPTIONS                                                    */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_CRYPTO_TLS_ALERT_CLOSE_NOTIFY        0
#define TIKU_KITS_CRYPTO_TLS_ALERT_UNEXPECTED_MSG      10
#define TIKU_KITS_CRYPTO_TLS_ALERT_BAD_RECORD_MAC      20
#define TIKU_KITS_CRYPTO_TLS_ALERT_DECODE_ERROR        50
#define TIKU_KITS_CRYPTO_TLS_ALERT_HANDSHAKE_FAILURE   40
#define TIKU_KITS_CRYPTO_TLS_ALERT_INTERNAL_ERROR      80

/*---------------------------------------------------------------------------*/
/* TLS STATE MACHINE                                                         */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_CRYPTO_TLS_STATE_IDLE         0
#define TIKU_KITS_CRYPTO_TLS_STATE_WAIT_SH      1
#define TIKU_KITS_CRYPTO_TLS_STATE_WAIT_EE      2
#define TIKU_KITS_CRYPTO_TLS_STATE_WAIT_FIN     3
#define TIKU_KITS_CRYPTO_TLS_STATE_SEND_FIN     4
#define TIKU_KITS_CRYPTO_TLS_STATE_ESTABLISHED  5
#define TIKU_KITS_CRYPTO_TLS_STATE_CLOSING      6
#define TIKU_KITS_CRYPTO_TLS_STATE_ERROR        7

/*---------------------------------------------------------------------------*/
/* TLS EVENTS (delivered to application via event callback)                   */
/*---------------------------------------------------------------------------*/

#define TIKU_KITS_CRYPTO_TLS_EVT_CONNECTED      1
#define TIKU_KITS_CRYPTO_TLS_EVT_CLOSED         2
#define TIKU_KITS_CRYPTO_TLS_EVT_ERROR          3

#endif /* TIKU_KITS_CRYPTO_TLS_CONFIG_H_ */
