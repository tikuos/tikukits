/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * TikuKits Example: Text Compression
 *
 * Demonstrates three compression algorithms for embedded data logging:
 *   1. RLE  -- Run-Length Encoding (best for repeated bytes)
 *   2. BPE  -- Byte Pair Encoding (best for text with repeated patterns)
 *   3. LZSS -- Heatshrink (general-purpose sliding-window compression)
 *
 * Each example compresses data, reports the ratio, and verifies
 * round-trip correctness by decompressing and comparing.
 *
 * Licensed under the Apache License, Version 2.0
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../textcompression/tiku_kits_textcompression.h"
#include "../textcompression/rle/tiku_kits_textcompression_rle.h"
#include "../textcompression/bpe/tiku_kits_textcompression_bpe.h"
#include "../textcompression/heatshrink/tiku_kits_textcompression_heatshrink.h"

/*---------------------------------------------------------------------------*/
/* Helper: print compression results                                         */
/*---------------------------------------------------------------------------*/

static void print_result(const char *algo, uint16_t orig_len,
                         uint16_t comp_len, int match)
{
    printf("  Original:     %u bytes\n", orig_len);
    printf("  Compressed:   %u bytes\n", comp_len);
    if (orig_len > 0) {
        printf("  Ratio:        %u%%\n",
               (unsigned)(comp_len * 100 / orig_len));
    }
    printf("  Round-trip:   %s\n", match ? "OK" : "MISMATCH");
}

/*---------------------------------------------------------------------------*/
/* Example 1: RLE -- ideal for sensor logs with repeated values              */
/*---------------------------------------------------------------------------*/

static void example_rle(void)
{
    /* Simulated sensor log: many repeated zeros (idle) with bursts */
    uint8_t data[] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 10 idle samples */
        42, 42, 42, 42, 42,                 /* 5 active samples */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,      /* 10 idle */
        0, 0, 0, 0, 0,                      /* 5 idle */
        99, 99, 99,                         /* 3 active */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0       /* 10 idle */
    };
    uint16_t data_len = sizeof(data);

    uint8_t compressed[96];
    uint8_t decompressed[64];
    uint16_t comp_len, decomp_len;
    int rc;

    printf("--- RLE (Run-Length Encoding) ---\n");

    rc = tiku_kits_textcompression_rle_encode(
        data, data_len, compressed, sizeof(compressed), &comp_len);
    if (rc != TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  Encode failed: %d\n", rc);
        return;
    }

    rc = tiku_kits_textcompression_rle_decode(
        compressed, comp_len,
        decompressed, sizeof(decompressed), &decomp_len);
    if (rc != TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  Decode failed: %d\n", rc);
        return;
    }

    print_result("RLE", data_len, comp_len,
                 decomp_len == data_len &&
                 memcmp(data, decompressed, data_len) == 0);
}

/*---------------------------------------------------------------------------*/
/* Example 2: BPE -- good for text with repeated byte pairs                  */
/*---------------------------------------------------------------------------*/

static void example_bpe(void)
{
    /* Repeated text patterns typical of structured log messages */
    uint8_t data[] = "TEMP:OK TEMP:OK TEMP:OK HUMI:OK HUMI:OK";
    uint16_t data_len = (uint16_t)strlen((char *)data);

    uint8_t compressed[128];
    uint8_t decompressed[128];
    uint16_t comp_len, decomp_len;
    int rc;

    printf("--- BPE (Byte Pair Encoding) ---\n");
    printf("  Input: \"%s\"\n", (char *)data);

    rc = tiku_kits_textcompression_bpe_encode(
        data, data_len, compressed, sizeof(compressed), &comp_len);
    if (rc != TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  Encode failed: %d\n", rc);
        return;
    }

    rc = tiku_kits_textcompression_bpe_decode(
        compressed, comp_len,
        decompressed, sizeof(decompressed), &decomp_len);
    if (rc != TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  Decode failed: %d\n", rc);
        return;
    }

    print_result("BPE", data_len, comp_len,
                 decomp_len == data_len &&
                 memcmp(data, decompressed, data_len) == 0);
}

/*---------------------------------------------------------------------------*/
/* Example 3: Heatshrink (LZSS) -- general-purpose compression               */
/*---------------------------------------------------------------------------*/

static void example_heatshrink(void)
{
    /* Repeated structure typical of JSON-like sensor payloads */
    uint8_t data[] =
        "{\"id\":1,\"temp\":23,\"humi\":45}"
        "{\"id\":2,\"temp\":24,\"humi\":44}"
        "{\"id\":3,\"temp\":23,\"humi\":46}";
    uint16_t data_len = (uint16_t)strlen((char *)data);

    uint8_t compressed[128];
    uint8_t decompressed[128];
    uint16_t comp_len, decomp_len;
    int rc;

    printf("--- Heatshrink (LZSS) ---\n");
    printf("  Input: \"%s\"\n", (char *)data);

    rc = tiku_kits_textcompression_heatshrink_encode(
        data, data_len, compressed, sizeof(compressed), &comp_len);
    if (rc != TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  Encode failed: %d\n", rc);
        return;
    }

    rc = tiku_kits_textcompression_heatshrink_decode(
        compressed, comp_len,
        decompressed, sizeof(decompressed), &decomp_len);
    if (rc != TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  Decode failed: %d\n", rc);
        return;
    }

    print_result("Heatshrink", data_len, comp_len,
                 decomp_len == data_len &&
                 memcmp(data, decompressed, data_len) == 0);
}

/*---------------------------------------------------------------------------*/
/* Example 4: Compare all three algorithms on the same data                  */
/*---------------------------------------------------------------------------*/

static void example_comparison(void)
{
    /* Mixed data: some runs, some text patterns */
    uint8_t data[] = "AAAAAABBBBCCCCDDDDAAAAAABBBBCCCCDDDD";
    uint16_t data_len = (uint16_t)strlen((char *)data);

    uint8_t buf[128];
    uint16_t len;

    printf("--- Algorithm Comparison ---\n");
    printf("  Input (%u bytes): \"%s\"\n", data_len, (char *)data);

    if (tiku_kits_textcompression_rle_encode(
            data, data_len, buf, sizeof(buf), &len)
        == TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  RLE:        %u bytes (%u%%)\n",
               len, (unsigned)(len * 100 / data_len));
    }

    if (tiku_kits_textcompression_bpe_encode(
            data, data_len, buf, sizeof(buf), &len)
        == TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  BPE:        %u bytes (%u%%)\n",
               len, (unsigned)(len * 100 / data_len));
    }

    if (tiku_kits_textcompression_heatshrink_encode(
            data, data_len, buf, sizeof(buf), &len)
        == TIKU_KITS_TEXTCOMPRESSION_OK) {
        printf("  Heatshrink: %u bytes (%u%%)\n",
               len, (unsigned)(len * 100 / data_len));
    }
}

/*---------------------------------------------------------------------------*/
/* Main                                                                      */
/*---------------------------------------------------------------------------*/

void example_textcompression_run(void)
{
    printf("=== TikuKits Text Compression Examples ===\n\n");

    example_rle();
    printf("\n");

    example_bpe();
    printf("\n");

    example_heatshrink();
    printf("\n");

    example_comparison();
    printf("\n");
}
