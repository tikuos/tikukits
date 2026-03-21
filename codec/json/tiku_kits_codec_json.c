/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_codec_json.c - Minimal JSON encoder/decoder for embedded systems
 *
 * Streaming JSON writer and pull-parser.  Zero heap, ~800 bytes code.
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

#include "tiku_kits_codec_json.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* PRIVATE HELPERS — WRITER                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Append a single byte to the writer buffer.
 * @return TIKU_KITS_CODEC_OK or ERR_OVERFLOW
 */
static int emit_byte(tiku_kits_codec_json_writer_t *w, uint8_t ch)
{
    if (w->pos >= w->capacity) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }
    w->buf[w->pos++] = ch;
    return TIKU_KITS_CODEC_OK;
}

/**
 * @brief Append raw bytes to the writer buffer.
 * @return TIKU_KITS_CODEC_OK or ERR_OVERFLOW
 */
static int emit_raw(tiku_kits_codec_json_writer_t *w,
                     const uint8_t *data, uint16_t len)
{
    if (w->pos + len > w->capacity) {
        return TIKU_KITS_CODEC_ERR_OVERFLOW;
    }
    memcpy(&w->buf[w->pos], data, len);
    w->pos += len;
    return TIKU_KITS_CODEC_OK;
}

/**
 * @brief Emit a comma separator if needed at the current depth.
 *
 * After the first value at a given nesting level, every subsequent
 * value must be preceded by a comma.  This function checks the
 * need_comma flag and emits one if set, then sets the flag for the
 * next value.
 */
static int emit_separator(tiku_kits_codec_json_writer_t *w)
{
    int rc;

    if (w->depth > 0 && w->need_comma[w->depth - 1]) {
        rc = emit_byte(w, ',');
        if (rc != TIKU_KITS_CODEC_OK) {
            return rc;
        }
    }

    /* Mark that the next sibling will need a comma */
    if (w->depth > 0) {
        w->need_comma[w->depth - 1] = 1;
    }

    return TIKU_KITS_CODEC_OK;
}

/**
 * @brief Hex digit for \uXXXX escapes (lowercase).
 */
