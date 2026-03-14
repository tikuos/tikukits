# TikuKits Signal Features Tests

**Suite name:** `TikuKits Signal Features`
**Source directory:** `tikukits/tests/sigfeatures/`
**Header:** `test_kits_sigfeatures.h`
**Source files:** `test_kits_sigfeatures.c`
**Flag:** `TEST_KITS_SIGFEATURES`

## Overview

Tests for the TikuKits signal feature extraction library, covering seven modules:

1. **Peak detection** - hysteresis-based peak detector with count, value, and index tracking
2. **Zero-crossing rate** - windowed zero-crossing counter for frequency estimation
3. **Histogram** - binning with configurable width, underflow/overflow, and mode detection
4. **First-order delta** - consecutive sample difference (streaming and batch)
5. **Goertzel DFT** - single-frequency energy detection using Goertzel algorithm
6. **Z-score normalization** - fixed-point z-score with configurable shift
7. **Min-max scaling** - range mapping with clamping and batch processing

All operations use integer arithmetic. Return codes are defined in
`tikukits/sigfeatures/tiku_kits_sigfeatures.h`.

---

## Tests

### 1. Peak Detection

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SIGFEATURES` |
| **Source** | `test_kits_sigfeatures.c` : `test_kits_sigfeatures_peak()` |
| **Group** | "SigFeatures Peak" |

**What it tests:**
Hysteresis-based peak detection: initialization, detection after a rise-then-fall
sequence, peak value and index retrieval, reset, and invalid parameter rejection.

**Assertions:**
1. `peak init OK`
2. `peak initial count is 0`
3. `no peak detected initially`
4. `peak last_value ERR_NODATA before any peak`
5. `no peak while still rising`
6. `peak detected after hysteresis drop`
7. `peak count is 1`
8. `peak last_value OK`, `peak value is 30`
9. `peak last_index OK`, `peak index is 3`
10. `no new peak while falling`
11. `peak count 0 after reset`
12. `peak hysteresis=0 rejected`

---

### 2. Zero-Crossing Rate

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SIGFEATURES` |
| **Source** | `test_kits_sigfeatures.c` : `test_kits_sigfeatures_zcr()` |
| **Group** | "SigFeatures ZCR" |

**What it tests:**
Zero-crossing counting: detection of sign changes, no false crossing between
same-sign samples, window size validation, and reset.

**Assertions:**
1. `zcr init OK`
2. `zcr initial count is 0`, `not full initially`
3. `no crossings after 1 sample`
4. `1 crossing: 5 -> -3`
5. `2 crossings: -3 -> 4`
6. `3 crossings: 4 -> -2`
7. `zcr count is 4`
8. `no crossing between same-sign samples` (4 crossings, not 5)
9. `zcr count 0 after reset`, `crossings 0 after reset`
10. `zcr window=0 rejected`, `window=1 rejected`

---

### 3. Histogram

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SIGFEATURES` |
| **Source** | `test_kits_sigfeatures.c` : `test_kits_sigfeatures_histogram()` |
| **Group** | "SigFeatures Histogram" |

**What it tests:**
Histogram binning with configurable bin width and offset: bin counting, mode
detection, underflow/overflow tracking, out-of-range bin access, reset, and
parameter validation.

**Assertions:**
1. `histogram init OK`, `initial total is 0`
2. `histogram total is 5`
3. `bin 0 count == 1`, `bin 1 count == 2`, `bin 2 count == 1`, `bin 3 count == 1`
4. `mode_bin OK`, `mode is bin 1`
5. `underflow count == 1`, `overflow count == 1`
6. `total includes under/overflow` (== 7)
7. `out-of-range bin returns 0`
8. `total 0 after reset`, `underflow 0 after reset`
9. `histogram num_bins=0 rejected`, `bin_width=0 rejected`
10. `mode_bin on empty rejected`

---

### 4. First-Order Delta

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SIGFEATURES` |
| **Source** | `test_kits_sigfeatures.c` : `test_kits_sigfeatures_delta()` |
| **Group** | "SigFeatures Delta" |

**What it tests:**
First-order difference computation: streaming (push-then-get) and batch modes,
error handling before sufficient data, and reset behavior.

