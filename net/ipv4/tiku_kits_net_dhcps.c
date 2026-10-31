/*
 * Tiku Operating System v0.06
 * Simple. Persistent. Intelligent.
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_dhcps.c - bounded DHCP offers and leases on a local AP.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tiku_kits_net_dhcps.h"
#include "tiku_kits_net_ipv4.h"
#include "tiku_kits_net_udp.h"
#include <tiku.h>
#include <kernel/timers/tiku_clock.h>
#include <string.h>
static const uint8_t server[4] = {192,168,4,1};
static const uint8_t cookie[4] = {99,130,83,99}, broadcast[4] = {255,255,255,255};
static uint8_t enabled, pending, route_enabled, reply[300];
static tiku_clock_time_t reply_until;
static struct {
    uint8_t owner[6], state;
    tiku_clock_time_t expires;
} leases[TIKU_KITS_NET_DHCPS_CLIENTS];
static tiku_kits_net_dhcps_send_t raw_send;
static void *raw_context;
/** @brief Release expired offers, leases and declined-address holds. */
static void expire(void)
{
    for (unsigned i = 0; i < TIKU_KITS_NET_DHCPS_CLIENTS; i++)
        if (leases[i].state && !TIKU_CLOCK_LT(tiku_clock_time(), leases[i].expires))
            memset(&leases[i], 0, sizeof leases[i]);
}
/** @brief Resolve a pool address to its lease slot. */
static int address_slot(const uint8_t *ip)
{
    return !memcmp(ip, server, 3) && ip[3] >= 2 &&
           ip[3] < 2 + TIKU_KITS_NET_DHCPS_CLIENTS ? ip[3] - 2 : -1;
}