static uint8_t hex_digit(uint8_t nibble)
{
    return (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
}

/**
 * @brief Emit a JSON-escaped string (the content between quotes).
 *
 * Escapes: \" \\ \n \r \t.  Control characters < 0x20 that are
 * not newline/return/tab are emitted as \u00XX.
 */
static int emit_escaped(tiku_kits_codec_json_writer_t *w,
                          const char *str, uint16_t len)
{
    uint16_t i;
    uint8_t ch;
    int rc;

    for (i = 0; i < len; i++) {
        ch = (uint8_t)str[i];

        if (ch == '"' || ch == '\\') {
            rc = emit_byte(w, '\\');
            if (rc != TIKU_KITS_CODEC_OK) return rc;
            rc = emit_byte(w, ch);
            if (rc != TIKU_KITS_CODEC_OK) return rc;
        } else if (ch == '\n') {
            rc = emit_byte(w, '\\');
            if (rc != TIKU_KITS_CODEC_OK) return rc;
            rc = emit_byte(w, 'n');
            if (rc != TIKU_KITS_CODEC_OK) return rc;
        } else if (ch == '\r') {
            rc = emit_byte(w, '\\');
            if (rc != TIKU_KITS_CODEC_OK) return rc;
            rc = emit_byte(w, 'r');
            if (rc != TIKU_KITS_CODEC_OK) return rc;
        } else if (ch == '\t') {
            rc = emit_byte(w, '\\');
            if (rc != TIKU_KITS_CODEC_OK) return rc;
            rc = emit_byte(w, 't');
            if (rc != TIKU_KITS_CODEC_OK) return rc;
        } else if (ch < 0x20) {
            /* \u00XX for other control characters */
            uint8_t esc[6];
            esc[0] = '\\';
            esc[1] = 'u';
            esc[2] = '0';
            esc[3] = '0';
            esc[4] = hex_digit(ch >> 4);
            esc[5] = hex_digit(ch & 0x0F);
            rc = emit_raw(w, esc, 6);
            if (rc != TIKU_KITS_CODEC_OK) return rc;
        } else {
            rc = emit_byte(w, ch);
            if (rc != TIKU_KITS_CODEC_OK) return rc;
        }
    }

    return TIKU_KITS_CODEC_OK;
}

/**
 * @brief Convert a signed 32-bit integer to decimal ASCII.
 *
 * Writes into a caller-provided buffer (must be >= 12 bytes:
 * '-' + 10 digits + NUL).  Returns the number of characters written
 * (not including NUL).
 */
static uint8_t int32_to_str(int32_t value, char *out)
{
    char tmp[12];
    uint8_t i = 0;
    uint8_t j;
    uint32_t uv;

    if (value < 0) {
        out[i++] = '-';
        /* Careful: -INT32_MIN overflows int32_t. Cast to unsigned first. */
        uv = (uint32_t)(-(value + 1)) + 1U;
    } else {
        uv = (uint32_t)value;
    }

    /* Generate digits in reverse */
    j = 0;
    if (uv == 0) {
        tmp[j++] = '0';
    } else {
        while (uv > 0) {
            tmp[j++] = '0' + (char)(uv % 10);
            uv /= 10;
        }
    }

    /* Reverse into output */
    while (j > 0) {
        out[i++] = tmp[--j];
    }

    out[i] = '\0';
    return i;
}

/*---------------------------------------------------------------------------*/
/* WRITER INIT / QUERY                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_json_writer_init(tiku_kits_codec_json_writer_t *w,
                                      uint8_t *buf, uint16_t capacity)
{
    if (w == NULL || buf == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    w->buf      = buf;
    w->capacity = capacity;
    w->pos      = 0;
    w->depth    = 0;
    memset(w->need_comma, 0, sizeof(w->need_comma));

    return TIKU_KITS_CODEC_OK;
}

uint16_t tiku_kits_codec_json_writer_len(
             const tiku_kits_codec_json_writer_t *w)
{
    if (w == NULL) {
        return 0;
    }
    return w->pos;
}

/*---------------------------------------------------------------------------*/
/* WRITER — CONTAINERS                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_json_open_object(tiku_kits_codec_json_writer_t *w)
{
    int rc;

    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (w->depth >= TIKU_KITS_CODEC_JSON_MAX_DEPTH) {
        return TIKU_KITS_CODEC_ERR_PARAM;
    }

    rc = emit_separator(w);
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    rc = emit_byte(w, '{');
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    w->need_comma[w->depth] = 0;
    w->depth++;

    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_json_close_object(tiku_kits_codec_json_writer_t *w)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (w->depth == 0) {
        return TIKU_KITS_CODEC_ERR_PARAM;
    }

    w->depth--;

    return emit_byte(w, '}');
}

int tiku_kits_codec_json_open_array(tiku_kits_codec_json_writer_t *w)
{
    int rc;

    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (w->depth >= TIKU_KITS_CODEC_JSON_MAX_DEPTH) {
        return TIKU_KITS_CODEC_ERR_PARAM;
    }

    rc = emit_separator(w);
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    rc = emit_byte(w, '[');
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    w->need_comma[w->depth] = 0;
    w->depth++;

    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_json_close_array(tiku_kits_codec_json_writer_t *w)
{
    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (w->depth == 0) {
        return TIKU_KITS_CODEC_ERR_PARAM;
    }

    w->depth--;

    return emit_byte(w, ']');
}

/*---------------------------------------------------------------------------*/
/* WRITER — KEY                                                              */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_json_write_key(tiku_kits_codec_json_writer_t *w,
                                    const char *key, uint16_t len)
{
    int rc;

    if (w == NULL || key == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    /* Comma before this key-value pair (not before the first) */
    if (w->depth > 0 && w->need_comma[w->depth - 1]) {
        rc = emit_byte(w, ',');
        if (rc != TIKU_KITS_CODEC_OK) return rc;
    }

    rc = emit_byte(w, '"');
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    rc = emit_escaped(w, key, len);
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    rc = emit_byte(w, '"');
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    rc = emit_byte(w, ':');
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    /*
     * After write_key, the next call is the value. We must NOT emit
     * a comma before it. But after the value, we need a comma before
     * the next key. So set need_comma = 0 now; the value emitter
     * will set it to 1 when it finishes.
     */
    if (w->depth > 0) {
        w->need_comma[w->depth - 1] = 0;
    }

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* WRITER — SCALAR VALUES                                                    */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_json_write_str(tiku_kits_codec_json_writer_t *w,
                                    const char *str, uint16_t len)
{
    int rc;

    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }
    if (str == NULL && len > 0) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = emit_separator(w);
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    rc = emit_byte(w, '"');
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    if (len > 0) {
        rc = emit_escaped(w, str, len);
        if (rc != TIKU_KITS_CODEC_OK) return rc;
    }

    return emit_byte(w, '"');
}

int tiku_kits_codec_json_write_int(tiku_kits_codec_json_writer_t *w,
                                    int32_t value)
{
    char tmp[12];
    uint8_t slen;
    int rc;

    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = emit_separator(w);
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    slen = int32_to_str(value, tmp);

    return emit_raw(w, (const uint8_t *)tmp, slen);
}

int tiku_kits_codec_json_write_bool(tiku_kits_codec_json_writer_t *w,
                                     uint8_t value)
{
    int rc;

    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = emit_separator(w);
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    if (value) {
        return emit_raw(w, (const uint8_t *)"true", 4);
    } else {
        return emit_raw(w, (const uint8_t *)"false", 5);
    }
}

int tiku_kits_codec_json_write_null(tiku_kits_codec_json_writer_t *w)
{
    int rc;

    if (w == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = emit_separator(w);
    if (rc != TIKU_KITS_CODEC_OK) return rc;

    return emit_raw(w, (const uint8_t *)"null", 4);
}

/*---------------------------------------------------------------------------*/
/* READER INIT / QUERY                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_json_reader_init(tiku_kits_codec_json_reader_t *r,
                                      const uint8_t *buf, uint16_t len)
{
    if (r == NULL || buf == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    r->buf       = buf;
    r->len       = len;
    r->pos       = 0;
    r->tok_start = 0;
    r->tok_len   = 0;
    r->tok_int   = 0;

    return TIKU_KITS_CODEC_OK;
}

uint16_t tiku_kits_codec_json_reader_remaining(
             const tiku_kits_codec_json_reader_t *r)
{
    if (r == NULL) {
        return 0;
    }
    return r->len - r->pos;
}

/*---------------------------------------------------------------------------*/
/* PRIVATE HELPERS — READER                                                  */
/*---------------------------------------------------------------------------*/

/** Skip whitespace (space, tab, CR, LF). */
static void skip_ws(tiku_kits_codec_json_reader_t *r)
{
    while (r->pos < r->len) {
        uint8_t ch = r->buf[r->pos];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            r->pos++;
        } else {
            break;
        }
    }
}

/**
 * @brief Match a literal keyword (true, false, null).
 * @return TIKU_KITS_CODEC_OK or ERR_CORRUPT
 */
static int match_keyword(tiku_kits_codec_json_reader_t *r,
                           const char *kw, uint8_t kwlen)
{
    if (r->pos + kwlen > r->len) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }
    if (memcmp(&r->buf[r->pos], kw, kwlen) != 0) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }
    r->pos += kwlen;
    return TIKU_KITS_CODEC_OK;
}

