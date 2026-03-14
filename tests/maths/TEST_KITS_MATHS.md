# TikuKits Maths Tests

**Suite name:** `TikuKits Maths`
**Source directory:** `tikukits/tests/maths/`
**Header:** `test_kits_maths.h`
**Source files:** `test_kits_matrix.c`, `test_kits_statistics.c`, `test_kits_distance.c`

## Overview

Tests for the TikuKits maths library, covering three components:

1. **Matrix operations** - init, identity, set/get, copy/equal, add/sub, multiply, scale, transpose, determinant, trace
2. **Statistics** - windowed tracker, Welford online variance, min/max tracking, EWMA filter, energy/RMS, integer square root
3. **Distance metrics** - Manhattan, squared Euclidean, dot product, cosine similarity, Hamming distance

All operations use integer-only arithmetic suitable for MCUs without FPU.
Return codes are defined in `tikukits/maths/tiku_kits_maths.h`.

---

## Matrix Tests

**Flag:** `TEST_KITS_MATRIX`
**Source:** `test_kits_matrix.c`

### 1. Matrix Init

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_init()` |
| **Group** | "Matrix Init" |

**What it tests:**
Matrix initialization with various dimensions, boundary conditions (zero rows/cols,
oversized), and the zero operation. Verifies elements start at zero and dimensions
are stored correctly.

**Assertions:**
1. `init 2x3 returns OK`
2. `rows is 2`
3. `cols is 3`
4. `2x3 is not square`
5. `element [0][0] is 0`
6. `element [1][2] is 0`
7. `zero rows rejected`
8. `zero cols rejected`
9. `oversized rows rejected`
10. `max size init OK`
11. `zero returns OK`
12. `element zeroed after zero()`
13. `rows preserved after zero()`

---

### 2. Matrix Identity

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_identity()` |
| **Group** | "Matrix Identity" |

**What it tests:**
Identity matrix creation and validation. Verifies diagonal elements are 1,
off-diagonal are 0, and invalid sizes are rejected.

**Assertions:**
1. `identity 3x3 returns OK`
2. `identity rows is 3`
3. `identity cols is 3`
4. `identity is square`
5. `I[0][0] is 1` through `I[2][2] is 1` (3 diagonal checks)
6. `I[0][1] is 0`, `I[1][0] is 0`, `I[0][2] is 0` (off-diagonal)
7. `identity n=0 rejected`
8. `identity oversized rejected`

---

### 3. Matrix Set/Get

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_set_get()` |
| **Group** | "Matrix Set/Get" |

**What it tests:**
Element access (set and get) with bounds checking. Out-of-bounds access returns 0
and does not corrupt data. NULL matrix returns 0.

**Assertions:**
1. `get [0][0] == 42`
2. `get [1][2] == -7`
3. `get [2][1] == 100`
4. `out-of-bounds row get returns 0`
5. `out-of-bounds col get returns 0`
6. `out-of-bounds set did not corrupt data`
7. `NULL get returns 0`

---

### 4. Matrix Copy/Equal

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_copy_equal()` |
| **Group** | "Matrix Copy/Equal" |

**What it tests:**
Matrix copy and equality checking. Copies a matrix, verifies equality, modifies
the copy and verifies inequality. Different dimensions and NULL inputs also tested.

**Assertions:**
1. `copy returns OK`
2. `copy is equal to original`
3. `modified copy is not equal`
4. `different dimensions not equal`
5. `NULL a returns not equal`
6. `NULL b returns not equal`

---

### 5. Matrix Add/Sub

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_add_sub()` |
| **Group** | "Matrix Add/Sub" |

**What it tests:**
Matrix addition and subtraction, including in-place aliasing (result == operand).

**Assertions:**
1. `add returns OK`
2. `add [0][0] == 11`, `[0][1] == 22`, `[1][0] == 33`, `[1][1] == 44`
3. `sub returns OK`
4. `sub [0][0] == -9`, `[1][1] == -36`
5. `in-place add returns OK`
6. `in-place add [0][0] == 11`

---

### 6. Matrix Mul

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_mul()` |
| **Group** | "Matrix Mul" |

**What it tests:**
Matrix multiplication for square (2x2 * 2x2) and non-square (2x3 * 3x1) cases.
Also verifies A * I == A (identity multiplication).

