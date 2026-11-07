# Optional, pinned CMSIS-NN dense adapter. Dependencies are supplied locally.
ifeq ($(strip $(CMSIS_NN_DIR)),)
$(error Set CMSIS_NN_DIR after git clone --branch v8.0.0 --depth 1 https://github.com/ARM-software/CMSIS-NN.git)
endif
CMSIS_NN_REV := $(shell git -C "$(CMSIS_NN_DIR)" rev-parse HEAD 2>/dev/null)
ifneq ($(CMSIS_NN_REV),13c97dbb6f781d4aab38ed34e6e441f42b79aff4)
$(error CMSIS_NN_DIR must contain CMSIS-NN v8.0.0 at 13c97dbb6f781d4aab38ed34e6e441f42b79aff4)
endif
SRCS += tikukits/ml/backends/cmsis_nn/tiku_kits_ml_cmsis_nn.c
SRCS += $(CMSIS_NN_DIR)/Source/FullyConnectedFunctions/arm_fully_connected_s8.c
SRCS += $(CMSIS_NN_DIR)/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s8.c
CFLAGS += -DTIKU_KITS_ML_CMSIS_NN=1 -I$(CMSIS_NN_DIR)/Include
