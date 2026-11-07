# ESP-NN dense adapter

This optional adapter builds the ESP32-P4 PIE dense kernel from ESP-NN
v1.4.2, commit `744d48deae6aa4a4d7b93b6a80f548d5298813a4` (Apache-2.0).
The checkout remains external to TikuKits. Its selected source files must
match that commit; the build refuses local vendor changes.

Add these variables to a FireBeetle 2 P4 firmware build:

```text
HAS_TIKUKITS=1 TIKU_KIT_ML_DENSE_ENABLE=1
TIKU_KIT_ML_ESP_NN_ENABLE=1 TIKU_COMPUTE_P4_REV=1
ESP_NN_DIR=/absolute/path/to/esp-nn
```

The adapter enables `TIKU_P4_HWLOOP_ENABLE=1`. Select the shared fallback
provider with `TIKU_COMPUTE_BACKEND=reference` or `p4`. CMSIS-NN and ESP-NN
cannot be selected in the same build. ESP-NN runs in the TikuKits layer;
the kernel has no ESP-NN includes, model types or library initialization.

Each eligible dense row calls the vendor implementation with symmetric
input/filter offsets and a checked bias correction for the input zero
point. The wrapper specializes the pinned source at compile time. Vendor
inline assembly is translated to the P4's verified instruction macros;
unknown mnemonics stop the build. The compiler removes unused offset
correction and per-channel entry points. The vendor source is unmodified.

Rows have 16 through 65535 inputs in the P4 vector memory window
`0x40000000..0x4fffffff`, and a quantization exponent of at least -30.
Input and weight addresses can be unaligned. The dense operator bounds
absolute products, bias correction, left shift and output offset before
calling ESP-NN. Other rows use the checked shared-compute path. The
numerical contract remains integer double rounding with output publication
after every row succeeds. Convolution uses this adapter for eligible
patch reductions through the dense operator.

The hardware scope stores CFG, PIE enable/state and both hardware-loop
slots in the calling stack. It restores configuration and loops after the
vendor call; vector data registers are call-clobbered. Interrupt entry
saves CFG, uses aligned mode for context transfers and C handlers, and
restores the interrupted CFG on return. Application C code uses CFG zero;
nonzero CFG belongs inside a bounded assembly/library scope. This also
keeps the C library's vector copies in their required aligned mode.

Calls use caller-owned output and scratch; the adapter has no mutable
workspace or global lock. Concurrent callers disable compute test tracing
or serialize its counters. A worker build needs `TIKU_THREADS_ENABLE=1`.
The `ml-dense` category includes vendor arithmetic, live caller loops,
CFG preservation, timer interrupts and worker switches when ESP-NN is
selected. The `ml-cnn` category executes imported and generated mixed models.