/**
 * @brief Parse a JSON string token.
 *
 * On entry, r->pos points to the opening quote.  On exit, r->pos
 * is past the closing quote.  tok_start/tok_len are set to the
 * content between the quotes.
 */
static int parse_string(tiku_kits_codec_json_reader_t *r)
{
    uint16_t start;

    /* Skip opening quote */
    r->pos++;
    start = r->pos;

    while (r->pos < r->len) {
        uint8_t ch = r->buf[r->pos];
        if (ch == '"') {
            r->tok_start = start;
            r->tok_len   = r->pos - start;
            r->pos++; /* skip closing quote */
            return TIKU_KITS_CODEC_OK;
        }
        if (ch == '\\') {
            /* Skip escaped character */
            r->pos++;
            if (r->pos >= r->len) {
                return TIKU_KITS_CODEC_ERR_CORRUPT;
            }
        }
        r->pos++;
    }

    return TIKU_KITS_CODEC_ERR_UNDERFLOW;
}

/**
 * @brief Parse a JSON number token (signed integer only).
 *
 * On entry, r->pos points to the first digit or '-'.  On exit,
 * r->pos is past the last digit.  tok_int holds the parsed value.
 */
static int parse_number(tiku_kits_codec_json_reader_t *r)
{
    int32_t sign = 1;
    uint32_t accum = 0;
    uint8_t digits = 0;
    uint8_t ch;

    if (r->buf[r->pos] == '-') {
        sign = -1;
        r->pos++;
        if (r->pos >= r->len) {
            return TIKU_KITS_CODEC_ERR_CORRUPT;
        }
    }

    while (r->pos < r->len) {
        ch = r->buf[r->pos];
        if (ch >= '0' && ch <= '9') {
            accum = accum * 10 + (ch - '0');
            digits++;
            r->pos++;
        } else {
            break;
        }
    }

    if (digits == 0) {
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }

    if (sign < 0) {
        r->tok_int = -(int32_t)accum;
    } else {
        r->tok_int = (int32_t)accum;
    }

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* READER — NEXT TOKEN                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_json_next_token(tiku_kits_codec_json_reader_t *r,
                                     tiku_kits_codec_json_tok_t *tok)
{
    uint8_t ch;
    int rc;

    if (r == NULL || tok == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    skip_ws(r);

    if (r->pos >= r->len) {
        *tok = TIKU_KITS_CODEC_JSON_TOK_END;
        return TIKU_KITS_CODEC_OK;
    }

    ch = r->buf[r->pos];

    switch (ch) {
    case '{':
        *tok = TIKU_KITS_CODEC_JSON_TOK_LBRACE;
        r->pos++;
        return TIKU_KITS_CODEC_OK;

    case '}':
        *tok = TIKU_KITS_CODEC_JSON_TOK_RBRACE;
        r->pos++;
        return TIKU_KITS_CODEC_OK;

    case '[':
        *tok = TIKU_KITS_CODEC_JSON_TOK_LBRACKET;
        r->pos++;
        return TIKU_KITS_CODEC_OK;

    case ']':
        *tok = TIKU_KITS_CODEC_JSON_TOK_RBRACKET;
        r->pos++;
        return TIKU_KITS_CODEC_OK;

    case ':':
        *tok = TIKU_KITS_CODEC_JSON_TOK_COLON;
        r->pos++;
        return TIKU_KITS_CODEC_OK;

    case ',':
        *tok = TIKU_KITS_CODEC_JSON_TOK_COMMA;
        r->pos++;
        return TIKU_KITS_CODEC_OK;

    case '"':
        rc = parse_string(r);
        if (rc != TIKU_KITS_CODEC_OK) return rc;
        *tok = TIKU_KITS_CODEC_JSON_TOK_STRING;
        return TIKU_KITS_CODEC_OK;

    case 't':
        rc = match_keyword(r, "true", 4);
        if (rc != TIKU_KITS_CODEC_OK) return rc;
        *tok = TIKU_KITS_CODEC_JSON_TOK_TRUE;
        return TIKU_KITS_CODEC_OK;

    case 'f':
        rc = match_keyword(r, "false", 5);
        if (rc != TIKU_KITS_CODEC_OK) return rc;
        *tok = TIKU_KITS_CODEC_JSON_TOK_FALSE;
        return TIKU_KITS_CODEC_OK;

    case 'n':
        rc = match_keyword(r, "null", 4);
        if (rc != TIKU_KITS_CODEC_OK) return rc;
        *tok = TIKU_KITS_CODEC_JSON_TOK_NULL;
        return TIKU_KITS_CODEC_OK;

    default:
        /* Number: starts with digit or '-' */
        if (ch == '-' || (ch >= '0' && ch <= '9')) {
            rc = parse_number(r);
            if (rc != TIKU_KITS_CODEC_OK) return rc;
            *tok = TIKU_KITS_CODEC_JSON_TOK_NUMBER;
            return TIKU_KITS_CODEC_OK;
        }

        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }
}

/*---------------------------------------------------------------------------*/
/* READER — TOKEN VALUE ACCESS                                               */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_json_token_string(
        const tiku_kits_codec_json_reader_t *r,
        const char **str, uint16_t *len)
{
    if (r == NULL || str == NULL || len == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    *str = (const char *)&r->buf[r->tok_start];
    *len = r->tok_len;

    return TIKU_KITS_CODEC_OK;
}

int tiku_kits_codec_json_token_int(
        const tiku_kits_codec_json_reader_t *r,
        int32_t *value)
{
    if (r == NULL || value == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    *value = r->tok_int;

    return TIKU_KITS_CODEC_OK;
}

/*---------------------------------------------------------------------------*/
/* READER — SKIP VALUE                                                       */
/*---------------------------------------------------------------------------*/

int tiku_kits_codec_json_skip_value(tiku_kits_codec_json_reader_t *r)
{
    tiku_kits_codec_json_tok_t tok;
    int rc;
    uint16_t depth;

    if (r == NULL) {
        return TIKU_KITS_CODEC_ERR_NULL;
    }

    rc = tiku_kits_codec_json_next_token(r, &tok);
    if (rc != TIKU_KITS_CODEC_OK) {
        return rc;
    }

    switch (tok) {
    case TIKU_KITS_CODEC_JSON_TOK_STRING:
    case TIKU_KITS_CODEC_JSON_TOK_NUMBER:
    case TIKU_KITS_CODEC_JSON_TOK_TRUE:
    case TIKU_KITS_CODEC_JSON_TOK_FALSE:
    case TIKU_KITS_CODEC_JSON_TOK_NULL:
        /* Scalar — already consumed by next_token */
        return TIKU_KITS_CODEC_OK;

    case TIKU_KITS_CODEC_JSON_TOK_LBRACE:
    case TIKU_KITS_CODEC_JSON_TOK_LBRACKET:
        /* Container — consume until matching close */
        depth = 1;
        while (depth > 0) {
            rc = tiku_kits_codec_json_next_token(r, &tok);
            if (rc != TIKU_KITS_CODEC_OK) {
                return rc;
            }
            if (tok == TIKU_KITS_CODEC_JSON_TOK_LBRACE ||
                tok == TIKU_KITS_CODEC_JSON_TOK_LBRACKET) {
                depth++;
            } else if (tok == TIKU_KITS_CODEC_JSON_TOK_RBRACE ||
                       tok == TIKU_KITS_CODEC_JSON_TOK_RBRACKET) {
                depth--;
            } else if (tok == TIKU_KITS_CODEC_JSON_TOK_END) {
                return TIKU_KITS_CODEC_ERR_UNDERFLOW;
            }
        }
        return TIKU_KITS_CODEC_OK;

    default:
        return TIKU_KITS_CODEC_ERR_CORRUPT;
    }
}