**Assertions:**
1. `mul 2x2 * 2x2 returns OK`
2. `mul [0][0] == 19`, `[0][1] == 22`, `[1][0] == 43`, `[1][1] == 50`
3. `mul 2x3 * 3x1 returns OK`
4. `result rows == 2`, `result cols == 1`
5. `mul [0][0] == 16`, `[1][0] == 23`
6. `A * I == A`

---

### 7. Matrix Scale

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_scale()` |
| **Group** | "Matrix Scale" |

**What it tests:**
Scalar multiplication including zero scaling and in-place operations.

**Assertions:**
1. `scale returns OK`
2. `scale [0][0] == 6`, `[0][1] == -9`, `[1][0] == 12`, `[1][1] == 0`
3. `scale by 0 returns OK`
4. `scale by 0 gives zero`
5. `in-place scale OK`
6. `in-place scale [0][0] == 10`, `[1][1] == 20`

---

### 8. Matrix Transpose

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_transpose()` |
| **Group** | "Matrix Transpose" |

**What it tests:**
Matrix transposition of a 2x3 matrix, verifying dimensions swap and elements
are correctly repositioned.

**Assertions:**
1. `transpose returns OK`
2. `transpose rows == 3`, `transpose cols == 2`
3. `T[0][0] == 1`, `T[0][1] == 4`, `T[1][0] == 2`, `T[2][0] == 3`, `T[2][1] == 6`

---

### 9. Matrix Determinant

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_det()` |
| **Group** | "Matrix Determinant" |

**What it tests:**
Determinant calculation for 1x1, 2x2, 3x3, singular, and identity matrices.
Non-square matrices are rejected.

**Assertions:**
1. `det 1x1 returns OK`, `det([5]) == 5`
2. `det 2x2 returns OK`, `det([1 2; 3 4]) == -2`
3. `det 3x3 returns OK`, `det 3x3 == 22`
4. `singular matrix det == 0`
5. `identity 3x3 det == 1`
6. `det non-square rejected`

---

### 10. Matrix Trace

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_trace()` |
| **Group** | "Matrix Trace" |

**What it tests:**
Trace (sum of diagonal elements) for 2x2 and identity matrices. Non-square
matrices are rejected.

**Assertions:**
1. `trace 2x2 returns OK`, `trace([1 2; 3 4]) == 5`
2. `trace(I_4) == 4`
3. `trace non-square rejected`

---

### 11. Matrix Dimension Mismatch

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_dim_mismatch()` |
| **Group** | "Matrix Dimension Mismatch" |

**What it tests:**
Dimension mismatch error handling for add, subtract, and multiply operations.

**Assertions:**
1. `add dim mismatch rejected`
2. `sub dim mismatch rejected`
3. `mul dim mismatch rejected`

---

### 12. Matrix NULL Inputs

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_MATRIX` |
| **Source** | `test_kits_matrix.c` : `test_kits_matrix_null_inputs()` |
| **Group** | "Matrix NULL Inputs" |

**What it tests:**
NULL pointer error handling across all matrix operations.

**Assertions:**
1. `init NULL rejected`
2. `zero NULL rejected`
3. `identity NULL rejected`
4. `copy NULL dst rejected`, `copy NULL src rejected`
5. `add NULL result rejected`
6. `sub NULL a rejected`
7. `mul NULL b rejected`
8. `scale NULL rejected`
9. `transpose NULL rejected`
10. `det NULL m rejected`, `det NULL out rejected`
11. `trace NULL m rejected`, `trace NULL out rejected`
12. `rows NULL returns 0`, `cols NULL returns 0`, `is_square NULL returns 0`

---

## Statistics Tests

**Flag:** `TEST_KITS_STATISTICS`
**Source:** `test_kits_statistics.c`

### 13. Statistics Windowed

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_STATISTICS` |
| **Source** | `test_kits_statistics.c` : `test_kits_stats_windowed()` |
| **Group** | "Statistics Windowed" |

**What it tests:**
Windowed statistics tracker: initialization, mean/min/max/variance calculation,
capacity checking, reset behavior, and invalid window sizes.

**Assertions:**
1. `windowed init OK`
2. `initial count is 0`, `not full initially`, `capacity is 4`
3. `mean on empty rejected`
4. `count is 3`, `not full at 3/4`
5. `mean(10,20,30) == 20`, `min == 10`, `max == 30`, `variance == 66`
6. `count is 4`, `full at 4/4`, `mean(10,20,30,40) == 25`
7. `reset returns OK`, `count 0 after reset`, `capacity preserved`
8. `window=0 rejected`, `oversized window rejected`

---

### 14. Statistics Windowed Eviction

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_STATISTICS` |
| **Source** | `test_kits_statistics.c` : `test_kits_stats_windowed_eviction()` |
| **Group** | "Statistics Windowed Eviction" |

