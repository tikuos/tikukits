/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_tls_record.c - TLS 1.3 record layer
 *
 * Implements TLS 1.3 record framing, AEAD encryption/decryption,
 * and nonce construction per RFC 8446 Section 5.  Plaintext records
 * are used for the initial ClientHello; all subsequent records use
 * AES-128-GCM authenticated encryption with the inner content type
 * appended to the plaintext before encryption.
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

#include "tiku_kits_crypto_tls_record.h"
#include "../gcm/tiku_kits_crypto_gcm.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* NONCE CONSTRUCTION                                                        */
/*---------------------------------------------------------------------------*/

/**
 * @brief Construct a GCM nonce from IV and sequence number.
 *
 * Per RFC 8446 Section 5.3, the nonce is formed by XOR-ing the
 * 12-byte IV with the 64-bit sequence number zero-padded on the
 * left to 12 bytes.
 */
void tiku_kits_crypto_tls_nonce(const uint8_t *iv,
                                uint64_t seq,
                                uint8_t *nonce)
{
    uint8_t padded_seq[TIKU_KITS_CRYPTO_TLS_IV_SIZE];
    uint8_t i;

    /* Zero-pad the sequence number into a 12-byte buffer.
     * The sequence number occupies the low 8 bytes (big-endian). */
    memset(padded_seq, 0, TIKU_KITS_CRYPTO_TLS_IV_SIZE);
    padded_seq[4]  = (uint8_t)(seq >> 56);
    padded_seq[5]  = (uint8_t)(seq >> 48);
    padded_seq[6]  = (uint8_t)(seq >> 40);
    padded_seq[7]  = (uint8_t)(seq >> 32);
    padded_seq[8]  = (uint8_t)(seq >> 24);
    padded_seq[9]  = (uint8_t)(seq >> 16);
    padded_seq[10] = (uint8_t)(seq >> 8);
    padded_seq[11] = (uint8_t)(seq);

    /* XOR each byte: nonce[i] = iv[i] ^ padded_seq[i] */
    for (i = 0; i < TIKU_KITS_CRYPTO_TLS_IV_SIZE; i++) {
        nonce[i] = iv[i] ^ padded_seq[i];
    }
}

/*---------------------------------------------------------------------------*/
/* PLAINTEXT RECORD                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Build a plaintext TLS record.
 *
 * Writes a 5-byte record header (content_type, legacy version
 * 0x0301 for ClientHello compatibility, length) followed by
 * the content payload.
 */
uint16_t tiku_kits_crypto_tls_record_build_plain(
    uint8_t *out,
    uint8_t content_type,
    const uint8_t *content,
    uint16_t content_len)
{
    /* 5-byte record header */
    out[0] = content_type;
    out[1] = 0x03;
    out[2] = 0x01;  /* legacy TLS 1.0 for ClientHello */
    out[3] = (uint8_t)(content_len >> 8);
    out[4] = (uint8_t)(content_len);

    /* Copy payload after header */
    memcpy(out + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
           content, content_len);

    return (uint16_t)(TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE
                      + content_len);
}

/*---------------------------------------------------------------------------*/
/* ENCRYPTED RECORD (ENCRYPT)                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encrypt and build a TLS 1.3 encrypted record.
 *
 * Constructs the inner plaintext (content || content_type), encrypts
 * it with AES-128-GCM, and frames the result as an encrypted TLS
 * record with outer type application_data (23).
 */
