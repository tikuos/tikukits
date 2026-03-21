/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec_cbor.h - CBOR encoder/decoder for embedded systems
 *
 * Implements a subset of CBOR (RFC 8949) suitable for ultra-low-power
 * microcontrollers.  Supports the five major types needed for IoT
 * payloads: unsigned integers, negative integers, byte strings, text
 * strings, arrays, and maps.  Floating-point and tags are not
 * supported to keep code size and RAM usage minimal.
 *
 * Design:
 *   - Zero heap allocation.  The encoder writes into a caller-provided
 *     buffer via a cursor struct (tiku_kits_codec_cbor_writer_t).  The
 *     decoder reads from a const buffer via a reader struct.
 *   - Incremental API: items are encoded/decoded one at a time, so
 *     arbitrarily large payloads can be built without holding the
 *     entire object model in memory.
 *   - No nested container tracking.  The caller is responsible for
 *     encoding the correct number of items after opening an array or
 *     map.  This keeps the struct size at ~8 bytes.
 *
 * Supported CBOR major types:
 *   0 - Unsigned integer  (0 .. 2^32-1)
 *   1 - Negative integer  (-1 .. -2^32)
 *   2 - Byte string       (definite length)
 *   3 - Text string       (definite length, UTF-8 not validated)
 *   4 - Array             (definite length)
 *   5 - Map               (definite length)
 *   7 - Simple values     (true, false, null)
 *
 * Not supported:
 *   - Indefinite-length containers
 *   - Floating-point (half/single/double)
 *   - Tags (major type 6)
 *   - 64-bit integers (values > 2^32-1)
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

#ifndef TIKU_KITS_CODEC_CBOR_H_
#define TIKU_KITS_CODEC_CBOR_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_codec.h"

/*---------------------------------------------------------------------------*/
/* CBOR MAJOR TYPES                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief CBOR major type identifiers (top 3 bits of the initial byte)
 *
 * Used by the decoder to report what type the next item is, and
 * internally by the encoder to construct initial bytes.
 */
typedef enum {
    TIKU_KITS_CODEC_CBOR_UINT    = 0, /**< Major type 0: unsigned integer */
    TIKU_KITS_CODEC_CBOR_NEGINT  = 1, /**< Major type 1: negative integer */
    TIKU_KITS_CODEC_CBOR_BSTR    = 2, /**< Major type 2: byte string */
    TIKU_KITS_CODEC_CBOR_TSTR    = 3, /**< Major type 3: text string */
    TIKU_KITS_CODEC_CBOR_ARRAY   = 4, /**< Major type 4: array */
    TIKU_KITS_CODEC_CBOR_MAP     = 5, /**< Major type 5: map */
    TIKU_KITS_CODEC_CBOR_SIMPLE  = 7  /**< Major type 7: simple/float */
} tiku_kits_codec_cbor_type_t;

/*---------------------------------------------------------------------------*/
/* SIMPLE VALUES                                                             */
/*---------------------------------------------------------------------------*/

/** CBOR simple value: false (major 7, additional 20) */
#define TIKU_KITS_CODEC_CBOR_FALSE  20
/** CBOR simple value: true (major 7, additional 21) */
#define TIKU_KITS_CODEC_CBOR_TRUE   21
/** CBOR simple value: null (major 7, additional 22) */
#define TIKU_KITS_CODEC_CBOR_NULL   22

/*---------------------------------------------------------------------------*/
/* ENCODER                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_codec_cbor_writer
 * @brief CBOR encoder cursor over a caller-provided buffer.
 *
 * The writer tracks a byte buffer, its capacity, and the current
 * write position.  Each encode function advances the position.
 * If any encode call would overflow the buffer, it returns
 * TIKU_KITS_CODEC_ERR_OVERFLOW and the writer is left unchanged
 * (the partial item is not written).
 *
 * Example:
 * @code
 *   uint8_t buf[64];
 *   tiku_kits_codec_cbor_writer_t w;
 *   tiku_kits_codec_cbor_writer_init(&w, buf, sizeof(buf));
 *
 *   tiku_kits_codec_cbor_encode_map(&w, 2);       // map with 2 pairs
 *   tiku_kits_codec_cbor_encode_tstr(&w, "temp", 4);
 *   tiku_kits_codec_cbor_encode_uint(&w, 23);
 *   tiku_kits_codec_cbor_encode_tstr(&w, "hum", 3);
 *   tiku_kits_codec_cbor_encode_uint(&w, 55);
 *
 *   uint16_t len = tiku_kits_codec_cbor_writer_len(&w);
 *   // buf[0..len) contains valid CBOR: {\"temp\": 23, \"hum\": 55}
 * @endcode
 */
typedef struct {
    uint8_t  *buf;      /**< Output buffer (caller-provided) */
    uint16_t  capacity; /**< Buffer capacity in bytes */
    uint16_t  pos;      /**< Current write position */
} tiku_kits_codec_cbor_writer_t;