**Assertions:**
1. `delta init OK`
2. `delta get before 2 samples rejected`
3. `delta get after 1 sample rejected`
4. `delta get OK`, `delta(100,150) == 50`
5. `delta(150,120) == -30`
6. `delta compute OK` (batch mode)
7. `batch delta[0] == 20`, `delta[1] == -5`, `delta[2] == 15`
8. `delta get after reset rejected`

---

### 5. Goertzel Single-Frequency DFT

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SIGFEATURES` |
| **Source** | `test_kits_sigfeatures.c` : `test_kits_sigfeatures_goertzel()` |
| **Group** | "SigFeatures Goertzel" |

**What it tests:**
Goertzel algorithm for detecting energy at a specific frequency bin: completion
detection, magnitude calculation for a tone signal vs DC signal, block processing,
and parameter validation.

**Assertions:**
1. `goertzel init OK`
2. `not complete initially`
3. `magnitude_sq before complete rejected`
4. `complete after N samples`
5. `magnitude_sq OK`, `tone at bin 1 has nonzero magnitude`
6. `DC signal has zero magnitude at bin 1`
7. `block returns OK`, `block tone magnitude > 0`
8. `goertzel block_size=0 rejected`

---

### 6. Z-Score Normalization

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SIGFEATURES` |
| **Source** | `test_kits_sigfeatures.c` : `test_kits_sigfeatures_zscore()` |
| **Group** | "SigFeatures Z-Score" |

**What it tests:**
Z-score normalization using fixed-point arithmetic: (value - mean) / stddev
scaled by 2^shift. Includes batch processing, statistics update, and parameter
validation.

**Assertions:**
1. `zscore init OK`
2. `zscore(mean) == 0`
3. `zscore(mean+stddev) ~= 1.0 in Q16` (== 65530)
4. `zscore(mean-stddev) ~= -1.0 in Q16` (== -65530)
5. `zscore batch OK`, `batch zscore[0] == 0`, `batch zscore[1] == 65530`
6. `zscore update OK`, `zscore(new_mean) == 0 after update`
7. `zscore stddev=0 rejected`, `shift=0 rejected`

---

### 7. Min-Max Scaling

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SIGFEATURES` |
| **Source** | `test_kits_sigfeatures.c` : `test_kits_sigfeatures_scale()` |
| **Group** | "SigFeatures Scale" |

**What it tests:**
Min-max range scaling with clamping: maps input range [in_min, in_max] to output
range [out_min, out_max]. Tests boundary clamping, batch processing, range update,
and parameter validation.

**Assertions:**
1. `scale init OK`
2. `scale(0) == 0`, `scale(100) == 255`, `scale(50) == 127`
3. `scale(-10) clamped to 0`, `scale(200) clamped to 255`
4. `scale batch OK`, `batch scale[0] == 0`, `batch scale[2] == 255`
5. `scale update OK`, `scale(100) == 127 after update to [0,200]`
6. `scale in_max==in_min rejected`, `in_max<in_min rejected`

---

### 8. NULL Pointer Handling

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SIGFEATURES` |
| **Source** | `test_kits_sigfeatures.c` : `test_kits_sigfeatures_null()` |
| **Group** | "SigFeatures NULL Inputs" |

**What it tests:**
NULL pointer rejection across all seven signal feature modules.

**Assertions:**
1. `peak init NULL rejected`, `push NULL rejected`, `count NULL returns 0`
2. `zcr init NULL rejected`, `push NULL rejected`
3. `histogram init NULL rejected`, `push NULL rejected`
4. `delta init NULL rejected`, `push NULL rejected`, `compute NULL src rejected`
5. `goertzel init NULL rejected`, `push NULL rejected`
6. `zscore init NULL rejected`
7. `scale init NULL rejected`

---

## Requests

<!--
  Add new test requests below. Specify which module (peak, zcr, histogram,
  delta, goertzel, zscore, scale) the test is for. Each request should describe:
  - What behavior to test
  - Expected outcome
  - Any specific parameters or edge cases

  An LLM will read these requests and generate the corresponding C test
  code in tikukits/tests/sigfeatures/test_kits_sigfeatures.c. After
  implementation, move the request to the Tests section above with full
  documentation.

  Example format:

  ### Request: [Goertzel] Multi-frequency sweep
  Initialize Goertzel for each bin 0..N/2, feed an 8-sample square wave,
  and verify that energy concentrates in odd harmonics. Expected: bin 1 has
  highest energy, bin 3 has next highest, even bins are near zero.
-->
