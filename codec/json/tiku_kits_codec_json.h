/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec_json.h - Minimal JSON encoder/decoder for embedded systems
 *
 * Streaming JSON emitter and pull-parser designed for ultra-low-power
 * MCUs.  No DOM tree, no heap, no recursion in the hot path.
 *
 * Design:
 *   - Zero heap allocation.  The writer emits JSON text into a
 *     caller-provided buffer via a cursor struct.  The reader
 *     tokenizes a const buffer one token at a time.
 *   - Streaming API: values are emitted/consumed incrementally so
 *     the caller never needs to hold an entire object model.
 *   - The writer tracks nesting depth and comma state so the caller
 *     does not need to manage separators manually.
 *   - The reader is a pull-parser: each call to next_token() returns
 *     the type and value of the next JSON token.
 *
 * Supported JSON types:
 *   - Numbers: signed 32-bit integers (no floating-point)
 *   - Strings: caller-provided, escaped on output, unescaped on input
 *   - Booleans: true / false
 *   - Null
 *   - Arrays: [ ... ]
 *   - Objects: { "key": value, ... }
 *
 * Not supported:
 *   - Floating-point numbers
 *   - Unicode escape sequences (\uXXXX) in the decoder
 *   - Nested depth beyond TIKU_KITS_CODEC_JSON_MAX_DEPTH
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

#ifndef TIKU_KITS_CODEC_JSON_H_
#define TIKU_KITS_CODEC_JSON_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_codec.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum nesting depth for the JSON writer.
 *
 * Each open array or object consumes one level.  The writer tracks
 * whether a comma is needed at each depth.  8 levels handles most
 * IoT payloads (e.g. {"sensors":[{"temp":23,"hum":55}]}).
 *
 * Override before including this header to change:
 * @code
 *   #define TIKU_KITS_CODEC_JSON_MAX_DEPTH 16
 * @endcode
 */
#ifndef TIKU_KITS_CODEC_JSON_MAX_DEPTH
#define TIKU_KITS_CODEC_JSON_MAX_DEPTH  8
#endif

/*---------------------------------------------------------------------------*/
/* JSON TOKEN TYPES                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Token types returned by the JSON reader.
 */
typedef enum {
    TIKU_KITS_CODEC_JSON_TOK_END        = 0,  /**< No more tokens */
    TIKU_KITS_CODEC_JSON_TOK_LBRACE     = 1,  /**< { */
    TIKU_KITS_CODEC_JSON_TOK_RBRACE     = 2,  /**< } */
    TIKU_KITS_CODEC_JSON_TOK_LBRACKET   = 3,  /**< [ */
    TIKU_KITS_CODEC_JSON_TOK_RBRACKET   = 4,  /**< ] */
    TIKU_KITS_CODEC_JSON_TOK_STRING     = 5,  /**< "..." */
    TIKU_KITS_CODEC_JSON_TOK_NUMBER     = 6,  /**< integer */
    TIKU_KITS_CODEC_JSON_TOK_TRUE       = 7,  /**< true */
    TIKU_KITS_CODEC_JSON_TOK_FALSE      = 8,  /**< false */
    TIKU_KITS_CODEC_JSON_TOK_NULL       = 9,  /**< null */
    TIKU_KITS_CODEC_JSON_TOK_COLON      = 10, /**< : */
    TIKU_KITS_CODEC_JSON_TOK_COMMA      = 11  /**< , */
} tiku_kits_codec_json_tok_t;

/*---------------------------------------------------------------------------*/
/* WRITER                                                                    */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_codec_json_writer
 * @brief JSON writer cursor over a caller-provided buffer.
 *
 * Tracks the output buffer, current position, nesting depth, and
 * per-level comma state.  The writer automatically inserts commas
 * between sibling values and colons after object keys.
 *
 * Example:
 * @code
 *   uint8_t buf[128];
 *   tiku_kits_codec_json_writer_t w;
 *   tiku_kits_codec_json_writer_init(&w, buf, sizeof(buf));
 *
 *   tiku_kits_codec_json_open_object(&w);
 *   tiku_kits_codec_json_write_key(&w, "temp", 4);
 *   tiku_kits_codec_json_write_int(&w, 23);
 *   tiku_kits_codec_json_write_key(&w, "ok", 2);
 *   tiku_kits_codec_json_write_bool(&w, 1);
 *   tiku_kits_codec_json_close_object(&w);
 *   // buf contains: {"temp":23,"ok":true}
 * @endcode
 */
