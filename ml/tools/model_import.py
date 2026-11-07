#!/usr/bin/env python3
#
# Tiku Operating System v0.06
# Simple. Persistent. Intelligent.
# http://tiku-os.org
#
# Authors: Ambuj Varshney <ambuj@tiku-os.org>
#
# model_import.py - validate model files and emit static quantized descriptors.
#
# SPDX-License-Identifier: Apache-2.0
"""Compile sequential int8 JSON or TensorFlow Lite models into static C metadata."""
import argparse
import json
import math
from pathlib import Path
import re
import struct


class ModelError(ValueError):
    """An input model does not satisfy the supported numerical or graph contract."""


def require(condition, message):
    if not condition:
        raise ModelError(message)


def integer(value, low, high, name):
    require(type(value) is int and low <= value <= high, f"invalid {name}: {value!r}")
    return value


def vector(value, count, low, high, name):
    require(isinstance(value, list) and len(value) == count, f"{name}: expected {count} integers")
    return [integer(x, low, high, name) for x in value]


def shape(value):
    result = vector(value, 3, 1, 65535, "shape")
    require(math.prod(result) <= 0xffffffff, "tensor exceeds 32-bit element count")
    return result


def normalize(model):
    """Validate JSON metadata and derive the input and output geometry of each layer."""
    require(isinstance(model, dict), "model must be an object")
    require(set(model) <= {"version", "input_shape", "input_zero_point", "layers"}, "unknown model field")
    integer(model.get("version"), 1, 1, "model version")
    current = shape(model["input_shape"])
    zero = integer(model["input_zero_point"], -128, 127, "input zero point")
    layers = model["layers"]
    require(isinstance(layers, list) and 0 < len(layers) <= 0xffffffff, "empty or oversized layer list")
    result = {"version": 1, "input_shape": current, "input_zero_point": zero, "layers": []}
    for index, source in enumerate(layers):
        require(isinstance(source, dict), f"layer {index} must be an object")
        op = source.get("op")
        common = {"op", "activation"}
        weighted = {"weights", "bias", "multiplier", "shift", "output_zero_point"}
        spatial = {"kernel", "stride", "dilation", "padding"}
        allowed = {"dense": common | weighted | {"outputs"},
                   "conv2d": common | weighted | spatial | {"channels"},
                   "max_pool2d": common | spatial, "average_pool2d": common | spatial,
                   "reshape": {"op", "shape"}}
        require(op in allowed, f"unsupported layer {index}: {op}")
        require(set(source) <= allowed[op], f"unknown field in layer {index}")
        layer = dict(source, input_shape=current, input_zero_point=zero)
        if op == "reshape":
            output = shape(source["shape"])
            require(math.prod(output) == math.prod(current), "reshape changes element count")
        else:
            act = vector(source.get("activation", [-128, 127]), 2, -128, 127, "activation")
            require(act[0] <= act[1], "reversed activation range")
            layer["activation"] = act
            if op != "dense":
                kh, kw = layer["kernel"] = vector(source["kernel"], 2, 1, 65535, "kernel")
                sh, sw = layer["stride"] = vector(source.get("stride", [1, 1]), 2, 1, 65535, "stride")
                dh, dw = layer["dilation"] = vector(source.get("dilation", [1, 1]), 2, 1, 65535, "dilation")
                pt, pb, pl, pr = layer["padding"] = vector(source.get("padding", [0]*4), 4, 0, 65535, "padding")
                oh = (current[0]+pt+pb-(kh-1)*dh-1)//sh+1
                ow = (current[1]+pl+pr-(kw-1)*dw-1)//sw+1
                output = shape([oh, ow, current[2]])
                # Pool windows must contain at least one real sample, including dilated windows.
                if op.endswith("pool2d"):
                    for length, out, k, stride, dil, pad in ((current[0],oh,kh,sh,dh,pt),(current[1],ow,kw,sw,dw,pl)):
                        for position in range(out):
                            start = position*stride-pad
                            first = max(0, (-start+dil-1)//dil)
                            last = min(k-1, (length-1-start)//dil)
                            require(first <= last, "pool window contains no input samples")
            if op in ("dense", "conv2d"):
                channels = integer(source["outputs" if op == "dense" else "channels"], 1, 65535, "output channels")
                inputs = math.prod(current) if op == "dense" else kh*kw*current[2]
                require(inputs <= 0xffffffff, "row exceeds 32-bit element count")
                layer["weights"] = vector(source["weights"], channels*inputs, -127, 127, "weights")
                if source.get("bias") is not None:
                    layer["bias"] = vector(source["bias"], channels, -(1 << 31), (1 << 31)-1, "bias")
                else:
                    layer["bias"] = None
                count = len(source["multiplier"])
                require(count in (1, channels), "scale count must be one or the output channels")
                layer["multiplier"] = vector(source["multiplier"], count, 0, (1 << 31)-1, "multiplier")
                layer["shift"] = vector(source["shift"], count, -31, 30, "shift")
                zero = integer(source["output_zero_point"], -128, 127, "output zero point")
                layer["inputs"], layer["outputs"] = inputs, channels
                output = [1, 1, channels] if op == "dense" else [output[0], output[1], channels]
        layer["output_shape"] = output
        result["layers"].append(layer)
        current = output
    return result


def emit_c(model, symbol):
    """Emit immutable model parameters; the application supplies input and workspace."""
    require(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", symbol) is not None, "invalid C symbol")
    require(symbol not in {"int", "float", "double", "static", "const", "struct", "union", "enum", "void", "char", "long", "short", "unsigned", "signed", "auto", "register", "extern", "return", "switch", "case", "default", "for", "do", "while", "if", "else", "break", "continue", "goto", "sizeof", "typedef", "volatile", "restrict", "inline"}, "C keyword is not a symbol")
    m = normalize(model)
    guard = "TIKU_MODEL_" + symbol.upper() + "_H_"
    lines = ["/* Generated by TikuKits model_import.py. */", f"#ifndef {guard}", f"#define {guard}",
             '#include <tikukits/ml/inference/tiku_kits_ml_model.h>']
    entries = []
    brace = lambda values: "{" + ",".join(map(str, values)) + "}"
    for index, layer in enumerate(m["layers"]):
        prefix = f"{symbol}_{index}"
        op = layer["op"]
        if op in ("dense", "conv2d"):
            for name, ctype in (("weights", "int8_t"), ("bias", "int32_t"), ("multiplier", "int32_t"), ("shift", "int8_t")):
                if layer[name] is not None:
                    lines.append(f"static _Alignas(16) const {ctype} {prefix}_{name}[] = {brace(layer[name])};")
            bias = f"{prefix}_bias" if layer["bias"] is not None else "NULL"
            dense = "{" + f'{layer["inputs"]},{layer["outputs"]},{layer["inputs"]},{prefix}_weights,{bias},{prefix}_multiplier,{prefix}_shift,{len(layer["shift"])},{layer["input_zero_point"]},{layer["output_zero_point"]},{layer["activation"][0]},{layer["activation"][1]}' + "}"
        if op == "reshape":
            kind, field, value = "RESHAPE", "reshape", brace(layer["shape"])
        elif op == "dense":
            kind, field, value = "DENSE", "dense", dense
        else:
            window = brace(layer["kernel"]+layer["stride"]+layer["dilation"]+layer["padding"])
            if op == "conv2d":
                kind, field, tail = "CONV2D", "conv", dense
            else:
                kind, field = "POOL2D", "pool"
                tail = f'TIKU_KITS_ML_POOL_{"MAX" if op == "max_pool2d" else "AVERAGE"},{layer["input_zero_point"]},{layer["activation"][0]},{layer["activation"][1]}'
            value = "{" + brace(layer["input_shape"]) + "," + window + "," + tail + "}"
        entries.append(f"    {{.kind=TIKU_KITS_ML_LAYER_{kind},.op.{field}={value}}}")
    lines += [f"static const tiku_kits_ml_layer_t {symbol}_layers[] = {{", ",\n".join(entries), "};",
              f"static const tiku_kits_ml_model_t {symbol} = " + "{" + brace(m["input_shape"]) + f',{m["input_zero_point"]},{symbol}_layers,{len(entries)}' + "};", "#endif", ""]
    return "\n".join(lines)


def round_away(value):
    return int(math.copysign(math.floor(abs(value)+0.5), value))


def quant_multiplier(scale):
    require(math.isfinite(scale) and scale > 0, "invalid effective scale")
    fraction, exponent = math.frexp(scale)
    multiplier = round_away(fraction*(1 << 31))
    if multiplier == 1 << 31:
        multiplier //= 2
        exponent += 1
    if exponent < -31:
        return 0, 0
    require(exponent <= 30, "effective scale exceeds supported left shift")
    return multiplier, exponent


def read_tflite(data):
    """Decode the static single-chain int8 subset of TensorFlow Lite schema version 3."""
    import tflite as tf
    require(tf.Model.ModelBufferHasIdentifier(data, 0), "missing TFL3 identifier")
    model = tf.Model.GetRootAsModel(data, 0)
    require(model.Version() == 3 and model.SubgraphsLength() == 1, "expected schema 3 and one subgraph")
    graph = model.Subgraphs(0)
    require(graph.InputsLength() == 1 and graph.OutputsLength() == 1, "expected one input and one output")

    def tensor(index):
        require(0 <= index < graph.TensorsLength(), "invalid tensor index")
        t = graph.Tensors(index)
        require(not t.IsVariable() and t.Sparsity() is None, "variable or sparse tensor")
        dims = [t.Shape(i) for i in range(t.ShapeLength())]
        require(dims and all(x > 0 for x in dims), "empty or dynamic tensor shape")
        require(t.ShapeSignatureLength() == 0 or [t.ShapeSignature(i) for i in range(t.ShapeSignatureLength())] == dims, "dynamic shape signature")
        return t, dims

    def image(dims):
        if len(dims) == 4:
            require(dims[0] == 1, "only batch one is supported")
            return shape(dims[1:])
        if len(dims) == 2:
            require(dims[0] == 1, "only batch one is supported")
            return shape([1, 1, dims[1]])
        require(len(dims) == 1, "activation rank must be 1, 2 or 4")
        return shape([1, 1, dims[0]])

    def quant(t, activation=False):
        q = t.Quantization()
        require(q is not None and q.ScaleLength() > 0, "missing quantization")
        require(q.DetailsType() == 0, "custom quantization details")
        scales = [float(q.Scale(i)) for i in range(q.ScaleLength())]
        zeros = [int(q.ZeroPoint(i)) for i in range(q.ZeroPointLength())]
        require(len(scales) == len(zeros) and all(math.isfinite(x) and x > 0 for x in scales), "invalid quantization")
        if activation:
            require(t.Type() == tf.TensorType.INT8 and len(scales) == 1 and -128 <= zeros[0] <= 127, "activations must be per-tensor int8")
        return scales, zeros, q.QuantizedDimension()

    def constant(t, count, code):
        index = t.Buffer()
        require(0 <= index < model.BuffersLength(), "invalid constant buffer")
        b = model.Buffers(index)
        require(b.Offset() == 0 and b.Size() == 0, "external buffer storage is unsupported")
        raw = bytes(b.Data(i) for i in range(b.DataLength()))
        require(len(raw) == count*struct.calcsize(code), "constant byte length does not match shape")
        return list(struct.unpack("<"+str(count)+code, raw))

    def activation(code, scale, zero):
        if code == tf.ActivationFunctionType.NONE:
            return [-128, 127]
        bounds = {tf.ActivationFunctionType.RELU: (0, math.inf), tf.ActivationFunctionType.RELU6: (0, 6), tf.ActivationFunctionType.RELU_N1_TO_1: (-1, 1)}
        require(code in bounds, "unsupported fused activation")
        low, high = bounds[code]
        convert = lambda x: max(-128, min(127, round_away(x/scale)+zero))
        return [convert(low), 127 if math.isinf(high) else convert(high)]

    current = int(graph.Inputs(0))
    inp, dims = tensor(current)
    iq, iz, _ = quant(inp, True)
    result = {"version": 1, "input_shape": image(dims), "input_zero_point": iz[0], "layers": []}
    require(inp.Buffer() == 0, "model input cannot be constant")
    written = {current}
    for position in range(graph.OperatorsLength()):
        operator = graph.Operators(position)
        require(operator.CustomOptionsLength() == 0 and operator.IntermediatesLength() == 0 and operator.MutatingVariableInputsLength() == 0, "custom, intermediate or mutating operator metadata")
        require(operator.InputsLength() >= 1 and operator.Inputs(0) == current and operator.OutputsLength() == 1, "graph must be a sequential single-input chain")
        require(0 <= operator.OpcodeIndex() < model.OperatorCodesLength(), "invalid opcode index")
        opcode = model.OperatorCodes(operator.OpcodeIndex())
        code = max(opcode.BuiltinCode(), opcode.DeprecatedBuiltinCode())
        # These versions share the int8 tensor and option fields consumed below.
        versions = {tf.BuiltinOperator.CONV_2D: (1, 3), tf.BuiltinOperator.FULLY_CONNECTED: (1, 4, 9, 12), tf.BuiltinOperator.MAX_POOL_2D: (1, 2), tf.BuiltinOperator.AVERAGE_POOL_2D: (1, 2), tf.BuiltinOperator.RESHAPE: (1,)}
        require(code in versions and opcode.Version() in versions[code], f"unsupported opcode/version {code}/{opcode.Version()}")
        output_index = int(operator.Outputs(0))
        require(output_index not in written, "tensor written twice or in-place graph")
        out, outdims = tensor(output_index)
        require(out.Buffer() == 0, "operator output cannot be constant")
        oq, oz, _ = quant(out, True)
        inshape, outshape = image(dims), image(outdims)
        names = {tf.BuiltinOperator.CONV_2D: "Conv2DOptions", tf.BuiltinOperator.FULLY_CONNECTED: "FullyConnectedOptions", tf.BuiltinOperator.MAX_POOL_2D: "Pool2DOptions", tf.BuiltinOperator.AVERAGE_POOL_2D: "Pool2DOptions", tf.BuiltinOperator.RESHAPE: "ReshapeOptions"}
        name = names[code]
        require(operator.BuiltinOptionsType() == getattr(tf.BuiltinOptions, name), "opcode/options mismatch")
        table = operator.BuiltinOptions()
        require(table is not None, "missing operator options")
        options = getattr(tf, name)()
        options.Init(table.Bytes, table.Pos)
        if code == tf.BuiltinOperator.RESHAPE:
            require(operator.InputsLength() in (1, 2) and iq == oq and iz == oz, "reshape changes quantization")
            if operator.InputsLength() == 2:
                st, sd = tensor(operator.Inputs(1))
                require(st.Type() == tf.TensorType.INT32, "reshape shape must be int32")
                target = constant(st, math.prod(sd), "i")
            else:
                target = [options.NewShape(i) for i in range(options.NewShapeLength())]
            require(len(target) == len(outdims) and target.count(-1) <= 1 and all(a == b or a == -1 for a, b in zip(target, outdims)), "reshape options disagree with output")
            require(math.prod(dims) == math.prod(outdims), "reshape changes size")
            layer = {"op": "reshape", "shape": outshape}
        else:
            layer = {"activation": activation(options.FusedActivationFunction(), oq[0], oz[0])}
            weighted = code in (tf.BuiltinOperator.CONV_2D, tf.BuiltinOperator.FULLY_CONNECTED)
            if weighted:
                require(options.QuantizedBiasType() in (0, tf.TensorType.INT32), "unsupported quantized bias type")
                require(operator.InputsLength() in (2, 3), "weighted operator input count")
                wt, wd = tensor(operator.Inputs(1))
                require(wt.Type() == tf.TensorType.INT8, "weights must be int8")
                wq, wz, axis = quant(wt)
                require(all(x == 0 for x in wz) and len(wq) in (1, wd[0]) and (len(wq) == 1 or axis == 0), "weights require symmetric output-channel quantization")
                layer["weights"] = constant(wt, math.prod(wd), "b")
                if operator.InputsLength() == 3 and operator.Inputs(2) >= 0:
                    bt, bd = tensor(operator.Inputs(2))
                    require(bt.Type() == tf.TensorType.INT32 and bd == [wd[0]], "bias must be one int32 per output")
                    bq, bz, _ = quant(bt)
                    require(len(bq) == len(wq) and all(x == 0 for x in bz) and all(math.isclose(a, iq[0]*b, rel_tol=1e-5) for a, b in zip(bq, wq)), "bias quantization differs from input times weight scale")
                    layer["bias"] = constant(bt, wd[0], "i")
                scales = [quant_multiplier(iq[0]*x/oq[0]) for x in wq]
                layer.update(multiplier=[x[0] for x in scales], shift=[x[1] for x in scales], output_zero_point=oz[0])
            if code == tf.BuiltinOperator.FULLY_CONNECTED:
                require(len(wd) == 2 and wd[1] == math.prod(inshape) and outshape == [1, 1, wd[0]], "dense geometry mismatch")
                require(options.WeightsFormat() == 0 and not options.AsymmetricQuantizeInputs() and not options.KeepNumDims(), "unsupported dense options")
                layer.update(op="dense", outputs=wd[0])
            else:
                require(len(dims) == 4 and len(outdims) == 4, "spatial operators require NHWC rank four")
                if code == tf.BuiltinOperator.CONV_2D:
                    require(len(wd) == 4 and wd[3] == inshape[2] and wd[0] == outshape[2], "OHWI filter geometry mismatch")
                    kernel = wd[1:3]
                    dilation = [options.DilationHFactor(), options.DilationWFactor()]
                    layer.update(op="conv2d", channels=wd[0])
                else:
                    require(operator.InputsLength() == 1 and iq == oq and iz == oz and inshape[2] == outshape[2], "pooling requires unchanged scale, zero point and channels")
                    kernel, dilation = [options.FilterHeight(), options.FilterWidth()], [1, 1]
                    layer["op"] = "max_pool2d" if code == tf.BuiltinOperator.MAX_POOL_2D else "average_pool2d"
                stride = [options.StrideH(), options.StrideW()]
                require(all(x > 0 for x in kernel+stride+dilation), "invalid spatial options")
                pads = []
                for i in range(2):
                    extent = (kernel[i]-1)*dilation[i]+1
                    if options.Padding() == tf.Padding.SAME:
                        expected = (inshape[i]+stride[i]-1)//stride[i]
                        total = max(0, (expected-1)*stride[i]+extent-inshape[i])
                    else:
                        require(options.Padding() == tf.Padding.VALID, "unknown padding")
                        expected, total = (inshape[i]-extent)//stride[i]+1, 0
                    require(outshape[i] == expected, "spatial output shape mismatch")
                    pads.extend([total//2, total-total//2])
                layer.update(kernel=kernel, stride=stride, dilation=dilation, padding=pads)
        result["layers"].append(layer)
        current, inp, dims, iq, iz = output_index, out, outdims, oq, oz
        written.add(current)
    require(current == graph.Outputs(0), "final operator is not the graph output")
    normalize(result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--symbol", default="tiku_model")
    parser.add_argument("--format", choices=("json", "tflite"))
    parser.add_argument("--emit-json", action="store_true", help="write validated JSON instead of C")
    args = parser.parse_args()
    try:
        fmt = args.format or ("tflite" if args.input.suffix == ".tflite" else "json")
        model = read_tflite(args.input.read_bytes()) if fmt == "tflite" else json.loads(args.input.read_text())
        normalize(model)
        output = json.dumps(model, indent=2)+"\n" if args.emit_json else emit_c(model, args.symbol)
        args.output.write_text(output)
    except (ModelError, ValueError, KeyError, TypeError, IndexError, AttributeError, struct.error, ImportError, OSError) as error:
        parser.exit(2, f"model import: {error}\n")


if __name__ == "__main__":
    main()
