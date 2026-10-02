# tikukits/net/bluetooth/build.mk
#
# Generic Bluetooth Low Energy protocol stack (HCI / L2CAP / ATT /
# GATT / GAP / SMP). Driver-agnostic: pairs with whichever driver
# registers a tiku_bt_transport_t via tiku_bt_register_transport().
# Two transports ship today: the CYW43439's BTSDIO
# (drivers/wifi/cyw43/bt_transport.c, TIKU_DRV_WIFI_CYW43_BT_ENABLE) and
# the ESP32-C61 controller's in-memory HCI (drivers/wifi/esp/esp_ble.c,
# TIKU_DRV_BLE_ESP_ENABLE).

ifneq ($(filter 1,$(TIKU_DRV_WIFI_CYW43_BT_ENABLE) $(TIKU_DRV_BLE_ESP_ENABLE)),)
SRCS += tikukits/net/bluetooth/tiku_bt.c
endif