typedef struct {
    uint8_t  *buf;       /**< Output buffer (caller-provided) */
    uint16_t  capacity;  /**< Buffer capacity in bytes */
    uint16_t  pos;       /**< Current write position */
    uint8_t   depth;     /**< Current nesting depth */
    /** Per-level flag: non-zero if a comma is needed before the next value */
    uint8_t   need_comma[TIKU_KITS_CODEC_JSON_MAX_DEPTH];
} tiku_kits_codec_json_writer_t;

/**
 * @brief Initialize a JSON writer.
 *
 * @param w        Writer to initialize
 * @param buf      Output buffer
 * @param capacity Buffer size in bytes
 * @return TIKU_KITS_CODEC_OK, or ERR_NULL
 */
int tiku_kits_codec_json_writer_init(tiku_kits_codec_json_writer_t *w,
                                      uint8_t *buf, uint16_t capacity);

/**
 * @brief Return the number of bytes written so far.
 *
 * @param w  Writer to query
 * @return Bytes written, or 0 if w is NULL
 */
uint16_t tiku_kits_codec_json_writer_len(
             const tiku_kits_codec_json_writer_t *w);

/**
 * @brief Open a JSON object: emit '{'.
 *
 * @param w  Writer
 * @return TIKU_KITS_CODEC_OK, ERR_OVERFLOW, or ERR_PARAM (max depth)
 */
int tiku_kits_codec_json_open_object(tiku_kits_codec_json_writer_t *w);

/**
 * @brief Close a JSON object: emit '}'.
 *
 * @param w  Writer
 * @return TIKU_KITS_CODEC_OK, ERR_OVERFLOW, or ERR_PARAM (underflow)
 */
int tiku_kits_codec_json_close_object(tiku_kits_codec_json_writer_t *w);

/**
 * @brief Open a JSON array: emit '['.
 *
 * @param w  Writer
 * @return TIKU_KITS_CODEC_OK, ERR_OVERFLOW, or ERR_PARAM (max depth)
 */
int tiku_kits_codec_json_open_array(tiku_kits_codec_json_writer_t *w);

/**
 * @brief Close a JSON array: emit ']'.
 *
 * @param w  Writer
 * @return TIKU_KITS_CODEC_OK, ERR_OVERFLOW, or ERR_PARAM (underflow)
 */
int tiku_kits_codec_json_close_array(tiku_kits_codec_json_writer_t *w);

/**
 * @brief Write an object key (quoted string followed by ':').
 *
 * Automatically inserts a comma before the key if this is not the
 * first key in the current object.
 *
 * @param w    Writer
 * @param key  Key string (not null-terminated; length given by len)
 * @param len  Key length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_json_write_key(tiku_kits_codec_json_writer_t *w,
                                    const char *key, uint16_t len);

/**
 * @brief Write a JSON string value (quoted, with escaping).
 *
 * Escapes the required characters: \", \\, \n, \r, \t.  Control
 * characters below 0x20 that are not \n/\r/\t are emitted as
 * \\uXXXX.
 *
 * @param w    Writer
 * @param str  String data
 * @param len  String length in bytes
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, or ERR_OVERFLOW
 */
int tiku_kits_codec_json_write_str(tiku_kits_codec_json_writer_t *w,
                                    const char *str, uint16_t len);

/**
 * @brief Write a signed 32-bit integer value.
 *
 * @param w      Writer
 * @param value  Signed integer
 * @return TIKU_KITS_CODEC_OK or ERR_OVERFLOW
 */
int tiku_kits_codec_json_write_int(tiku_kits_codec_json_writer_t *w,
                                    int32_t value);

/**
 * @brief Write a boolean value (true or false).
 *
 * @param w      Writer
 * @param value  Non-zero for true, zero for false
 * @return TIKU_KITS_CODEC_OK or ERR_OVERFLOW
 */
int tiku_kits_codec_json_write_bool(tiku_kits_codec_json_writer_t *w,
                                     uint8_t value);

/**
 * @brief Write a null value.
 *
 * @param w  Writer
 * @return TIKU_KITS_CODEC_OK or ERR_OVERFLOW
 */
int tiku_kits_codec_json_write_null(tiku_kits_codec_json_writer_t *w);

