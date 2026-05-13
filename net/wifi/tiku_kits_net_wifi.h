/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_wifi.h - WiFi link backend for tikukits/net
 *
 * Wraps the CYW43439 WHD driver (drivers/wifi/cyw43) so the IPv4
 * stack in tikukits/net/ipv4/ can use WiFi as its link layer in
 * place of (or alongside) SLIP.
 *
 * Same shape as tikukits/net/slip/: this module exports a
 * `tiku_kits_net_link_t` struct that the IPv4 layer installs via
 * `tiku_kits_net_ipv4_set_link(&tiku_kits_net_wifi_link)`.
 *
 * RESPONSIBILITIES
 *   - Ethernet framing: IP packet in / EthII frame out (and back).
 *     Source MAC comes from the radio (tiku_wireless_status).
 *     Destination MAC for v1 = broadcast (ff:ff:ff:ff:ff:ff); this is
 *     enough for DHCP DISCOVER, which is the bootstrap traffic that
 *     unblocks every other v4 protocol. Unicast routing (ARP cache,
 *     gateway-MAC resolution) is phase 5.A.1.
 *   - RX buffering: the WHD driver's RX callback fires from runner
 *     context with a borrowed frame pointer. This adapter copies the
 *     IP payload into a staging buffer that the kit's polling RX
 *     path consumes.
 *   - Optional ARP reply: when an incoming ARP request asks for the
 *     IP the kit considers "ours", we send back an ARP reply so
 *     other devices on the LAN can reach us.
 *
 * BUILD GATE
 *   Compiled only when both submodules are present:
 *     TIKU_DRV_WIFI_CYW43_ENABLE=1  (driver)
 *     TIKU_KITS_NET_WIFI_ENABLE=1   (this adapter)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_NET_WIFI_H_
#define TIKU_KITS_NET_WIFI_H_

#include "../tiku_kits_net.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Pre-built link backend.  Pass to
 *        tiku_kits_net_ipv4_set_link() once the radio is joined.
 *
 * Lifetime: static (lives forever once the binary is loaded). Safe
 * to install before WHD has finished joining — send/poll_rx will
 * report "no link" until the radio is up.
 */
extern const tiku_kits_net_link_t tiku_kits_net_wifi_link;

/**
 * @brief Initialise the WiFi link backend.
 *
 * Registers the adapter's RX callback with the WHD driver and
 * installs `tiku_kits_net_wifi_link` as the active IPv4 link.
 * Idempotent — safe to call again after a wifi disconnect/reconnect.
 *
 * Pre-conditions:
 *   - drivers/wifi/cyw43/ initialised (cyw43_runner started)
 *   - tikukits/net/ipv4 initialised (net_proc running)
 * Joined state is NOT required to call this — the adapter only
 * succeeds at send/poll_rx once `tiku_wireless_status().up == 1`.
 *
 * @return TIKU_KITS_NET_OK on success, TIKU_KITS_NET_ERR_NOLINK if
 *         the driver-side RX callback could not be installed.
 */
int8_t tiku_kits_net_wifi_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TIKU_KITS_NET_WIFI_H_ */
