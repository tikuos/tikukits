# CMSIS-NN dense adapter

The adapter imports the fully connected int8 kernel and its vector-matrix
support function from CMSIS-NN v8.0.0, revision
`13c97dbb6f781d4aab38ed34e6e441f42b79aff4`. CMSIS-NN is Apache-2.0 licensed.
The checkout is supplied by the caller and is not vendored in TikuKits.

```bash
git clone --branch v8.0.0 --depth 1 https://github.com/ARM-software/CMSIS-NN.git /path/to/CMSIS-NN
```

Add `TIKU_KIT_ML_DENSE_ENABLE=1 TIKU_KIT_ML_CMSIS_NN_ENABLE=1
CMSIS_NN_DIR=/path/to/CMSIS-NN` to the firmware build. The source manifest
checks the revision. Keep that checkout unmodified when reproducing the
qualified source build. CMSIS-NN selects integer MVE or DSP from compiler
target features; host builds execute its C implementation. Single-rounding
configuration is rejected at compile time.

Each invocation processes one output row with stack-local dimensions,
parameters and corrected bias. There is no global vendor scratch setter
or shared context. The caller's dense scratch holds staged outputs.
Concurrent calls require independent output and scratch buffers, with
shared-compute tracing disabled or serialized.

The vendor path requires an input count divisible by 16, an exponent of
at least -30 and a conservative int32 bound on products, bias, zero-point
correction, scaling and output-offset addition. Other valid rows use the
shared-compute path, including exponent -31. Invalid rows still obey the
dense API's output-preserving error contract. Per-channel scaling is
implemented by passing each row's multiplier and exponent separately.

```bash
make -C TikuBench/tests/host/nonkernel dense-cmsis \
    CMSIS_NN_DIR=/path/to/CMSIS-NN
```

The host test exercises the pinned vendor implementation with concurrent
callers. Arm object builds cover Cortex-M33, Cortex-M55 and Cortex-M85.
The C vendor path passes the P4 dense category. It averages 185.5 µs for
the measured 8×256 SRAM layer at 360 MHz; native PIE averages 89.3 µs for
the same layer. The P4 path therefore uses the native provider unless the
CMSIS-NN adapter is explicitly enabled. Arm DSP/MVE vendor execution and
performance qualification remain pending on their boards.
