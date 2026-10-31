/*
 * Tiku Operating System
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 * tiku_kits_net_router.c - AP DHCP, endpoint-dependent NAT and DNS forwarding.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tiku_kits_net_router.h"
#include "../ipv4/tiku_kits_net_dhcps.h"
#include <tiku.h>
#include <kernel/timers/tiku_clock.h>
#include <string.h>
static const uint8_t local[4] = {192,168,4,1};
static uint8_t running, enabled, up, ap_mac[6], uplink[4], resolver[4];
static uint32_t dropped;
static tiku_router_send_t send_station, send_ap;
static void *send_context;
#if defined(PLATFORM_ESP32P4)
#define ROUTER_BUFFER __attribute__((section(".p4.ipc"), aligned(64)))
#else
#define ROUTER_BUFFER
#endif
static struct {
    uint8_t protocol, ip[4], remote[4], mac[6], dns;
    uint16_t port, remote_port, external;
    tiku_clock_time_t expires;
} flows[TIKU_ROUTER_FLOWS] ROUTER_BUFFER;
static uint16_t next_port = TIKU_ROUTER_PORT_BASE;
static uint8_t packet[14 + TIKU_ROUTER_MTU] ROUTER_BUFFER;
static uint8_t busy;
/** @brief Read or write network-order halfwords in bounded packet fields. */
static unsigned get16(const uint8_t *p) { return (unsigned)p[0] << 8 | p[1]; }
static void put16(uint8_t *p, unsigned v) { p[0] = v >> 8; p[1] = v; }
/** @brief Accumulate an Internet checksum, including an odd final byte. */
static uint32_t sum(const uint8_t *p, unsigned n, uint32_t s)
{
    while (n > 1) { s += get16(p); p += 2; n -= 2; }
    if (n) s += (unsigned)*p << 8;
    return s;
}
static unsigned fold(uint32_t s)
{
    while (s >> 16) s = (s & 65535u) + (s >> 16);
    return (~s) & 65535u;
}
/** @brief Validate an ordinary unicast IPv4 address. */
static int unicast(const uint8_t *p)
{ return p[0] && p[0] != 127 && p[0] < 224 && p[3] != 255; }
/** @brief Compute a transport checksum with the IPv4 pseudoheader. */
static unsigned transport_sum(const uint8_t *ip, unsigned n)
{ return fold(sum(ip + 20, n - 20, sum(ip + 12, 8, ip[9] + n - 20))); }
/** @brief Recalculate checksums after address, port and TTL changes. */
static void checksums(uint8_t *ip, unsigned n)
{
    uint8_t *p = ip + 20;
    if (ip[9] == 1) { p[2] = p[3] = 0; put16(p + 2, fold(sum(p, n - 20, 0))); }
    else {
        unsigned offset = ip[9] == 6 ? 16 : 6;
        p[offset] = p[offset + 1] = 0;
        unsigned c = transport_sum(ip, n);
        put16(p + offset, c ? c : 65535);
    }
    ip[10] = ip[11] = 0; put16(ip + 10, fold(sum(ip, 20, 0)));
}
/** @brief Emit the current packet to an AP client. */
static int ap_output(const uint8_t *mac, unsigned n)
{
    memcpy(packet, mac, 6); memcpy(packet + 6, ap_mac, 6);
    put16(packet + 12, 0x0800);
    return send_ap(packet, n + 14, send_context);
}
/** @brief Form an IPv4/UDP broadcast carrying a deferred DHCP response. */
static int dhcp_output(const uint8_t *p, uint16_t n, void *unused)
{
    (void)unused;
    if (busy || n > TIKU_ROUTER_MTU - 28) return -1;
    busy = 1;
    uint8_t *ip = packet + 14;
    memset(ip, 0, 28); ip[0] = 0x45; ip[8] = 64; ip[9] = 17;
    put16(ip + 2, n + 28); memcpy(ip + 12, local, 4); memset(ip + 16, 255, 4);
    put16(ip + 20, 67); put16(ip + 22, 68); put16(ip + 24, n + 8);
    memcpy(ip + 28, p, n); checksums(ip, n + 28);
    int rc = ap_output((uint8_t[6]){255,255,255,255,255,255}, n + 28);
    busy = 0; return rc;
}
/** @brief Expire mappings before lookup or allocation. */
static void expire(void)
{
    for (unsigned i = 0; i < TIKU_ROUTER_FLOWS; i++)
        if (flows[i].protocol && (!TIKU_CLOCK_LT(tiku_clock_time(), flows[i].expires) ||
            !tiku_kits_net_dhcps_owner(flows[i].ip, flows[i].mac)))
            memset(&flows[i], 0, sizeof flows[i]);
}
int tiku_kits_net_router_start(const uint8_t mac[6], tiku_router_send_t station,
                              tiku_router_send_t ap, void *context)
{
    if (!mac || !station || !ap || running || (mac[0] & 1)) return -1;
    if (tiku_kits_net_dhcps_start_raw(dhcp_output, NULL)) return -1;
    memcpy(ap_mac, mac, 6); send_station = station; send_ap = ap; send_context = context;
    running = 1; up = 0; dropped = 0;
    memset(flows, 0, sizeof flows);
    tiku_kits_net_dhcps_route(enabled);
    return 0;
}
void tiku_kits_net_router_stop(void)
{
    if (running) tiku_kits_net_dhcps_stop();
    running = up = 0;
    memset(flows, 0, sizeof flows);
}
void tiku_kits_net_router_enable(uint8_t enable)
{
    enabled = !!enable;
    if (!enabled) memset(flows, 0, sizeof flows);
    if (running) tiku_kits_net_dhcps_route(enabled);
}
void tiku_kits_net_router_uplink(const uint8_t ip[4], const uint8_t dns[4])
{
    uint8_t valid = ip && unicast(ip) && memcmp(ip, local, 3);
    if (!valid || !up || memcmp(ip, uplink, 4) ||
        (dns && memcmp(dns, resolver, 4))) memset(flows, 0, sizeof flows);
    up = valid;
    if (up) memcpy(uplink, ip, 4); else memset(uplink, 0, 4);
    if (dns && unicast(dns) && memcmp(dns, local, 3)) memcpy(resolver, dns, 4);
    else memset(resolver, 0, 4);
}
/** @brief Answer ARP for the AP address without learning untrusted sender claims. */
static void arp(const uint8_t *f, unsigned n)
{
    if (n < 42 || memcmp(f + 14, (uint8_t[]){0,1,8,0,6,4,0,1}, 8) ||
        memcmp(f + 38, local, 4) || memcmp(f + 6, f + 22, 6)) return;
    memcpy(packet, f + 6, 6); memcpy(packet + 6, ap_mac, 6);
    put16(packet + 12, 0x0806);
    memcpy(packet + 14, f + 14, 28); put16(packet + 20, 2);
    memcpy(packet + 22, ap_mac, 6); memcpy(packet + 28, local, 4);
    memcpy(packet + 32, f + 22, 10);
    if (send_ap(packet, 42, send_context)) dropped++;
}
/** @brief Return time-exceeded to a leased AP sender with a quoted IPv4 header. */
static void ttl_expired(const uint8_t *f)
{
    uint8_t *ip = packet + 14;
    memset(ip, 0, 56); ip[0] = 0x45; ip[8] = 64; ip[9] = 1;
    put16(ip + 2, 56); memcpy(ip + 12, local, 4); memcpy(ip + 16, f + 26, 4);
    ip[20] = 11; memcpy(ip + 28, f + 14, 28); checksums(ip, 56);
    if (ap_output(f + 6, 56)) dropped++;
}
/** @brief Allocate an external port distinct from live mappings. */
static uint16_t allocate_port(void)
{
    for (unsigned tries = 0; tries <= TIKU_ROUTER_FLOWS; tries++) {
        uint16_t port = next_port++;
        if (next_port >= TIKU_ROUTER_PORT_BASE + 256) next_port = TIKU_ROUTER_PORT_BASE;
        unsigned i;
        for (i = 0; i < TIKU_ROUTER_FLOWS; i++)
            if (flows[i].protocol && flows[i].external == port) break;
        if (i == TIKU_ROUTER_FLOWS) return port;
    }
    return 0;
}
/** @brief Adjust one checksum word when a quoted transport segment is incomplete. */
static unsigned replace_word(unsigned checksum, unsigned old, unsigned value)
{ return fold((~checksum & 65535u) + (~old & 65535u) + value); }
/** @brief Translate ICMP errors whose quoted tuple matches a current mapping. */
static int inbound_error(const uint8_t *in, unsigned n)
{
    const uint8_t *q = in + 28;
    if (n < 56 || memcmp(in + 16, uplink, 4) || in[8] <= 1 ||
        !unicast(in + 12) || fold(sum(in + 20, n - 20, 0)) || q[0] != 0x45 ||
        get16(q + 2) < 28 || (get16(q + 6) & 0xbfff) || fold(sum(q, 20, 0)) ||
        memcmp(q + 12, uplink, 4)) return 0;
    unsigned protocol = q[9];
    if (protocol != 1 && protocol != 6 && protocol != 17) return 0;
    if (protocol == 1 && (q[20] != 8 || q[21])) return 0;
    unsigned external = get16(q + (protocol == 1 ? 24 : 20));
    unsigned remote_port = protocol == 1 ? 0 : get16(q + 22);
    expire();
    int slot = -1;
    for (unsigned i = 0; i < TIKU_ROUTER_FLOWS; i++)
        if (flows[i].protocol == protocol && flows[i].external == external &&
            flows[i].remote_port == remote_port && !memcmp(flows[i].remote, q + 16, 4) &&
            tiku_kits_net_dhcps_owner(flows[i].ip, flows[i].mac)) { slot = i; break; }
    if (slot < 0) return 0;
    memcpy(packet + 14, in, n);
    uint8_t *ip = packet + 14, *quote = ip + 28;
    unsigned offset = protocol == 6 ? 36 : protocol == 17 ? 26 : 22;
    if (n >= 28 + offset + 2 && (protocol != 17 || get16(quote + offset))) {
        unsigned c = get16(quote + offset);
        c = replace_word(c, external, flows[slot].port);
        if (protocol != 1) {
            c = replace_word(c, get16(quote + 12), get16(flows[slot].ip));
            c = replace_word(c, get16(quote + 14), get16(flows[slot].ip + 2));
            if (flows[slot].dns) {
                c = replace_word(c, get16(quote + 16), get16(local));
                c = replace_word(c, get16(quote + 18), get16(local + 2));
            }
        }
        put16(quote + offset, protocol == 17 && !c ? 65535 : c);
    }
    memcpy(quote + 12, flows[slot].ip, 4);
    if (flows[slot].dns) memcpy(quote + 16, local, 4);
    put16(quote + (protocol == 1 ? 24 : 20), flows[slot].port);
    quote[10] = quote[11] = 0; put16(quote + 10, fold(sum(quote, 20, 0)));
    memcpy(ip + 16, flows[slot].ip, 4); ip[8]--; checksums(ip, n);
    if (ap_output(flows[slot].mac, n)) dropped++;
    return 1;
}
int tiku_kits_net_router_input(uint8_t interface, const uint8_t *f, uint16_t length)
{
    if (!running || interface > 1 || !f || length < 14) return 0;
    if (!interface && get16(f + 12) != 0x0800) return 0;
    if (busy) { dropped++; return interface != 0; }
    busy = 1;
    int consumed = interface != 0;
    if (interface && (!(f[6] & 1)) && get16(f + 12) == 0x0806) {
        arp(f, length); goto done;
    }
    if (get16(f + 12) != 0x0800 || length < 42) goto drop;
    const uint8_t *in = f + 14, *payload = in + 20;
    unsigned n = get16(in + 2), protocol = in[9];
    if (in[0] != 0x45 || n < 28 || n > TIKU_ROUTER_MTU || n > length - 14u ||
        (get16(in + 6) & 0xbfff) || fold(sum(in, 20, 0))) goto drop;
    if (!interface && enabled && up && protocol == 1 &&
        ((payload[0] == 3 && payload[1] <= 15) || (payload[0] == 11 && payload[1] <= 1) ||
         (payload[0] == 12 && payload[1] <= 2))) {
        consumed = inbound_error(in, n); goto done;
    }
    if (protocol == 17) {
        if (get16(payload + 4) != n - 20 ||
            (get16(payload + 6) && transport_sum(in, n))) goto drop;
    } else if (protocol == 6) {
        if (n < 40 || (payload[12] >> 4) < 5 || (unsigned)(payload[12] >> 4) * 4 > n - 20 ||
            transport_sum(in, n)) goto drop;
    } else if (protocol == 1) {
        if (payload[1] || (payload[0] != 8 && payload[0] != 0) || fold(sum(payload, n - 20, 0))) goto drop;
    } else goto drop;
    if (interface && protocol == 17 && get16(payload) == 68 && get16(payload + 2) == 67) {
        if (n >= 28 + 34 && !memcmp(payload + 8 + 28, f + 6, 6))
            tiku_kits_net_dhcps_input(payload + 8, n - 28);
        goto done;
    }
    if (interface && protocol == 1 && payload[0] == 8 && !memcmp(in + 16, local, 4) &&
        tiku_kits_net_dhcps_owner(in + 12, f + 6)) {
        memcpy(packet + 14, in, n);
        uint8_t *ip = packet + 14;
        memcpy(ip + 16, in + 12, 4); memcpy(ip + 12, local, 4);
        ip[8] = 64; ip[20] = 0; checksums(ip, n);
        if (ap_output(f + 6, n)) dropped++;
        goto done;
    }
    if (!enabled || !up) goto drop;
    expire();
    int slot = -1;
    unsigned source_port = protocol == 1 ? get16(payload + 4) : get16(payload);
    unsigned dest_port = protocol == 1 ? 0 : get16(payload + 2);
    uint8_t dns = interface && protocol == 17 && dest_port == 53 && !memcmp(in + 16, local, 4);
    const uint8_t *remote = dns ? resolver : in + (interface ? 16 : 12);
    if (interface) {
        if (!tiku_kits_net_dhcps_owner(in + 12, f + 6) || !unicast(remote) ||
            !memcmp(remote, local, 3) || !memcmp(remote, uplink, 4) ||
            (protocol == 1 && payload[0] != 8) ||
            (dns && (n < 40 || (payload[10] & 0xf8)))) goto drop;
        if (in[8] <= 1) { ttl_expired(f); goto done; }
        for (unsigned i = 0; i < TIKU_ROUTER_FLOWS; i++)
            if (flows[i].protocol == protocol && flows[i].port == source_port &&
                flows[i].remote_port == dest_port && flows[i].dns == dns &&
                !memcmp(flows[i].ip, in + 12, 4) && !memcmp(flows[i].remote, remote, 4) &&
                !memcmp(flows[i].mac, f + 6, 6)) { slot = i; break; }
        if (slot < 0) {
            if (protocol == 6 && (payload[13] & 0x17) != 2) goto drop;
            for (unsigned i = 0; i < TIKU_ROUTER_FLOWS; i++)
                if (!flows[i].protocol) { slot = i; break; }
            if (slot < 0) goto drop;
            flows[slot].external = allocate_port();
            if (!flows[slot].external) goto drop;
            flows[slot].protocol = protocol; flows[slot].port = source_port;
            flows[slot].remote_port = dest_port; flows[slot].dns = dns;
            memcpy(flows[slot].ip, in + 12, 4); memcpy(flows[slot].mac, f + 6, 6);
            memcpy(flows[slot].remote, remote, 4);
        }
    } else {
        if (memcmp(in + 16, uplink, 4)) goto done;
        unsigned external = protocol == 1 ? source_port : dest_port;
        if (external < TIKU_ROUTER_PORT_BASE || external >= TIKU_ROUTER_PORT_BASE + 256) goto done;
        consumed = 1;
        for (unsigned i = 0; i < TIKU_ROUTER_FLOWS; i++)
            if (flows[i].protocol == protocol && flows[i].external == external &&
                (protocol == 1 ? payload[0] == 0 : flows[i].remote_port == source_port) &&
                !memcmp(flows[i].remote, remote, 4) &&
                tiku_kits_net_dhcps_owner(flows[i].ip, flows[i].mac)) { slot = i; break; }
        if (slot < 0 || in[8] <= 1) goto drop;
    }
    memcpy(packet + 14, in, n);
    uint8_t *ip = packet + 14, *out = ip + 20;
    ip[8]--;
    if (interface) {
        memcpy(ip + 12, uplink, 4);
        if (dns) memcpy(ip + 16, resolver, 4);
        put16(out + (protocol == 1 ? 4 : 0), flows[slot].external);
    } else {
        memcpy(ip + 16, flows[slot].ip, 4);
        if (flows[slot].dns) memcpy(ip + 12, local, 4);
        put16(out + (protocol == 1 ? 4 : 2), flows[slot].port);
    }
    checksums(ip, n);
    flows[slot].expires = tiku_clock_time() + (protocol == 6 ? 300 : 60) * TIKU_CLOCK_SECOND;
    if (interface ? send_station(ip, n, send_context) : ap_output(flows[slot].mac, n)) dropped++;
    if (protocol == 6 && (out[13] & 5)) flows[slot].expires = tiku_clock_time() + 10 * TIKU_CLOCK_SECOND;
    goto done;
drop:
    if (consumed) dropped++;
done:
    busy = 0; return consumed;
}
void tiku_kits_net_router_poll(void)
{
    if (running) { expire(); tiku_kits_net_dhcps_poll(); }
}
void tiku_kits_net_router_status(uint8_t *on, uint8_t *usable, uint8_t *count, uint32_t *drops)
{
    unsigned n = 0; expire();
    for (unsigned i = 0; i < TIKU_ROUTER_FLOWS; i++) n += flows[i].protocol != 0;
    if (on) *on = enabled;
    if (usable) *usable = running && up;
    if (count) *count = n;
    if (drops) *drops = dropped;
}