/** @brief Parse bounded DHCP options and queue one response while retaining distinct leases. */
static void receive(const uint8_t *src, uint16_t port, const uint8_t *p, uint16_t n)
{
    (void)src;
    if (!enabled || pending || !p || port != 68 || n < 241 || p[0] != 1 ||
        p[1] != 1 || p[2] != 6 || p[3] || memcmp(p + 236, cookie, 4) ||
        memcmp(p + 24, (uint8_t[4]){0}, 4) || (p[28] & 1) ||
        !memcmp(p + 28, (uint8_t[6]){0}, 6)) return;
    uint8_t type = 0, end = 0;
    const uint8_t *requested = NULL, *selected = NULL;
    for (unsigned at = 240; at < n;) {
        unsigned tag = p[at++];
        if (!tag) continue;
        if (tag == 255) { end = 1; break; }
        if (at == n) return;
        unsigned len = p[at++];
        if (len > n - at) return;
        if (tag == 53) {
            if (len != 1 || type) return;
            type = p[at];
        } else if (tag == 50) {
            if (len != 4 || requested) return;
            requested = p + at;
        } else if (tag == 54) {
            if (len != 4 || selected) return;
            selected = p + at;
        } else if (tag == 52) return; /* Overloaded option areas are unsupported. */
        at += len;
    }
    if (!end || (selected && memcmp(selected, server, 4))) return;
    expire();
    int slot = -1;
    for (unsigned i = 0; i < TIKU_KITS_NET_DHCPS_CLIENTS; i++)
        if (leases[i].state && !memcmp(leases[i].owner, p + 28, 6)) slot = i;
    if (type == 7) {
        if (slot >= 0 && selected && address_slot(p + 12) == slot)
            memset(&leases[slot], 0, sizeof leases[slot]);
        return;
    }
    if (type == 4) {
        if (slot >= 0 && selected && requested && address_slot(requested) == slot) {
            memset(leases[slot].owner, 0, 6);
            leases[slot].state = 3;
            leases[slot].expires = tiku_clock_time() + 10 * TIKU_CLOCK_SECOND;
        }
        return;
    }
    if (type != 1 && type != 3) return;
    uint8_t answer = type == 1 ? 2 : 5;
    if (type == 3) {
        int wanted = address_slot(requested ? requested : p + 12);
        if (wanted < 0 || (slot >= 0 && wanted != slot) ||
            (leases[wanted].state && memcmp(leases[wanted].owner, p + 28, 6))) answer = 6;
        else slot = wanted;
    } else if (slot < 0) {
        for (unsigned i = 0; i < TIKU_KITS_NET_DHCPS_CLIENTS; i++)
            if (!leases[i].state) { slot = i; break; }
        if (slot < 0) return;
    }
    memset(reply, 0, sizeof reply);
    reply[0] = 2; reply[1] = 1; reply[2] = 6;
    memcpy(reply + 4, p + 4, 4);
    reply[10] = 0x80;
    memcpy(reply + 28, p + 28, 6);
    memcpy(reply + 236, cookie, 4);
    if (answer != 6) {
        memcpy(reply + 16, server, 4); reply[19] = slot + 2;
        memcpy(leases[slot].owner, p + 28, 6);
        if (answer == 5 || leases[slot].state != 2) {
            leases[slot].state = answer == 5 ? 2 : 1;
            leases[slot].expires = tiku_clock_time() + (answer == 2 ? 30 : 120) * TIKU_CLOCK_SECOND;
        }
    }
    unsigned at = 240;
    reply[at++] = 53; reply[at++] = 1; reply[at++] = answer;
    reply[at++] = 54; reply[at++] = 4;
    memcpy(reply + at, server, 4); at += 4;
    if (answer != 6) {
        reply[at++] = 1; reply[at++] = 4;
        memcpy(reply + at, (uint8_t[]){255,255,255,0}, 4); at += 4;
        reply[at++] = 51; reply[at++] = 4;
        memcpy(reply + at, (uint8_t[]){0,0,0,120}, 4); at += 4;
    }
    if (answer != 6 && route_enabled) {
        reply[at++] = 3; reply[at++] = 4;
        memcpy(reply + at, server, 4); at += 4;
        reply[at++] = 6; reply[at++] = 4;
        memcpy(reply + at, server, 4); at += 4;
    }
    reply[at] = 255;
    pending = 1;
    reply_until = tiku_clock_time() + 2 * TIKU_CLOCK_SECOND;
}
int tiku_kits_net_dhcps_start(void)
{
    if (enabled) return 0;
    int r = tiku_kits_net_udp_bind(67, receive);
    if (r) return r;
    enabled = 1;
    pending = 0;
    memset(leases, 0, sizeof leases);
    tiku_kits_net_ipv4_set_addr(server);
    return 0;
}
int tiku_kits_net_dhcps_start_raw(tiku_kits_net_dhcps_send_t send, void *ctx)
{
    if (!send || enabled) return -1;
    memset(leases, 0, sizeof leases);
    pending = 0; enabled = 2; raw_send = send; raw_context = ctx;
    return 0;
}
void tiku_kits_net_dhcps_route(uint8_t enable) { route_enabled = !!enable; }
void tiku_kits_net_dhcps_input(const uint8_t *p, uint16_t n)
{
    if (enabled == 2) receive(NULL, 68, p, n);
}
int tiku_kits_net_dhcps_owner(const uint8_t ip[4], const uint8_t mac[6])
{
    int slot = address_slot(ip);
    expire();
    return slot >= 0 && leases[slot].state == 2 && !memcmp(leases[slot].owner, mac, 6);
}
void tiku_kits_net_dhcps_stop(void)
{
    if (enabled == 1) (void)tiku_kits_net_udp_unbind(67);
    enabled = pending = route_enabled = 0;
    memset(reply, 0, sizeof reply);
    memset(leases, 0, sizeof leases);
    raw_send = NULL; raw_context = NULL;
}
void tiku_kits_net_dhcps_poll(void)
{
    if (pending && (!TIKU_CLOCK_LT(tiku_clock_time(), reply_until) ||
        !(enabled == 2 ? raw_send(reply, sizeof reply, raw_context) :
          tiku_kits_net_udp_send(broadcast, 68, 67, reply, sizeof reply)))) pending = 0;
    expire();
}
