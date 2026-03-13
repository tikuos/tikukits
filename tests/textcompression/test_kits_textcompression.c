/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * test_kits_textcompression.c - Text compression library tests
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

#include "test_kits_textcompression.h"

/*---------------------------------------------------------------------------*/
/* TEST 1: RLE ENCODE / DECODE                                               */
/*---------------------------------------------------------------------------*/

void test_kits_textcomp_rle(void)
{
    uint8_t enc[64];
    uint8_t dec[64];
    uint16_t enc_len;
    uint16_t dec_len;
    int rc;

    TEST_PRINT("\n--- Test: TextCompression RLE ---\n");

    /* Encode "AAABBC" -> [3,'A', 2,'B', 1,'C'] = 6 bytes */
    {
        const uint8_t src[] = {'A', 'A', 'A', 'B', 'B', 'C'};

        rc = tiku_kits_textcompression_rle_encode(
            src, 6, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK, "rle encode OK");
        TEST_ASSERT(enc_len == 6, "rle encoded length == 6");
        TEST_ASSERT(enc[0] == 3 && enc[1] == 'A', "rle pair 0: 3xA");
        TEST_ASSERT(enc[2] == 2 && enc[3] == 'B', "rle pair 1: 2xB");
        TEST_ASSERT(enc[4] == 1 && enc[5] == 'C', "rle pair 2: 1xC");

        /* Decode back */
        rc = tiku_kits_textcompression_rle_decode(
            enc, enc_len, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK, "rle decode OK");
        TEST_ASSERT(dec_len == 6, "rle decoded length == 6");
        TEST_ASSERT(memcmp(dec, src, 6) == 0, "rle roundtrip matches");
    }

    /* Single byte */
    {
        const uint8_t src[] = {'X'};

        rc = tiku_kits_textcompression_rle_encode(
            src, 1, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "rle encode single OK");
        TEST_ASSERT(enc_len == 2, "rle single encoded to 2 bytes");
        TEST_ASSERT(enc[0] == 1 && enc[1] == 'X', "rle single: 1xX");
    }

    /* All same bytes (good compression) */
    {
        uint8_t src[10];
        memset(src, 'Z', 10);

        tiku_kits_textcompression_rle_encode(
            src, 10, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(enc_len == 2, "rle 10 same bytes -> 2 encoded");

        tiku_kits_textcompression_rle_decode(
            enc, enc_len, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(dec_len == 10, "rle decode back to 10");
        TEST_ASSERT(memcmp(dec, src, 10) == 0, "rle same-byte roundtrip");
    }

    /* Output buffer too small */
    rc = tiku_kits_textcompression_rle_encode(
        (const uint8_t *)"AB", 2, enc, 2, &enc_len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE,
                "rle encode buffer too small rejected");

    /* Decode corrupt: odd length */
    {
        uint8_t bad[] = {3, 'A', 2};

        rc = tiku_kits_textcompression_rle_decode(
            bad, 3, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT,
                    "rle decode odd length rejected");
    }

    /* Decode corrupt: zero count */
    {
        uint8_t bad[] = {0, 'A'};

        rc = tiku_kits_textcompression_rle_decode(
            bad, 2, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_CORRUPT,
                    "rle decode zero count rejected");
    }
}

/*---------------------------------------------------------------------------*/
/* TEST 2: BPE ENCODE / DECODE                                               */
/*---------------------------------------------------------------------------*/

void test_kits_textcomp_bpe(void)
{
    uint8_t enc[128];
    uint8_t dec[128];
    uint16_t enc_len;
    uint16_t dec_len;
    int rc;

    TEST_PRINT("\n--- Test: TextCompression BPE ---\n");

    /* Repeating pattern should compress */
    {
        const uint8_t src[] = "abababababab";
        uint16_t src_len = 12;

        rc = tiku_kits_textcompression_bpe_encode(
            src, src_len, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK, "bpe encode OK");
        TEST_ASSERT(enc_len < src_len + 4,
                    "bpe compressed (with header overhead)");

        /* Decode back */
        rc = tiku_kits_textcompression_bpe_decode(
            enc, enc_len, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK, "bpe decode OK");
        TEST_ASSERT(dec_len == src_len, "bpe decoded length matches");
        TEST_ASSERT(memcmp(dec, src, src_len) == 0,
                    "bpe roundtrip matches");
    }

    /* Short input (no compression possible) */
    {
        const uint8_t src[] = "xy";

        rc = tiku_kits_textcompression_bpe_encode(
            src, 2, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "bpe encode short OK");

        rc = tiku_kits_textcompression_bpe_decode(
            enc, enc_len, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "bpe decode short OK");
        TEST_ASSERT(dec_len == 2, "bpe short roundtrip length");
        TEST_ASSERT(memcmp(dec, src, 2) == 0, "bpe short roundtrip");
    }

    /* Single byte */
    {
        const uint8_t src[] = {0x42};

        rc = tiku_kits_textcompression_bpe_encode(
            src, 1, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "bpe encode 1-byte OK");

        rc = tiku_kits_textcompression_bpe_decode(
            enc, enc_len, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(dec_len == 1 && dec[0] == 0x42,
                    "bpe 1-byte roundtrip");
    }

    /* Output buffer too small */
    rc = tiku_kits_textcompression_bpe_encode(
        (const uint8_t *)"test", 4, enc, 1, &enc_len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE,
                "bpe encode buffer too small rejected");
}

/*---------------------------------------------------------------------------*/
/* TEST 3: HEATSHRINK LZSS ENCODE / DECODE                                   */
/*---------------------------------------------------------------------------*/

void test_kits_textcomp_heatshrink(void)
{
    uint8_t enc[128];
    uint8_t dec[128];
    uint16_t enc_len;
    uint16_t dec_len;
    int rc;

    TEST_PRINT("\n--- Test: TextCompression Heatshrink ---\n");

    /* Repeating pattern should compress well */
    {
        const uint8_t src[] = "hellohellohellohello";
        uint16_t src_len = 20;

        rc = tiku_kits_textcompression_heatshrink_encode(
            src, src_len, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "heatshrink encode OK");
        TEST_ASSERT(enc_len < src_len + 2,
                    "heatshrink compressed repeating pattern");

        /* Decode back */
        rc = tiku_kits_textcompression_heatshrink_decode(
            enc, enc_len, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "heatshrink decode OK");
        TEST_ASSERT(dec_len == src_len,
                    "heatshrink decoded length matches");
        TEST_ASSERT(memcmp(dec, src, src_len) == 0,
                    "heatshrink roundtrip matches");
    }

    /* Short non-repeating input */
    {
        const uint8_t src[] = "abcdef";
        uint16_t src_len = 6;

        rc = tiku_kits_textcompression_heatshrink_encode(
            src, src_len, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "heatshrink encode short OK");

        rc = tiku_kits_textcompression_heatshrink_decode(
            enc, enc_len, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "heatshrink decode short OK");
        TEST_ASSERT(dec_len == src_len, "heatshrink short length");
        TEST_ASSERT(memcmp(dec, src, src_len) == 0,
                    "heatshrink short roundtrip");
    }

    /* Single byte */
    {
        const uint8_t src[] = {0xFF};

        rc = tiku_kits_textcompression_heatshrink_encode(
            src, 1, enc, sizeof(enc), &enc_len);
        TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_OK,
                    "heatshrink encode 1-byte OK");

        rc = tiku_kits_textcompression_heatshrink_decode(
            enc, enc_len, dec, sizeof(dec), &dec_len);
        TEST_ASSERT(dec_len == 1 && dec[0] == 0xFF,
                    "heatshrink 1-byte roundtrip");
    }

    /* Output buffer too small */
    rc = tiku_kits_textcompression_heatshrink_encode(
        (const uint8_t *)"test", 4, enc, 1, &enc_len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_SIZE,
                "heatshrink encode buffer too small rejected");

    /* Decode with truncated header */
    rc = tiku_kits_textcompression_heatshrink_decode(
        enc, 1, dec, sizeof(dec), &dec_len);
    TEST_ASSERT(rc != TIKU_KITS_TEXTCOMPRESSION_OK,
                "heatshrink decode truncated header rejected");
}

/*---------------------------------------------------------------------------*/
/* TEST 4: NULL POINTER HANDLING                                             */
/*---------------------------------------------------------------------------*/

void test_kits_textcomp_null(void)
{
    uint8_t buf[32];
    uint16_t len;
    int rc;

    TEST_PRINT("\n--- Test: TextCompression NULL Inputs ---\n");

    /* RLE */
    rc = tiku_kits_textcompression_rle_encode(
        NULL, 4, buf, sizeof(buf), &len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_NULL,
                "rle encode NULL src rejected");

    rc = tiku_kits_textcompression_rle_encode(
        buf, 4, NULL, sizeof(buf), &len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_NULL,
                "rle encode NULL dst rejected");

    rc = tiku_kits_textcompression_rle_encode(
        buf, 4, buf, sizeof(buf), NULL);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_NULL,
                "rle encode NULL out_len rejected");

    rc = tiku_kits_textcompression_rle_decode(
        NULL, 4, buf, sizeof(buf), &len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_NULL,
                "rle decode NULL src rejected");

    /* BPE */
    rc = tiku_kits_textcompression_bpe_encode(
        NULL, 4, buf, sizeof(buf), &len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_NULL,
                "bpe encode NULL src rejected");

    rc = tiku_kits_textcompression_bpe_decode(
        NULL, 4, buf, sizeof(buf), &len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_NULL,
                "bpe decode NULL src rejected");

    /* Heatshrink */
    rc = tiku_kits_textcompression_heatshrink_encode(
        NULL, 4, buf, sizeof(buf), &len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_NULL,
                "heatshrink encode NULL src rejected");

    rc = tiku_kits_textcompression_heatshrink_decode(
        NULL, 4, buf, sizeof(buf), &len);
    TEST_ASSERT(rc == TIKU_KITS_TEXTCOMPRESSION_ERR_NULL,
                "heatshrink decode NULL src rejected");
}
