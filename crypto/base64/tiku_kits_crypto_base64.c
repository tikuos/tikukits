/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_base64.c - Base64 encoding/decoding for embedded systems
 *
 * Implements RFC 4648 standard Base64.  Zero heap allocation.
 *
 * Encoding uses a 64-byte lookup table.  Decoding uses a 128-byte
 * reverse lookup table (ASCII range), where -1 marks invalid
 * characters.  Total const data: 192 bytes.
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

#include "tiku_kits_crypto_base64.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* ENCODING TABLE                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief RFC 4648 Base64 alphabet (64 characters + padding '=').
 *
 * Index 0..63 maps to 'A'..'Z', 'a'..'z', '0'..'9', '+', '/'.
 */
static const char b64_encode_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

/** Base64 padding character. */
#define B64_PAD '='

/*---------------------------------------------------------------------------*/
/* DECODING TABLE                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reverse lookup table: ASCII code -> 6-bit value, or -1.
 *
 * Covers ASCII 0..127.  Characters not in the Base64 alphabet are
 * marked with -1.  The '=' padding character is also -1 here and
 * handled separately by the decoder.
 */
static const int8_t b64_decode_table[128] = {
    /* 0x00-0x0F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x10-0x1F */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 0x20-0x2F  ('+' = 0x2B -> 62, '/' = 0x2F -> 63) */
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    /* 0x30-0x3F  ('0'..'9' -> 52..61, '=' = 0x3D -> -1) */
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    /* 0x40-0x4F  ('A'..'O' -> 0..14) */
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    /* 0x50-0x5F  ('P'..'Z' -> 15..25) */
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    /* 0x60-0x6F  ('a'..'o' -> 26..40) */
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    /* 0x70-0x7F  ('p'..'z' -> 41..51) */
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
};

