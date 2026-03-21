/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec_cbor.c - CBOR encoder/decoder for embedded systems
 *
 * Implements a subset of CBOR (RFC 8949) for ultra-low-power MCUs.
 * Zero heap allocation, incremental encode/decode, ~600 bytes code.
 *
 * CBOR wire format (initial byte):
 *   [major type (3 bits)][additional info (5 bits)]
 *
 *   additional info:
 *     0..23   — value is the additional info itself
 *     24      — next 1 byte is the value
 *     25      — next 2 bytes are the value (big-endian)
 *     26      — next 4 bytes are the value (big-endian)
 *     27      — next 8 bytes are the value (not supported here)
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

#include "tiku_kits_codec_cbor.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* PRIVATE HELPERS — ENCODING                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Compute how many bytes a (major_type, value) head will occupy.
 *
 * @param value  The unsigned argument
 * @return 1, 2, 3, or 5 bytes
 */
static uint8_t head_size(uint32_t value)
{
    if (value <= 23) {
        return 1;
    } else if (value <= 0xFF) {
        return 2;
    } else if (value <= 0xFFFF) {
        return 3;
    } else {
        return 5;
    }
}

/**
 * @brief Write a CBOR head (initial byte + optional argument bytes).
 *
 * Encodes the 3-bit major type and up to 4-byte unsigned argument
 * in the smallest possible form.  Does NOT check capacity — the
 * caller must verify space before calling.
 *
 * @param dst   Destination buffer (write position)
 * @param major Major type (0..7)
 * @param value Unsigned argument
 * @return Number of bytes written (1, 2, 3, or 5)
 */
static uint8_t write_head(uint8_t *dst, uint8_t major, uint32_t value)
{
    uint8_t mt = (uint8_t)(major << 5);

    if (value <= 23) {
        dst[0] = mt | (uint8_t)value;
        return 1;
    } else if (value <= 0xFF) {
        dst[0] = mt | 24;
        dst[1] = (uint8_t)value;
        return 2;
    } else if (value <= 0xFFFF) {
        dst[0] = mt | 25;
        dst[1] = (uint8_t)(value >> 8);
        dst[2] = (uint8_t)(value);
        return 3;
    } else {
        dst[0] = mt | 26;
        dst[1] = (uint8_t)(value >> 24);
        dst[2] = (uint8_t)(value >> 16);
        dst[3] = (uint8_t)(value >> 8);
        dst[4] = (uint8_t)(value);
        return 5;
    }
}

/**
 * @brief Encode a CBOR head into the writer (with overflow check).
 *
 * @param w     Writer
 * @param major Major type (0..7)
 * @param value Unsigned argument
 * @return TIKU_KITS_CODEC_OK or ERR_OVERFLOW
 */
static int encode_head(tiku_kits_codec_cbor_writer_t *w,
                        uint8_t major, uint32_t value)
{
    uint8_t needed = head_size(value);

    if (w->pos + needed > w->capacity) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }

    w->pos += write_head(&w->buf[w->pos], major, value);
    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* PRIVATE HELPERS — DECODING                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Read a CBOR head from the reader buffer.
 *
 * Parses the initial byte to extract the major type and additional
 * info, then reads 0/1/2/4 extra bytes for the unsigned argument.
 * Does NOT advance the reader — the caller decides how far to move.
 *
 * @param r         Reader
 * @param major_out Output: major type (0..7)
 * @param value_out Output: unsigned argument
 * @param consumed  Output: number of bytes consumed by the head
 * @return TIKU_KITS_CODEC_OK, ERR_UNDERFLOW, or ERR_CORRUPT
 */
