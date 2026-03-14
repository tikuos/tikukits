# TikuKits ML Tests

**Suite name:** `TikuKits ML`
**Source directory:** `tikukits/tests/ml/`
**Header:** `test_kits_ml.h`
**Source files:** `test_kits_ml_linreg.c`
**Flag:** `TEST_KITS_ML_LINREG`
**Parent flag:** `TEST_KITS_ML` (auto-derived from sub-flags)

## Overview

Tests for the TikuKits machine learning library. Currently covers streaming
ordinary least squares (OLS) linear regression using fixed-point Q-format
arithmetic suitable for MCUs without FPU.

The linear regression module uses five `int64_t` accumulators
(sum_x, sum_y, sum_xx, sum_xy, sum_yy) for O(1) push and O(1) query operations.
Output values (slope, intercept, R-squared) are in Q-format with configurable
shift (default Q8). Predictions are returned in original-scale integers.

Return codes are defined in `tikukits/ml/tiku_kits_ml.h`.

---

## Linear Regression Tests

### 1. Perfect Fit

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_perfect_fit()` |
| **Group** | "LinReg Perfect Fit" |

**What it tests:**
Validates initialization, push, count, and slope computation with perfect-fit
data along the line y = 2x (points: (0,0), (10,20), (20,40)).

**Assertions:**
1. `init returns OK`
2. `initial count is 0`
3. `count is 3 after 3 pushes`
4. `slope(y=2x) == 512 (2.0 Q8)`

---

### 2. Non-Zero Intercept

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_intercept()` |
| **Group** | "LinReg Intercept" |

**What it tests:**
Slope and intercept computation for y = 2x + 1.

**Assertions:**
1. `slope(y=2x+1) == 512 (2.0 Q8)`
2. `intercept(y=2x+1) == 256 (1.0 Q8)`

---

### 3. Fractional Slope

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_fractional_slope()` |
| **Group** | "LinReg Fractional Slope" |

**What it tests:**
Validates fixed-point representation of fractional slope for y = 1.5x + 10.

**Assertions:**
1. `slope(y=1.5x+10) == 384 (1.5 Q8)`
2. `intercept(y=1.5x+10) == 2560 (10.0 Q8)`

---

### 4. Prediction

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_predict()` |
| **Group** | "LinReg Predict" |

**What it tests:**
Prediction at interpolation, training, extrapolation, and intercept points for
the model y = 3x + 5. Predictions are returned as original-scale integers
(not Q-format).

**Assertions:**
1. `predict(15) for y=3x+5 == 50` (interpolation)
2. `predict(10) for y=3x+5 == 35` (training point)
3. `predict(30) for y=3x+5 == 95` (extrapolation)
4. `predict(0) for y=3x+5 == 5` (intercept)

---

### 5. R-Squared

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_r2()` |
| **Group** | "LinReg R-squared" |

**What it tests:**
Coefficient of determination (R-squared) for a perfect-fit model (R2 = 1.0)
and a noisy model (0 < R2 < 1.0).

**Assertions:**
1. `R2 for perfect fit == 256 (1.0 Q8)`
2. `R2 for noisy data in (0, 256)`

---

### 6. Negative Values

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_negative_values()` |
| **Group** | "LinReg Negative Values" |

**What it tests:**
Handling of negative slope and large intercept for y = -2x + 100.

**Assertions:**
1. `slope(y=-2x+100) == -512 (-2.0 Q8)`
2. `intercept(y=-2x+100) == 25600 (100.0 Q8)`
3. `predict(25) for y=-2x+100 == 50`

---

### 7. Reset

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_reset()` |
| **Group** | "LinReg Reset" |

**What it tests:**
Reset clears all accumulators and allows refitting with a different model.
Verifies error handling on empty model and correct slope after refit.

**Assertions:**
1. `slope before reset == 512`
2. `reset returns OK`
3. `count 0 after reset`
4. `slope on empty model rejected` (ERR_SIZE)
5. `predict on empty model rejected` (ERR_SIZE)
6. `slope after refit == 1280 (5.0 Q8)`

---

### 8. Edge Cases

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_edge_cases()` |
| **Group** | "LinReg Edge Cases" |

**What it tests:**
Error handling for degenerate inputs: single data point (need >= 2), identical
x values (singular denominator), constant y (zero SS_yy for R2), and invalid
shift parameter.

**Assertions:**
1. `slope with 1 point rejected` (ERR_SIZE)
2. `slope with identical x values rejected` (ERR_SINGULAR)
3. `R2 with constant y rejected` (ERR_SINGULAR)
4. `slope with constant y OK`, `slope with constant y == 0`
5. `shift=31 rejected` (ERR_PARAM)

---

### 9. NULL Inputs

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_ML_LINREG` |
| **Source** | `test_kits_ml_linreg.c` : `test_kits_ml_linreg_null_inputs()` |
| **Group** | "LinReg NULL Inputs" |

**What it tests:**
NULL pointer rejection across all API functions.

**Assertions:**
1. `init NULL rejected`
2. `reset NULL rejected`
3. `push NULL rejected`
4. `slope NULL out rejected`, `slope NULL lr rejected`
5. `intercept NULL out rejected`
6. `predict NULL out rejected`, `predict NULL lr rejected`
7. `r2 NULL out rejected`
8. `count NULL returns 0`

---

## Requests

<!--
  Add new test requests below. Each request should describe:
  - What behavior to test
  - Expected outcome
  - Any specific parameters or edge cases

  An LLM will read these requests and generate the corresponding C test
  code in a new file under tikukits/tests/ml/. After implementation, move
  the request to the Tests section above with full documentation.

  Example format:

  ### Request: Linear regression with large values (overflow resistance)
  Push data points with x and y values near INT16_MAX (32767). Verify that
  int64_t accumulators handle the multiplication without overflow and that
  slope/intercept/predict return correct results. Expected: correct Q8 slope,
  no truncation artifacts.
-->
