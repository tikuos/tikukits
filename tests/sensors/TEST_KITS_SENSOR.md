# TikuKits Sensor Tests

**Suite name:** `TikuKits Sensor`
**Source directory:** `tikukits/tests/sensors/`
**Header:** `test_kits_sensor.h`
**Source files:** `test_kits_sensor.c`
**Flag:** `TEST_KITS_SENSOR`

## Overview

Tests for the TikuKits sensor library common utilities and driver name functions.
The sensor kit provides a hardware-abstracted interface for temperature sensors
(MCP9808, ADT7410, DS18B20) with shared types and fractional-to-decimal
conversion helpers defined in `tikukits/sensors/tiku_kits_sensor.h`.

These tests validate the software-only parts of the sensor interface (conversion
macros and name strings). Actual I2C/1-Wire hardware communication is not tested
here as it requires on-target execution.

---

## Tests

### 1. Fractional Conversion

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SENSOR` |
| **Source** | `test_kits_sensor.c` : `test_kits_sensor_frac_conv()` |
| **Group** | "Sensor FRAC_TO_DEC" |

**What it tests:**
The `TIKU_KITS_SENSOR_FRAC_TO_DEC()` macro, which converts 1/16 degree C
fractional units to two-digit decimal representation for display purposes.
Tests all boundary values (0/16 through 15/16) and verifies monotonic ordering.

**Assertions:**
1. `FRAC_TO_DEC(0) == 0` - "frac 0/16 -> 00"
2. `FRAC_TO_DEC(1) == 6` - "frac 1/16 -> 06"
3. `FRAC_TO_DEC(2) == 12` - "frac 2/16 -> 12"
4. `FRAC_TO_DEC(4) == 25` - "frac 4/16 -> 25"
5. `FRAC_TO_DEC(8) == 50` - "frac 8/16 -> 50"
6. `FRAC_TO_DEC(12) == 75` - "frac 12/16 -> 75"
7. `FRAC_TO_DEC(15) == 93` - "frac 15/16 -> 93"
8. All values 0-15 are monotonically increasing - "FRAC_TO_DEC monotonically increasing 0-15"

---

### 2. Sensor Name Lookup

| Field | Value |
|-------|-------|
| **Flag** | `TEST_KITS_SENSOR` |
| **Source** | `test_kits_sensor.c` : `test_kits_sensor_name()` |
| **Group** | "Sensor Name" |

**What it tests:**
Sensor name retrieval functions for three temperature sensor drivers. Verifies
each driver's name function returns a non-null pointer with the correct string.

**Assertions:**
1. `MCP9808 name not NULL`
2. `MCP9808 name is 'MCP9808'`
3. `ADT7410 name not NULL`
4. `ADT7410 name is 'ADT7410'`
5. `DS18B20 name not NULL`
6. `DS18B20 name is 'DS18B20'`

---

## Requests

<!--
  Add new test requests below. Each request should describe:
  - What behavior to test
  - Expected outcome
  - Any specific parameters or edge cases

  An LLM will read these requests and generate the corresponding C test
  code in a new file under tikukits/tests/sensors/. After implementation,
  move the request to the Tests section above with full documentation.

  Example format:

  ### Request: Sensor temperature struct packing
  Verify that tiku_kits_sensor_temp_t fields (integer, fraction) are correctly
  packed and that sign propagation works for negative temperatures (e.g., -5.5 C).
  Expected: integer part is -6, fraction part is 8 (8/16 = 0.5).
-->