uint16_t tiku_kits_crypto_tls_record_encrypt(
    uint8_t *out,
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *iv,
    uint64_t *seq,
    uint8_t content_type,
    const uint8_t *content,
    uint16_t content_len)
{
    uint16_t inner_len;
    uint16_t enc_len;
    uint8_t nonce[TIKU_KITS_CRYPTO_TLS_IV_SIZE];
    uint8_t aad[TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE];
    uint8_t *inner_pt;
    uint8_t *ct;
    uint8_t *tag;

    /* Inner plaintext length: content + content_type byte */
    inner_len = content_len + 1;

    /* Total encrypted fragment: inner_len + 16-byte tag */
    enc_len = inner_len + TIKU_KITS_CRYPTO_TLS_TAG_SIZE;

    /* Build inner plaintext directly at out[5..] so we can
     * encrypt in place.  We write to a temporary area offset
     * by the record header. */
    inner_pt = out + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE;
    memcpy(inner_pt, content, content_len);
    inner_pt[content_len] = content_type;

    /* Build the 5-byte record header (also serves as AAD) */
    aad[0] = TIKU_KITS_CRYPTO_TLS_CT_APPLICATION;
    aad[1] = 0x03;
    aad[2] = 0x03;  /* legacy version 0x0303 */
    aad[3] = (uint8_t)(enc_len >> 8);
    aad[4] = (uint8_t)(enc_len);

    /* Write header to output */
    memcpy(out, aad, TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE);

    /* Construct nonce from IV and current sequence number */
    tiku_kits_crypto_tls_nonce(iv, *seq, nonce);

    /* Ciphertext goes at out[5], tag follows ciphertext */
    ct  = out + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE;
    tag = ct + inner_len;

    /* Encrypt inner plaintext (in-place: inner_pt == ct) */
    tiku_kits_crypto_gcm_encrypt(ctx, nonce,
                                 aad,
                                 TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
                                 inner_pt, inner_len,
                                 ct, tag);

    /* Increment sequence number */
    (*seq)++;

    return (uint16_t)(TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE
                      + enc_len);
}

/*---------------------------------------------------------------------------*/
/* ENCRYPTED RECORD (DECRYPT)                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Decrypt a TLS 1.3 encrypted record.
 *
 * Parses the 5-byte header, decrypts the AEAD-protected fragment,
 * verifies the authentication tag, and extracts the real content
 * type from the last byte of the decrypted inner plaintext.
 * Trailing zero padding (per RFC 8446 Section 5.4) is stripped.
 */
int tiku_kits_crypto_tls_record_decrypt(
    const uint8_t *record,
    uint16_t record_len,
    const tiku_kits_crypto_gcm_ctx_t *ctx,
    const uint8_t *iv,
    uint64_t *seq,
    uint8_t *out_content,
    uint16_t *out_len,
    uint8_t *out_type)
{
    uint16_t frag_len;
    uint16_t ct_len;
    const uint8_t *fragment;
    const uint8_t *ct;
    const uint8_t *tag;
    uint8_t nonce[TIKU_KITS_CRYPTO_TLS_IV_SIZE];
    uint8_t aad[TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE];
    int ret;
    uint16_t i;

    /* Need at least the 5-byte header */
    if (record_len < TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    /* Verify outer content type is application_data (23) */
    if (record[0] != TIKU_KITS_CRYPTO_TLS_CT_APPLICATION) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    /* Extract fragment length from header */
    frag_len = ((uint16_t)record[3] << 8) | (uint16_t)record[4];

    /* Fragment must contain at least 1 byte content_type + tag */
    if (frag_len < (1 + TIKU_KITS_CRYPTO_TLS_TAG_SIZE)) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    /* Verify record_len is consistent */
    if (record_len < TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE
                     + frag_len) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    /* Locate ciphertext and tag within the fragment */
    fragment = record + TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE;
    ct_len   = frag_len - TIKU_KITS_CRYPTO_TLS_TAG_SIZE;
    ct       = fragment;
    tag      = fragment + ct_len;

    /* Build nonce from IV and current sequence number */
    tiku_kits_crypto_tls_nonce(iv, *seq, nonce);

    /* AAD is the 5-byte record header */
    memcpy(aad, record, TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE);

    /* Decrypt and verify tag */
    ret = tiku_kits_crypto_gcm_decrypt(ctx, nonce,
                                       aad,
                                       TIKU_KITS_CRYPTO_TLS_RECORD_HDR_SIZE,
                                       ct, ct_len,
                                       tag, out_content);
    if (ret != TIKU_KITS_CRYPTO_OK) {
        return TIKU_KITS_CRYPTO_ERR_CORRUPT;
    }

    /* The last byte of the decrypted plaintext is the real
     * content type.  Per RFC 8446 Section 5.4, there may be
     * trailing zero-padding before the content type byte.
     * Scan backwards to find the last non-zero byte. */
    i = ct_len;
    while (i > 0) {
        i--;
        if (out_content[i] != 0) {
            *out_type = out_content[i];
            *out_len  = i;
            (*seq)++;
            return TIKU_KITS_CRYPTO_OK;
        }
    }

    /* All bytes are zero -- malformed inner plaintext */
    return TIKU_KITS_CRYPTO_ERR_CORRUPT;
}
