# tikukits/net/bluetooth/build.mk
#
# Generic Bluetooth Low Energy protocol stack (HCI / L2CAP / ATT /
# GATT / GAP / SMP). Driver-agnostic: pairs with whichever driver
# registers a tiku_bt_transport_t via tiku_bt_register_transport().
# Today that's the CYW43439 BTSDIO transport
# (drivers/wifi/cyw43/bt_transport.c); a future UART-HCI driver for
# Nordic / ESP32 / TI parts would plug into the same vtable.
#
# Gated on TIKU_DRV_WIFI_CYW43_BT_ENABLE because that's the only
# transport shipping in public TikuOS today. When a second driver
# arrives (UART-HCI etc.) we'll widen the condition to an "any BT
# driver enabled" OR-chain.

ifeq ($(TIKU_DRV_WIFI_CYW43_BT_ENABLE),1)
SRCS += tikukits/net/bluetooth/tiku_bt.c
endif
