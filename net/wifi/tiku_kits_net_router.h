/*
 * Tiku Operating System
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 * tiku_kits_net_router.h - Bounded IPv4 translation between AP and station.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TIKU_KITS_NET_ROUTER_H_
#define TIKU_KITS_NET_ROUTER_H_
#include <stdint.h>
#define TIKU_ROUTER_MTU 1500u
#define TIKU_ROUTER_FLOWS 16u
#define TIKU_ROUTER_PORT_BASE 61000u
/** @brief Transmit a station IPv4 packet or AP Ethernet frame, copying before return. */
typedef int (*tiku_router_send_t)(const uint8_t *, uint16_t, void *);
/** @brief Start AP DHCP at 192.168.4.1/24; return zero on success. */
int tiku_kits_net_router_start(const uint8_t mac[6], tiku_router_send_t station,
                              tiku_router_send_t ap, void *context);
/** @brief Stop AP DHCP and erase active translations. */
void tiku_kits_net_router_stop(void);
/** @brief Change uplink IPv4/DNS addresses; NULL removes the uplink and its mappings. */
void tiku_kits_net_router_uplink(const uint8_t ip[4], const uint8_t dns[4]);
/** @brief Consume AP Ethernet or matched inbound translations; return one when consumed. */
int tiku_kits_net_router_input(uint8_t interface, const uint8_t *frame, uint16_t length);
/** @brief Expire translations and transmit deferred DHCP responses. */
void tiku_kits_net_router_poll(void);
/** @brief Enable forwarding and DNS; DHCP remains available when disabled. */
void tiku_kits_net_router_enable(uint8_t enable);
/** @brief Read forwarding intent, usable uplink, live mappings and dropped packets. */
void tiku_kits_net_router_status(uint8_t *enabled, uint8_t *up, uint8_t *flows, uint32_t *drops);
#endif
