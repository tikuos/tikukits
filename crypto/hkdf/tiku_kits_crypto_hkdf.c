/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_hkdf.c - HKDF key derivation (RFC 5869)
 *
 * Implements HKDF-Extract, HKDF-Expand, and TLS 1.3
 * HKDF-Expand-Label using HMAC-SHA256 as the underlying PRF.
 * Zero heap allocation -- all scratch buffers live on the stack
 * or as static locals.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_crypto_hkdf.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* PRIVATE CONSTANTS                                                         */
/*---------------------------------------------------------------------------*/

/** @brief SHA-256 hash output length in bytes. */
#define HKDF_HASH_LEN   32

/** @brief "tls13 " prefix for HKDF-Expand-Label. */
#define TLS13_LABEL_PREFIX      "tls13 "

/** @brief Length of the "tls13 " prefix (6 bytes). */
#define TLS13_LABEL_PREFIX_LEN  6

/**
 * @brief Maximum HkdfLabel buffer size.
 *
 * Layout: 2 (length) + 1 (label_len) + 6 ("tls13 ") + 12 (label)
 *       + 1 (context_len) + 32 (context) = 54 bytes.
 * Round up to 64 for safety.
 */
#define HKDF_LABEL_BUF_SIZE     64

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_hkdf_extract(const uint8_t *salt,
                                  uint16_t salt_len,
                                  const uint8_t *ikm,
                                  uint16_t ikm_len,
                                  uint8_t *prk)
{
    static const uint8_t zero_salt[HKDF_HASH_LEN] = { 0 };

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (ikm == NULL || prk == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* RFC 5869 Section 2.2: PRK = HMAC-Hash(salt, IKM)               */
    /*   If salt not provided, use a HashLen string of zeros.          */
    /* ---------------------------------------------------------------*/

    if (salt == NULL) {
        return tiku_kits_crypto_hmac_sha256(zero_salt,
                                            HKDF_HASH_LEN,
                                            ikm, ikm_len, prk);
    }

    return tiku_kits_crypto_hmac_sha256(salt, salt_len,
                                        ikm, ikm_len, prk);
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_hkdf_expand(const uint8_t *prk,
                                 const uint8_t *info,
                                 uint16_t info_len,
                                 uint8_t *okm,
                                 uint16_t okm_len)
{
    /*
     * T(0) = empty
     * T(i) = HMAC(PRK, T(i-1) || info || i)   for i = 1..N
     *
     * We feed the HMAC inputs via a concatenated buffer built on
     * the stack.  The maximum single-HMAC input is:
     *   32 (T prev) + info_len + 1 (counter)
     *
     * For embedded use info_len is small, so a static scratch
     * buffer works well.  We assemble each iteration's input
     * into hmac_input[].
     */
    static uint8_t t_prev[HKDF_HASH_LEN];
    static uint8_t t_curr[HKDF_HASH_LEN];
    static uint8_t hmac_input[HKDF_HASH_LEN + 256 + 1];
    uint16_t n;
    uint16_t offset;
    uint16_t copy_len;
    uint16_t input_len;
    uint16_t i;
    uint8_t counter;
    int rc;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (prk == NULL || okm == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* ---------------------------------------------------------------*/
    /* Parameter validation                                           */
    /* ---------------------------------------------------------------*/

    if (okm_len == 0 || okm_len > TIKU_KITS_CRYPTO_HKDF_MAX_OKM) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    /* Treat NULL info as zero-length */
    if (info == NULL) {
        info_len = 0;
    }

    /* ---------------------------------------------------------------*/
    /* N = ceil(okm_len / HashLen)                                    */
    /* ---------------------------------------------------------------*/

    n = (okm_len + HKDF_HASH_LEN - 1) / HKDF_HASH_LEN;
    offset = 0;

    for (i = 1; i <= n; i++) {
        counter = (uint8_t)i;
        input_len = 0;

        /* T(i-1): skip for the first iteration (T(0) = empty) */
        if (i > 1) {
            memcpy(hmac_input, t_prev, HKDF_HASH_LEN);
            input_len = HKDF_HASH_LEN;
        }

        /* info */
        if (info_len > 0) {
            memcpy(hmac_input + input_len, info, info_len);
            input_len += info_len;
        }

        /* single-byte counter */
        hmac_input[input_len] = counter;
        input_len += 1;

        /* T(i) = HMAC(PRK, T(i-1) || info || i) */
        rc = tiku_kits_crypto_hmac_sha256(prk, HKDF_HASH_LEN,
                                          hmac_input, input_len,
                                          t_curr);
        if (rc != TIKU_KITS_CRYPTO_OK) {
            return rc;
        }

        /* Copy to OKM (last block may be partial) */
        copy_len = okm_len - offset;
        if (copy_len > HKDF_HASH_LEN) {
            copy_len = HKDF_HASH_LEN;
        }
        memcpy(okm + offset, t_curr, copy_len);
        offset += copy_len;

        /* Save for next iteration */
        memcpy(t_prev, t_curr, HKDF_HASH_LEN);
    }

    /* ---------------------------------------------------------------*/
    /* Scrub sensitive material from static buffers                    */
    /* ---------------------------------------------------------------*/

    memset(t_prev, 0, HKDF_HASH_LEN);
    memset(t_curr, 0, HKDF_HASH_LEN);
    memset(hmac_input, 0, sizeof(hmac_input));

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_hkdf_expand_label(const uint8_t *secret,
                                       const char *label,
                                       const uint8_t *context,
                                       uint16_t context_len,
                                       uint8_t *out,
                                       uint16_t out_len)
{
    /*
     * RFC 8446 Section 7.1 -- HkdfLabel encoding:
     *
     *   uint16  length       = out_len
     *   opaque  label<7..255> = "tls13 " + label
     *   opaque  context<0..255> = context
     *
     * Wire format:
     *   [0..1]  out_len          (big-endian uint16)
     *   [2]     label_len_byte   (length of "tls13 " + label)
     *   [3..]   "tls13 " + label
     *   [..]    context_len_byte
     *   [..]    context
     */
    uint8_t label_buf[HKDF_LABEL_BUF_SIZE];
    uint16_t label_str_len;
    uint16_t full_label_len;
    uint16_t pos;

    /* ---------------------------------------------------------------*/
    /* NULL checks                                                    */
    /* ---------------------------------------------------------------*/

    if (secret == NULL || label == NULL || out == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* Treat NULL context as zero-length */
    if (context == NULL) {
        context_len = 0;
    }

    /* ---------------------------------------------------------------*/
    /* Compute label string length                                    */
    /* ---------------------------------------------------------------*/

    label_str_len = 0;
    while (label[label_str_len] != '\0') {
        label_str_len++;
    }

    full_label_len = TLS13_LABEL_PREFIX_LEN + label_str_len;

    /* ---------------------------------------------------------------*/
    /* Validate that the HkdfLabel fits in the buffer                 */
    /*   Total = 2 + 1 + full_label_len + 1 + context_len            */
    /* ---------------------------------------------------------------*/

    pos = 2 + 1 + full_label_len + 1 + context_len;
    if (pos > HKDF_LABEL_BUF_SIZE) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    if (out_len == 0
        || out_len > TIKU_KITS_CRYPTO_HKDF_MAX_OKM) {
        return TIKU_KITS_CRYPTO_ERR_PARAM;
    }

    /* ---------------------------------------------------------------*/
    /* Encode HkdfLabel into label_buf                                */
    /* ---------------------------------------------------------------*/

    pos = 0;

    /* uint16 length (big-endian) */
    label_buf[pos++] = (uint8_t)(out_len >> 8);
    label_buf[pos++] = (uint8_t)(out_len & 0xFF);

    /* uint8 label length */
    label_buf[pos++] = (uint8_t)full_label_len;

    /* "tls13 " prefix */
    memcpy(label_buf + pos, TLS13_LABEL_PREFIX,
           TLS13_LABEL_PREFIX_LEN);
    pos += TLS13_LABEL_PREFIX_LEN;

    /* label (without prefix) */
    memcpy(label_buf + pos, label, label_str_len);
    pos += label_str_len;

    /* uint8 context length */
    label_buf[pos++] = (uint8_t)context_len;

    /* context */
    if (context_len > 0) {
        memcpy(label_buf + pos, context, context_len);
        pos += context_len;
    }

    /* ---------------------------------------------------------------*/
    /* HKDF-Expand(Secret, HkdfLabel, Length)                         */
    /* ---------------------------------------------------------------*/

    return tiku_kits_crypto_hkdf_expand(secret, label_buf, pos,
                                        out, out_len);
}