/*---------------------------------------------------------------------------*/
/* ENCODE                                                                    */
/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_base64_encode(const uint8_t *src,
                                    uint16_t src_len,
                                    char *dst,
                                    uint16_t dst_size,
                                    uint16_t *out_len)
{
    uint16_t needed;
    uint16_t si;
    uint16_t di;
    uint32_t triple;
    uint16_t remaining;

    if (src == NULL || dst == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* Compute required output size including null terminator */
    needed = (uint16_t)(((src_len + 2) / 3) * 4 + 1);

    if (dst_size < needed) {
        return TIKU_KITS_CRYPTO_ERR_OVERFLOW;
    }

    di = 0;

    /* Process complete 3-byte groups */
    for (si = 0; si + 2 < src_len; si += 3) {
        triple = ((uint32_t)src[si] << 16) |
                 ((uint32_t)src[si + 1] << 8) |
                  (uint32_t)src[si + 2];

        dst[di++] = b64_encode_table[(triple >> 18) & 0x3F];
        dst[di++] = b64_encode_table[(triple >> 12) & 0x3F];
        dst[di++] = b64_encode_table[(triple >> 6) & 0x3F];
        dst[di++] = b64_encode_table[triple & 0x3F];
    }

    /* Handle remaining 1 or 2 bytes with padding */
    remaining = src_len - si;

    if (remaining == 1) {
        triple = (uint32_t)src[si] << 16;

        dst[di++] = b64_encode_table[(triple >> 18) & 0x3F];
        dst[di++] = b64_encode_table[(triple >> 12) & 0x3F];
        dst[di++] = B64_PAD;
        dst[di++] = B64_PAD;
    } else if (remaining == 2) {
        triple = ((uint32_t)src[si] << 16) |
                 ((uint32_t)src[si + 1] << 8);

        dst[di++] = b64_encode_table[(triple >> 18) & 0x3F];
        dst[di++] = b64_encode_table[(triple >> 12) & 0x3F];
        dst[di++] = b64_encode_table[(triple >> 6) & 0x3F];
        dst[di++] = B64_PAD;
    }

    /* Null-terminate */
    dst[di] = '\0';

    if (out_len != NULL) {
        *out_len = di;
    }

    return TIKU_KITS_CRYPTO_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODE                                                                    */
/*---------------------------------------------------------------------------*/

int tiku_kits_crypto_base64_decode(const char *src,
                                    uint16_t src_len,
                                    uint8_t *dst,
                                    uint16_t dst_size,
                                    uint16_t *out_len)
{
    uint16_t max_decoded;
    uint16_t pad;
    uint16_t si;
    uint16_t di;
    int8_t a, b, c, d;

    if (src == NULL || dst == NULL) {
        return TIKU_KITS_CRYPTO_ERR_NULL;
    }

    /* Empty input decodes to empty output */
    if (src_len == 0) {
        if (out_len != NULL) {
            *out_len = 0;
        }
        return TIKU_KITS_CRYPTO_OK;
    }

    /* Non-empty input length must be a multiple of 4 */
    if ((src_len & 3) != 0) {
        return TIKU_KITS_CRYPTO_ERR_CORRUPT;
    }

    /* Count padding characters to determine actual output size */
    pad = 0;
    if (src[src_len - 1] == B64_PAD) {
        pad++;
    }
    if (src[src_len - 2] == B64_PAD) {
        pad++;
    }

    max_decoded = (uint16_t)((src_len / 4) * 3 - pad);

    if (dst_size < max_decoded) {
        return TIKU_KITS_CRYPTO_ERR_OVERFLOW;
    }

    di = 0;

    for (si = 0; si < src_len; si += 4) {
        /* Validate and look up each character */
        if ((uint8_t)src[si] > 127 ||
            (uint8_t)src[si + 1] > 127) {
            return TIKU_KITS_CRYPTO_ERR_CORRUPT;
        }

        a = b64_decode_table[(uint8_t)src[si]];
        b = b64_decode_table[(uint8_t)src[si + 1]];

        if (a < 0 || b < 0) {
            return TIKU_KITS_CRYPTO_ERR_CORRUPT;
        }

        /* Third character: may be padding in last group */
        if (src[si + 2] == B64_PAD) {
            /* Must be last group, fourth must also be padding */
            if (src[si + 3] != B64_PAD ||
                si + 4 != src_len) {
                return TIKU_KITS_CRYPTO_ERR_CORRUPT;
            }
            dst[di++] = (uint8_t)((a << 2) | (b >> 4));
            break;
        }

        if ((uint8_t)src[si + 2] > 127) {
            return TIKU_KITS_CRYPTO_ERR_CORRUPT;
        }
        c = b64_decode_table[(uint8_t)src[si + 2]];
        if (c < 0) {
            return TIKU_KITS_CRYPTO_ERR_CORRUPT;
        }

        /* Fourth character: may be padding in last group */
        if (src[si + 3] == B64_PAD) {
            if (si + 4 != src_len) {
                return TIKU_KITS_CRYPTO_ERR_CORRUPT;
            }
            dst[di++] = (uint8_t)((a << 2) | (b >> 4));
            dst[di++] = (uint8_t)(((b & 0x0F) << 4) |
                                   (c >> 2));
            break;
        }

        if ((uint8_t)src[si + 3] > 127) {
            return TIKU_KITS_CRYPTO_ERR_CORRUPT;
        }
        d = b64_decode_table[(uint8_t)src[si + 3]];
        if (d < 0) {
            return TIKU_KITS_CRYPTO_ERR_CORRUPT;
        }

        /* Decode the full 4-character group into 3 bytes */
        dst[di++] = (uint8_t)((a << 2) | (b >> 4));
        dst[di++] = (uint8_t)(((b & 0x0F) << 4) | (c >> 2));
        dst[di++] = (uint8_t)(((c & 0x03) << 6) | d);
    }

    if (out_len != NULL) {
        *out_len = di;
    }

    return TIKU_KITS_CRYPTO_OK;
}
