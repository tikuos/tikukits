# Static dense models

`tiku_kits_ml_chain_s8` executes an ordered array of quantized int8 dense
layers. Enable it with `TIKU_KIT_ML_INFERENCE_ENABLE=1`; this includes the
dense operator and shared compute. Provider and CMSIS-NN selection use the
[dense operator flags](../operators/README.md).

The model contains a pointer to immutable dense descriptors and their
count. Each layer's input count and zero point match the preceding output.
Its activation bounds can express a clamped hidden tensor, including ReLU
by setting the lower bound to that tensor's zero point. The caller supplies
consistent quantization scales through each layer's multiplier and shift.
The runtime checks dimensions and zero points; it has no floating-point
scale metadata to validate.

`tiku_kits_ml_chain_workspace_size` returns three times the largest layer
output count, in bytes, or zero for invalid shape metadata. This space
holds two alternating intermediate tensors and dense output staging.
The workspace requires byte alignment. There is no heap allocation or
fixed maximum layer count. The query validates shape metadata and its
address spans; it does not evaluate weights, biases or scaling arrays.

The input count must equal the first layer's input count. Output capacity
must cover the last layer's outputs. The required workspace span and the
logical output span are disjoint from each other and from the input,
model descriptors and every layer's weights, bias and scaling arrays.
The runtime checks these spans before executing the first layer, so an
intermediate tensor cannot overwrite a later layer's parameters.

The dense operator validates and evaluates each layer. Only after the last
layer succeeds does the runtime copy its result to the caller's output.
An error leaves that output unchanged, including failures after earlier
layers have run. Workspace can change on failure. Calls use independent
workspace/output buffers and immutable shared models; compute tracing
must be disabled or serialized for concurrent calls.

This runtime accepts static C dense descriptors. It does not parse LiteRT
or ONNX files, execute convolution or attention graphs, or interpret an
NPU command stream. Compiled NPU models retain the TikuOS `interfaces/npu`
execution path and require their device-specific artifact and backend.

## Tests

```bash
make -C TikuBench/tests/host/nonkernel chain compute-contract
```

The `ml-chain` TikuBench category executes a 16→3→1 model with clamped
hidden activations, output `17` and nine bytes of workspace. It checks
insufficient workspace, a later accumulator overflow and protection of
future layer parameters. Host tests also exercise one- and three-layer
models and incompatible tensor connections. The
`model-future-weights-unprotected` defect removes the preflight weight
check so that workspace writes occur before a later layer rejects them.
