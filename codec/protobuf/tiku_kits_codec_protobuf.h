/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec_protobuf.h - Protocol Buffers (nanopb-style) codec
 *
 * Implements a subset of the Protocol Buffers binary wire format
 * suitable for ultra-low-power microcontrollers.  Provides the same
 * primitives as nanopb but with the TikuKits API conventions: zero
 * heap allocation, cursor-based streaming, shared codec return codes.
 *
 * Supported wire types:
 *   0 - Varint   (uint32, int32, sint32, bool, enum)
 *   2 - Length-delimited (bytes, string, submessage)
 *   5 - 32-bit   (fixed32, sfixed32)
 *
 * Not supported:
 *   1 - 64-bit   (fixed64, sfixed64, double) -- too wide for 16-bit MCU
 *   Groups (deprecated wire types 3 and 4)
 *
 * Varint encoding uses the standard LEB128 format (7 bits per byte,
 * MSB continuation flag).  Signed integers use ZigZag encoding for
 * efficient representation of small negative values.
 *
 * Design:
 *   - Zero heap allocation.  The encoder writes into a caller-provided
 *     buffer via an ostream struct.  The decoder reads from a const
 *     buffer via an istream struct.
 *   - Incremental API: fields are encoded/decoded one at a time.
 *   - Field tags are explicit (field_number << 3 | wire_type), matching
 *     the protobuf wire format exactly.
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

#ifndef TIKU_KITS_CODEC_PROTOBUF_H_
#define TIKU_KITS_CODEC_PROTOBUF_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_codec.h"

/*---------------------------------------------------------------------------*/
/* WIRE TYPES                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Protobuf wire type identifiers (bottom 3 bits of a field tag).
 */
#define TIKU_KITS_CODEC_PB_WIRE_VARINT    0  /**< Varint (LEB128) */
#define TIKU_KITS_CODEC_PB_WIRE_64BIT     1  /**< 64-bit (unsupported) */
#define TIKU_KITS_CODEC_PB_WIRE_BYTES     2  /**< Length-delimited */
#define TIKU_KITS_CODEC_PB_WIRE_32BIT     5  /**< 32-bit fixed */

/**
 * @brief Build a field tag from field number and wire type.
 *
 * @param field_number  Protobuf field number (1..2^29-1)
 * @param wire_type     Wire type (0, 2, or 5)
 * @return Tag value to encode
 */
#define TIKU_KITS_CODEC_PB_TAG(field_number, wire_type) \
    (((uint32_t)(field_number) << 3) | (uint32_t)(wire_type))

/*---------------------------------------------------------------------------*/
/* ZIGZAG HELPERS                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief ZigZag-encode a signed 32-bit integer.
 *
 * Maps signed values to unsigned: 0->0, -1->1, 1->2, -2->3, ...
 * This allows small negative values to use fewer varint bytes.
 */
#define TIKU_KITS_CODEC_PB_ZIGZAG_ENCODE(n) \
    ((uint32_t)(((int32_t)(n) << 1) ^ ((int32_t)(n) >> 31)))

/**
 * @brief ZigZag-decode an unsigned value to signed 32-bit integer.
 */
#define TIKU_KITS_CODEC_PB_ZIGZAG_DECODE(n) \
    ((int32_t)(((n) >> 1) ^ -(int32_t)((n) & 1)))

/*---------------------------------------------------------------------------*/
/* ENCODER (OUTPUT STREAM)                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_codec_pb_ostream
 * @brief Protobuf encoder cursor over a caller-provided buffer.
 *
 * Example:
 * @code
 *   uint8_t buf[64];
 *   tiku_kits_codec_pb_ostream_t os;
 *   tiku_kits_codec_pb_ostream_init(&os, buf, sizeof(buf));
 *
 *   // Encode field 1 (uint32): temperature = 23
 *   tiku_kits_codec_pb_encode_tag(&os, 1, TIKU_KITS_CODEC_PB_WIRE_VARINT);
 *   tiku_kits_codec_pb_encode_varint(&os, 23);
 *
 *   // Encode field 2 (string): name = "sensor"
 *   tiku_kits_codec_pb_encode_tag(&os, 2, TIKU_KITS_CODEC_PB_WIRE_BYTES);
 *   tiku_kits_codec_pb_encode_string(&os, "sensor", 6);
 *
 *   uint16_t len = tiku_kits_codec_pb_ostream_len(&os);
 * @endcode
 */
typedef struct {
    uint8_t  *buf;      /**< Output buffer (caller-provided) */
    uint16_t  capacity; /**< Buffer capacity in bytes */
    uint16_t  pos;      /**< Current write position */
} tiku_kits_codec_pb_ostream_t;