**What it tests:**
Window eviction behavior when the buffer is full and older elements are replaced.

**Assertions:**
1. `mean before eviction == 200`
2. `count stays at capacity`
3. `mean after eviction == 300`
4. `min after eviction == 200`, `max after eviction == 400`
5. `mean after second eviction == 400`

---

### 15. Statistics Welford

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_STATISTICS` |
| **Source** | `test_kits_statistics.c` : `test_kits_stats_welford()` |
| **Group** | "Statistics Welford" |

**What it tests:**
Welford's online algorithm for running mean and variance without storing all
samples. Tests mean, variance, stddev, reset, and single-sample behavior.

**Assertions:**
1. `welford init OK`, `initial count is 0`
2. `welford mean empty rejected`
3. `welford count is 5`, `mean(10..50) == 30`
4. `welford variance == 200`, `stddev == 14`
5. `welford count 0 after reset`
6. `single sample mean == 42`, `single sample variance == 0`

---

### 16. Statistics MinMax

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_STATISTICS` |
| **Source** | `test_kits_statistics.c` : `test_kits_stats_minmax()` |
| **Group** | "Statistics MinMax" |

**What it tests:**
Sliding-window min/max tracking with eviction when the window is full.

**Assertions:**
1. `minmax init OK`, `initial count is 0`
2. `min empty rejected`
3. `count is 3`, `full at capacity`
4. `min(50,20,80) == 20`, `max(50,20,80) == 80`
5. `min after eviction == 10`, `max after eviction == 80`
6. `min == 5`, `max == 80`
7. `min after max evicted == 5`, `max after max evicted == 90`
8. `count 0 after reset`, `window=0 rejected`

---

### 17. Statistics EWMA

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_STATISTICS` |
| **Source** | `test_kits_statistics.c` : `test_kits_stats_ewma()` |
| **Group** | "Statistics EWMA" |

**What it tests:**
Exponential Weighted Moving Average filter with configurable alpha and shift.

**Assertions:**
1. `ewma init OK`
2. `ewma get empty rejected`
3. `ewma first push == 1000`
4. `ewma after push(0) == 500`
5. `ewma empty after reset`
6. `ewma shift=17 rejected`
7. `ewma full tracking == last sample`

---

### 18. Statistics Energy/RMS

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_STATISTICS` |
| **Source** | `test_kits_statistics.c` : `test_kits_stats_energy()` |
| **Group** | "Statistics Energy/RMS" |

**What it tests:**
Energy (mean of squared values) and RMS (root-mean-square) calculations.

**Assertions:**
1. `energy init OK`, `initial count is 0`
2. `energy empty rejected`
3. `count is 2`, `mean_sq(3,4) == 12`, `rms(3,4) == 3`
4. `mean_sq(3,4,-5) == 16`, `rms(3,4,-5) == 4`
5. `energy count 0 after reset`

---

### 19. Statistics isqrt

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_STATISTICS` |
| **Source** | `test_kits_statistics.c` : `test_kits_stats_isqrt()` |
| **Group** | "Statistics isqrt" |

**What it tests:**
Integer square root function with perfect squares, non-perfect squares, and
negative inputs.

**Assertions:**
1. `isqrt(0) == 0`, `isqrt(1) == 1`, `isqrt(4) == 2`, `isqrt(9) == 3`
2. `isqrt(16) == 4`, `isqrt(100) == 10`, `isqrt(10000) == 100`
3. `isqrt(2) == 1`, `isqrt(3) == 1`, `isqrt(5) == 2`, `isqrt(8) == 2`, `isqrt(99) == 9`
4. `isqrt(-1) == 0`, `isqrt(-100) == 0`

---

### 20. Statistics NULL Inputs

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_STATISTICS` |
| **Source** | `test_kits_statistics.c` : `test_kits_stats_null_inputs()` |
| **Group** | "Statistics NULL Inputs" |

**What it tests:**
NULL pointer error handling across all statistics modules (windowed, Welford,
minmax, EWMA, energy).

**Assertions:**
1. `windowed init NULL rejected`, `reset NULL`, `push NULL`, `mean NULL out/s`, `count NULL returns 0`
2. `welford init NULL`, `push NULL`, `count NULL returns 0`
3. `minmax init NULL`, `push NULL`, `count NULL returns 0`
4. `ewma init NULL`, `push NULL`, `reset NULL`
5. `energy init NULL`, `push NULL`, `count NULL returns 0`

