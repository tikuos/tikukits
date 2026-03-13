/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_textcompression_rle.c - Run-Length Encoding compression
 *
 * Simple (count, byte) pair encoding. Zero extra RAM beyond the
 * input and output buffers.
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

#include "tiku_kits_textcompression_rle.h"

/*---------------------------------------------------------------------------*/
/* ENCODING                                                                  */
/*---------------------------------------------------------------------------*/

int tiku_kits_textcompression_rle_encode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len)
{
    uint16_t si;
    uint16_t di;
    uint8_t count;

    if (src == NULL || dst == NULL || out_len == NULL) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_NULL;
    }

    si = 0;
    di = 0;

    while (si < src_len) {
        count = 1;
        while (si + count < src_len
               && src[si + count] == src[si]
               && count < 255) {
            count++;
        }

        if (di + 2 > dst_cap) {
            return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
        }

        dst[di++] = count;
        dst[di++] = src[si];
        si += count;
    }

    *out_len = di;
    return TIKU_KITS_TEXTCOMPRESSION_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODING                                                                  */
/*---------------------------------------------------------------------------*/

int tiku_kits_textcompression_rle_decode(const uint8_t *src, uint16_t src_len,
                                         uint8_t *dst, uint16_t dst_cap,
                                         uint16_t *out_len)
{
    uint16_t si;
    uint16_t di;
    uint8_t count;
    uint8_t val;
    uint8_t j;

    if (src == NULL || dst == NULL || out_len == NULL) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_NULL;
    }

    if (src_len % 2 != 0) {
        return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
    }

    si = 0;
    di = 0;

    while (si < src_len) {
        count = src[si++];
        val = src[si++];

        if (count == 0) {
            return TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT;
        }

        if (di + count > dst_cap) {
            return TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE;
        }

        for (j = 0; j < count; j++) {
            dst[di++] = val;
        }
    }

    *out_len = di;
    return TIKU_KITS_TEXTCOMPRESSION_OK;
}
