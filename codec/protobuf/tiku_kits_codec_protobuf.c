/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec_protobuf.c - Protocol Buffers (nanopb-style) codec
 *
 * Implements the protobuf binary wire format for ultra-low-power MCUs.
 * Zero heap allocation, incremental encode/decode, ~800 bytes code.
 *
 * Protobuf wire format:
 *   Each field is: tag (varint) + payload
 *   Tag = (field_number << 3) | wire_type
 *
 *   Varint (wire type 0): LEB128, 7 bits per byte, MSB = continuation
 *   Length-delimited (wire type 2): length varint + raw bytes
 *   32-bit (wire type 5): 4 bytes little-endian
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

#include "tiku_kits_codec_protobuf.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* PRIVATE HELPERS                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute how many bytes a varint will occupy.
 *
 * @param value  Unsigned value
 * @return 1..5 bytes
 */
static uint8_t varint_size(uint32_t value)
{
    if (value < (1UL << 7)) {
        return 1;
    } else if (value < (1UL << 14)) {
        return 2;
    } else if (value < (1UL << 21)) {
        return 3;
    } else if (value < (1UL << 28)) {
        return 4;
    } else {
        return 5;
    }
}

/*---------------------------------------------------------------------------*/
/* OUTPUT STREAM INIT / QUERY                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_ostream_init(tiku_kits_codec_pb_ostream_t *os,
                                     uint8_t *buf, uint16_t capacity)
{
    if (os == NULL || buf == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    os->buf      = buf;
    os->capacity = capacity;
    os->pos      = 0;

    return TIKU_KITS_CODEC_OK;
}

uint16_t tiku_kits_codec_pb_ostream_len(
             const tiku_kits_codec_pb_ostream_t *os)
{
    if (os == NULL) {
        return 0;
    }
    return os->pos;
}

/*---------------------------------------------------------------------------*/
/* ENCODER — VARINT                                                          */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_encode_varint(tiku_kits_codec_pb_ostream_t *os,
                                      uint32_t value)
{
    uint8_t needed;

    if (os == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    needed = varint_size(value);

    if (os->pos + needed > os->capacity) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }

    while (value > 0x7F) {
        os->buf[os->pos++] = (uint8_t)(value & 0x7F) | 0x80;
        value >>= 7;
    }
    os->buf[os->pos++] = (uint8_t)value;

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* ENCODER — TAG                                                             */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_encode_tag(tiku_kits_codec_pb_ostream_t *os,
                                    uint32_t field_number,
                                    uint8_t wire_type)
{
    if (os == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    if (field_number == 0) {
        return TIKU_KITS_CODEC_ERR_PARAM;
    }

    return tiku_kits_codec_pb_encode_varint(
               os, TIKU_KITS_CODEC_PB_TAG(field_number, wire_type));
}

/*---------------------------------------------------------------------------*/
/* ENCODER — FIXED32                                                         */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_encode_fixed32(tiku_kits_codec_pb_ostream_t *os,
                                       uint32_t value)
{
    if (os == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    if (os->pos + 4 > os->capacity) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }

    os->buf[os->pos++] = (uint8_t)(value);
    os->buf[os->pos++] = (uint8_t)(value >> 8);
    os->buf[os->pos++] = (uint8_t)(value >> 16);
    os->buf[os->pos++] = (uint8_t)(value >> 24);

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* ENCODER — LENGTH-DELIMITED                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_encode_bytes(tiku_kits_codec_pb_ostream_t *os,
                                     const uint8_t *data, uint16_t len)
{
    uint8_t hdr_size;

    if (os == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (data == NULL && len > 0) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    hdr_size = varint_size(len);

    if (os->pos + hdr_size + len > os->capacity) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }

    /* Write length varint */
    tiku_kits_codec_pb_encode_varint(os, len);

    /* Write payload */
    if (len > 0) {
        memcpy(&os->buf[os->pos], data, len);
        os->pos += len;
    }

    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_pb_encode_string(tiku_kits_codec_pb_ostream_t *os,
                                      const char *str, uint16_t len)
{
    return tiku_kits_codec_pb_encode_bytes(
               os, (const uint8_t *)str, len);
}

/*---------------------------------------------------------------------------*/
/* ENCODER — FIELD-LEVEL CONVENIENCE                                         */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_encode_uint32(tiku_kits_codec_pb_ostream_t *os,
                                      uint32_t field_number,
                                      uint32_t value)
{
    int rc;

    if (os == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = tiku_kits_codec_pb_encode_tag(
             os, field_number, TIKU_KITS_CODEC_PB_WIRE_VARINT);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    return tiku_kits_codec_pb_encode_varint(os, value);
}

int tiku_kits_codec_pb_encode_sint32(tiku_kits_codec_pb_ostream_t *os,
                                      uint32_t field_number,
                                      int32_t value)
{
    return tiku_kits_codec_pb_encode_uint32(
               os, field_number,
               TIKU_KITS_CODEC_PB_ZIGZAG_ENCODE(value));
}

int tiku_kits_codec_pb_encode_bool(tiku_kits_codec_pb_ostream_t *os,
                                    uint32_t field_number,
                                    uint8_t value)
{
    return tiku_kits_codec_pb_encode_uint32(
               os, field_number, value ? 1 : 0);
}

int tiku_kits_codec_pb_encode_string_field(
        tiku_kits_codec_pb_ostream_t *os,
        uint32_t field_number,
        const char *str, uint16_t len)
{
    int rc;

    if (os == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = tiku_kits_codec_pb_encode_tag(
             os, field_number, TIKU_KITS_CODEC_PB_WIRE_BYTES);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    return tiku_kits_codec_pb_encode_string(os, str, len);
}

int tiku_kits_codec_pb_encode_bytes_field(
        tiku_kits_codec_pb_ostream_t *os,
        uint32_t field_number,
        const uint8_t *data, uint16_t len)
{
    int rc;

    if (os == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = tiku_kits_codec_pb_encode_tag(
             os, field_number, TIKU_KITS_CODEC_PB_WIRE_BYTES);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    return tiku_kits_codec_pb_encode_bytes(os, data, len);
}

int tiku_kits_codec_pb_encode_fixed32_field(
        tiku_kits_codec_pb_ostream_t *os,
        uint32_t field_number,
        uint32_t value)
{
    int rc;

    if (os == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = tiku_kits_codec_pb_encode_tag(
             os, field_number, TIKU_KITS_CODEC_PB_WIRE_32BIT);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    return tiku_kits_codec_pb_encode_fixed32(os, value);
}

int tiku_kits_codec_pb_encode_submessage(
        tiku_kits_codec_pb_ostream_t *os,
        uint32_t field_number,
        const uint8_t *data, uint16_t len)
{
    return tiku_kits_codec_pb_encode_bytes_field(
               os, field_number, data, len);
}

/*---------------------------------------------------------------------------*/
/* INPUT STREAM INIT / QUERY                                                 */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_istream_init(tiku_kits_codec_pb_istream_t *is,
                                     const uint8_t *buf, uint16_t len)
{
    if (is == NULL || buf == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    is->buf = buf;
    is->len = len;
    is->pos = 0;

    return TIKU_KITS_CODEC_OK;
}

uint16_t tiku_kits_codec_pb_istream_remaining(
             const tiku_kits_codec_pb_istream_t *is)
{
    if (is == NULL) {
        return 0;
    }
    return is->len - is->pos;
}

/*---------------------------------------------------------------------------*/
/* DECODER — VARINT                                                          */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_decode_varint(tiku_kits_codec_pb_istream_t *is,
                                      uint32_t *value)
{
    uint32_t result = 0;
    uint8_t  shift  = 0;
    uint8_t  byte;

    if (is == NULL || value == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    do {
        if (is->pos >= is->len) {
            return TIKU_KITS_CODEC_ERR_UNDERFLOW;
        }

        byte = is->buf[is->pos++];
        result |= (uint32_t)(byte & 0x7F) << shift;
        shift += 7;

        if (shift > 35) {
            /* Varint too long for 32-bit value */
            return TIKU_KITS_CODEC_ERR_CORRUPT;
        }
    } while (byte & 0x80);

    *value = result;
    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER — TAG                                                             */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_decode_tag(tiku_kits_codec_pb_istream_t *is,
                                    uint32_t *field_number,
                                    uint8_t *wire_type)
{
    uint32_t tag;
    int rc;

    if (is == NULL || field_number == NULL || wire_type == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = tiku_kits_codec_pb_decode_varint(is, &tag);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    *wire_type    = (uint8_t)(tag & 0x07);
    *field_number = tag >> 3;

    if (*field_number == 0) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER — FIXED32                                                         */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_decode_fixed32(tiku_kits_codec_pb_istream_t *is,
                                       uint32_t *value)
{
    if (is == NULL || value == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    if (is->pos + 4 > is->len) {
        return TIKU_KITS_CODEC_ERR_UNDERFLOW;
    }

    *value = (uint32_t)is->buf[is->pos]            |
             ((uint32_t)is->buf[is->pos + 1] << 8)  |
             ((uint32_t)is->buf[is->pos + 2] << 16) |
             ((uint32_t)is->buf[is->pos + 3] << 24);
    is->pos += 4;

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER — LENGTH-DELIMITED                                                */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_decode_bytes(tiku_kits_codec_pb_istream_t *is,
                                     const uint8_t **data, uint16_t *len)
{
    uint32_t raw_len;
    int rc;

    if (is == NULL || data == NULL || len == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = tiku_kits_codec_pb_decode_varint(is, &raw_len);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (raw_len > 0xFFFF) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }

    if (is->pos + (uint16_t)raw_len > is->len) {
        return TIKU_KITS_CODEC_ERR_UNDERFLOW;
    }

    *data = &is->buf[is->pos];
    *len  = (uint16_t)raw_len;
    is->pos += (uint16_t)raw_len;

    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_pb_decode_string(tiku_kits_codec_pb_istream_t *is,
                                      const char **str, uint16_t *len)
{
    return tiku_kits_codec_pb_decode_bytes(
               is, (const uint8_t **)str, len);
}

/*---------------------------------------------------------------------------*/
/* DECODER — SKIP                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_pb_skip_field(tiku_kits_codec_pb_istream_t *is,
                                   uint8_t wire_type)
{
    uint32_t dummy;
    const uint8_t *skip_data;
    uint16_t skip_len;

    if (is == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    switch (wire_type) {
    case TIKU_KITS_CODEC_PB_WIRE_VARINT:
        return tiku_kits_codec_pb_decode_varint(is, &dummy);

    case TIKU_KITS_CODEC_PB_WIRE_32BIT:
        return tiku_kits_codec_pb_decode_fixed32(is, &dummy);

    case TIKU_KITS_CODEC_PB_WIRE_64BIT:
        if (is->pos + 8 > is->len) {
            return TIKU_KITS_CODEC_ERR_UNDERFLOW;
        }
        is->pos += 8;
        return TIKU_KITS_CODEC_OK;

    case TIKU_KITS_CODEC_PB_WIRE_BYTES:
        return tiku_kits_codec_pb_decode_bytes(
                   is, &skip_data, &skip_len);

    default:
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }
}
