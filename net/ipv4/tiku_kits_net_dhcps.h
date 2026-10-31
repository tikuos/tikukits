/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_dhcps.h - Four-client DHCP service for a local access point.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TIKU_KITS_NET_DHCPS_H_
#define TIKU_KITS_NET_DHCPS_H_
#include <stdint.h>
#define TIKU_KITS_NET_DHCPS_CLIENTS 4u
typedef int (*tiku_kits_net_dhcps_send_t)(const uint8_t *, uint16_t, void *);
/** @brief Start without changing the IPv4 stack; callback sends broadcast UDP 67 to 68. */
int tiku_kits_net_dhcps_start_raw(tiku_kits_net_dhcps_send_t send, void *ctx);
/** @brief Advertise the AP as router and DNS server when enabled. */
void tiku_kits_net_dhcps_route(uint8_t enable);
/** @brief Consume a validated UDP payload from client port 68 in raw mode. */
void tiku_kits_net_dhcps_input(const uint8_t *p, uint16_t n);
/** @brief Whether a current bound lease belongs to this MAC. */
int tiku_kits_net_dhcps_owner(const uint8_t ip[4], const uint8_t mac[6]);
/** @brief Bind UDP 67 and assign 192.168.4.1/24 to the server; return 0 or a net error. */
int tiku_kits_net_dhcps_start(void);
/** @brief Release the server binding and discard pending replies and leases. */
void tiku_kits_net_dhcps_stop(void);
/** @brief Send a queued reply outside UDP receive context and expire the lease. */
void tiku_kits_net_dhcps_poll(void);
#endif