/**
 * @brief Initialize a CBOR writer over a caller-provided buffer.
 *
 * @param w       Writer to initialize
 * @param buf     Output buffer
 * @param capacity Buffer size in bytes
 * @return TIKU_KITS_CODEC_OK, or TIKU_KITS_CODEC_ERR_NULL
 */
int tiku_kits_codec_cbor_writer_init(tiku_kits_codec_cbor_writer_t *w,
                                      uint8_t *buf, uint16_t capacity);

/**
 * @brief Return the number of bytes written so far.
 *
 * @param w  Writer to query
 * @return Bytes written, or 0 if w is NULL
 */
uint16_t tiku_kits_codec_cbor_writer_len(
             const tiku_kits_codec_cbor_writer_t *w);

/**
 * @brief Encode an unsigned integer (major type 0).
 *
 * Supports values 0 .. 2^32-1.  Uses the smallest CBOR encoding:
 * single byte for 0..23, 2 bytes for 24..255, 3 bytes for 256..65535,
 * 5 bytes for 65536..2^32-1.
 *
 * @param w     Writer
 * @param value Unsigned integer value
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_uint(tiku_kits_codec_cbor_writer_t *w,
                                      uint32_t value);

/**
 * @brief Encode a negative integer (major type 1).
 *
 * CBOR encodes negative integers as -1 - n, where n is the unsigned
 * argument.  This function takes the absolute value and encodes it
 * as major type 1.  The range is -1 .. -2^32.
 *
 * @param w     Writer
 * @param value Absolute value minus one (e.g. pass 0 for -1, 9 for -10)
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_negint(tiku_kits_codec_cbor_writer_t *w,
                                        uint32_t value);

/**
 * @brief Encode a signed integer (convenience wrapper).
 *
 * Dispatches to encode_uint for non-negative values or encode_negint
 * for negative values.
 *
 * @param w     Writer
 * @param value Signed integer (-2^31 .. 2^31-1)
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_int(tiku_kits_codec_cbor_writer_t *w,
                                     int32_t value);

/**
 * @brief Encode a byte string (major type 2).
 *
 * Writes the length header followed by the raw bytes.  The caller
 * retains ownership of the source data.
 *
 * @param w     Writer
 * @param data  Byte string data (may be NULL if len == 0)
 * @param len   Length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_bstr(tiku_kits_codec_cbor_writer_t *w,
                                      const uint8_t *data, uint16_t len);

/**
 * @brief Encode a text string (major type 3).
 *
 * Identical to bstr encoding but uses major type 3.  UTF-8 validity
 * is NOT checked — the caller is responsible for providing valid UTF-8.
 *
 * @param w     Writer
 * @param str   Text string data (may be NULL if len == 0)
 * @param len   Length in bytes (not characters)
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_tstr(tiku_kits_codec_cbor_writer_t *w,
                                      const char *str, uint16_t len);

/**
 * @brief Encode an array header (major type 4).
 *
 * Writes the array length header.  The caller must then encode
 * exactly @p count items immediately after this call.
 *
 * @param w     Writer
 * @param count Number of items in the array
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_array(tiku_kits_codec_cbor_writer_t *w,
                                       uint16_t count);

/**
 * @brief Encode a map header (major type 5).
 *
 * Writes the map length header.  The caller must then encode
 * exactly @p count key-value pairs (2 * count items) immediately
 * after this call.
 *
 * @param w     Writer
 * @param count Number of key-value pairs
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_map(tiku_kits_codec_cbor_writer_t *w,
                                     uint16_t count);

/**
 * @brief Encode a boolean value.
 *
 * Encodes CBOR simple value true (0xF5) or false (0xF4).
 *
 * @param w     Writer
 * @param value Non-zero for true, zero for false
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_bool(tiku_kits_codec_cbor_writer_t *w,
                                      uint8_t value);

/**
 * @brief Encode a null value.
 *
 * Encodes CBOR simple value null (0xF6).
 *
 * @param w     Writer
 * @return TIKU_KITS_CODEC_OK or TIKU_KITS_CODEC_ERR_OVERFLOW
 */
int tiku_kits_codec_cbor_encode_null(tiku_kits_codec_cbor_writer_t *w);

/*---------------------------------------------------------------------------*/
/* DECODER                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_codec_cbor_reader
 * @brief CBOR decoder cursor over a const buffer.
 *
 * The reader tracks a byte buffer, its length, and the current read
 * position.  Each decode function advances the position past the
 * consumed item.  If the buffer is exhausted mid-item, the function
 * returns TIKU_KITS_CODEC_ERR_UNDERFLOW and the reader position is
 * left unchanged.
 *
 * Example:
 * @code
 *   tiku_kits_codec_cbor_reader_t r;
 *   tiku_kits_codec_cbor_reader_init(&r, buf, len);
 *
 *   tiku_kits_codec_cbor_type_t type;
 *   uint32_t val;
 *   tiku_kits_codec_cbor_peek_type(&r, &type);  // check what's next
 *   tiku_kits_codec_cbor_decode_uint(&r, &val);  // read it
 * @endcode
 */