/*---------------------------------------------------------------------------*/
/* READER (PULL-PARSER)                                                      */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_codec_json_reader
 * @brief JSON pull-parser over a const buffer.
 *
 * Each call to next_token() returns the type of the next token and,
 * for strings and numbers, the value.  Structural tokens ({, }, [, ],
 * :, ,) are returned individually.
 *
 * Example:
 * @code
 *   tiku_kits_codec_json_reader_t r;
 *   tiku_kits_codec_json_reader_init(&r, buf, len);
 *
 *   tiku_kits_codec_json_tok_t tok;
 *   while (tiku_kits_codec_json_next_token(&r, &tok) == TIKU_KITS_CODEC_OK
 *          && tok != TIKU_KITS_CODEC_JSON_TOK_END) {
 *       if (tok == TIKU_KITS_CODEC_JSON_TOK_STRING) {
 *           const char *s; uint16_t slen;
 *           tiku_kits_codec_json_token_string(&r, &s, &slen);
 *       }
 *   }
 * @endcode
 */
typedef struct {
    const uint8_t *buf;      /**< Input buffer (const) */
    uint16_t       len;      /**< Buffer length */
    uint16_t       pos;      /**< Current read position */
    /** Start offset of the most recent string/number token */
    uint16_t       tok_start;
    /** Length of the most recent string/number token */
    uint16_t       tok_len;
    /** Parsed integer value of the most recent number token */
    int32_t        tok_int;
} tiku_kits_codec_json_reader_t;

/**
 * @brief Initialize a JSON reader.
 *
 * @param r    Reader to initialize
 * @param buf  JSON input data
 * @param len  Input length in bytes
 * @return TIKU_KITS_CODEC_OK, or ERR_NULL
 */
int tiku_kits_codec_json_reader_init(tiku_kits_codec_json_reader_t *r,
                                      const uint8_t *buf, uint16_t len);

/**
 * @brief Return the number of unread bytes remaining.
 *
 * @param r  Reader to query
 * @return Bytes remaining, or 0 if r is NULL
 */
uint16_t tiku_kits_codec_json_reader_remaining(
             const tiku_kits_codec_json_reader_t *r);

/**
 * @brief Advance to the next JSON token.
 *
 * Skips whitespace, then identifies the next token.  For strings,
 * the token boundaries are stored so that token_string() can
 * retrieve the value.  For numbers, the value is parsed into
 * tok_int.
 *
 * @param r    Reader
 * @param tok  Output: token type
 * @return TIKU_KITS_CODEC_OK, ERR_NULL, ERR_CORRUPT, or ERR_UNDERFLOW
 */
int tiku_kits_codec_json_next_token(tiku_kits_codec_json_reader_t *r,
                                     tiku_kits_codec_json_tok_t *tok);

/**
 * @brief Retrieve the string value of the most recent STRING token.
 *
 * Returns a pointer into the input buffer (between the quotes).
 * The string is NOT unescaped — escape sequences like \" and \\
 * appear as-is.  For IoT payloads with simple ASCII keys this is
 * sufficient.
 *
 * @param r    Reader
 * @param str  Output: pointer to string start (within input buffer)
 * @param len  Output: string length in bytes (excluding quotes)
 * @return TIKU_KITS_CODEC_OK, or ERR_NULL
 */
int tiku_kits_codec_json_token_string(
        const tiku_kits_codec_json_reader_t *r,
        const char **str, uint16_t *len);

/**
 * @brief Retrieve the integer value of the most recent NUMBER token.
 *
 * @param r      Reader
 * @param value  Output: parsed signed 32-bit integer
 * @return TIKU_KITS_CODEC_OK, or ERR_NULL
 */
int tiku_kits_codec_json_token_int(
        const tiku_kits_codec_json_reader_t *r,
        int32_t *value);

/**
 * @brief Skip the next JSON value (any type) without processing it.
 *
 * For scalars (string, number, bool, null), consumes one token.
 * For arrays and objects, consumes the entire nested structure.
 *
 * @param r  Reader
 * @return TIKU_KITS_CODEC_OK, ERR_UNDERFLOW, or ERR_CORRUPT
 */
int tiku_kits_codec_json_skip_value(tiku_kits_codec_json_reader_t *r);

#endif /* TIKU_KITS_CODEC_JSON_H_ */