static int read_head(const tiku_kits_codec_cbor_reader_t *r,
                      uint8_t *major_out, uint32_t *value_out,
                      uint8_t *consumed)
{
    uint8_t ib;
    uint8_t ai;
    uint16_t avail;

    avail = r->len - r->pos;
    if (avail == 0) {
        return TIKU_KITS_CODEC_ERR_UNDERFLOW;
    }

    ib = r->buf[r->pos];
    *major_out = (ib >> 5) & 0x07;
    ai = ib & 0x1F;

    if (ai <= 23) {
        *value_out = ai;
        *consumed = 1;
    } else if (ai == 24) {
        if (avail < 2) {
            return TIKU_KITS_CODEC_ERR_UNDERFLOW;
        }
        *value_out = r->buf[r->pos + 1];
        *consumed = 2;
    } else if (ai == 25) {
        if (avail < 3) {
            return TIKU_KITS_CODEC_ERR_UNDERFLOW;
        }
        *value_out = ((uint32_t)r->buf[r->pos + 1] << 8) |
                      (uint32_t)r->buf[r->pos + 2];
        *consumed = 3;
    } else if (ai == 26) {
        if (avail < 5) {
            return TIKU_KITS_CODEC_ERR_UNDERFLOW;
        }
        *value_out = ((uint32_t)r->buf[r->pos + 1] << 24) |
                      ((uint32_t)r->buf[r->pos + 2] << 16) |
                      ((uint32_t)r->buf[r->pos + 3] << 8)  |
                       (uint32_t)r->buf[r->pos + 4];
        *consumed = 5;
    } else {
        /* ai 27 = 8-byte value (not supported), 28-30 reserved, 31 break */
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }

    return TIKU_KITS_CODEC_OK;
}

/**
 * @brief Decode a head and verify the expected major type.
 *
 * Convenience wrapper around read_head() that also checks the major
 * type matches expectations.
 *
 * @param r         Reader
 * @param expected  Expected major type
 * @param value_out Output: unsigned argument
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, ERR_UNDERFLOW, or ERR_CORRUPT
 */
static int decode_typed_head(tiku_kits_codec_cbor_reader_t *r,
                              uint8_t expected, uint32_t *value_out)
{
    uint8_t major;
    uint8_t consumed;
    int rc;

    rc = read_head(r, &major, value_out, &consumed);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (major != expected) {
        return TIKU_KITS_CODEC_ERR_TYPE;
    }

    r->pos += consumed;
    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* WRITER INIT / QUERY                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_writer_init(tiku_kits_codec_cbor_writer_t *w,
                                      uint8_t *buf, uint16_t capacity)
{
    if (w == NULL || buf == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    w->buf      = buf;
    w->capacity = capacity;
    w->pos      = 0;

    return TIKU_KITS_CODEC_OK;
}

uint16_t tiku_kits_codec_cbor_writer_len(
             const tiku_kits_codec_cbor_writer_t *w)
{
    if (w == NULL) {
        return 0;
    }
    return w->pos;
}

/*---------------------------------------------------------------------------*/
/* ENCODER — INTEGERS                                                        */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_encode_uint(tiku_kits_codec_cbor_writer_t *w,
                                      uint32_t value)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    return encode_head(w, 0, value);
}

int tiku_kits_codec_cbor_encode_negint(tiku_kits_codec_cbor_writer_t *w,
                                        uint32_t value)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    return encode_head(w, 1, value);
}

int tiku_kits_codec_cbor_encode_int(tiku_kits_codec_cbor_writer_t *w,
                                     int32_t value)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    if (value >= 0) {
        return encode_head(w, 0, (uint32_t)value);
    } else {
        /* CBOR negative: -1 - n, so n = -(value + 1) = -value - 1 */
        return encode_head(w, 1, (uint32_t)(-(value + 1)));
    }
}

/*---------------------------------------------------------------------------*/
/* ENCODER — STRINGS                                                         */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_encode_bstr(tiku_kits_codec_cbor_writer_t *w,
                                      const uint8_t *data, uint16_t len)
{
    uint8_t hdr_size;

    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (data == NULL && len > 0) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    hdr_size = head_size(len);

    if (w->pos + hdr_size + len > w->capacity) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }

    w->pos += write_head(&w->buf[w->pos], 2, len);

    if (len > 0) {
        memcpy(&w->buf[w->pos], data, len);
        w->pos += len;
    }

    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_cbor_encode_tstr(tiku_kits_codec_cbor_writer_t *w,
                                      const char *str, uint16_t len)
{
    uint8_t hdr_size;

    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (str == NULL && len > 0) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    hdr_size = head_size(len);

    if (w->pos + hdr_size + len > w->capacity) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }

    w->pos += write_head(&w->buf[w->pos], 3, len);

    if (len > 0) {
        memcpy(&w->buf[w->pos], str, len);
        w->pos += len;
    }

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* ENCODER — CONTAINERS                                                      */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_encode_array(tiku_kits_codec_cbor_writer_t *w,
                                       uint16_t count)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    return encode_head(w, 4, count);
}