---

## Distance Metric Tests

**Flag:** `TEST_KITS_DISTANCE`
**Source:** `test_kits_distance.c`

### 21. Distance Manhattan

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_DISTANCE` |
| **Source** | `test_kits_distance.c` : `test_kits_distance_manhattan()` |
| **Group** | "Distance Manhattan" |

**What it tests:**
Manhattan (L1) distance with positive/negative values and edge cases.

**Assertions:**
1. `manhattan returns OK`
2. `manhattan({1,2,3},{4,0,5}) == 7`
3. `manhattan same vectors == 0`
4. `manhattan with negatives == 30`
5. `manhattan single element == 58`
6. `manhattan len=0 rejected`

---

### 22. Distance Euclidean Squared

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_DISTANCE` |
| **Source** | `test_kits_distance.c` : `test_kits_distance_euclidean_sq()` |
| **Group** | "Distance Euclidean Squared" |

**What it tests:**
Squared Euclidean distance (avoids square root for MCU efficiency).

**Assertions:**
1. `euclidean_sq returns OK`
2. `euclidean_sq({1,2,3},{4,0,5}) == 17`
3. `euclidean_sq same vectors == 0`
4. `euclidean_sq (3,4) == 25`
5. `euclidean_sq len=0 rejected`

---

### 23. Distance Dot Product

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_DISTANCE` |
| **Source** | `test_kits_distance.c` : `test_kits_distance_dot()` |
| **Group** | "Distance Dot Product" |

**What it tests:**
Dot product with positive, negative, and orthogonal vectors.

**Assertions:**
1. `dot returns OK`
2. `dot({1,2,3},{4,5,6}) == 32`
3. `dot orthogonal == 0`
4. `dot with negatives == -32`
5. `dot len=0 rejected`

---

### 24. Distance Cosine Components

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_DISTANCE` |
| **Source** | `test_kits_distance.c` : `test_kits_distance_cosine_sq()` |
| **Group** | "Distance Cosine Components" |

**What it tests:**
Cosine similarity components (three dot products: dot(a,b), dot(a,a), dot(b,b))
for identical, orthogonal, and anti-parallel vectors.

**Assertions:**
1. `cosine_sq returns OK`
2. `dot(a,b) == 25`, `dot(a,a) == 25`, `dot(b,b) == 25`
3. `identical vectors: cos^2 == 1`
4. `orthogonal dot_ab == 0`, `dot_aa == 1`, `dot_bb == 1`
5. `anti-parallel dot_ab == -1`, `cos^2 == 1`
6. `cosine_sq len=0 rejected`

---

### 25. Distance Hamming

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_DISTANCE` |
| **Source** | `test_kits_distance.c` : `test_kits_distance_hamming()` |
| **Group** | "Distance Hamming" |

**What it tests:**
Hamming distance (bit-level differences) for byte arrays.

**Assertions:**
1. `hamming returns OK`
2. `hamming(FF00AA, 0F0055) == 12`
3. `hamming identical == 0`
4. `hamming all-ones vs all-zeros == 8`
5. `hamming single bit == 1`
6. `hamming len=0 rejected`

---

### 26. Distance NULL Inputs

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_DISTANCE` |
| **Source** | `test_kits_distance.c` : `test_kits_distance_null_inputs()` |
| **Group** | "Distance NULL Inputs" |

**What it tests:**
NULL pointer error handling across all distance metric functions.

**Assertions:**
1. `manhattan NULL a/b/result rejected`
2. `euclidean_sq NULL a/result rejected`
3. `dot NULL a/result rejected`
4. `cosine_sq NULL a/dot_ab rejected`
5. `hamming NULL a/result rejected`

---

## Requests

<!--
  Add new test requests below. Specify which component (matrix, statistics,
  distance) the test is for. Each request should describe:
  - What behavior to test
  - Expected outcome
  - Any specific parameters or edge cases

  An LLM will read these requests and generate the corresponding C test
  code in the appropriate source file under tikukits/tests/maths/. After
  implementation, move the request to the relevant Tests section above
  with full documentation.

  Example format:

  ### Request: [Matrix] Large matrix multiplication stress test
  Multiply two TIKU_KITS_MATRIX_MAX_SIZE x TIKU_KITS_MATRIX_MAX_SIZE matrices
  and verify the result against hand-computed values. Expected: correct result,
  no overflow, no stack issues.
-->
