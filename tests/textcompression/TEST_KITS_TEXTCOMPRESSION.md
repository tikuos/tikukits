# TikuKits Text Compression Tests

**Suite name:** `TikuKits Text Compression`
**Source directory:** `tikukits/tests/textcompression/`
**Header:** `test_kits_textcompression.h`
**Source files:** `test_kits_textcompression.c`
**Flag:** `TEST_KITS_TEXTCOMPRESSION`

## Overview

Tests for the TikuKits text compression library, covering three algorithms:

1. **RLE (Run-Length Encoding)** - simple count+byte pair compression
2. **BPE (Byte Pair Encoding)** - dictionary-based compression using byte-pair replacement
3. **Heatshrink** - LZSS-variant sliding-window compression suitable for embedded systems

All algorithms support encode/decode round-trips with error detection for
corrupted input, buffer overflows, and NULL inputs. Return codes are defined
in `tikukits/textcompression/tiku_kits_textcompression.h`.

---

## Tests

### 1. RLE Encode/Decode

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_TEXTCOMPRESSION` |
| **Source** | `test_kits_textcompression.c` : `test_kits_textcomp_rle()` |
| **Group** | "TextCompression RLE" |

**What it tests:**
Run-length encoding and decoding: multi-character input ("AAABBC"), single byte,
repeated identical bytes (10x same), round-trip verification, and error handling
for small output buffers, odd-length encoded data, and zero-count pairs.

**Assertions:**
1. `rle encode OK`
2. `rle encoded length == 6`
3. `rle pair 0: 3xA`, `pair 1: 2xB`, `pair 2: 1xC`
4. `rle decode OK`, `decoded length == 6`, `roundtrip matches`
5. `rle encode single OK`, `single encoded to 2 bytes`, `single: 1xX`
6. `rle 10 same bytes -> 2 encoded`
7. `rle decode back to 10`, `same-byte roundtrip`
8. `rle encode buffer too small rejected`
9. `rle decode odd length rejected`
10. `rle decode zero count rejected`

---

### 2. BPE Encode/Decode

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_TEXTCOMPRESSION` |
| **Source** | `test_kits_textcompression.c` : `test_kits_textcomp_bpe()` |
| **Group** | "TextCompression BPE" |

**What it tests:**
Byte-pair encoding and decoding: repeating pattern compression, round-trip
verification for normal, short (2-byte), and single-byte inputs, and buffer
overflow error handling.

**Assertions:**
1. `bpe encode OK`
2. `bpe compressed (with header overhead)`
3. `bpe decode OK`, `decoded length matches`, `roundtrip matches`
4. `bpe encode short OK`, `decode short OK`, `short roundtrip length`, `short roundtrip`
5. `bpe encode 1-byte OK`, `1-byte roundtrip`
6. `bpe encode buffer too small rejected`

---

### 3. Heatshrink Compress/Decompress

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_TEXTCOMPRESSION` |
| **Source** | `test_kits_textcompression.c` : `test_kits_textcomp_heatshrink()` |
| **Group** | "TextCompression Heatshrink" |

**What it tests:**
Heatshrink LZSS compression: repeating pattern compression, round-trip
verification for normal, short non-repeating, and single-byte inputs, buffer
overflow error handling, and truncated header detection.

**Assertions:**
1. `heatshrink encode OK`
2. `heatshrink compressed repeating pattern`
3. `heatshrink decode OK`, `decoded length matches`, `roundtrip matches`
4. `heatshrink encode short OK`, `decode short OK`, `short length`, `short roundtrip`
5. `heatshrink encode 1-byte OK`, `1-byte roundtrip`
6. `heatshrink encode buffer too small rejected`
7. `heatshrink decode truncated header rejected`

---

### 4. NULL Pointer Handling

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_TEXTCOMPRESSION` |
| **Source** | `test_kits_textcompression.c` : `test_kits_textcomp_null()` |
| **Group** | "TextCompression NULL Inputs" |

**What it tests:**
NULL pointer rejection across all three compression algorithms (encode and decode).

**Assertions:**
1. `rle encode NULL src rejected`, `NULL dst rejected`, `NULL out_len rejected`
2. `rle decode NULL src rejected`
3. `bpe encode NULL src rejected`, `decode NULL src rejected`
4. `heatshrink encode NULL src rejected`, `decode NULL src rejected`

---

## Requests

<!--
  Add new test requests below. Specify which algorithm (RLE, BPE, heatshrink)
  the test is for. Each request should describe:
  - What behavior to test
  - Expected outcome
  - Any specific parameters or edge cases

  An LLM will read these requests and generate the corresponding C test
  code in tikukits/tests/textcompression/test_kits_textcompression.c. After
  implementation, move the request to the Tests section above with full
  documentation.

  Example format:

  ### Request: [RLE] Run length exceeding 255
  Feed a buffer of 300 identical bytes to RLE encode. Verify it splits into
  two runs (255 + 45) and that decode reproduces the original 300 bytes.
  Expected: encoded length == 4, round-trip matches.
-->
