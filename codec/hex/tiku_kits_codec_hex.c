/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec_hex.c - Hex encoder/decoder implementation
 *
 * Converts between raw byte arrays and lowercase hexadecimal ASCII
 * strings.  Zero heap allocation; all buffers are caller-provided.
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

#include "tiku_kits_codec_hex.h"
#include <stddef.h>

/*---------------------------------------------------------------------------*/
/* PRIVATE HELPERS                                                           */
/*---------------------------------------------------------------------------*/

/** @brief Lookup table for byte-to-hex conversion. */
static const char hex_chars[] = "0123456789abcdef";

/**
 * @brief Convert a single hex ASCII character to its 4-bit value.
 *
 * @param c  Hex character ('0'-'9', 'A'-'F', or 'a'-'f')
 * @return   0..15 on success, -1 if @p c is not a valid hex digit
 */
static int8_t hex_nibble(char c)
{
    if (c >= '0' && c <= '9') {
        return (int8_t)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (int8_t)(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return (int8_t)(c - 'A' + 10);
    }
    return -1;
}

/*---------------------------------------------------------------------------*/
/* ENCODER                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Encode a byte array to a lowercase hex string.
 *
 * Iterates over each source byte, writing the high nibble followed
 * by the low nibble as lowercase ASCII hex characters.  A null
 * terminator is appended after the last character.
 */
int tiku_kits_codec_hex_encode(const uint8_t *src,
                               uint16_t src_len,
                               char *dst,
                               uint16_t dst_size,
                               uint16_t *out_len)
{
    uint16_t i;
    uint16_t needed;

    if (dst == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (src_len > 0 && src == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    /* Two hex chars per byte plus null terminator. */
    needed = (uint16_t)(src_len * 2 + 1);
    if (dst_size < needed) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }

    for (i = 0; i < src_len; i++) {
        dst[i * 2]     = hex_chars[(src[i] >> 4) & 0x0F];
        dst[i * 2 + 1] = hex_chars[ src[i]       & 0x0F];
    }
    dst[src_len * 2] = '\0';

    if (out_len != NULL) {
        *out_len = (uint16_t)(src_len * 2);
    }
    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Decode a hex string to a byte array.
 *
 * Processes pairs of hex characters, converting each pair into a
 * single byte.  Accepts both uppercase and lowercase digits.
 * Returns ERR_CORRUPT on the first invalid hex character encountered.
 */
int tiku_kits_codec_hex_decode(const char *src,
                               uint16_t src_len,
                               uint8_t *dst,
                               uint16_t dst_size,
                               uint16_t *out_len)
{
    uint16_t i;
    uint16_t num_bytes;
    int8_t hi;
    int8_t lo;

    if (dst == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (src_len > 0 && src == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    /* Hex strings must have an even number of characters. */
    if (src_len & 1) {
        return TIKU_KITS_CODEC_ERR_PARAM;
    }

    num_bytes = (uint16_t)(src_len / 2);
    if (dst_size < num_bytes) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }

    for (i = 0; i < num_bytes; i++) {
        hi = hex_nibble(src[i * 2]);
        lo = hex_nibble(src[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return TIKU_KITS_CODEC_ERR_CORRUPT;
        }
        dst[i] = (uint8_t)((hi << 4) | lo);
    }

    if (out_len != NULL) {
        *out_len = num_bytes;
    }
    return TIKU_KITS_CODEC_OK;
}