/**
 * @brief Initialize a protobuf output stream.
 *
 * @param os       Stream to initialize
 * @param buf      Output buffer
 * @param capacity Buffer size in bytes
 * @return TIKU_KITS_CODEC_OK, or TIKU_KITS_CODEC_ERR_NULL
 */
int tiku_kits_codec_pb_ostream_init(tiku_kits_codec_pb_ostream_t *os,
                                     uint8_t *buf, uint16_t capacity);

/**
 * @brief Return the number of bytes written so far.
 *
 * @param os  Stream to query
 * @return Bytes written, or 0 if os is NULL
 */
uint16_t tiku_kits_codec_pb_ostream_len(
             const tiku_kits_codec_pb_ostream_t *os);

/**
 * @brief Encode a varint (LEB128, up to 32-bit value).
 *
 * Uses 1..5 bytes depending on magnitude.
 *
 * @param os    Output stream
 * @param value Unsigned value to encode
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_varint(tiku_kits_codec_pb_ostream_t *os,
                                      uint32_t value);

/**
 * @brief Encode a field tag (field_number << 3 | wire_type) as a varint.
 *
 * @param os           Output stream
 * @param field_number Protobuf field number (1..536870911)
 * @param wire_type    Wire type (0, 2, or 5)
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_tag(tiku_kits_codec_pb_ostream_t *os,
                                    uint32_t field_number,
                                    uint8_t wire_type);

/**
 * @brief Encode a fixed 32-bit value (little-endian on wire).
 *
 * @param os    Output stream
 * @param value 32-bit value
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_fixed32(tiku_kits_codec_pb_ostream_t *os,
                                       uint32_t value);

/**
 * @brief Encode a length-delimited byte string.
 *
 * Writes the length as a varint followed by the raw bytes.
 *
 * @param os   Output stream
 * @param data Byte data (may be NULL if len == 0)
 * @param len  Length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_bytes(tiku_kits_codec_pb_ostream_t *os,
                                     const uint8_t *data, uint16_t len);

/**
 * @brief Encode a length-delimited text string.
 *
 * Same wire format as bytes but typed for convenience.
 *
 * @param os   Output stream
 * @param str  String data (may be NULL if len == 0)
 * @param len  Length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_string(tiku_kits_codec_pb_ostream_t *os,
                                      const char *str, uint16_t len);

/**
 * @brief Encode a uint32 field (tag + varint).
 *
 * Convenience: encodes the tag and varint value in one call.
 *
 * @param os           Output stream
 * @param field_number Field number
 * @param value        Unsigned 32-bit value
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_uint32(tiku_kits_codec_pb_ostream_t *os,
                                      uint32_t field_number,
                                      uint32_t value);

/**
 * @brief Encode a sint32 field (tag + ZigZag varint).
 *
 * Uses ZigZag encoding for efficient small-negative-value representation.
 *
 * @param os           Output stream
 * @param field_number Field number
 * @param value        Signed 32-bit value
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_sint32(tiku_kits_codec_pb_ostream_t *os,
                                      uint32_t field_number,
                                      int32_t value);

/**
 * @brief Encode a bool field (tag + varint 0 or 1).
 *
 * @param os           Output stream
 * @param field_number Field number
 * @param value        Non-zero for true, zero for false
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_bool(tiku_kits_codec_pb_ostream_t *os,
                                    uint32_t field_number,
                                    uint8_t value);

/**
 * @brief Encode a string field (tag + length-delimited string).
 *
 * @param os           Output stream
 * @param field_number Field number
 * @param str          String data
 * @param len          Length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_string_field(
        tiku_kits_codec_pb_ostream_t *os,
        uint32_t field_number,
        const char *str, uint16_t len);

/**
 * @brief Encode a bytes field (tag + length-delimited bytes).
 *
 * @param os           Output stream
 * @param field_number Field number
 * @param data         Byte data
 * @param len          Length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_bytes_field(
        tiku_kits_codec_pb_ostream_t *os,
        uint32_t field_number,
        const uint8_t *data, uint16_t len);

/**
 * @brief Encode a fixed32 field (tag + 4 bytes little-endian).
 *
 * @param os           Output stream
 * @param field_number Field number
 * @param value        32-bit value
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_fixed32_field(
        tiku_kits_codec_pb_ostream_t *os,
        uint32_t field_number,
        uint32_t value);

/**
 * @brief Encode a submessage field (tag + length-prefixed payload).
 *
 * Writes the tag, then the payload length as a varint, then copies
 * the pre-encoded submessage bytes.  The caller encodes the
 * submessage into a separate buffer first, then passes it here.
 *
 * @param os           Output stream
 * @param field_number Field number
 * @param data         Pre-encoded submessage bytes
 * @param len          Length of submessage in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_pb_encode_submessage(
        tiku_kits_codec_pb_ostream_t *os,
        uint32_t field_number,
        const uint8_t *data, uint16_t len);

/*---------------------------------------------------------------------------*/
/* DECODER (INPUT STREAM)                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_codec_pb_istream
 * @brief Protobuf decoder cursor over a const buffer.
 *
 * Example:
 * @code
 *   tiku_kits_codec_pb_istream_t is;
 *   tiku_kits_codec_pb_istream_init(&is, buf, len);
 *
 *   uint32_t field_number;
 *   uint8_t  wire_type;
 *   while (tiku_kits_codec_pb_istream_remaining(&is) > 0) {
 *       tiku_kits_codec_pb_decode_tag(&is, &field_number, &wire_type);
 *       if (field_number == 1 && wire_type == 0) {
 *           uint32_t val;
 *           tiku_kits_codec_pb_decode_varint(&is, &val);
 *       } else {
 *           tiku_kits_codec_pb_skip_field(&is, wire_type);
 *       }
 *   }
 * @endcode
 */