typedef struct {
    const uint8_t *buf;  /**< Input buffer (caller-owned, const) */
    uint16_t       len;  /**< Total buffer length in bytes */
    uint16_t       pos;  /**< Current read position */
} tiku_kits_codec_cbor_reader_t;

/**
 * @brief Initialize a CBOR reader over a const buffer.
 *
 * @param r    Reader to initialize
 * @param buf  CBOR-encoded input data
 * @param len  Length of input in bytes
 * @return TIKU_KITS_CODEC_OK, or TIKU_KITS_CODEC_ERR_NULL
 */
int tiku_kits_codec_cbor_reader_init(tiku_kits_codec_cbor_reader_t *r,
                                      const uint8_t *buf, uint16_t len);

/**
 * @brief Return the number of unread bytes remaining.
 *
 * @param r  Reader to query
 * @return Bytes remaining, or 0 if r is NULL
 */
uint16_t tiku_kits_codec_cbor_reader_remaining(
             const tiku_kits_codec_cbor_reader_t *r);

/**
 * @brief Peek at the major type of the next item without consuming it.
 *
 * @param r     Reader
 * @param type  Output: major type of the next item
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_UNDERFLOW
 */
int tiku_kits_codec_cbor_peek_type(const tiku_kits_codec_cbor_reader_t *r,
                                    tiku_kits_codec_cbor_type_t *type);

/**
 * @brief Decode an unsigned integer (major type 0).
 *
 * @param r     Reader
 * @param value Output: decoded value (0 .. 2^32-1)
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_cbor_decode_uint(tiku_kits_codec_cbor_reader_t *r,
                                      uint32_t *value);

/**
 * @brief Decode a negative integer (major type 1).
 *
 * Returns the unsigned argument n where the actual value is -1 - n.
 * The caller reconstructs the negative value.
 *
 * @param r     Reader
 * @param value Output: unsigned argument n (actual value = -1 - n)
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_cbor_decode_negint(tiku_kits_codec_cbor_reader_t *r,
                                        uint32_t *value);

/**
 * @brief Decode a signed integer (convenience wrapper).
 *
 * Accepts both major type 0 and 1 and returns a signed int32_t.
 *
 * @param r     Reader
 * @param value Output: decoded signed integer
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_cbor_decode_int(tiku_kits_codec_cbor_reader_t *r,
                                     int32_t *value);

/**
 * @brief Decode a byte string header (major type 2).
 *
 * Reads the length and returns a pointer into the reader's buffer
 * for zero-copy access.  The reader advances past the string bytes.
 *
 * @param r     Reader
 * @param data  Output: pointer to string bytes within the input buffer
 * @param len   Output: string length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_cbor_decode_bstr(tiku_kits_codec_cbor_reader_t *r,
                                      const uint8_t **data, uint16_t *len);

/**
 * @brief Decode a text string header (major type 3).
 *
 * Same as bstr but for text strings.  Returns a pointer into the
 * reader's buffer.  UTF-8 is NOT validated.
 *
 * @param r     Reader
 * @param str   Output: pointer to string bytes within the input buffer
 * @param len   Output: string length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_cbor_decode_tstr(tiku_kits_codec_cbor_reader_t *r,
                                      const char **str, uint16_t *len);

/**
 * @brief Decode an array header (major type 4).
 *
 * Returns the item count.  The caller must then decode exactly @p count
 * items from the reader.
 *
 * @param r     Reader
 * @param count Output: number of items in the array
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_cbor_decode_array(tiku_kits_codec_cbor_reader_t *r,
                                       uint16_t *count);

/**
 * @brief Decode a map header (major type 5).
 *
 * Returns the pair count.  The caller must then decode exactly
 * @p count key-value pairs (2 * count items) from the reader.
 *
 * @param r     Reader
 * @param count Output: number of key-value pairs
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_cbor_decode_map(tiku_kits_codec_cbor_reader_t *r,
                                     uint16_t *count);

/**
 * @brief Decode a boolean simple value.
 *
 * @param r     Reader
 * @param value Output: 1 for true, 0 for false
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, or ERR_UNDERFLOW
 */
int tiku_kits_codec_cbor_decode_bool(tiku_kits_codec_cbor_reader_t *r,
                                      uint8_t *value);

/**
 * @brief Decode a null simple value.
 *
 * @param r  Reader
 * @return TIKU_KITS_CODEC_OK, ERR_TYPE, or ERR_UNDERFLOW
 */
int tiku_kits_codec_cbor_decode_null(tiku_kits_codec_cbor_reader_t *r);

/**
 * @brief Skip the next CBOR item (any type) without decoding it.
 *
 * Useful for ignoring unknown map keys or array elements.  Handles
 * nested arrays and maps recursively (bounded by call stack depth).
 *
 * @param r  Reader
 * @return TIKU_KITS_CODEC_OK, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_cbor_skip(tiku_kits_codec_cbor_reader_t *r);

#endif /* TIKU_KITS_CODEC_CBOR_H_ */
