# tikukits/net/bluetooth/build.mk
#
# Generic Bluetooth Low Energy protocol stack (HCI / L2CAP / ATT /
# GATT / GAP / SMP). Driver-agnostic: pairs with whichever driver
# registers a tiku_bt_transport_t via tiku_bt_register_transport().
# Three transports ship today: the CYW43439's BTSDIO
# (drivers/wifi/cyw43/bt_transport.c), the ESP32-C61 controller's in-memory
# HCI (drivers/wifi/esp/esp_ble.c) and the EM9305's SPI-HCI
# (arch/ambiq/tiku_em9305.c).  The firmware Makefile sets TIKU_BT_HOST when
# the build has one of them.

ifeq ($(TIKU_BT_HOST),1)
SRCS += tikukits/net/bluetooth/tiku_bt.c
# SMP crypto for a controller without its own (TIKU_BT_SW_CRYPTO; the stack
# asks the controller first); SRCS is de-duplicated, so a build that lists
# the crypto kit already compiles each once.
ifeq ($(TIKU_BT_SW_CRYPTO),1)
SRCS += tikukits/crypto/aes128/tiku_kits_crypto_aes128.c
SRCS += tikukits/crypto/p256/tiku_kits_crypto_p256.c
endif
endif
