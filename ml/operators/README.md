# Quantized dense inference

`tiku_kits_ml_dense_s8` evaluates one row-major int8 dense layer. Enable it
with `TIKU_KIT_ML_DENSE_ENABLE=1`; this also includes the shared compute
HAL. Select `TIKU_COMPUTE_BACKEND=reference`, `p4` or `mve` independently.
The P4 backend also requires `TIKU_COMPUTE_P4_REV=1`.

The caller provides dimensions, weights, optional bias, quantization
metadata, input, output and scratch. No allocation or initialization is
required. Weights are symmetric int8 values in -127 through 127. Input
and output zero points are in -128 through 127. Dimensions are nonzero;
row stride is at least the input count. Padding between rows is skipped.
Bias contains one int32 value per output or is null for zero bias.

Each accumulator is `bias + sum(weight * (input - input_zero_point))`,
computed in int64 and checked against int32 bounds. Quantization has one
nonnegative Q31 multiplier and exponent per tensor or per output channel.
Exponents range from -31 through 30. A positive exponent left-scales the
accumulator, which must still fit int32. High multiplication rounds to
nearest with ties toward positive infinity; a negative exponent then
divides by a power of two, rounding to nearest with ties away from zero.
The output zero point is added before clamping to the activation interval.
The API implements integer double rounding; it does not load a model file
or infer quantization metadata.

`tiku_kits_ml_dense_scratch_size` reports one byte per output. This query
does not validate the layer's operands or quantization. Scratch needs byte
alignment and is disjoint from metadata, weights, input, bias, scaling
arrays and output. Output is also disjoint from readable operands.
The caller supplies valid memory for every declared extent. The operator
checks representable spans and natural alignment, not allocation ownership.

Output is published after every row succeeds. Invalid parameters,
overlapping buffers, insufficient scratch or an out-of-range accumulator
leave output unchanged; scratch can change on failure. Calls are
synchronous and reentrant with independent outputs and scratch. Compute
tracing must be disabled or serialized when calls overlap in time.

The optional [CMSIS-NN adapter](../backends/cmsis_nn/README.md) executes
supported rows and falls back to shared compute for other shapes. Existing
classification operators retain their separate numerical contracts.
The optional [static model runtime](../inference/README.md) connects dense
layers with caller-owned intermediate tensors and stages final output.

## Example and tests

With the examples checkout present, `TIKU_EXAMPLE_KITS_ML_DENSE=1` selects
`examples/kits/ml/example_ml_dense.c` and enables dense inference. Its
two signed sums produce `34 -34`, using two bytes of caller scratch.
Apply this flag to the board's normal firmware build command with
`HAS_EXAMPLES=1 HAS_TIKUKITS=1`.

```bash
make -C TikuBench/tests/host/nonkernel dense compute-contract
```

The `ml-dense` TikuBench category runs a per-channel fixture, scratch
rejection and a negative rounding tie. Accelerated builds check vector
element counters. Reference and PIE builds pass on the FireBeetle 2 P4.
At 360 MHz, a 256-call batch of an 8×256 layer with SRAM operands averages
89.3 µs per call with PIE and 166.2 µs with the reference provider. Both
produce the same eight outputs and use eight bytes of caller scratch.
The first calls measured 120.0 µs and 197.0 µs respectively; these are
first-invocation measurements, without a forced cold-cache condition.

## Convolution and pooling

`tiku_kits_ml_cnn.h` provides one-image NHWC int8 Conv2D, max pooling and
average pooling. Dense enable includes these operators. Convolution
weights use OHWI order and dense per-tensor or per-output-channel scaling.
The window specifies stride, dilation and explicit top/bottom/left/right
padding. Convolution pads with the input zero point. Each patch executes
through the dense operator and its selected shared or vendor backend.

Pooling preserves tensor scale and zero point. It excludes padding from
both the maximum and the average divisor. A window with no real samples
is invalid. Average pooling rounds nearest with ties away from zero;
activation limits apply afterward. Spatial dimensions, window dimensions,
strides, dilation and padding are bounded to 65535; image element counts
must fit both `uint32_t` and `size_t`.

Convolution scratch holds the staged output, one input patch and one dense
row-output buffer. Pooling scratch holds the staged output. Size queries
return zero for invalid geometry or an unrepresentable requirement. They
do not read or evaluate weight values. Buffers need byte alignment, and
the APIs accept exact input length and output capacity. Readable model,
parameter and input spans are disjoint from scratch and logical output.
Errors preserve output; scratch can change. Independent workspaces allow
concurrent calls.

The optional [ESP-NN adapter](../backends/esp_nn/README.md) supports P4
reductions. `cnn` and `model-import` host targets cover geometry, padding,
dilation, quantization, alias checks and output preservation.
