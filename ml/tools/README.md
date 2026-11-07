# Offline int8 model conversion

`model_import.py` converts JSON or TensorFlow Lite into a C header for
`tiku_kits_ml_model_s8`. It uses Python's standard library for JSON. Install
`requirements.txt` in a computer-side Python environment to read TensorFlow
Lite FlatBuffers. The pinned schema is `tflite==2.18.0`.

From the TikuOS checkout:

```bash
python3 -m pip install -r tikukits/ml/tools/requirements.txt
python3 tikukits/ml/tools/model_import.py network.tflite network.h --symbol network
python3 tikukits/ml/tools/model_import.py network.tflite network.json --emit-json
python3 tikukits/ml/tools/model_import.py network.json network.h --symbol network
```

Include the header and query `tiku_kits_ml_model_workspace_size(&network)`.
Provide that many bytes, exact input bytes and enough output capacity to
`tiku_kits_ml_model_s8`. Generated weights and metadata are immutable;
the application decides their placement and supplies writable buffers.
The device runs static descriptors and does not parse the input file.

## TensorFlow Lite subset

The input is schema version 3 (`TFL3`), with one subgraph, one input, one
output and a sequential operator chain. Tensors have static dimensions,
batch one, int8 activations, symmetric int8 weights and optional int32
bias. Activations use per-tensor scales and zero points; weights use
per-tensor or output-channel scales. Bias scales must equal input scale
times weight scale. Sparse, variable, external-buffer and dynamic-shape
models are rejected.

| Operator | Accepted versions | Options |
| --- | --- | --- |
| Conv2D | 1, 3 | NHWC/OHWI, stride, dilation, SAME or VALID |
| FullyConnected | 1, 4, 9, 12 | Default weights format, flattened batch one |
| MaxPool2D, AveragePool2D | 1, 2 | SAME or VALID, unchanged quantization |
| Reshape | 1 | Constant shape or options, unchanged element count |

Fused NONE, RELU, RELU6 and RELU_N1_TO1 activations become quantized clamp
bounds. The converter calculates Q31 multipliers and exponents from scale
ratios; exponents below -31 flush to zero, and those above 30 are rejected.
Execution uses the operator contract's double rounding. Results are not
promised to match a TensorFlow Lite build configured for single rounding.
Unsupported operators and graph structures fail conversion with an error;
they are not omitted. This is a model subset, not a general TensorFlow Lite
interpreter or an ONNX importer.

## JSON version 1

The root has `version: 1`, `input_shape: [height,width,channels]`, an int8
`input_zero_point`, and a nonempty `layers` array. Unknown fields are
rejected. Shapes and quantization connections are derived in layer order.

| Layer `op` | Required fields | Optional fields |
| --- | --- | --- |
| `dense` | `outputs`, `weights`, `multiplier`, `shift`, `output_zero_point` | `bias`, `activation` |
| `conv2d` | `kernel`, `channels`, `weights`, `multiplier`, `shift`, `output_zero_point` | `bias`, `stride`, `dilation`, `padding`, `activation` |
| `max_pool2d`, `average_pool2d` | `kernel` | `stride`, `dilation`, `padding`, `activation` |
| `reshape` | `shape` | none |

Weights are flat output-major arrays (OHWI for convolution), containing
-127 through 127. Bias is null/absent or one int32 per output channel.
`multiplier` contains nonnegative Q31 integers; `shift` contains exponents
in -31 through 30. Both arrays have one entry or one per output channel.
`kernel`, `stride` and `dilation` are `[height,width]`; default stride and
dilation are `[1,1]`. Padding is `[top,bottom,left,right]`, default zero.
Activation defaults to `[-128,127]`. Pooling preserves its input zero
point and scale. The JSON format has integer scaling metadata; the author
is responsible for consistent real-valued scales between layers.

The checked-in `TikuBench/tests/kits/ml/cnn_fixture.json` is a complete
example. `test_model_import.py` checks deterministic generated headers,
a real FlatBuffer fixture, rejected branches and malformed models, and
executes generated C against independent scalar arithmetic.
