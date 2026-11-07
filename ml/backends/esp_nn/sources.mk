# ESP-NN v1.4.2 is supplied as an external, unmodified checkout.
ifeq ($(strip $(ESP_NN_DIR)),)
$(error Set ESP_NN_DIR to the ESP-NN v1.4.2 checkout)
endif
ESP_NN_REV := $(shell git -C "$(ESP_NN_DIR)" rev-parse HEAD 2>/dev/null)
ifneq ($(ESP_NN_REV),744d48deae6aa4a4d7b93b6a80f548d5298813a4)
$(error ESP_NN_DIR must contain ESP-NN v1.4.2 at 744d48deae6aa4a4d7b93b6a80f548d5298813a4)
endif
ifneq ($(shell git -C "$(ESP_NN_DIR)" diff --quiet HEAD -- src/fully_connected/esp_nn_fully_connected_s8_riscv_pie.c src/common; echo $$?),0)
$(error ESP-NN dense sources must match the pinned revision)
endif
ESP_NN_FC := tikukits/ml/backends/esp_nn/tiku_kits_ml_esp_kernel.c
SRCS += tikukits/ml/backends/esp_nn/tiku_kits_ml_esp_nn.c $(ESP_NN_FC)
CFLAGS += -DTIKU_KITS_ML_ESP_NN=1

$(BUILD_DIR)/$(ESP_NN_FC:.c=.o): $(ESP_NN_FC) $(ESP_NN_DIR)/src/fully_connected/esp_nn_fully_connected_s8_riscv_pie.c $(wildcard $(ESP_NN_DIR)/src/common/*.h) tikukits/ml/backends/esp_nn/translate.py arch/esp32p4/pie/tiku_pie.inc arch/esp32p4/pie/tiku_hwloop.inc
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DCONFIG_IDF_TARGET_ESP32P4=1 -I$(ESP_NN_DIR)/src/common -I$(ESP_NN_DIR)/src/fully_connected -Wno-unused-function -Wno-old-style-declaration -MMD -MP -MT $@ -MF $(@:.o=.d) -S $< -o $(@:.o=.vendor.s)
	python3 tikukits/ml/backends/esp_nn/translate.py --root $(PROJ_DIR) $(@:.o=.vendor.s) $(@:.o=.s)
	$(CC) $(CFLAGS) -c $(@:.o=.s) -o $@