int tiku_kits_codec_cbor_encode_map(tiku_kits_codec_cbor_writer_t *w,
                                     uint16_t count)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    return encode_head(w, 5, count);
}

/*---------------------------------------------------------------------------*/
/* ENCODER — SIMPLE VALUES                                                   */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_encode_bool(tiku_kits_codec_cbor_writer_t *w,
                                      uint8_t value)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    return encode_head(w, 7, value ? TIKU_KITS_CODEC_CBOR_TRUE
                                   : TIKU_KITS_CODEC_CBOR_FALSE);
}

int tiku_kits_codec_cbor_encode_null(tiku_kits_codec_cbor_writer_t *w)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    return encode_head(w, 7, TIKU_KITS_CODEC_CBOR_NULL);
}

/*---------------------------------------------------------------------------*/
/* READER INIT / QUERY                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_reader_init(tiku_kits_codec_cbor_reader_t *r,
                                      const uint8_t *buf, uint16_t len)
{
    if (r == NULL || buf == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    r->buf = buf;
    r->len = len;
    r->pos = 0;

    return TIKU_KITS_CODEC_OK;
}

uint16_t tiku_kits_codec_cbor_reader_remaining(
             const tiku_kits_codec_cbor_reader_t *r)
{
    if (r == NULL) {
        return 0;
    }
    return r->len - r->pos;
}

/*---------------------------------------------------------------------------*/
/* DECODER — PEEK                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_peek_type(const tiku_kits_codec_cbor_reader_t *r,
                                    tiku_kits_codec_cbor_type_t *type)
{
    if (r == NULL || type == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    if (r->pos >= r->len) {
        return TIKU_KITS_CODEC_ERR_UNDERFLOW;
    }

    *type = (tiku_kits_codec_cbor_type_t)((r->buf[r->pos] >> 5) & 0x07);

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER — INTEGERS                                                        */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_decode_uint(tiku_kits_codec_cbor_reader_t *r,
                                      uint32_t *value)
{
    if (r == NULL || value == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    return decode_typed_head(r, 0, value);
}

int tiku_kits_codec_cbor_decode_negint(tiku_kits_codec_cbor_reader_t *r,
                                        uint32_t *value)
{
    if (r == NULL || value == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    return decode_typed_head(r, 1, value);
}

int tiku_kits_codec_cbor_decode_int(tiku_kits_codec_cbor_reader_t *r,
                                     int32_t *value)
{
    uint8_t major;
    uint32_t raw;
    uint8_t consumed;
    int rc;

    if (r == NULL || value == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = read_head(r, &major, &raw, &consumed);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (major == 0) {
        if (raw > (uint32_t)INT32_MAX) {
            return TIKU_KITS_CODEC_ERR_CORRUPT;
        }
        *value = (int32_t)raw;
    } else if (major == 1) {
        /* CBOR: actual value = -1 - raw */
        if (raw > (uint32_t)(-(int32_t)INT32_MIN - 1)) {
            return TIKU_KITS_CODEC_ERR_CORRUPT;
        }
        *value = -1 - (int32_t)raw;
    } else {
        return TIKU_KITS_CODEC_ERR_TYPE;
    }

    r->pos += consumed;
    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER — STRINGS                                                         */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_decode_bstr(tiku_kits_codec_cbor_reader_t *r,
                                      const uint8_t **data, uint16_t *len)
{
    uint32_t str_len;
    int rc;

    if (r == NULL || data == NULL || len == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = decode_typed_head(r, 2, &str_len);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (str_len > 0xFFFF) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }

    if (r->pos + (uint16_t)str_len > r->len) {
        return TIKU_KITS_CODEC_ERR_UNDERFLOW;
    }

    *data = &r->buf[r->pos];
    *len  = (uint16_t)str_len;
    r->pos += (uint16_t)str_len;

    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_cbor_decode_tstr(tiku_kits_codec_cbor_reader_t *r,
                                      const char **str, uint16_t *len)
{
    uint32_t str_len;
    int rc;

    if (r == NULL || str == NULL || len == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = decode_typed_head(r, 3, &str_len);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (str_len > 0xFFFF) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }

    if (r->pos + (uint16_t)str_len > r->len) {
        return TIKU_KITS_CODEC_ERR_UNDERFLOW;
    }

    *str = (const char *)&r->buf[r->pos];
    *len = (uint16_t)str_len;
    r->pos += (uint16_t)str_len;

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER — CONTAINERS                                                      */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_decode_array(tiku_kits_codec_cbor_reader_t *r,
                                       uint16_t *count)
{
    uint32_t raw;
    int rc;

    if (r == NULL || count == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = decode_typed_head(r, 4, &raw);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (raw > 0xFFFF) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }

    *count = (uint16_t)raw;
    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_cbor_decode_map(tiku_kits_codec_cbor_reader_t *r,
                                     uint16_t *count)
{
    uint32_t raw;
    int rc;

    if (r == NULL || count == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = decode_typed_head(r, 5, &raw);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (raw > 0xFFFF) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }

    *count = (uint16_t)raw;
    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER — SIMPLE VALUES                                                   */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_cbor_decode_bool(tiku_kits_codec_cbor_reader_t *r,
                                      uint8_t *value)
{
    uint32_t raw;
    int rc;

    if (r == NULL || value == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = decode_typed_head(r, 7, &raw);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (raw == TIKU_KITS_CODEC_CBOR_TRUE) {
        *value = 1;
    } else if (raw == TIKU_KITS_CODEC_CBOR_FALSE) {
        *value = 0;
    } else {
        return TIKU_KITS_CODEC_ERR_TYPE;
    }

    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_cbor_decode_null(tiku_kits_codec_cbor_reader_t *r)
{
    uint32_t raw;
    int rc;

    if (r == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = decode_typed_head(r, 7, &raw);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    if (raw != TIKU_KITS_CODEC_CBOR_NULL) {
        return TIKU_KITS_CODEC_ERR_TYPE;
    }

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* DECODER — SKIP                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Skip the next CBOR item without decoding its payload.
 *
 * Reads the head to determine the type and size, then advances the
 * reader past the entire item.  For containers (array, map), this
 * recursively skips all contained items.
 */
int tiku_kits_codec_cbor_skip(tiku_kits_codec_cbor_reader_t *r)
{
    uint8_t major;
    uint32_t value;
    uint8_t consumed;
    int rc;
    uint32_t i;
    uint32_t items;

    if (r == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = read_head(r, &major, &value, &consumed);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    r->pos += consumed;

    switch (major) {
    case 0: /* unsigned int — no payload */
    case 1: /* negative int — no payload */
        return TIKU_KITS_CODEC_OK;

    case 2: /* byte string */
    case 3: /* text string */
        if (value > 0xFFFF || r->pos + (uint16_t)value > r->len) {
            return TIKU_KITS_CODEC_ERR_UNDERFLOW;
        }
        r->pos += (uint16_t)value;
        return TIKU_KITS_CODEC_OK;

    case 4: /* array — skip 'value' items */
        items = value;
        for (i = 0; i < items; i++) {
            rc = tiku_kits_codec_cbor_skip(r);
            if (rc != TIKU_KITS_CODEC_OK) {
                return rc;
            }
        }
        return TIKU_KITS_CODEC_OK;

    case 5: /* map — skip 'value' pairs (2 items each) */
        items = value;
        for (i = 0; i < items; i++) {
            rc = tiku_kits_codec_cbor_skip(r); /* key */
            if (rc != TIKU_KITS_CODEC_OK) {
                return rc;
            }
            rc = tiku_kits_codec_cbor_skip(r); /* value */
            if (rc != TIKU_KITS_CODEC_OK) {
                return rc;
            }
        }
        return TIKU_KITS_CODEC_OK;

    case 7: /* simple value — no payload */
        return TIKU_KITS_CODEC_OK;

    default:
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }
}