typedef struct {
    const uint8_t *buf;  /**< Input buffer (caller-owned, const) */
    uint16_t       len;  /**< Total buffer length in bytes */
    uint16_t       pos;  /**< Current read position */
} tiku_kits_codec_pb_istream_t;

/**
 * @brief Initialize a protobuf input stream.
 *
 * @param is   Stream to initialize
 * @param buf  Encoded input data
 * @param len  Length of input in bytes
 * @return TIKU_KITS_CODEC_OK, or TIKU_KITS_CODEC_ERR_NULL
 */
int tiku_kits_codec_pb_istream_init(tiku_kits_codec_pb_istream_t *is,
                                     const uint8_t *buf, uint16_t len);

/**
 * @brief Return the number of unread bytes remaining.
 *
 * @param is  Stream to query
 * @return Bytes remaining, or 0 if is is NULL
 */
uint16_t tiku_kits_codec_pb_istream_remaining(
             const tiku_kits_codec_pb_istream_t *is);

/**
 * @brief Decode a varint (LEB128, up to 32-bit value).
 *
 * @param is    Input stream
 * @param value Output: decoded unsigned value
 * @return TIKU_KITS_CODEC_OK, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_pb_decode_varint(tiku_kits_codec_pb_istream_t *is,
                                      uint32_t *value);

/**
 * @brief Decode a field tag into field number and wire type.
 *
 * @param is           Input stream
 * @param field_number Output: field number
 * @param wire_type    Output: wire type (0, 2, or 5)
 * @return TIKU_KITS_CODEC_OK, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_pb_decode_tag(tiku_kits_codec_pb_istream_t *is,
                                    uint32_t *field_number,
                                    uint8_t *wire_type);

/**
 * @brief Decode a fixed 32-bit value (little-endian on wire).
 *
 * @param is    Input stream
 * @param value Output: decoded 32-bit value
 * @return TIKU_KITS_CODEC_OK or ERR_UNDERFLOW
 */
int tiku_kits_codec_pb_decode_fixed32(tiku_kits_codec_pb_istream_t *is,
                                       uint32_t *value);

/**
 * @brief Decode a length-delimited field (bytes or string).
 *
 * Returns a pointer into the input buffer for zero-copy access.
 * The stream advances past the payload bytes.
 *
 * @param is   Input stream
 * @param data Output: pointer to payload within input buffer
 * @param len  Output: payload length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_pb_decode_bytes(tiku_kits_codec_pb_istream_t *is,
                                     const uint8_t **data, uint16_t *len);

/**
 * @brief Decode a length-delimited string field.
 *
 * Same as decode_bytes but typed for strings.
 *
 * @param is   Input stream
 * @param str  Output: pointer to string within input buffer
 * @param len  Output: string length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_pb_decode_string(tiku_kits_codec_pb_istream_t *is,
                                      const char **str, uint16_t *len);

/**
 * @brief Skip a field based on its wire type.
 *
 * Advances the stream past the field's payload without decoding.
 * Useful for ignoring unknown fields during decoding.
 *
 * @param is        Input stream
 * @param wire_type Wire type of the field to skip
 * @return TIKU_KITS_CODEC_OK, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_pb_skip_field(tiku_kits_codec_pb_istream_t *is,
                                   uint8_t wire_type);

#endif /* TIKU_KITS_CODEC_PROTOBUF_H_ */
