/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_bt.c - generic Bluetooth Low Energy protocol stack
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Layered model:
 *
 * tiku_bt.c (this file)        HCI / L2CAP / ATT / GATT / GAP / SMP
 * ^
 * |  tiku_bt_transport_t vtable (send / recv / is_ready)
 * v
 * driver-provided transport    e.g. drivers/wifi/cyw43/bt_transport.c
 * (BTSDIO over CYW43439 backplane),
 * or a UART-HCI driver for Nordic /
 * ESP32 / TI parts.
 *
 * This file owns everything above the transport: the HCI command/event
 * machinery, L2CAP channel demux (ATT on CID 4, SMP on CID 6), the ATT
 * server and minimal client, the GATT service registry + notification
 * push, the GAP advertising / scanning / connection control plane, and
 * the SMP "not supported" stub. No chip register accesses, no firmware
 * upload, no ring buffer arithmetic -- those belong to the transport.
 *
 * The transport is selected at runtime: the driver registers its
 * @ref tiku_bt_transport_t vtable via tiku_bt_register_transport(),
 * then calls tiku_bt_init() once the chip-side bring-up is complete.
 */

#include <interfaces/bluetooth/tiku_bt.h>
#include <interfaces/bluetooth/tiku_bt_transport.h>
#include "tiku.h"
#include <kernel/cpu/tiku_common.h>
#include <kernel/memory/tiku_mem.h>
#include <kernel/process/tiku_process.h>
#include <kernel/timers/tiku_clock.h>
#include <kernel/timers/tiku_timer.h>

#if PLATFORM_RP2350
/* TRNG access for Phase 14 SMP work (ECDH ephemeral keys, Pairing
 * Random nonces). Wrapped in a platform guard because the API is
 * still arch-specific; a kernel-level wrapper (tiku_trng_*) is a
 * follow-up. */
#include <arch/arm-rp2350/tiku_trng_arch.h>
#endif

/* Driver-side BTFW version string. The blob header lives in the
 * transport translation unit (it parses the .S-baked image), so the
 * generic stack's tiku_bt_fw_version() forwards to this getter.
 * Longer term the vtable could grow a get_fw_version member. */
extern const char *cyw43_bt_fw_version(void);

#ifndef TIKU_BT_PRINTF
#define TIKU_BT_PRINTF(...) TIKU_PRINTF("[bt] " __VA_ARGS__)
#endif

/*---------------------------------------------------------------------------*/
/* BT runner process (forward declaration)                                   */
/*---------------------------------------------------------------------------*/
/* The BT stack runs on its own protothread, separate from the WHD WiFi
 * runner, so the two driver layers stay decoupled. The
 * runner ticks at ~128 Hz while BT is active (scanning, advertising,
 * or has an open connection) and YIELDs to idle otherwise. Wake
 * events posted via bt_wake_runner() re-dispatch it after state
 * flips (scan on/off, advertise on/off, connect/disconnect). */
TIKU_PROCESS(tiku_bt_runner, "bt");

/*---------------------------------------------------------------------------*/
/* Transport registry (a driver registers its vtable via                     */
/* tiku_bt_register_transport; this layer calls through it for every         */
/* HCI packet send/recv)                                                     */
/*---------------------------------------------------------------------------*/

static const tiku_bt_transport_t *g_bt_transport;

int tiku_bt_register_transport(const tiku_bt_transport_t *t)
{
    if (t == (const tiku_bt_transport_t *)0) return TIKU_DRV_ERR_INVALID;
    if (t->send == 0 || t->recv == 0 || t->is_ready == 0) {
        return TIKU_DRV_ERR_INVALID;
    }
    g_bt_transport = t;
    return TIKU_DRV_OK;
}

const tiku_bt_transport_t *tiku_bt_get_transport(void)
{
    return g_bt_transport;
}

/*---------------------------------------------------------------------------*/
/* HCI OPCODES                                                               */
/*---------------------------------------------------------------------------*/
/* Format: opcode = (OGF << 10) | OCF. OGF=0x03 is "Controller and
 * Baseband" (Reset etc.), 0x04 is "Informational Parameters" (chip
 * identity), 0x08 is "LE Controller". The OCF lower bits match the
 * Bluetooth Core Spec command tables. */
#define HCI_OP_DISCONNECT               0x0406U  /* OGF=0x01 (link ctrl) */
#define HCI_OP_RESET                    0x0C03U
#define HCI_OP_READ_LOCAL_VERSION       0x1001U
#define HCI_OP_READ_BD_ADDR             0x1009U
#define HCI_OP_LE_SET_ADV_PARAMS        0x2006U
#define HCI_OP_LE_SET_ADV_DATA          0x2008U
#define HCI_OP_LE_SET_ADV_ENABLE        0x200AU
#define HCI_OP_LE_SET_SCAN_PARAMS       0x200BU
#define HCI_OP_LE_SET_SCAN_ENABLE       0x200CU
#define HCI_OP_LE_CREATE_CONNECTION     0x200DU
/* Phase 14: SMP crypto offload + encryption key management. The chip
 * has a dedicated AES-128 + P-256 ECDH engine reachable via these LE
 * controller HCI commands, which avoid a software AES/ECDH
 * port (which would dwarf the rest of the stack on Cortex-M33). */
#define HCI_OP_LE_ENCRYPT                       0x2017U
#define HCI_OP_LE_LTK_REQUEST_REPLY             0x201AU
#define HCI_OP_LE_LTK_REQUEST_NEGATIVE_REPLY    0x201BU
#define HCI_OP_LE_READ_LOCAL_P256_PUBKEY        0x2025U
#define HCI_OP_LE_GENERATE_DHKEY_V2             0x205EU

/* HCI Event codes. */
#define HCI_EVT_DISCONNECTION_COMPLETE  0x05U
#define HCI_EVT_ENCRYPTION_CHANGE       0x08U
#define HCI_EVT_COMMAND_COMPLETE        0x0EU
#define HCI_EVT_COMMAND_STATUS          0x0FU
#define HCI_EVT_LE_META                 0x3EU
#define HCI_EVT_ENCRYPTION_KEY_REFRESH_COMPLETE 0x30U

/* HCI packet types (byte 0 returned by tiku_bt_recv). */
#define HCI_PKT_TYPE_CMD                0x01U
#define HCI_PKT_TYPE_ACL                0x02U
#define HCI_PKT_TYPE_EVENT              0x04U

/* LE Meta subevent codes (event[3] in an HCI_EVT_LE_META frame). */
#define LE_SUBEVT_CONNECTION_COMPLETE   0x01U
#define LE_SUBEVT_ADVERTISING_REPORT    0x02U
/* Phase 14: encryption + P-256 crypto offload async results. */
#define LE_SUBEVT_LONG_TERM_KEY_REQUEST         0x05U
#define LE_SUBEVT_READ_LOCAL_P256_PUBKEY_CPL    0x08U
#define LE_SUBEVT_GENERATE_DHKEY_COMPLETE       0x09U

/* L2CAP fixed-channel IDs (Core Spec Vol 3 Part A 2.1). */
#define L2CAP_CID_ATT                   0x0004U
#define L2CAP_CID_LE_SIGNALING          0x0005U
#define L2CAP_CID_SMP                   0x0006U

/* ATT opcodes (subset; full list in Core Spec Vol 3 Part F 3.4). */
#define ATT_OP_ERROR_RSP                0x01U
#define ATT_OP_EXCHANGE_MTU_REQ         0x02U
#define ATT_OP_EXCHANGE_MTU_RSP         0x03U
#define ATT_OP_FIND_INFORMATION_REQ     0x04U
#define ATT_OP_FIND_INFORMATION_RSP     0x05U
#define ATT_OP_READ_BY_TYPE_REQ         0x08U
#define ATT_OP_READ_BY_TYPE_RSP         0x09U
#define ATT_OP_READ_REQ                 0x0AU
#define ATT_OP_READ_RSP                 0x0BU
#define ATT_OP_READ_BY_GROUP_TYPE_REQ   0x10U
#define ATT_OP_READ_BY_GROUP_TYPE_RSP   0x11U
#define ATT_OP_WRITE_REQ                0x12U
#define ATT_OP_WRITE_RSP                0x13U
#define ATT_OP_HANDLE_VALUE_NOTIFY      0x1BU

/* ATT error codes used in Error Response (0x01) PDUs. */
#define ATT_ERR_INVALID_HANDLE          0x01U
#define ATT_ERR_READ_NOT_PERMITTED      0x02U
#define ATT_ERR_WRITE_NOT_PERMITTED     0x03U
#define ATT_ERR_REQUEST_NOT_SUPPORTED   0x06U
#define ATT_ERR_ATTRIBUTE_NOT_FOUND     0x0AU

/* SMP opcodes + Pairing Failed reason codes (Core Spec Vol 3 Part H 3.5).
 * Phase 14 added LE Secure Connections Just-Works (peripheral role):
 * Pairing Request → Response → Public Key swap → Confirm + Random
 * exchange → DHKey Check pair, then encryption start via LE LTK
 * Request. Security Request (peripheral→central) is still ignored
 * because this stack is typically the peripheral. */
#define SMP_OP_PAIRING_REQUEST          0x01U
#define SMP_OP_PAIRING_RESPONSE         0x02U
#define SMP_OP_PAIRING_CONFIRM          0x03U
#define SMP_OP_PAIRING_RANDOM           0x04U
#define SMP_OP_PAIRING_FAILED           0x05U
#define SMP_OP_PAIRING_PUBLIC_KEY       0x0CU
#define SMP_OP_PAIRING_DHKEY_CHECK      0x0DU
#define SMP_OP_SECURITY_REQUEST         0x0BU

#define SMP_ERR_PAIRING_NOT_SUPPORTED   0x05U
#define SMP_ERR_DHKEY_CHECK_FAILED      0x0BU
#define SMP_ERR_UNSPECIFIED_REASON      0x08U

/* AuthReq bits (Pairing Request/Response octet 3). */
#define SMP_AUTHREQ_BONDING_MASK        0x03U
#define SMP_AUTHREQ_MITM                0x04U
#define SMP_AUTHREQ_SC                  0x08U

/* SMP session lifecycle (Phase 14 Just-Works LE-SC, peripheral side).
 * Each peripheral session walks: WAITING_PUBKEY (after sending Pairing
 * Response) → WAITING_CONFIRM (after receiving the peer pubkey -- but in
 * Just-Works the peer waits for the local confirm, so this state is brief
 * on peripheral) → WAITING_RANDOM (after sending that confirm) →
 * WAITING_DHCHECK (after exchanging randoms) → ENCRYPTING (LTK
 * Request reply sent) → ENCRYPTED (encryption-change OK). The IDLE
 * state is the "no pairing in flight" terminal state and lets the
 * bond_save -> link-encryption restart cleanly. */
typedef enum {
    SMP_IDLE          = 0U,
    SMP_WAITING_PUBKEY,
    SMP_WAITING_CONFIRM,
    SMP_WAITING_RANDOM,
    SMP_WAITING_DHCHECK,
    SMP_ENCRYPTING,
    SMP_ENCRYPTED,
} smp_state_t;

/* Bluetooth SIG-assigned 16-bit UUIDs used in the minimal table. */
#define UUID_PRIMARY_SERVICE            0x2800U
#define UUID_CHARACTERISTIC             0x2803U
#define UUID_CCCD                       0x2902U
#define UUID_GAP_SERVICE                0x1800U
#define UUID_GATT_SERVICE               0x1801U
#define UUID_DEVICE_NAME                0x2A00U
#define UUID_APPEARANCE                 0x2A01U

/* Demo "Tiku Stats" service + uptime characteristic. Vendor-allocated
 * 16-bit UUIDs in the 0xF000+ space (reserved for vendor use per
 * Bluetooth SIG assigned-numbers convention). Real production
 * services should use a 128-bit UUID, which Phase 11.x will add. */
#define UUID_TIKU_STATS_SERVICE         0xF100U
#define UUID_TIKU_UPTIME_CHAR           0xF101U

/* Characteristic property bits (Core Spec Vol 3 Part G 3.3.1.1). */
#define ATT_PROP_READ                   0x02U

/* Default ATT MTU per spec (Core Spec Vol 3 Part F 3.4.2). The chip's
 * BT firmware may permit larger via Exchange MTU; this caps at 23 for
 * the demo because that's all the GAP service replies need to fit. */
#define ATT_MTU_DEFAULT                 23U

/* AD record types (from "Generic Access Profile" assigned numbers). */
#define AD_TYPE_FLAGS                   0x01U
#define AD_TYPE_INCOMPLETE_LOCAL_NAME   0x08U
#define AD_TYPE_COMPLETE_LOCAL_NAME     0x09U

/* AD Flags value sent in the advertising data:
 *  bit 1 = LE General Discoverable Mode
 *  bit 2 = BR/EDR Not Supported (LE-only device) */
#define AD_FLAGS_LE_GENERAL_DISC        0x06U

/*---------------------------------------------------------------------------*/
/* Module-scope scratch buffer                                               */
/*---------------------------------------------------------------------------*/
/*
 * HCI command construction (1 type byte + 3-byte header + up to 255
 * bytes of params) needs a ~260-byte buffer. Keeping it as a local
 * would inflate the runner / shell stack frames; making it file-scope
 * keeps the stack frames small at the cost of non-reentrancy. Safe
 * under TikuOS's cooperative scheduler because:
 *
 *   - bt_hci_cmd_response never PT_YIELDs inside;
 *   - only one BT-side caller is active at a time (the shell process
 *     when running `bt scan / advertise / status`, or the runner
 *     during bring-up + tiku_bt_poll).
 *
 * The buffer is allocated from a tiku_arena (id=0xB1) at init time
 * rather than living as a static .bss array, so `ps` / region
 * listings see BT's memory budget rather than the buffer being an
 * invisible static. The transport has its own arena (id=0xB2) for
 * its ring-staging scratch.
 *
 * If a future caller needs to invoke this from an ISR or a second
 * process, switch to a per-call arena allocation.
 */
#define BT_SCRATCH_CMD_SIZE   260U
#define BT_ARENA_BYTES        (BT_SCRATCH_CMD_SIZE + 32U /* alignment slack */)

static uint8_t       bt_arena_buf[BT_ARENA_BYTES] __attribute__((aligned(4)));
static tiku_arena_t  bt_arena;
static uint8_t      *bt_scratch_cmd;

static struct {
    uint8_t  ready;         /* 1 after tiku_bt_init() completes */

    /* Cached chip identity (filled by phase 6.D queries). bd_addr[0..5]
     * being all zero is taken as "identity not yet cached" since real
     * BD_ADDRs from any vendor have at least one non-zero byte (the
     * OUI). Avoids an extra info_ready flag. */
    uint8_t                 bd_addr[6];       /* MSB-first print order */
    tiku_bt_version_t version;

    /* Phase 7 — GAP advertising state. The name is cached because
     * the ATT layer needs to serve it back when a peer reads the
     * GAP service's Device Name characteristic (handle 0x0003). */
    uint8_t  advertising;
    uint8_t  adv_name_len;
    char     adv_name[TIKU_BT_ADV_NAME_MAX];

    /* Phase 8 — GAP scanning state + dedup'd results cache. */
    uint8_t                    scanning;
    uint8_t                    scan_count;
    tiku_bt_scan_entry_t scan[TIKU_BT_SCAN_MAX];

    /* Phase 9 — active LE connections. `in_use` is 0 for free slots,
     * 1 for live links. ATT context (current_mtu) is per-connection
     * because Exchange MTU only affects the requester's link. */
    struct {
        uint8_t                    in_use;
        tiku_bt_connection_t info;
        uint16_t                   att_mtu;
    } conns[TIKU_BT_CONN_MAX];

    /* Phase 11 — user-registered services. user_svc_count grows on
     * tiku_bt_register_service() and is capped at
     * TIKU_BT_SVC_MAX. user_svc[k] points to caller-owned
     * storage; callers must keep the tiku_bt_service_t alive. */
    uint8_t                              user_svc_count;
    const tiku_bt_service_t       *user_svc[TIKU_BT_SVC_MAX];

    /* Phase 12 — per-char CCCD state. The CCCD value (2 bytes:
     * bit 0 = notify, bit 1 = indicate) is conceptually per
     * {char, connection} but the legacy GATT model keeps it as
     * per-char because most clients only subscribe on one link.
     * cccd_char_uuid[i] identifies which char each slot belongs to;
     * cccd_value[i] holds the current bits. */
    uint8_t   cccd_count;
    uint16_t  cccd_char_uuid[TIKU_BT_CHAR_MAX];
    uint16_t  cccd_value[TIKU_BT_CHAR_MAX];

    /* Per-char Characteristic Declaration value scratch (5 bytes:
     * props + value_handle_lo + value_handle_hi + uuid_lo + uuid_hi).
     * Filled at snapshot time because value_handle depends on the
     * order of registration; can't be a static const. */
    uint8_t   char_decl_buf[TIKU_BT_CHAR_MAX][5];

    /* Phase 14 — per-connection SMP session state (one parallel slot
     * per conns[] slot). Lives outside conns[] so the bookkeeping is
     * easy to clear on disconnect without disturbing the connection
     * table fields, and to keep the per-link cost (~180 B / link)
     * visible. */
    struct {
        uint8_t  state;             /* smp_state_t */
        uint8_t  have_pubkey;       /* 1 once chip returned the P-256 pubkey  */
        uint8_t  have_dhkey;        /* 1 once chip handed back DHKey */
        uint8_t  have_peer_pubkey;  /* 1 once the SMP Public Key arrived */
        uint8_t  have_peer_random;  /* 1 once the SMP Random arrived  */
        uint8_t  pending_pubkey_tx; /* 1 if local pubkey is queued to send */
        uint8_t  pending_f5;        /* 1 if f5/LTK derivation is pending DHKey */
        uint8_t  peer_pubkey[64];   /* LE byte order, X(32) || Y(32)        */
        uint8_t  local_pubkey[64];
        uint8_t  dhkey[32];         /* LE byte order from chip              */
        uint8_t  peer_nonce[16];    /* Na (central)                          */
        uint8_t  local_nonce[16];   /* Nb (local, peripheral)                */
        uint8_t  ltk[16];
        uint8_t  mackey[16];
        uint8_t  peer_iocap[3];     /* AuthReq, OOB, IOcap from Pairing Req  */
        uint8_t  own_iocap[3];      /* what went out in Pairing Response     */
        uint8_t  peer_addr_le[6];   /* LE-order copy of peer addr            */
        uint8_t  peer_addr_type;    /* 0 public, 1 random                    */
    } smp[TIKU_BT_CONN_MAX];
} bt_state;

/*---------------------------------------------------------------------------*/
/* HCI send / recv / is_ready — vtable indirection                           */
/*---------------------------------------------------------------------------*/

int tiku_bt_send(const uint8_t *packet, uint16_t len)
{
    if (!g_bt_transport) return TIKU_DRV_ERR_NOT_PRESENT;
    return g_bt_transport->send(packet, len);
}

int tiku_bt_recv(uint8_t *out, uint16_t out_max)
{
    if (!g_bt_transport) return 0;
    return g_bt_transport->recv(out, out_max);
}

int tiku_bt_is_ready(void)
{
    return (g_bt_transport && g_bt_transport->is_ready()
            && bt_state.ready) ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
/* Connection table (phase 9) helpers                                        */
/*---------------------------------------------------------------------------*/

/** Find a connection slot by handle. Returns its index or -1. */
static int bt_conn_find(uint16_t handle)
{
    uint8_t i;
    for (i = 0U; i < TIKU_BT_CONN_MAX; ++i) {
        if (bt_state.conns[i].in_use
            && bt_state.conns[i].info.handle == handle) {
            return (int)i;
        }
    }
    return -1;
}

/** Allocate a free connection slot. Returns its index or -1. */
static int bt_conn_alloc(void)
{
    uint8_t i;
    for (i = 0U; i < TIKU_BT_CONN_MAX; ++i) {
        if (bt_state.conns[i].in_use == 0U) return (int)i;
    }
    return -1;
}

/** Return 1 if at least one connection is active. */
static int bt_any_connection(void)
{
    uint8_t i;
    for (i = 0U; i < TIKU_BT_CONN_MAX; ++i) {
        if (bt_state.conns[i].in_use) return 1;
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
/* ACL / L2CAP send (phase 10)                                               */
/*---------------------------------------------------------------------------*/

/*
 * Builds the HCI ACL header (type 0x02 + 12-bit handle + flags +
 * total length) and the L2CAP header (length + CID), then hands the
 * resulting packet to tiku_bt_send. PB flag is set to 0b10
 * (first automatically-flushable packet) which is the only value
 * permitted on LE-U links per Core Spec Vol 4 Part E 5.4.2.
 */

/**
 * @brief Send one L2CAP PDU over the named connection
 *
 * @param handle   Chip-assigned connection handle (12 bits)
 * @param cid      L2CAP channel ID (e.g. L2CAP_CID_ATT)
 * @param payload  Bytes of the L2CAP payload (the ATT/SMP PDU)
 * @param len      Payload length in bytes
 * @return TIKU_DRV_OK on send success.
 */
static int bt_send_acl(uint16_t handle, uint16_t cid,
                       const uint8_t *payload, uint16_t len)
{
    /* Header layout (9 bytes before payload):
     *   [0]   0x02                       HCI ACL type
     *   [1..2] handle(12) | PB=10 | BC=00  little-endian
     *   [3..4] HCI data total length      little-endian (= 4 + len)
     *   [5..6] L2CAP payload length       little-endian (= len)
     *   [7..8] L2CAP CID                  little-endian
     *   [9..]  payload
     */
    {
        uint8_t  pkt[260];
        uint16_t hl = (uint16_t)((handle & 0x0FFFU) | (0x2U << 12)); /* PB=10 */
        uint16_t total = (uint16_t)(4U + len);
        uint16_t i;
        if ((uint32_t)9U + len > sizeof pkt) return TIKU_DRV_ERR_INVALID;
        pkt[0] = HCI_PKT_TYPE_ACL;
        pkt[1] = (uint8_t)(hl & 0xFFU);
        pkt[2] = (uint8_t)((hl >> 8) & 0xFFU);
        pkt[3] = (uint8_t)(total & 0xFFU);
        pkt[4] = (uint8_t)((total >> 8) & 0xFFU);
        pkt[5] = (uint8_t)(len & 0xFFU);
        pkt[6] = (uint8_t)((len >> 8) & 0xFFU);
        pkt[7] = (uint8_t)(cid & 0xFFU);
        pkt[8] = (uint8_t)((cid >> 8) & 0xFFU);
        for (i = 0U; i < len; ++i) pkt[9U + i] = payload[i];
        return tiku_bt_send(pkt, (uint16_t)(9U + len));
    }
}

/*---------------------------------------------------------------------------*/
/* Minimal ATT attribute table (phase 10)                                    */
/*---------------------------------------------------------------------------*/
/*
 * Six attributes covering the mandatory GAP service plus a stub GATT
 * service so generic clients (nRF Connect, lightblue) get something
 * sensible on first browse:
 *
 *   0x0001  Primary Service          UUID=0x2800  value=0x1800 (GAP)
 *   0x0002  Characteristic Decl      UUID=0x2803  value=[READ, 0x0003, 0x2A00]
 *   0x0003  Device Name              UUID=0x2A00  value=adv_name
 *   0x0004  Characteristic Decl      UUID=0x2803  value=[READ, 0x0005, 0x2A01]
 *   0x0005  Appearance               UUID=0x2A01  value=0x0000 (Unknown)
 *   0x0006  Primary Service          UUID=0x2800  value=0x1801 (GATT)
 *
 * The attribute table is built dynamically when the first ATT
 * request arrives because attribute 0x0003 (Device Name) needs to
 * reflect whatever bt_advertise_setup cached as adv_name. That keeps
 * the on-air identity consistent with what the advertise step set.
 */

/* Maximum attributes ever in the snapshot table. Sized for:
 *   - 6 built-in (GAP service + 2 chars × {decl+value} + GATT stub)
 *   - For each user char: 2 (decl+value) or 3 (with CCCD)
 *   - 8 user chars * 3 = 24 worst case
 * 32 leaves headroom. */
#define ATT_HANDLE_MAX                  32U

/*
 * Built fresh in bt_att_snapshot every time an ATT request arrives.
 * One of three value sources is active per row:
 * - @p value + @p value_len  static bytes (services, char decls,
 * built-in Device Name etc.)
 * - @p char_ref              user characteristic with read/write
 * callbacks (or its own static value)
 * - @p cccd_ref              points into bt_state.cccd_value[]
 * for a CCCD attribute (read returns
 * the 2 bytes; write updates them)
 * Read/write dispatch in bt_att_handle_read / bt_att_handle_write
 * picks the active source by checking pointers in priority order:
 * cccd_ref → char_ref → static value.
 */

/**
 * @brief One row of the per-request attribute snapshot
 */
typedef struct {
    uint16_t                          handle;
    uint16_t                          uuid;
    const uint8_t                    *value;
    uint8_t                           value_len;
    const tiku_bt_char_t       *char_ref;
    uint16_t                         *cccd_ref;
} bt_att_entry_t;

/* Value payloads for the static (non-name) attributes. The two
 * Characteristic Declarations encode {properties, value_handle, char_uuid}. */
static const uint8_t att_val_gap_svc[2]   = { 0x00U, 0x18U }; /* 0x1800 LE */
static const uint8_t att_val_gatt_svc[2]  = { 0x01U, 0x18U }; /* 0x1801 LE */
static const uint8_t att_val_appearance[2] = { 0x00U, 0x00U };
static const uint8_t att_val_char_devname[5] = {
    ATT_PROP_READ,           /* properties */
    0x03U, 0x00U,            /* value handle 0x0003 LE */
    0x00U, 0x2AU,            /* char UUID    0x2A00 LE */
};
static const uint8_t att_val_char_appear[5] = {
    ATT_PROP_READ,
    0x05U, 0x00U,            /* value handle 0x0005 LE */
    0x01U, 0x2AU,            /* char UUID    0x2A01 LE */
};

/** Find or assign the CCCD slot for @p char_uuid. Returns -1 if the
 *  pool is exhausted. CCCD slots are created lazily on the first
 *  snapshot that sees a notify-capable char with this UUID. */
static int bt_cccd_slot_for(uint16_t char_uuid)
{
    uint8_t i;
    for (i = 0U; i < bt_state.cccd_count; ++i) {
        if (bt_state.cccd_char_uuid[i] == char_uuid) return (int)i;
    }
    if (bt_state.cccd_count >= TIKU_BT_CHAR_MAX) return -1;
    i = bt_state.cccd_count;
    bt_state.cccd_char_uuid[i] = char_uuid;
    bt_state.cccd_value[i]     = 0U;
    bt_state.cccd_count = (uint8_t)(bt_state.cccd_count + 1U);
    return (int)i;
}

/** Initialise one attribute row (helper to keep bt_att_snapshot compact). */
static void bt_att_set_static(bt_att_entry_t *e, uint16_t handle,
                              uint16_t uuid, const uint8_t *val,
                              uint8_t len)
{
    e->handle    = handle;
    e->uuid      = uuid;
    e->value     = val;
    e->value_len = len;
    e->char_ref  = (const tiku_bt_char_t *)0;
    e->cccd_ref  = (uint16_t *)0;
}

/*
 * Built-in (always present, handles assigned first):
 * GAP service with Device Name + Appearance
 * GATT stub service (Primary Service Decl only)
 * Then for each registered user service:
 * Primary Service Decl
 * For each characteristic:
 * Char Declaration (5-byte value into bt_state.char_decl_buf[])
 * Char Value       (refs char def for callbacks / static)
 * If NOTIFY/INDICATE in properties:
 * CCCD attribute (refs bt_state.cccd_value[slot])
 */

/**
 * @brief Build the attribute snapshot from built-in + user services
 *
 * @param out  Destination array, sized for ATT_HANDLE_MAX entries
 * @return Number of entries written
 */
static uint8_t bt_att_snapshot(bt_att_entry_t *out)
{
    static const char default_name[] = "TikuPico";
    uint16_t        h = 1U;
    uint8_t         n = 0U;
    uint8_t         char_buf_slot = 0U;
    uint8_t         svc;
    const uint8_t  *name_bytes;
    uint8_t         name_len;

    if (bt_state.adv_name_len > 0U) {
        name_bytes = (const uint8_t *)bt_state.adv_name;
        name_len   = bt_state.adv_name_len;
    } else {
        name_bytes = (const uint8_t *)default_name;
        name_len   = (uint8_t)(sizeof default_name - 1U);
    }

    /* GAP service: 5 attributes. */
    bt_att_set_static(&out[n++], h++, UUID_PRIMARY_SERVICE,
                      att_val_gap_svc, sizeof att_val_gap_svc);
    bt_att_set_static(&out[n++], h++, UUID_CHARACTERISTIC,
                      att_val_char_devname, sizeof att_val_char_devname);
    bt_att_set_static(&out[n++], h++, UUID_DEVICE_NAME,
                      name_bytes, name_len);
    bt_att_set_static(&out[n++], h++, UUID_CHARACTERISTIC,
                      att_val_char_appear, sizeof att_val_char_appear);
    bt_att_set_static(&out[n++], h++, UUID_APPEARANCE,
                      att_val_appearance, sizeof att_val_appearance);

    /* GATT stub service: 1 attribute. */
    bt_att_set_static(&out[n++], h++, UUID_PRIMARY_SERVICE,
                      att_val_gatt_svc, sizeof att_val_gatt_svc);

    /* User services. */
    for (svc = 0U; svc < bt_state.user_svc_count; ++svc) {
        const tiku_bt_service_t *s = bt_state.user_svc[svc];
        uint8_t  ch;
        /* Primary Service Decl: value = service UUID (2 bytes LE).
         * The UUID bytes are stored inline in a slot in char_decl_buf
         * (any spare 2 bytes will do; this takes the high 2 of the next
         * available slot since char_decl values are 5 bytes wide and
         * a service decl only needs 2). */
        if (char_buf_slot >= TIKU_BT_CHAR_MAX) break;
        bt_state.char_decl_buf[char_buf_slot][0] = (uint8_t)(s->uuid & 0xFFU);
        bt_state.char_decl_buf[char_buf_slot][1] =
            (uint8_t)((s->uuid >> 8) & 0xFFU);
        bt_att_set_static(&out[n++], h++, UUID_PRIMARY_SERVICE,
                          &bt_state.char_decl_buf[char_buf_slot][0], 2U);
        ++char_buf_slot;

        for (ch = 0U; ch < s->char_count && n + 3U <= ATT_HANDLE_MAX; ++ch) {
            const tiku_bt_char_t *c = &s->chars[ch];
            uint16_t value_handle = (uint16_t)(h + 1U);
            uint8_t *decl_bytes;
            if (char_buf_slot >= TIKU_BT_CHAR_MAX) break;
            decl_bytes = bt_state.char_decl_buf[char_buf_slot++];
            decl_bytes[0] = c->properties;
            decl_bytes[1] = (uint8_t)(value_handle & 0xFFU);
            decl_bytes[2] = (uint8_t)((value_handle >> 8) & 0xFFU);
            decl_bytes[3] = (uint8_t)(c->uuid & 0xFFU);
            decl_bytes[4] = (uint8_t)((c->uuid >> 8) & 0xFFU);
            /* Char Decl attribute. */
            bt_att_set_static(&out[n], h++, UUID_CHARACTERISTIC,
                              decl_bytes, 5U);
            ++n;
            /* Char Value attribute. Static value used as fallback in
             * the read handler when on_read is NULL. */
            out[n].handle    = h++;
            out[n].uuid      = c->uuid;
            out[n].value     = c->static_value;
            out[n].value_len = (uint8_t)c->static_value_len;
            out[n].char_ref  = c;
            out[n].cccd_ref  = (uint16_t *)0;
            ++n;
            /* CCCD attribute if NOTIFY/INDICATE set. */
            if ((c->properties &
                 (TIKU_BT_PROP_NOTIFY |
                  TIKU_BT_PROP_INDICATE)) != 0U) {
                int slot = bt_cccd_slot_for(c->uuid);
                if (slot >= 0) {
                    out[n].handle    = h++;
                    out[n].uuid      = UUID_CCCD;
                    out[n].value     = (const uint8_t *)
                                         &bt_state.cccd_value[slot];
                    out[n].value_len = 2U;
                    out[n].char_ref  = (const tiku_bt_char_t *)0;
                    out[n].cccd_ref  = &bt_state.cccd_value[slot];
                    ++n;
                }
            }
        }
    }
    return n;
}

/** Find the Char Value attribute for a given char UUID. Returns -1
 *  if not present in the snapshot. */
static int bt_att_find_char_value(const bt_att_entry_t *table,
                                  uint8_t n, uint16_t char_uuid)
{
    uint8_t i;
    for (i = 0U; i < n; ++i) {
        if (table[i].uuid == char_uuid
            && table[i].char_ref != (const tiku_bt_char_t *)0) {
            return (int)i;
        }
    }
    return -1;
}

/*---------------------------------------------------------------------------*/
/* ATT request handlers (phase 10)                                           */
/*---------------------------------------------------------------------------*/

/** Send an ATT Error Response (opcode 0x01) on @p conn_idx. */
static void bt_att_send_error(uint8_t conn_idx, uint8_t req_op,
                              uint16_t handle, uint8_t err)
{
    uint8_t rsp[5];
    rsp[0] = ATT_OP_ERROR_RSP;
    rsp[1] = req_op;
    rsp[2] = (uint8_t)(handle & 0xFFU);
    rsp[3] = (uint8_t)((handle >> 8) & 0xFFU);
    rsp[4] = err;
    (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                      L2CAP_CID_ATT, rsp, sizeof rsp);
}

/**
 * @brief Handle ATT Exchange MTU Request (opcode 0x02)
 *
 * Request layout: opcode(1) + client_rx_mtu(2 LE). The reply carries
 * server_rx_mtu (also 23 for now) so both sides know to keep PDUs
 * <= ATT_MTU_DEFAULT. Larger MTUs are a Phase 11+ feature.
 *
 * @param conn_idx  Connection table index
 * @param pdu       ATT PDU bytes (opcode at [0])
 * @param len       Length of @p pdu in bytes
 */
static void bt_att_handle_mtu(uint8_t conn_idx, const uint8_t *pdu,
                              uint16_t len)
{
    uint8_t rsp[3];
    if (len < 3U) {
        bt_att_send_error(conn_idx, ATT_OP_EXCHANGE_MTU_REQ, 0U,
                          ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }
    {
        uint16_t client_mtu = (uint16_t)(pdu[1] | ((uint16_t)pdu[2] << 8));
        TIKU_BT_PRINTF("p10.att: Exchange MTU req client=%u "
                        "-> rsp server=%u\n",
                        client_mtu, (unsigned)ATT_MTU_DEFAULT);
    }
    rsp[0] = ATT_OP_EXCHANGE_MTU_RSP;
    rsp[1] = (uint8_t)(ATT_MTU_DEFAULT & 0xFFU);
    rsp[2] = (uint8_t)((ATT_MTU_DEFAULT >> 8) & 0xFFU);
    bt_state.conns[conn_idx].att_mtu = ATT_MTU_DEFAULT;
    (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                      L2CAP_CID_ATT, rsp, sizeof rsp);
}

/*
 * Used by clients to enumerate Primary Service declarations. The
 * request gives a handle range + group UUID (usually 0x2800); the
 * reply is a list of {start_handle, end_handle, service_uuid}
 * tuples for matching attributes in range.
 */

/**
 * @brief Handle ATT Read By Group Type Request (opcode 0x10)
 *
 * @param conn_idx  Connection table index
 * @param pdu       ATT PDU bytes
 * @param len       Length of @p pdu in bytes
 */
static void bt_att_handle_read_by_group(uint8_t conn_idx,
                                        const uint8_t *pdu, uint16_t len)
{
    uint16_t start_h, end_h, group_uuid;
    bt_att_entry_t table[ATT_HANDLE_MAX];
    uint8_t        n_attrs;
    uint8_t        rsp[32];
    uint8_t        off = 2U;
    uint8_t        i;
    int            first = 1;

    if (len < 7U) {
        bt_att_send_error(conn_idx, ATT_OP_READ_BY_GROUP_TYPE_REQ, 0U,
                          ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }
    start_h    = (uint16_t)(pdu[1] | ((uint16_t)pdu[2] << 8));
    end_h      = (uint16_t)(pdu[3] | ((uint16_t)pdu[4] << 8));
    group_uuid = (uint16_t)(pdu[5] | ((uint16_t)pdu[6] << 8));
    TIKU_BT_PRINTF("p10.att: Read By Group Type 0x%04x range "
                    "0x%04x..0x%04x\n", group_uuid, start_h, end_h);
    if (group_uuid != UUID_PRIMARY_SERVICE) {
        bt_att_send_error(conn_idx, ATT_OP_READ_BY_GROUP_TYPE_REQ,
                          start_h, ATT_ERR_ATTRIBUTE_NOT_FOUND);
        return;
    }

    n_attrs = bt_att_snapshot(table);
    rsp[0] = ATT_OP_READ_BY_GROUP_TYPE_RSP;
    rsp[1] = 6U;                         /* length per entry */

    for (i = 0U; i < n_attrs; ++i) {
        if (table[i].uuid != UUID_PRIMARY_SERVICE) continue;
        if (table[i].handle < start_h || table[i].handle > end_h) continue;
        if (off + 6U > sizeof rsp) break;
        /* Find end-of-group: the next service handle - 1, or the
         * last attribute handle in the table. */
        {
            uint16_t end_grp = ATT_HANDLE_MAX;
            uint8_t  j;
            for (j = (uint8_t)(i + 1U); j < n_attrs; ++j) {
                if (table[j].uuid == UUID_PRIMARY_SERVICE) {
                    end_grp = (uint16_t)(table[j].handle - 1U);
                    break;
                }
            }
            rsp[off++] = (uint8_t)(table[i].handle & 0xFFU);
            rsp[off++] = (uint8_t)((table[i].handle >> 8) & 0xFFU);
            rsp[off++] = (uint8_t)(end_grp & 0xFFU);
            rsp[off++] = (uint8_t)((end_grp >> 8) & 0xFFU);
            rsp[off++] = table[i].value[0];
            rsp[off++] = table[i].value[1];
            first = 0;
        }
    }
    if (first) {
        bt_att_send_error(conn_idx, ATT_OP_READ_BY_GROUP_TYPE_REQ,
                          start_h, ATT_ERR_ATTRIBUTE_NOT_FOUND);
        return;
    }
    (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                      L2CAP_CID_ATT, rsp, off);
}

/**
 * @brief Handle ATT Read By Type Request (opcode 0x08)
 *
 * Used by clients to enumerate Characteristic declarations within a
 * service. The reply is a list of {attr_handle, value} pairs for
 * matching attributes in range.
 *
 * @param conn_idx  Connection table index
 * @param pdu       ATT PDU bytes
 * @param len       Length of @p pdu in bytes
 */
static void bt_att_handle_read_by_type(uint8_t conn_idx,
                                       const uint8_t *pdu, uint16_t len)
{
    uint16_t start_h, end_h, type_uuid;
    bt_att_entry_t table[ATT_HANDLE_MAX];
    uint8_t        n_attrs;
    uint8_t        rsp[32];
    uint8_t        off = 2U;
    uint8_t        i;
    uint8_t        rec_len = 0U;
    int            first = 1;

    if (len < 7U) {
        bt_att_send_error(conn_idx, ATT_OP_READ_BY_TYPE_REQ, 0U,
                          ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }
    start_h   = (uint16_t)(pdu[1] | ((uint16_t)pdu[2] << 8));
    end_h     = (uint16_t)(pdu[3] | ((uint16_t)pdu[4] << 8));
    type_uuid = (uint16_t)(pdu[5] | ((uint16_t)pdu[6] << 8));
    TIKU_BT_PRINTF("p10.att: Read By Type 0x%04x range "
                    "0x%04x..0x%04x\n", type_uuid, start_h, end_h);

    n_attrs = bt_att_snapshot(table);
    rsp[0] = ATT_OP_READ_BY_TYPE_RSP;

    for (i = 0U; i < n_attrs; ++i) {
        if (table[i].uuid != type_uuid) continue;
        if (table[i].handle < start_h || table[i].handle > end_h) continue;
        if (first) {
            rec_len    = (uint8_t)(2U + table[i].value_len);
            rsp[1]     = rec_len;
            first      = 0;
        } else if ((uint8_t)(2U + table[i].value_len) != rec_len) {
            /* Spec says all returned entries in one Read By Type Rsp
             * must share the same length. Stop at the first mismatch
             * -- client will issue another request to walk further. */
            break;
        }
        if (off + rec_len > sizeof rsp) break;
        rsp[off++] = (uint8_t)(table[i].handle & 0xFFU);
        rsp[off++] = (uint8_t)((table[i].handle >> 8) & 0xFFU);
        {
            uint8_t k;
            for (k = 0U; k < table[i].value_len; ++k)
                rsp[off++] = table[i].value[k];
        }
    }
    if (first) {
        bt_att_send_error(conn_idx, ATT_OP_READ_BY_TYPE_REQ,
                          start_h, ATT_ERR_ATTRIBUTE_NOT_FOUND);
        return;
    }
    (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                      L2CAP_CID_ATT, rsp, off);
}

/**
 * @brief Handle ATT Read Request (opcode 0x0A)
 *
 * Returns the value of the named attribute.
 *
 * @param conn_idx  Connection table index
 * @param pdu       ATT PDU bytes (opcode + handle LE)
 * @param len       Length of @p pdu in bytes
 */
static void bt_att_handle_read(uint8_t conn_idx, const uint8_t *pdu,
                               uint16_t len)
{
    uint16_t       h;
    bt_att_entry_t table[ATT_HANDLE_MAX];
    uint8_t        n_attrs;
    uint8_t        i;

    if (len < 3U) {
        bt_att_send_error(conn_idx, ATT_OP_READ_REQ, 0U,
                          ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }
    h = (uint16_t)(pdu[1] | ((uint16_t)pdu[2] << 8));
    TIKU_BT_PRINTF("p10.att: Read handle 0x%04x\n", h);
    n_attrs = bt_att_snapshot(table);
    for (i = 0U; i < n_attrs; ++i) {
        if (table[i].handle != h) continue;
        {
            uint8_t  rsp[ATT_MTU_DEFAULT];
            uint8_t  copy;
            uint8_t  k;
            uint16_t produced = 0U;

            rsp[0] = ATT_OP_READ_RSP;

            /* Value source priority: CCCD ref → char read callback →
             * char static value → entry static value. */
            if (table[i].cccd_ref != (uint16_t *)0) {
                rsp[1] = (uint8_t)(*table[i].cccd_ref & 0xFFU);
                rsp[2] = (uint8_t)((*table[i].cccd_ref >> 8) & 0xFFU);
                produced = 2U;
            } else if (table[i].char_ref != (const tiku_bt_char_t *)0
                       && table[i].char_ref->on_read != (tiku_bt_char_read_t)0) {
                if (table[i].char_ref->on_read(
                        table[i].char_ref->user, &rsp[1],
                        (uint16_t)(sizeof rsp - 1U), &produced) != 0) {
                    bt_att_send_error(conn_idx, ATT_OP_READ_REQ, h,
                                      ATT_ERR_READ_NOT_PERMITTED);
                    return;
                }
            } else {
                copy = table[i].value_len;
                if ((uint16_t)copy + 1U > (uint16_t)sizeof rsp)
                    copy = (uint8_t)(sizeof rsp - 1U);
                for (k = 0U; k < copy; ++k) rsp[1U + k] = table[i].value[k];
                produced = copy;
            }
            (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                              L2CAP_CID_ATT, rsp,
                              (uint16_t)(1U + produced));
            return;
        }
    }
    bt_att_send_error(conn_idx, ATT_OP_READ_REQ, h,
                      ATT_ERR_INVALID_HANDLE);
}

/*
 * The handle either resolves to a CCCD (in which case the 2-byte
 * value is stored in bt_state.cccd_value via the row's cccd_ref) or
 * to a user characteristic whose on_write callback is invoked.
 * Anything else gets Write Not Permitted (0x03).
 */

/**
 * @brief Handle ATT Write Request (opcode 0x12)
 *
 * @param conn_idx  Connection table index
 * @param pdu       ATT PDU (opcode at [0])
 * @param len       PDU length
 */
static void bt_att_handle_write(uint8_t conn_idx, const uint8_t *pdu,
                                uint16_t len)
{
    uint16_t       h;
    bt_att_entry_t table[ATT_HANDLE_MAX];
    uint8_t        n_attrs;
    uint8_t        i;
    uint8_t        rsp;
    if (len < 3U) {
        bt_att_send_error(conn_idx, ATT_OP_WRITE_REQ, 0U,
                          ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }
    h = (uint16_t)(pdu[1] | ((uint16_t)pdu[2] << 8));
    TIKU_BT_PRINTF("p11.att: Write handle 0x%04x len=%u\n",
                    h, (unsigned)(len - 3U));
    n_attrs = bt_att_snapshot(table);
    for (i = 0U; i < n_attrs; ++i) {
        if (table[i].handle != h) continue;

        /* CCCD write: 2 bytes little-endian, store + ack. */
        if (table[i].cccd_ref != (uint16_t *)0) {
            uint16_t new_val = (len >= 5U)
                ? (uint16_t)(pdu[3] | ((uint16_t)pdu[4] << 8))
                : 0U;
            *table[i].cccd_ref = new_val;
            TIKU_BT_PRINTF("p12: CCCD set to 0x%04x "
                            "(notify=%u indicate=%u)\n",
                            new_val,
                            (new_val & 0x0001U) ? 1U : 0U,
                            (new_val & 0x0002U) ? 1U : 0U);
            rsp = ATT_OP_WRITE_RSP;
            (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                              L2CAP_CID_ATT, &rsp, 1U);
            return;
        }

        /* User char with on_write. */
        if (table[i].char_ref != (const tiku_bt_char_t *)0
            && table[i].char_ref->on_write
                != (tiku_bt_char_write_t)0) {
            int wrc = table[i].char_ref->on_write(
                table[i].char_ref->user, &pdu[3],
                (uint16_t)(len - 3U));
            if (wrc != 0) {
                bt_att_send_error(conn_idx, ATT_OP_WRITE_REQ, h,
                                  ATT_ERR_WRITE_NOT_PERMITTED);
                return;
            }
            rsp = ATT_OP_WRITE_RSP;
            (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                              L2CAP_CID_ATT, &rsp, 1U);
            return;
        }

        bt_att_send_error(conn_idx, ATT_OP_WRITE_REQ, h,
                          ATT_ERR_WRITE_NOT_PERMITTED);
        return;
    }
    bt_att_send_error(conn_idx, ATT_OP_WRITE_REQ, h,
                      ATT_ERR_INVALID_HANDLE);
}

/**
 * @brief Handle ATT Find Information Request (opcode 0x04)
 *
 * Returns {handle, UUID-16} pairs for attributes in the requested
 * range. Used by clients to discover descriptors when Characteristic
 * Declaration values don't reveal them.
 *
 * @param conn_idx  Connection table index
 * @param pdu       ATT PDU bytes
 * @param len       Length of @p pdu in bytes
 */
static void bt_att_handle_find_info(uint8_t conn_idx,
                                    const uint8_t *pdu, uint16_t len)
{
    uint16_t start_h, end_h;
    bt_att_entry_t table[ATT_HANDLE_MAX];
    uint8_t        n_attrs;
    uint8_t        rsp[32];
    uint8_t        off = 2U;
    uint8_t        i;
    int            first = 1;

    if (len < 5U) {
        bt_att_send_error(conn_idx, ATT_OP_FIND_INFORMATION_REQ, 0U,
                          ATT_ERR_REQUEST_NOT_SUPPORTED);
        return;
    }
    start_h = (uint16_t)(pdu[1] | ((uint16_t)pdu[2] << 8));
    end_h   = (uint16_t)(pdu[3] | ((uint16_t)pdu[4] << 8));
    TIKU_BT_PRINTF("p10.att: Find Information range "
                    "0x%04x..0x%04x\n", start_h, end_h);
    n_attrs = bt_att_snapshot(table);

    rsp[0] = ATT_OP_FIND_INFORMATION_RSP;
    rsp[1] = 0x01U;                  /* format: 16-bit UUIDs */
    for (i = 0U; i < n_attrs; ++i) {
        if (table[i].handle < start_h || table[i].handle > end_h) continue;
        if (off + 4U > sizeof rsp) break;
        rsp[off++] = (uint8_t)(table[i].handle & 0xFFU);
        rsp[off++] = (uint8_t)((table[i].handle >> 8) & 0xFFU);
        rsp[off++] = (uint8_t)(table[i].uuid & 0xFFU);
        rsp[off++] = (uint8_t)((table[i].uuid >> 8) & 0xFFU);
        first = 0;
    }
    if (first) {
        bt_att_send_error(conn_idx, ATT_OP_FIND_INFORMATION_REQ,
                          start_h, ATT_ERR_ATTRIBUTE_NOT_FOUND);
        return;
    }
    (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                      L2CAP_CID_ATT, rsp, off);
}

/**
 * @brief Top-level ATT dispatcher
 *
 * Routes by opcode to the per-request handler. Unsupported opcodes
 * get Error Response with code 0x06 (Request Not Supported), which
 * is the spec-correct way to surface "I don't implement that".
 *
 * @param conn_idx  Connection table index
 * @param pdu       ATT PDU bytes
 * @param len       Length of @p pdu in bytes
 */
static void bt_handle_att(uint8_t conn_idx, const uint8_t *pdu,
                          uint16_t len)
{
    if (len < 1U) return;
    switch (pdu[0]) {
    case ATT_OP_EXCHANGE_MTU_REQ:
        bt_att_handle_mtu(conn_idx, pdu, len);
        break;
    case ATT_OP_READ_BY_GROUP_TYPE_REQ:
        bt_att_handle_read_by_group(conn_idx, pdu, len);
        break;
    case ATT_OP_READ_BY_TYPE_REQ:
        bt_att_handle_read_by_type(conn_idx, pdu, len);
        break;
    case ATT_OP_FIND_INFORMATION_REQ:
        bt_att_handle_find_info(conn_idx, pdu, len);
        break;
    case ATT_OP_READ_REQ:
        bt_att_handle_read(conn_idx, pdu, len);
        break;
    case ATT_OP_WRITE_REQ:
        bt_att_handle_write(conn_idx, pdu, len);
        break;
    /* Phase 13 client-side responses: log + drop. A future
     * Phase 13.x pass will add per-request state so callers can
     * receive results, but for shell-driven `bt read / discover`
     * tracing in the UART log is enough. */
    case ATT_OP_ERROR_RSP:
        if (len >= 5) {
            TIKU_BT_PRINTF("p13.att: Error Rsp op=0x%02x "
                            "handle=0x%04x err=0x%02x\n",
                            pdu[1],
                            (uint16_t)(pdu[2] | (pdu[3] << 8)),
                            pdu[4]);
        }
        break;
    case ATT_OP_EXCHANGE_MTU_RSP:
        if (len >= 3) {
            TIKU_BT_PRINTF("p13.att: MTU Rsp server=%u\n",
                            (uint16_t)(pdu[1] | (pdu[2] << 8)));
        }
        break;
    case ATT_OP_READ_RSP:
        TIKU_BT_PRINTF("p13.att: Read Rsp %u B: ", (unsigned)(len - 1U));
        {
            uint16_t k;
            for (k = 1U; k < len && k < 17U; ++k) {
                /* TIKU_PRINTF doesn't support %02x zero-pad reliably
                 * across platforms; emit two nibbles via hex digits. */
                static const char hex[] = "0123456789abcdef";
                /* Concatenated in a single string to fit one printf. */
                TIKU_BT_PRINTF("%c%c ", hex[(pdu[k] >> 4) & 0xF],
                                          hex[pdu[k] & 0xF]);
            }
            TIKU_BT_PRINTF("\n");
        }
        break;
    case ATT_OP_READ_BY_TYPE_RSP:
    case ATT_OP_READ_BY_GROUP_TYPE_RSP:
    case ATT_OP_FIND_INFORMATION_RSP:
        TIKU_BT_PRINTF("p13.att: discovery Rsp opcode=0x%02x "
                        "len=%u (decode in shell)\n",
                        pdu[0], (unsigned)len);
        break;
    case ATT_OP_WRITE_RSP:
        TIKU_BT_PRINTF("p13.att: Write Rsp\n");
        break;
    case ATT_OP_HANDLE_VALUE_NOTIFY:
        if (len >= 3) {
            TIKU_BT_PRINTF("p13.att: *** Notify handle=0x%04x "
                            "%u B ***\n",
                            (uint16_t)(pdu[1] | (pdu[2] << 8)),
                            (unsigned)(len - 3U));
        }
        break;
    default:
        bt_att_send_error(conn_idx, pdu[0], 0U,
                          ATT_ERR_REQUEST_NOT_SUPPORTED);
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* SMP forward declaration (full impl after bt_hci_cmd_response)             */
/*---------------------------------------------------------------------------*/
/*
 * The real Security Manager Protocol -- Just-Works LE Secure
 * Connections pairing + bonding -- lives further down because it
 * leans on bt_hci_cmd_response (for the chip-side AES-128 ECB and
 * P-256 ECDH offloads). Forward-declare so bt_handle_acl_pkt can
 * dispatch CID 6 PDUs without depending on file ordering.
 */
static void bt_handle_smp(uint8_t conn_idx, const uint8_t *pdu,
                          uint16_t len);

/*---------------------------------------------------------------------------*/
/* ACL / L2CAP receive (phase 10)                                            */
/*---------------------------------------------------------------------------*/

/*
 * Header layout:
 * [0]    HCI_PKT_TYPE_ACL
 * [1..2] handle(12) | PB(2) | BC(2)  little-endian
 * [3..4] data_total_length            little-endian
 * [5..6] L2CAP payload length         little-endian
 * [7..8] L2CAP CID                    little-endian
 * [9..]  L2CAP payload
 */

/**
 * @brief Decode one HCI ACL data packet and dispatch by L2CAP CID
 *
 * @param pkt  Packet bytes including the type byte at offset 0
 * @param len  Length of @p pkt in bytes
 */
static void bt_handle_acl_pkt(const uint8_t *pkt, int len)
{
    uint16_t handle;
    uint16_t cid;
    int      conn_idx;
    uint16_t l2cap_len;
    if (len < 9) return;

    handle    = (uint16_t)((pkt[1] | ((uint16_t)pkt[2] << 8)) & 0x0FFFU);
    l2cap_len = (uint16_t)(pkt[5] | ((uint16_t)pkt[6] << 8));
    cid       = (uint16_t)(pkt[7] | ((uint16_t)pkt[8] << 8));
    conn_idx  = bt_conn_find(handle);
    if (conn_idx < 0) {
        TIKU_BT_PRINTF("acl: dropping pkt for unknown handle 0x%04x\n",
                        handle);
        return;
    }
    if (9 + (int)l2cap_len > len) {
        TIKU_BT_PRINTF("acl: truncated L2CAP frame "
                        "(len=%d expected=%d)\n", len, 9 + (int)l2cap_len);
        return;
    }

    if (cid == L2CAP_CID_ATT) {
        bt_handle_att((uint8_t)conn_idx, &pkt[9], l2cap_len);
    } else if (cid == L2CAP_CID_SMP) {
        bt_handle_smp((uint8_t)conn_idx, &pkt[9], l2cap_len);
    } else {
        TIKU_BT_PRINTF("acl: dropping CID 0x%04x (only ATT + SMP "
                        "handled today)\n", cid);
    }
}

/*---------------------------------------------------------------------------*/
/* HCI event dispatcher (phase 9 + 10)                                       */
/*---------------------------------------------------------------------------*/

/**
 * @brief Top-level HCI event dispatcher
 *
 * Routes events by code. LE Meta events (0x3E) go to bt_handle_le_meta
 * which then routes by subevent. Disconnection Complete (0x05) clears
 * the connection table entry. Everything else is logged + ignored.
 *
 * @param pkt  Event bytes including the type byte at offset 0
 * @param len  Length of @p pkt in bytes
 */
static void bt_handle_hci_event(const uint8_t *pkt, int len);  /* fwd */

/*
 * Builds the 4-byte HCI command packet (1 byte type + 3 byte header
 * + plen bytes of params), sends it through tiku_bt_send, then
 * polls tiku_bt_recv up to ~1 s waiting for a Command Complete
 * (event 0x0E) carrying the same opcode. Used by the phase-6.D
 * bring-up queries to fetch chip identity.
 */

/**
 * @brief Issue an HCI command and poll for its Command Complete event
 *
 * @param opcode    16-bit HCI opcode (OGF << 10 | OCF)
 * @param params    Parameter bytes (may be NULL when plen=0)
 * @param plen      Parameter byte count
 * @param out_evt   Destination for the full HCI event packet
 * (type byte first, matching tiku_bt_recv)
 * @param out_max   Capacity of @p out_evt
 * @param out_n     Set to the number of bytes written to @p out_evt
 * (always populated on success)
 * @return TIKU_DRV_OK on a matching Command Complete event, otherwise
 * a transport error or TIKU_DRV_ERR_TIMEOUT.
 */
static int bt_hci_cmd_response(uint16_t opcode, const uint8_t *params,
                               uint8_t plen, uint8_t *out_evt,
                               uint16_t out_max, int *out_n)
{
    unsigned int polls;
    int          tx_rc;

    if (plen > 252U) return TIKU_DRV_ERR_INVALID;
    bt_scratch_cmd[0] = 0x01U;                     /* HCI cmd type */
    bt_scratch_cmd[1] = (uint8_t)(opcode & 0xFFU);
    bt_scratch_cmd[2] = (uint8_t)((opcode >> 8) & 0xFFU);
    bt_scratch_cmd[3] = plen;
    if (plen > 0U) {
        uint8_t i;
        for (i = 0U; i < plen; ++i) bt_scratch_cmd[4U + i] = params[i];
    }
    tx_rc = tiku_bt_send(bt_scratch_cmd, (uint16_t)(4U + plen));
    if (tx_rc != TIKU_DRV_OK) return tx_rc;

    /* ~1 s budget: 200 polls × 5 ms. Long enough for slow commands
     * like Reset; short enough that a missed event surfaces fast. */
    for (polls = 0U; polls < 200U; ++polls) {
        int n = tiku_bt_recv(out_evt, out_max);
        if (n > 0) {
            /* Discriminate Command Complete (evt 0x0E) carrying the
             * opcode just sent; ignore anything else (vendor events,
             * stale Number_Of_Completed_Packets, etc.). */
            if (n >= 6 && out_evt[0] == HCI_PKT_TYPE_EVENT
                && out_evt[1] == HCI_EVT_COMMAND_COMPLETE
                && out_evt[4] == (uint8_t)(opcode & 0xFFU)
                && out_evt[5] == (uint8_t)((opcode >> 8) & 0xFFU)) {
                *out_n = n;
                return TIKU_DRV_OK;
            }
            /* Someone else's event — route through the dispatchers so
             * an async LE Connection Complete / Disconnection / Adv
             * Report arriving during a command flight is still
             * delivered. */
            if (out_evt[0] == HCI_PKT_TYPE_EVENT) {
                bt_handle_hci_event(out_evt, n);
            } else if (out_evt[0] == HCI_PKT_TYPE_ACL) {
                bt_handle_acl_pkt(out_evt, n);
            }
            continue;
        }
        tiku_common_delay_ms(5U);
    }
    *out_n = 0;
    return TIKU_DRV_ERR_TIMEOUT;
}

/*===========================================================================*/
/* Phase 14 -- LE Secure Connections Just-Works pairing + bonding            */
/*===========================================================================*/

/* TRNG wrapper for SMP entropy (Nb nonce). Sits inside the Phase 14
 * block because it's only consumed by the pairing state machine.
 * Returns 0 on success. */
#if PLATFORM_RP2350
static int bt_rand_bytes(uint8_t *out, size_t n)
{
    if (out == (uint8_t *)0) return TIKU_TRNG_ERR_INVALID;
    return tiku_trng_arch_read_bytes(out, n);
}
#else
static int bt_rand_bytes(uint8_t *out, size_t n)
{
    /* TRNG is only wired on RP2350 today; refuse pairing on platforms
     * without one rather than emit predictable nonces. */
    (void)out; (void)n;
    return -1;
}
#endif

/*
 * Crypto strategy: the CYW43439 controller exposes an AES-128 ECB
 * primitive (HCI_LE_Encrypt, 0x2017) and a P-256 ECDH engine
 * (HCI_LE_Read_Local_P256_Public_Key 0x2025 + HCI_LE_Generate_DHKey
 * 0x205E) -- the heavy lifting goes to the chip, leaving only
 * implement AES-CMAC (RFC 4493) and the Bluetooth f4/f5/f6 KDFs
 * (Core Spec Vol 3 Part H 2.2) in software. That keeps the code
 * footprint manageable on the Cortex-M33 host: ~3 KB of text rather
 * than a software P-256 + AES port that would run several times
 * larger.
 *
 * State machine (peripheral, Just-Works only):
 *
 *   IDLE                                  --recv Pairing Request-->
 *   WAITING_PUBKEY  (sent Pairing Rsp, awaiting peer Public Key)
 *                                         --recv Public Key-------->
 *   WAITING_RANDOM  (sent Public Key + Confirm Cb, awaiting Na)
 *                                         --recv Random------------>
 *   WAITING_DHCHECK (sent Nb + computed LTK; awaiting peer DH check)
 *                                         --recv DH Check---------->
 *   ENCRYPTING      (sent the DH check, awaiting LE LTK Request)
 *                                         --LE LTK Request reply--->
 *   ENCRYPTED       (Encryption Change OK -- bond persisted)
 *
 * Any error during pairing surfaces as SMP Pairing Failed (the spec
 * way of saying "give up cleanly"); the session walks back to IDLE
 * without persisting partial state.
 */

/*---------------------------------------------------------------------------*/
/* AES-128 ECB single block via chip-side HCI_LE_Encrypt                     */
/*---------------------------------------------------------------------------*/
/*
 * Synchronous helper. Per Core Spec Vol 4 Part E 7.8.22 the
 * HCI_LE_Encrypt command's Key and Plaintext_Data are "16 octets
 * least significant octet first" -- meaning the bytes are sent
 * LE-first but the AES algorithm internally treats them as an
 * MSB-first 128-bit integer. To make AES-CMAC's byte-oriented
 * arithmetic line up with NimBLE / RFC4493, the inputs are byte-reversed
 * before sending and the output reversed back.
 * The CYW43439 sends the encrypted block as the 16 bytes immediately
 * after the standard CC header (evt[7..22] in this buffer indexing).
 */

/**
 * @brief One AES-128 ECB block via HCI_LE_Encrypt
 *
 * @param key  16-byte AES key (MSB-first byte order)
 * @param in   16-byte plaintext (MSB-first byte order)
 * @param out  16-byte ciphertext destination (MSB-first byte order)
 * @return TIKU_DRV_OK on success, transport error otherwise
 */
static int bt_aes128_ecb(const uint8_t key[16], const uint8_t in[16],
                         uint8_t out[16])
{
    uint8_t  params[32];
    uint8_t  evt[40];
    int      n  = 0;
    int      rc;
    uint8_t  i;
    /* Reverse key/plaintext to LE wire form for HCI_LE_Encrypt. */
    for (i = 0U; i < 16U; ++i) params[i]       = key[15U - i];
    for (i = 0U; i < 16U; ++i) params[16U + i] = in[15U - i];
    rc = bt_hci_cmd_response(HCI_OP_LE_ENCRYPT, params, 32U,
                             evt, sizeof evt, &n);
    if (rc != TIKU_DRV_OK) return rc;
    /* CC layout: [0]=0x04 [1]=0x0E [2]=plen [3]=num_pkts [4..5]=opcode
     * [6]=status [7..22]=encrypted_data (LE). Reverse back to MSB-first. */
    if (n < 23 || evt[6] != 0x00U) return TIKU_DRV_ERR_INVALID;
    for (i = 0U; i < 16U; ++i) out[i] = evt[22U - i];
    return TIKU_DRV_OK;
}

/*---------------------------------------------------------------------------*/
/* AES-CMAC-128 (RFC 4493) on top of chip-side AES                           */
/*---------------------------------------------------------------------------*/

/** Left-shift a 128-bit big-endian buffer by one bit. */
static void cmac_lshift1(const uint8_t in[16], uint8_t out[16])
{
    int     i;
    uint8_t carry = 0U;
    for (i = 15; i >= 0; --i) {
        uint8_t b = in[i];
        out[i]    = (uint8_t)((b << 1) | carry);
        carry     = (uint8_t)((b >> 7) & 0x01U);
    }
}

/** Sub-key derivation per RFC 4493 §2.3: K1, K2 from L = AES(K, 0^128). */
static int bt_aes_cmac_subkeys(const uint8_t key[16],
                               uint8_t K1[16], uint8_t K2[16])
{
    static const uint8_t Rb = 0x87U;
    uint8_t L[16];
    uint8_t zero[16] = {0};
    int     rc       = bt_aes128_ecb(key, zero, L);
    if (rc != TIKU_DRV_OK) return rc;
    cmac_lshift1(L, K1);
    if (L[0] & 0x80U) K1[15] = (uint8_t)(K1[15] ^ Rb);
    cmac_lshift1(K1, K2);
    if (K1[0] & 0x80U) K2[15] = (uint8_t)(K2[15] ^ Rb);
    return TIKU_DRV_OK;
}

/*
 * Process the message in 16-byte blocks. The final block is either
 * complete (XOR with K1) or padded with 0x80 0x00 ... (XOR with K2).
 * The empty-message case (msg_len == 0) is the "padded with 0x80
 * followed by zeros" path because there is no last-block-as-is option.
 */

/**
 * @brief AES-CMAC-128 (RFC 4493)
 *
 * @param key      16-byte CMAC key
 * @param msg      Message bytes (may be NULL when msg_len == 0)
 * @param msg_len  Message length in bytes
 * @param mac      16-byte MAC destination
 */
static int bt_aes_cmac(const uint8_t key[16],
                       const uint8_t *msg, size_t msg_len,
                       uint8_t mac[16])
{
    uint8_t K1[16], K2[16];
    uint8_t X[16] = {0};
    uint8_t Y[16];
    uint8_t M_last[16];
    size_t  n_blocks;
    int     last_complete;
    int     rc;
    size_t  i;

    rc = bt_aes_cmac_subkeys(key, K1, K2);
    if (rc != TIKU_DRV_OK) return rc;

    if (msg_len == 0U) {
        n_blocks      = 1U;
        last_complete = 0;
    } else {
        n_blocks      = (msg_len + 15U) / 16U;
        last_complete = ((msg_len % 16U) == 0U);
    }

    /* Build the final block (with padding XOR'd into the right subkey). */
    if (last_complete) {
        size_t off = (n_blocks - 1U) * 16U;
        for (i = 0U; i < 16U; ++i) M_last[i] = (uint8_t)(msg[off + i] ^ K1[i]);
    } else {
        size_t off = (n_blocks - 1U) * 16U;
        size_t rem = msg_len - off;
        for (i = 0U; i < rem; ++i)  M_last[i] = msg[off + i];
        M_last[rem] = 0x80U;
        for (i = rem + 1U; i < 16U; ++i) M_last[i] = 0x00U;
        for (i = 0U; i < 16U; ++i) M_last[i] = (uint8_t)(M_last[i] ^ K2[i]);
    }

    /* Hash all but the last block. */
    for (i = 0U; i < n_blocks - 1U; ++i) {
        uint8_t k;
        for (k = 0U; k < 16U; ++k) {
            Y[k] = (uint8_t)(X[k] ^ msg[i * 16U + k]);
        }
        rc = bt_aes128_ecb(key, Y, X);
        if (rc != TIKU_DRV_OK) return rc;
    }
    {
        uint8_t k;
        for (k = 0U; k < 16U; ++k) Y[k] = (uint8_t)(X[k] ^ M_last[k]);
    }
    rc = bt_aes128_ecb(key, Y, mac);
    return rc;
}

/*---------------------------------------------------------------------------*/
/* Bluetooth KDFs: f4, f5, f6 (Core Spec Vol 3 Part H 2.2)                   */
/*---------------------------------------------------------------------------*/

/* Swap N bytes in place (in-place reverse), converting between
 * the LE byte order of the SMP wire and the spec's MSB-first
 * "mathematical" byte order that AES-CMAC operates on for the Core
 * Spec sample data to match. */
static void bt_smp_swap_buf(uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t i;
    for (i = 0U; i < n; ++i) dst[i] = src[n - 1U - i];
}

/*
 * Computes Cb = AES-CMAC(X, U || V || Z) over a 65-byte input where
 * the spec defines U, V, X as integer values (MSB-first conceptually).
 * The wire byte order for these fields is LE; they are swapped to MSB-first
 * before feeding AES-CMAC and swap the output back to LE.
 * U = peer  public key X coordinate (32 B, LE on the wire)
 * V = local public key X coordinate (32 B, LE)
 * X = the nonce of the side computing the confirm (16 B, LE)
 * Z = 1 byte, 0x00 for Just Works / Numeric Comparison
 */

/**
 * @brief f4 confirm-value generator (Core Spec Vol 3 Part H 2.2.6)
 *
 * @param U   peer PK X coordinate (32 B, LE byte order)
 * @param V   local PK X coordinate (32 B, LE byte order)
 * @param X   16-byte nonce (LE byte order)
 * @param Z   single byte
 * @param out 16-byte confirm value (LE byte order, matches wire)
 */
static int bt_smp_f4(const uint8_t U[32], const uint8_t V[32],
                     const uint8_t X[16], uint8_t Z, uint8_t out[16])
{
    uint8_t buf[65];
    uint8_t xs[16];
    int     rc;
    bt_smp_swap_buf(buf,           U, 32U);
    bt_smp_swap_buf(buf + 32U,     V, 32U);
    buf[64] = Z;
    bt_smp_swap_buf(xs, X, 16U);
    rc = bt_aes_cmac(xs, buf, sizeof buf, out);
    if (rc == TIKU_DRV_OK) {
        uint8_t tmp[16];
        bt_smp_swap_buf(tmp, out, 16U);
        {
            uint8_t i;
            for (i = 0U; i < 16U; ++i) out[i] = tmp[i];
        }
    }
    return rc;
}

/*
 * Two-stage AES-CMAC:
 * T = AES-CMAC(salt, W)
 * where salt = 0x6C888391AAF5A538_60370BDB5A6083BE  (big-endian)
 * MacKey = AES-CMAC(T, 0x00 || "btle" || N1 || N2 || A1 || A2 || 0x0100)
 * LTK    = AES-CMAC(T, 0x01 || "btle" || N1 || N2 || A1 || A2 || 0x0100)
 * The wire byte order for W, N1, N2 and the 6 address bytes inside
 * A1/A2 is LE; those fields are swapped to MSB-first before feeding
 * AES-CMAC (the spec defines the inputs as integer values). The
 * addr_type byte at the head of each A-block, the "btle" keyID, the
 * Counter, and the Length are byte-string constants and pass through
 * unchanged. Outputs are swapped back to LE.
 * Counter is one byte (BE/LE equivalent). Length is the 16-bit value
 * 256 encoded MSB-first (0x01 0x00). The 2-byte Length field is the
 * only place in this function where byte ordering is anti-intuitive.
 */

/**
 * @brief f5 LTK + MacKey derivation (Core Spec Vol 3 Part H 2.2.7)
 *
 * @param W      32-byte DHKey (LE byte order from the chip)
 * @param N1     16-byte initiator nonce (Na, LE)
 * @param N2     16-byte responder nonce (Nb, LE)
 * @param A1     7-byte initiator address (addr_type || addr_LE)
 * @param A2     7-byte responder address
 * @param mackey 16-byte MacKey output (LE byte order)
 * @param ltk    16-byte LTK output (LE byte order)
 */
static int bt_smp_f5(const uint8_t W[32],
                     const uint8_t N1[16], const uint8_t N2[16],
                     const uint8_t A1[7],  const uint8_t A2[7],
                     uint8_t mackey[16], uint8_t ltk[16])
{
    /* salt = 0x6C888391_AAF5A538_60370BDB_5A6083BE   (MSB-first per spec) */
    static const uint8_t salt[16] = {
        0x6CU, 0x88U, 0x83U, 0x91U,
        0xAAU, 0xF5U, 0xA5U, 0x38U,
        0x60U, 0x37U, 0x0BU, 0xDBU,
        0x5AU, 0x60U, 0x83U, 0xBEU,
    };
    uint8_t Ws[32];
    uint8_t T[16];
    uint8_t msg[1 + 4 + 16 + 16 + 7 + 7 + 2];   /* 53 bytes */
    int     rc;
    size_t  off;
    uint8_t tmp[16];

    bt_smp_swap_buf(Ws, W, 32U);
    rc = bt_aes_cmac(salt, Ws, 32U, T);
    if (rc != TIKU_DRV_OK) return rc;

    /* msg layout: Counter(1) || keyID(4) || N1_BE(16) || N2_BE(16)
     *             || addr_type1(1) || addr1_BE(6)
     *             || addr_type2(1) || addr2_BE(6) || Length(2 BE)
     * Note A1/A2 hold {addr_type, addr_LE x6}; addr_type is copied as-is
     * and swap the 6 address bytes to BE. */
    off          = 0U;
    msg[off++]   = 0x00U;       /* Counter -- placeholder, fixed below */
    msg[off++]   = 'b';
    msg[off++]   = 't';
    msg[off++]   = 'l';
    msg[off++]   = 'e';
    bt_smp_swap_buf(&msg[off], N1, 16U); off = (size_t)(off + 16U);
    bt_smp_swap_buf(&msg[off], N2, 16U); off = (size_t)(off + 16U);
    msg[off++]   = A1[0];                            /* addr_type1     */
    bt_smp_swap_buf(&msg[off], &A1[1], 6U); off = (size_t)(off + 6U);
    msg[off++]   = A2[0];                            /* addr_type2     */
    bt_smp_swap_buf(&msg[off], &A2[1], 6U); off = (size_t)(off + 6U);
    msg[off++]   = 0x01U;       /* Length high (BE) = 0x0100 = 256 */
    msg[off++]   = 0x00U;       /* Length low  (BE)                 */

    msg[0] = 0x00U;
    rc = bt_aes_cmac(T, msg, sizeof msg, tmp);
    if (rc != TIKU_DRV_OK) return rc;
    bt_smp_swap_buf(mackey, tmp, 16U);
    msg[0] = 0x01U;
    rc = bt_aes_cmac(T, msg, sizeof msg, tmp);
    if (rc != TIKU_DRV_OK) return rc;
    bt_smp_swap_buf(ltk, tmp, 16U);
    return TIKU_DRV_OK;
}

/*
 * E = AES-CMAC(W, N1 || N2 || R || IOcap || A1 || A2)
 * where W is the MacKey from f5, R is 16 B (zero for Just Works), and
 * IOcap is the 3-byte block {AuthReq, OOB, IOcap}.
 * Byte-order treatment matches f5: integer fields (W, N1, N2, R, and
 * the 6 address bytes within A1/A2) are swapped to MSB-first; the
 * byte-string fields (addr_type at head of each A-block, IOcap)
 * pass through unchanged. Output is swapped back to LE.
 */

/**
 * @brief f6 DH-key check value (Core Spec Vol 3 Part H 2.2.8)
 *
 * @param W      16-byte MacKey (LE)
 * @param N1     16-byte own-side nonce (this side computing E, LE)
 * @param N2     16-byte peer-side nonce (LE)
 * @param R      16-byte randomiser (zero for Just Works)
 * @param IOcap  3-byte IO Capabilities block in {AuthReq,OOB,IOcap} form
 * @param A1     7-byte address (initiator: addr_type || addr_LE)
 * @param A2     7-byte address (responder: addr_type || addr_LE)
 * @param out    16-byte check value (LE byte order)
 */
static int bt_smp_f6(const uint8_t W[16],
                     const uint8_t N1[16], const uint8_t N2[16],
                     const uint8_t R[16],  const uint8_t IOcap[3],
                     const uint8_t A1[7],  const uint8_t A2[7],
                     uint8_t out[16])
{
    uint8_t Ws[16];
    uint8_t msg[16 + 16 + 16 + 3 + 7 + 7];   /* 65 bytes */
    size_t  off = 0U;
    uint8_t tmp[16];
    int     rc;
    bt_smp_swap_buf(Ws, W, 16U);
    bt_smp_swap_buf(&msg[off], N1,    16U); off = (size_t)(off + 16U);
    bt_smp_swap_buf(&msg[off], N2,    16U); off = (size_t)(off + 16U);
    bt_smp_swap_buf(&msg[off], R,     16U); off = (size_t)(off + 16U);
    bt_smp_swap_buf(&msg[off], IOcap,  3U); off = (size_t)(off + 3U);
    msg[off++]   = A1[0];
    bt_smp_swap_buf(&msg[off], &A1[1], 6U); off = (size_t)(off + 6U);
    msg[off++]   = A2[0];
    bt_smp_swap_buf(&msg[off], &A2[1], 6U); off = (size_t)(off + 6U);
    rc = bt_aes_cmac(Ws, msg, sizeof msg, tmp);
    if (rc != TIKU_DRV_OK) return rc;
    bt_smp_swap_buf(out, tmp, 16U);
    return TIKU_DRV_OK;
}

/*---------------------------------------------------------------------------*/
/* SMP test vectors (Core Spec Vol 3 Part H 2.7)                             */
/*---------------------------------------------------------------------------*/
#ifdef BT_SMP_SELFTEST
/* Sample data 2.7 vectors. All input + expected output bytes are in
 * LE wire order (matches NimBLE ble_sm_test.c, which is itself a
 * verbatim copy of the Core Spec 5.x §2.7 sample suite). */
static int bt_smp_selftest(void)
{
    int      pass = 1;
    uint8_t  got[16];
    uint8_t  i;

    /* ---- AES-128 ECB sanity (FIPS-197 known answer + spec-vector)
     * Failing AES here means the chip's HCI_LE_Encrypt isn't doing
     * what it should -- surface it before f4/f5/f6 confuse the picture.
     *   AES(0, 0)                  = 66e94bd4ef8a2c3b884cfa59ca342b2e
     *   AES(FIPS-197 K, FIPS P)    = 3ad77bb40d7a3660a89ecaf32466ef97
     */
    {
        uint8_t k0[16] = {0};
        uint8_t p0[16] = {0};
        uint8_t c[16];
        static const uint8_t expect[16] = {
            0x66,0xe9,0x4b,0xd4,0xef,0x8a,0x2c,0x3b,
            0x88,0x4c,0xfa,0x59,0xca,0x34,0x2b,0x2e,
        };
        int match = 1;
        if (bt_aes128_ecb(k0, p0, c) != TIKU_DRV_OK) {
            TIKU_BT_PRINTF("p14.selftest: AES(0,0) ERR (HCI fail)\n");
            pass = 0;
        } else {
            for (i = 0U; i < 16U; ++i) if (c[i] != expect[i]) match = 0;
            TIKU_BT_PRINTF("p14.selftest: AES(0,0) %s\n",
                            match ? "PASS" : "FAIL");
            if (!match) pass = 0;
        }
    }
    {
        static const uint8_t k[16] = {
            0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
            0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c,
        };
        static const uint8_t p[16] = {
            0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
            0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a,
        };
        static const uint8_t expect[16] = {
            0x3a,0xd7,0x7b,0xb4,0x0d,0x7a,0x36,0x60,
            0xa8,0x9e,0xca,0xf3,0x24,0x66,0xef,0x97,
        };
        uint8_t c[16];
        int match = 1;
        if (bt_aes128_ecb(k, p, c) != TIKU_DRV_OK) {
            TIKU_BT_PRINTF("p14.selftest: AES(FIPS) ERR (HCI fail)\n");
            pass = 0;
        } else {
            for (i = 0U; i < 16U; ++i) if (c[i] != expect[i]) match = 0;
            TIKU_BT_PRINTF("p14.selftest: AES(FIPS) %s\n",
                            match ? "PASS" : "FAIL");
            if (!match) pass = 0;
        }
    }

    /* ---- f4 sample vector ---- */
    {
        static const uint8_t U[32] = {
            0xE6,0x9D,0x35,0x0E,0x48,0x01,0x03,0xCC,
            0xDB,0xFD,0xF4,0xAC,0x11,0x91,0xF4,0xEF,
            0xB9,0xA5,0xF9,0xE9,0xA7,0x83,0x2C,0x5E,
            0x2C,0xBE,0x97,0xF2,0xD2,0x03,0xB0,0x20,
        };
        static const uint8_t V[32] = {
            0xFD,0xC5,0x7F,0xF4,0x49,0xDD,0x4F,0x6B,
            0xFB,0x7C,0x9D,0xF1,0xC2,0x9A,0xCB,0x59,
            0x2A,0xE7,0xD4,0xEE,0xFB,0xFC,0x0A,0x90,
            0x9A,0xBB,0xF6,0x32,0x3D,0x8B,0x18,0x55,
        };
        static const uint8_t X[16] = {
            0xAB,0xAE,0x2B,0x71,0xEC,0xB2,0xFF,0xFF,
            0x3E,0x73,0x77,0xD1,0x54,0x84,0xCB,0xD5,
        };
        static const uint8_t expect_v[16] = {
            0x2D,0x87,0x74,0xA9,0xBE,0xA1,0xED,0xF1,
            0x1C,0xBD,0xA9,0x07,0xF1,0x16,0xC9,0xF2,
        };
        if (bt_smp_f4(U, V, X, 0x00U, got) != TIKU_DRV_OK) {
            TIKU_BT_PRINTF("p14.selftest: f4 ERR (HCI fail)\n");
            pass = 0;
        } else {
            int match = 1;
            for (i = 0U; i < 16U; ++i) if (got[i] != expect_v[i]) match = 0;
            if (!match) {
                TIKU_BT_PRINTF("p14.selftest: f4 FAIL got=");
                for (i = 0U; i < 16U; ++i) TIKU_BT_PRINTF("%02x", got[i]);
                TIKU_BT_PRINTF("\n");
                pass = 0;
            } else {
                TIKU_BT_PRINTF("p14.selftest: f4 PASS\n");
            }
        }
    }

    /* ---- f5 sample vector (NimBLE ble_sm_test_case_f5) ---- */
    {
        static const uint8_t W[32] = {
            0x98,0xA6,0xBF,0x73,0xF3,0x34,0x8D,0x86,
            0xF1,0x66,0xF8,0xB4,0x13,0x6B,0x79,0x99,
            0x9B,0x7D,0x39,0x0A,0xA6,0x10,0x10,0x34,
            0x05,0xAD,0xC8,0x57,0xA3,0x34,0x02,0xEC,
        };
        static const uint8_t N1[16] = {
            0xAB,0xAE,0x2B,0x71,0xEC,0xB2,0xFF,0xFF,
            0x3E,0x73,0x77,0xD1,0x54,0x84,0xCB,0xD5,
        };
        static const uint8_t N2[16] = {
            0xCF,0xC4,0x3D,0xFF,0xF7,0x83,0x65,0x21,
            0x6E,0x5F,0xA7,0x25,0xCC,0xE7,0xE8,0xA6,
        };
        /* A1/A2 = addr_type(1) || addr_LE(6). NimBLE test passes the
         * addresses as 6-byte buffers separately; here the code builds the
         * 7-byte block that bt_smp_f5 takes. */
        static const uint8_t A1[7] = {
            0x00, 0xCE,0xBF,0x37,0x37,0x12,0x56,
        };
        static const uint8_t A2[7] = {
            0x00, 0xC1,0xCF,0x2D,0x70,0x13,0xA7,
        };
        static const uint8_t mackey_exp[16] = {
            0x20,0x6E,0x63,0xCE,0x20,0x6A,0x3F,0xFD,
            0x02,0x4A,0x08,0xA1,0x76,0xF1,0x65,0x29,
        };
        static const uint8_t ltk_exp[16] = {
            0x38,0x0A,0x75,0x94,0xB5,0x22,0x05,0x98,
            0x23,0xCD,0xD7,0x69,0x11,0x79,0x86,0x69,
        };
        uint8_t mackey[16], ltk[16];
        int     match;
        if (bt_smp_f5(W, N1, N2, A1, A2, mackey, ltk) != TIKU_DRV_OK) {
            TIKU_BT_PRINTF("p14.selftest: f5 ERR (HCI fail)\n");
            pass = 0;
        } else {
            match = 1;
            for (i = 0U; i < 16U; ++i) if (mackey[i] != mackey_exp[i]) match = 0;
            for (i = 0U; i < 16U; ++i) if (ltk[i]    != ltk_exp[i])    match = 0;
            if (!match) {
                TIKU_BT_PRINTF("p14.selftest: f5 FAIL got_mackey=");
                for (i = 0U; i < 16U; ++i) TIKU_BT_PRINTF("%02x", mackey[i]);
                TIKU_BT_PRINTF(" got_ltk=");
                for (i = 0U; i < 16U; ++i) TIKU_BT_PRINTF("%02x", ltk[i]);
                TIKU_BT_PRINTF("\n");
                pass = 0;
            } else {
                TIKU_BT_PRINTF("p14.selftest: f5 PASS\n");
            }
        }
    }

    /* ---- f6 sample vector ---- */
    {
        static const uint8_t W[16] = {
            0x20,0x6E,0x63,0xCE,0x20,0x6A,0x3F,0xFD,
            0x02,0x4A,0x08,0xA1,0x76,0xF1,0x65,0x29,
        };
        static const uint8_t N1[16] = {
            0xAB,0xAE,0x2B,0x71,0xEC,0xB2,0xFF,0xFF,
            0x3E,0x73,0x77,0xD1,0x54,0x84,0xCB,0xD5,
        };
        static const uint8_t N2[16] = {
            0xCF,0xC4,0x3D,0xFF,0xF7,0x83,0x65,0x21,
            0x6E,0x5F,0xA7,0x25,0xCC,0xE7,0xE8,0xA6,
        };
        static const uint8_t R[16] = {
            0xC8,0x0F,0x2D,0x0C,0xD2,0x42,0xDA,0x08,
            0x54,0xBB,0x53,0xB4,0x3B,0x34,0xA3,0x12,
        };
        static const uint8_t IOcap[3] = { 0x02, 0x01, 0x01 };
        static const uint8_t A1[7]    = { 0x00,0xCE,0xBF,0x37,0x37,0x12,0x56 };
        static const uint8_t A2[7]    = { 0x00,0xC1,0xCF,0x2D,0x70,0x13,0xA7 };
        static const uint8_t expect_v[16] = {
            0x61,0x8F,0x95,0xDA,0x09,0x0B,0x6C,0xD2,
            0xC5,0xE8,0xD0,0x9C,0x98,0x73,0xC4,0xE3,
        };
        if (bt_smp_f6(W, N1, N2, R, IOcap, A1, A2, got) != TIKU_DRV_OK) {
            TIKU_BT_PRINTF("p14.selftest: f6 ERR (HCI fail)\n");
            pass = 0;
        } else {
            int match = 1;
            for (i = 0U; i < 16U; ++i) if (got[i] != expect_v[i]) match = 0;
            if (!match) {
                TIKU_BT_PRINTF("p14.selftest: f6 FAIL got=");
                for (i = 0U; i < 16U; ++i) TIKU_BT_PRINTF("%02x", got[i]);
                TIKU_BT_PRINTF("\n");
                pass = 0;
            } else {
                TIKU_BT_PRINTF("p14.selftest: f6 PASS\n");
            }
        }
    }

    TIKU_BT_PRINTF("p14.selftest: %s\n", pass ? "ALL PASS" : "FAIL");
    return pass;
}
#endif /* BT_SMP_SELFTEST */

/*---------------------------------------------------------------------------*/
/* SMP session helpers                                                       */
/*---------------------------------------------------------------------------*/

/*
 * addr_type(1) || addr_LE(6).
 * Source @p addr_msb is in MSB-first display order (as cached in
 * conns[].peer_addr / bt_state.bd_addr); the LE order is restored
 * *  by reversing the 6 address bytes.
 */

/**
 * Build the 7-byte A_init / A_resp block for f5 / f6:
 */
static void bt_smp_addr_block(uint8_t addr_type,
                              const uint8_t addr_msb[6],
                              uint8_t out[7])
{
    int k;
    out[0] = addr_type;
    for (k = 0; k < 6; ++k) out[1 + k] = addr_msb[5 - k];
}

/** Send SMP Pairing Failed and reset the session to IDLE. */
static void bt_smp_send_failed(uint8_t conn_idx, uint8_t reason)
{
    uint8_t rsp[2];
    rsp[0] = SMP_OP_PAIRING_FAILED;
    rsp[1] = reason;
    (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                      L2CAP_CID_SMP, rsp, sizeof rsp);
    bt_state.smp[conn_idx].state            = SMP_IDLE;
    bt_state.smp[conn_idx].have_pubkey      = 0U;
    bt_state.smp[conn_idx].have_dhkey       = 0U;
    bt_state.smp[conn_idx].have_peer_pubkey = 0U;
    bt_state.smp[conn_idx].have_peer_random = 0U;
    bt_state.smp[conn_idx].pending_pubkey_tx = 0U;
    bt_state.smp[conn_idx].pending_f5       = 0U;
    TIKU_BT_PRINTF("p14.smp: pairing failed reason=0x%02x\n", reason);
}

/** Forward decl -- bond find helper; full implementation is alongside
 *  the bond store at the bottom of the file. */
static int bt_bond_find_by_addr(uint8_t addr_type, const uint8_t addr_le[6],
                                tiku_bt_bond_record_t *out);

/*
 * replies with Command Status then later a LE Meta subevent 0x08
 * carrying the 64-byte public key. Synchronous Command Status check
 * *  only; the actual pubkey arrives async via bt_handle_le_meta.
 */

/**
 * Issue HCI_LE_Read_Local_P256_Public_Key (no params). The chip
 */
static int bt_smp_request_local_pubkey(void)
{
    /* Read_Local_P256_Public_Key has no params and replies with
     * Command Status. Send raw rather than via bt_hci_cmd_response
     * (which polls for Command Complete). */
    uint8_t cmd[4];
    cmd[0] = HCI_PKT_TYPE_CMD;
    cmd[1] = (uint8_t)(HCI_OP_LE_READ_LOCAL_P256_PUBKEY & 0xFFU);
    cmd[2] = (uint8_t)((HCI_OP_LE_READ_LOCAL_P256_PUBKEY >> 8) & 0xFFU);
    cmd[3] = 0U;
    return tiku_bt_send(cmd, sizeof cmd);
}

/*
 * and key_type=0 (private key from prior P256_Public_Key). Replies
 * with Command Status then later LE Meta subevent 0x09 carrying the
 * *  32-byte DHKey.
 */

/**
 * Issue HCI_LE_Generate_DHKey_V2 with the peer's 64-byte public key
 */
static int bt_smp_request_dhkey(const uint8_t peer_pubkey[64])
{
    uint8_t cmd[4 + 65];
    uint8_t i;
    cmd[0] = HCI_PKT_TYPE_CMD;
    cmd[1] = (uint8_t)(HCI_OP_LE_GENERATE_DHKEY_V2 & 0xFFU);
    cmd[2] = (uint8_t)((HCI_OP_LE_GENERATE_DHKEY_V2 >> 8) & 0xFFU);
    cmd[3] = 65U;
    for (i = 0U; i < 64U; ++i) cmd[4U + i] = peer_pubkey[i];
    cmd[68] = 0x00U;   /* key_type = 0 (use prior P256 private key) */
    return tiku_bt_send(cmd, sizeof cmd);
}

/*
 * are all in hand. Idempotent: ignores calls where prerequisites
 * aren't set yet. Returns 0 on success (or already done), -1 on
 * cryptographic failure. Does NOT emit anything on the wire --
 * *  emission of Eb happens only after Ea arrives.
 */

/**
 * Derive MacKey + LTK via f5 once {DHKey, peer_nonce, local_nonce}
 */
static int bt_smp_try_derive_keys(uint8_t conn_idx)
{
    uint8_t Na[16], Nb[16];
    uint8_t A_init[7], A_resp[7];
    uint8_t i;
    int     rc;
    if (!bt_state.smp[conn_idx].have_dhkey)        return 0;
    if (!bt_state.smp[conn_idx].pending_f5)        return 0;
    if (!bt_state.smp[conn_idx].have_peer_random)  return 0;

    for (i = 0U; i < 16U; ++i) Na[i] = bt_state.smp[conn_idx].peer_nonce[i];
    for (i = 0U; i < 16U; ++i) Nb[i] = bt_state.smp[conn_idx].local_nonce[i];

    /* Peripheral: A_init = central (peer), A_resp = local. */
    bt_smp_addr_block(bt_state.smp[conn_idx].peer_addr_type,
                      bt_state.conns[conn_idx].info.peer_addr, A_init);
    bt_smp_addr_block(0x00U /* public */,
                      bt_state.bd_addr, A_resp);

    rc = bt_smp_f5(bt_state.smp[conn_idx].dhkey,
                   Na, Nb, A_init, A_resp,
                   bt_state.smp[conn_idx].mackey,
                   bt_state.smp[conn_idx].ltk);
    if (rc != TIKU_DRV_OK) {
        TIKU_BT_PRINTF("p14.smp: f5 FAIL rc=%d\n", rc);
        bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
        return -1;
    }
    bt_state.smp[conn_idx].pending_f5 = 0U;
    TIKU_BT_PRINTF("p14.smp: f5 derived MacKey + LTK\n");
    return 0;
}

/** Compute the DH check Eb and emit it as SMP_OP_PAIRING_DHKEY_CHECK.
 *  Called only after the peer's Ea is validated -- per spec,
 *  responder sends Eb in response to receiving Ea, not before. */
static int bt_smp_send_dhkey_check(uint8_t conn_idx)
{
    uint8_t Na[16], Nb[16];
    uint8_t A_init[7], A_resp[7];
    uint8_t Eb[16];
    static const uint8_t R0[16] = {0};
    uint8_t IOcap_b[3];
    uint8_t i;
    int     rc;
    for (i = 0U; i < 16U; ++i) Na[i] = bt_state.smp[conn_idx].peer_nonce[i];
    for (i = 0U; i < 16U; ++i) Nb[i] = bt_state.smp[conn_idx].local_nonce[i];
    bt_smp_addr_block(bt_state.smp[conn_idx].peer_addr_type,
                      bt_state.conns[conn_idx].info.peer_addr, A_init);
    bt_smp_addr_block(0x00U, bt_state.bd_addr, A_resp);
    /* Eb = f6(MacKey, Nb, Na, 0, IOcapB, B, A): own nonce first per
     * spec 2.2.8. IOcap in f6 order = {AuthReq, OOB, IOcap}. */
    IOcap_b[0] = bt_state.smp[conn_idx].own_iocap[2]; /* AuthReq */
    IOcap_b[1] = bt_state.smp[conn_idx].own_iocap[1]; /* OOB     */
    IOcap_b[2] = bt_state.smp[conn_idx].own_iocap[0]; /* IOCap   */
    rc = bt_smp_f6(bt_state.smp[conn_idx].mackey,
                   Nb, Na, R0, IOcap_b, A_resp, A_init, Eb);
    if (rc != TIKU_DRV_OK) {
        TIKU_BT_PRINTF("p14.smp: f6(Eb) FAIL rc=%d\n", rc);
        return rc;
    }
    {
        uint8_t pdu[17];
        pdu[0] = SMP_OP_PAIRING_DHKEY_CHECK;
        for (i = 0U; i < 16U; ++i) pdu[1U + i] = Eb[i];
        (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                          L2CAP_CID_SMP, pdu, sizeof pdu);
        TIKU_BT_PRINTF("p14.smp: sent DH Check Eb\n");
    }
    return TIKU_DRV_OK;
}

/** Once both the local pubkey (from the chip) and the peer's are in hand,
 *  send the local pubkey out and emit the Pairing Confirm Cb. */
static void bt_smp_try_send_pubkey_confirm(uint8_t conn_idx)
{
    if (!bt_state.smp[conn_idx].pending_pubkey_tx)         return;
    if (!bt_state.smp[conn_idx].have_pubkey)               return;

    /* Send the local public key (opcode 0x0C + 64 B in LE order). */
    {
        uint8_t pdu[65];
        uint8_t i;
        pdu[0] = SMP_OP_PAIRING_PUBLIC_KEY;
        for (i = 0U; i < 64U; ++i) {
            pdu[1U + i] = bt_state.smp[conn_idx].local_pubkey[i];
        }
        (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                          L2CAP_CID_SMP, pdu, sizeof pdu);
        TIKU_BT_PRINTF("p14.smp: sent local Public Key\n");
    }
    bt_state.smp[conn_idx].pending_pubkey_tx = 0U;

    /* Compute Cb = f4(PKbx, PKax, Nb, 0) and send Pairing Confirm.
     *   PKbx = local pubkey X (= U for the confirming side)
     *   PKax = peer  pubkey X (= V) */
    {
        uint8_t Cb[16];
        int     rc;
        rc = bt_smp_f4(bt_state.smp[conn_idx].local_pubkey,
                       bt_state.smp[conn_idx].peer_pubkey,
                       bt_state.smp[conn_idx].local_nonce,
                       0x00U, Cb);
        if (rc != TIKU_DRV_OK) {
            TIKU_BT_PRINTF("p14.smp: f4(Cb) FAIL rc=%d\n", rc);
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        {
            uint8_t pdu[17];
            uint8_t i;
            pdu[0] = SMP_OP_PAIRING_CONFIRM;
            for (i = 0U; i < 16U; ++i) pdu[1U + i] = Cb[i];
            (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                              L2CAP_CID_SMP, pdu, sizeof pdu);
            TIKU_BT_PRINTF("p14.smp: sent Pairing Confirm Cb\n");
        }
    }
    bt_state.smp[conn_idx].state = SMP_WAITING_RANDOM;
}

/*---------------------------------------------------------------------------*/
/* SMP main handler                                                          */
/*---------------------------------------------------------------------------*/
/**
 * @brief Handle one incoming SMP PDU on L2CAP CID 6
 *
 * Drives the LE-SC Just-Works peripheral pairing state machine. See
 * the comment block above bt_aes128_ecb for the full sequence.
 *
 * @param conn_idx  Connection table index
 * @param pdu       SMP PDU bytes (opcode at [0])
 * @param len       Length of @p pdu in bytes
 */
static void bt_handle_smp(uint8_t conn_idx, const uint8_t *pdu,
                          uint16_t len)
{
    if (len < 1U) return;
    switch (pdu[0]) {
    case SMP_OP_PAIRING_REQUEST: {
        if (len < 7U) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        /* Peer's IOcap block sits at pdu[1..3]: IOCap, OOB, AuthReq.
         * SMP Pairing Request octet order is IOcap || OOB || AuthReq
         * but the IOcap input to f6 is AuthReq || OOB || IOcap --
         * keep that ordering distinction crisp by storing as on-wire
         * (octet 1..3) and re-laying-out into f6 form on use. */
        bt_state.smp[conn_idx].peer_iocap[0] = pdu[1]; /* IOCap   */
        bt_state.smp[conn_idx].peer_iocap[1] = pdu[2]; /* OOB     */
        bt_state.smp[conn_idx].peer_iocap[2] = pdu[3]; /* AuthReq */
        /* Legacy pairing not supported: refuse if peer didn't set SC. */
        if ((pdu[3] & SMP_AUTHREQ_SC) == 0U) {
            bt_smp_send_failed(conn_idx, SMP_ERR_PAIRING_NOT_SUPPORTED);
            TIKU_BT_PRINTF("p14.smp: peer requested legacy pairing "
                            "(AuthReq=0x%02x); LE-SC only\n", pdu[3]);
            return;
        }
        /* Build Pairing Response: IOCap=NoInputNoOutput (0x03),
         * OOB=0, AuthReq=SC|Bonding, MaxKeySize=16. KeyDist masks
         * echo what the initiator (phone) requested in the Pairing
         * Request, ANDed with what this stack can practically send --
         * today none, but echoing the bits lets the phone consider
         * the bond legitimate. Phones disconnect right after Pairing
         * Response if 0x00 comes back here (interpretation: "this peer
         * has nothing to offer"). Mask to IdKey + SignKey only --
         * EncKey is legacy and irrelevant under LE Secure Connections. */
        {
            uint8_t rsp[7];
            uint8_t peer_initkd = (len >= 6U) ? pdu[5] : 0U;
            uint8_t peer_respkd = (len >= 7U) ? pdu[6] : 0U;
            rsp[0] = SMP_OP_PAIRING_RESPONSE;
            rsp[1] = 0x03U;                           /* IOCap NoIN/NoOUT */
            rsp[2] = 0x00U;                           /* OOB no           */
            rsp[3] = (uint8_t)(SMP_AUTHREQ_SC | 0x01U); /* SC + Bonding   */
            rsp[4] = 16U;                             /* MaxKeySize       */
            rsp[5] = (uint8_t)(peer_initkd & 0x06U);  /* IdKey + SignKey  */
            rsp[6] = (uint8_t)(peer_respkd & 0x06U);
            bt_state.smp[conn_idx].own_iocap[0] = rsp[1];
            bt_state.smp[conn_idx].own_iocap[1] = rsp[2];
            bt_state.smp[conn_idx].own_iocap[2] = rsp[3];
            (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                              L2CAP_CID_SMP, rsp, sizeof rsp);
        }
        /* Cache peer addr in LE form for f5/f6 / bond keying. */
        {
            int k;
            for (k = 0; k < 6; ++k) {
                bt_state.smp[conn_idx].peer_addr_le[k] =
                    bt_state.conns[conn_idx].info.peer_addr[5 - k];
            }
            bt_state.smp[conn_idx].peer_addr_type =
                bt_state.conns[conn_idx].info.peer_addr_type;
        }
        /* Pull entropy and pre-arm chip-side P-256 public key fetch. */
        if (bt_rand_bytes(bt_state.smp[conn_idx].local_nonce, 16U) != 0) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        bt_state.smp[conn_idx].state             = SMP_WAITING_PUBKEY;
        bt_state.smp[conn_idx].have_pubkey       = 0U;
        bt_state.smp[conn_idx].have_dhkey        = 0U;
        bt_state.smp[conn_idx].have_peer_pubkey  = 0U;
        bt_state.smp[conn_idx].have_peer_random  = 0U;
        bt_state.smp[conn_idx].pending_pubkey_tx = 0U;
        bt_state.smp[conn_idx].pending_f5        = 0U;
        if (bt_smp_request_local_pubkey() != TIKU_DRV_OK) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        TIKU_BT_PRINTF("p14.smp: Pairing Request -> Pairing Response "
                        "(LE-SC Just-Works)\n");
        break;
    }

    case SMP_OP_PAIRING_PUBLIC_KEY: {
        uint8_t i;
        if (len < 65U) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        if (bt_state.smp[conn_idx].state != SMP_WAITING_PUBKEY) {
            TIKU_BT_PRINTF("p14.smp: unexpected Public Key (state=%u)\n",
                            bt_state.smp[conn_idx].state);
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        for (i = 0U; i < 64U; ++i) {
            bt_state.smp[conn_idx].peer_pubkey[i] = pdu[1U + i];
        }
        bt_state.smp[conn_idx].have_peer_pubkey  = 1U;
        bt_state.smp[conn_idx].pending_pubkey_tx = 1U;
        /* Kick chip-side DHKey computation immediately so it overlaps
         * with the SMP confirm-exchange. */
        if (bt_smp_request_dhkey(bt_state.smp[conn_idx].peer_pubkey)
            != TIKU_DRV_OK) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        /* If the local pubkey is already in hand from the chip, fire off
         * pubkey + confirm now. Otherwise the pubkey-complete handler
         * will trigger it later. */
        bt_smp_try_send_pubkey_confirm(conn_idx);
        TIKU_BT_PRINTF("p14.smp: received peer Public Key\n");
        break;
    }

    case SMP_OP_PAIRING_RANDOM: {
        uint8_t i;
        if (len < 17U) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        if (bt_state.smp[conn_idx].state != SMP_WAITING_RANDOM) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        for (i = 0U; i < 16U; ++i) {
            bt_state.smp[conn_idx].peer_nonce[i] = pdu[1U + i];
        }
        bt_state.smp[conn_idx].have_peer_random = 1U;
        /* Send the Pairing Random (Nb). */
        {
            uint8_t pdu_out[17];
            pdu_out[0] = SMP_OP_PAIRING_RANDOM;
            for (i = 0U; i < 16U; ++i) {
                pdu_out[1U + i] = bt_state.smp[conn_idx].local_nonce[i];
            }
            (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                              L2CAP_CID_SMP, pdu_out, sizeof pdu_out);
            TIKU_BT_PRINTF("p14.smp: sent Pairing Random Nb\n");
        }
        bt_state.smp[conn_idx].state      = SMP_WAITING_DHCHECK;
        bt_state.smp[conn_idx].pending_f5 = 1U;
        (void)bt_smp_try_derive_keys(conn_idx);
        break;
    }

    case SMP_OP_PAIRING_DHKEY_CHECK: {
        uint8_t Na[16], Nb[16];
        uint8_t A_init[7], A_resp[7];
        uint8_t Ea_exp[16];
        int     rc;
        uint8_t i;
        if (len < 17U) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        if (bt_state.smp[conn_idx].state != SMP_WAITING_DHCHECK) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        for (i = 0U; i < 16U; ++i) Na[i] = bt_state.smp[conn_idx].peer_nonce[i];
        for (i = 0U; i < 16U; ++i) Nb[i] = bt_state.smp[conn_idx].local_nonce[i];
        bt_smp_addr_block(bt_state.smp[conn_idx].peer_addr_type,
                          bt_state.conns[conn_idx].info.peer_addr, A_init);
        bt_smp_addr_block(0x00U, bt_state.bd_addr, A_resp);
        /* Expected Ea = f6(MacKey, Na, Nb, 0, IOcapA, A_init, A_resp).
         * IOcapA is the peer's (initiator) IO cap from Pairing Request,
         * BUT in f6 order: AuthReq || OOB || IOCap. */
        {
            static const uint8_t R0[16] = {0};
            uint8_t IOcap_a[3];
            IOcap_a[0] = bt_state.smp[conn_idx].peer_iocap[2]; /* AuthReq */
            IOcap_a[1] = bt_state.smp[conn_idx].peer_iocap[1]; /* OOB     */
            IOcap_a[2] = bt_state.smp[conn_idx].peer_iocap[0]; /* IOCap   */
            rc = bt_smp_f6(bt_state.smp[conn_idx].mackey,
                           Na, Nb, R0, IOcap_a, A_init, A_resp, Ea_exp);
        }
        if (rc != TIKU_DRV_OK) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        {
            int match = 1;
            for (i = 0U; i < 16U; ++i) {
                if (Ea_exp[i] != pdu[1U + i]) match = 0;
            }
            if (!match) {
                bt_smp_send_failed(conn_idx, SMP_ERR_DHKEY_CHECK_FAILED);
                return;
            }
        }
        /* Peer's Ea is valid. Compute and emit Eb -- spec
         * requires responder to send DHKey Check in response to
         * receiving the initiator's DHKey Check. */
        if (bt_smp_send_dhkey_check(conn_idx) != TIKU_DRV_OK) {
            bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
            return;
        }
        /* Persist bond + wait for LE LTK Request (the chip raises it
         * once the central issues LL_ENC_REQ on the link). */
        {
            tiku_bt_bond_record_t rec;
            uint8_t k;
            rec.magic          = TIKU_BT_BOND_MAGIC;
            rec.peer_addr_type = bt_state.smp[conn_idx].peer_addr_type;
            /* Bond record stores addr in MSB-first display order
             * (matches scan / conn API for easy lookup). */
            for (k = 0U; k < 6U; ++k) {
                rec.peer_addr[k] =
                    bt_state.conns[conn_idx].info.peer_addr[k];
            }
            rec._pad = 0U;
            for (k = 0U; k < 16U; ++k) {
                rec.ltk[k] = bt_state.smp[conn_idx].ltk[k];
            }
            rec.flags = (uint32_t)SMP_AUTHREQ_SC;    /* SC, no MITM */
            (void)tiku_bt_bond_save(0U, &rec);
            TIKU_BT_PRINTF("p14.smp: bond saved slot 0\n");
        }
        bt_state.smp[conn_idx].state = SMP_ENCRYPTING;
        break;
    }

    case SMP_OP_PAIRING_FAILED: {
        if (len >= 2U) {
            TIKU_BT_PRINTF("p14.smp: peer sent Pairing Failed "
                            "reason=0x%02x\n", pdu[1]);
        }
        bt_state.smp[conn_idx].state            = SMP_IDLE;
        bt_state.smp[conn_idx].have_pubkey      = 0U;
        bt_state.smp[conn_idx].have_dhkey       = 0U;
        bt_state.smp[conn_idx].have_peer_pubkey = 0U;
        bt_state.smp[conn_idx].have_peer_random = 0U;
        bt_state.smp[conn_idx].pending_pubkey_tx = 0U;
        bt_state.smp[conn_idx].pending_f5       = 0U;
        break;
    }

    case SMP_OP_SECURITY_REQUEST:
        /* From an already-paired central; honoured by
         * doing nothing here (the chip drives the encryption restart
         * via LE LTK Request when the central asks). */
        TIKU_BT_PRINTF("p14.smp: Security Request received (ignored)\n");
        break;

    default:
        TIKU_BT_PRINTF("p14.smp: unknown opcode 0x%02x\n", pdu[0]);
        bt_smp_send_failed(conn_idx, SMP_ERR_UNSPECIFIED_REASON);
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* LE LTK Request -- central asks for the bond LTK                          */
/*---------------------------------------------------------------------------*/
/*
 * Triggered by the central via LL_ENC_REQ; the chip surfaces it as
 * LE Meta subevent 0x05. The bond is looked up by peer addr (or falls
 * back to the in-memory SMP session if pairing just completed) and the
 * LTK returned -- or an LTK Request Negative Reply is sent when no key
 * is held for this peer.
 */

/**
 * @brief Handle one LE Long Term Key Request meta-event
 *
 * @param pkt  HCI event bytes including the type byte at offset 0
 * @param len  Length of @p pkt in bytes
 */
static void bt_handle_le_ltk_request(const uint8_t *pkt, int len)
{
    /* Layout (after pkt[3]=subevt 0x05):
     *   [4..5]   connection handle (LE)
     *   [6..13]  random_number (8 B, LE)
     *   [14..15] ediv (2 B, LE)
     * For LE-SC, random_number and ediv are both 0. */
    uint16_t handle;
    int      conn_idx;
    uint8_t  ltk[16];
    int      have_ltk = 0;
    uint8_t  k;

    if (len < 16) return;
    handle   = (uint16_t)(pkt[4] | ((uint16_t)pkt[5] << 8));
    conn_idx = bt_conn_find(handle);
    if (conn_idx < 0) {
        TIKU_BT_PRINTF("p14.smp: LE LTK Request for unknown handle "
                        "0x%04x\n", handle);
        return;
    }

    /* Try in-memory session first (just-paired path). */
    if (bt_state.smp[conn_idx].state == SMP_ENCRYPTING
        || bt_state.smp[conn_idx].state == SMP_ENCRYPTED) {
        for (k = 0U; k < 16U; ++k) ltk[k] = bt_state.smp[conn_idx].ltk[k];
        have_ltk = 1;
    } else {
        /* Reconnect path: look up by peer addr. */
        tiku_bt_bond_record_t rec;
        uint8_t addr_le[6];
        int     j;
        for (j = 0; j < 6; ++j) {
            addr_le[j] =
                bt_state.conns[conn_idx].info.peer_addr[5 - j];
        }
        if (bt_bond_find_by_addr(
                bt_state.conns[conn_idx].info.peer_addr_type,
                addr_le, &rec) == TIKU_DRV_OK) {
            for (k = 0U; k < 16U; ++k) ltk[k] = rec.ltk[k];
            have_ltk = 1;
            /* Hydrate the SMP session so the ENCRYPTED state lands. */
            for (k = 0U; k < 16U; ++k) {
                bt_state.smp[conn_idx].ltk[k] = ltk[k];
            }
        }
    }

    if (have_ltk) {
        uint8_t cmd[4 + 18];
        cmd[0] = HCI_PKT_TYPE_CMD;
        cmd[1] = (uint8_t)(HCI_OP_LE_LTK_REQUEST_REPLY & 0xFFU);
        cmd[2] = (uint8_t)((HCI_OP_LE_LTK_REQUEST_REPLY >> 8) & 0xFFU);
        cmd[3] = 18U;
        cmd[4] = (uint8_t)(handle & 0xFFU);
        cmd[5] = (uint8_t)((handle >> 8) & 0xFFU);
        for (k = 0U; k < 16U; ++k) cmd[6U + k] = ltk[k];
        (void)tiku_bt_send(cmd, sizeof cmd);
        bt_state.smp[conn_idx].state = SMP_ENCRYPTING;
        TIKU_BT_PRINTF("p14.smp: LE LTK Request -> Reply with stored "
                        "LTK (handle=0x%04x)\n", handle);
    } else {
        uint8_t cmd[4 + 2];
        cmd[0] = HCI_PKT_TYPE_CMD;
        cmd[1] = (uint8_t)(HCI_OP_LE_LTK_REQUEST_NEGATIVE_REPLY & 0xFFU);
        cmd[2] = (uint8_t)((HCI_OP_LE_LTK_REQUEST_NEGATIVE_REPLY >> 8) & 0xFFU);
        cmd[3] = 2U;
        cmd[4] = (uint8_t)(handle & 0xFFU);
        cmd[5] = (uint8_t)((handle >> 8) & 0xFFU);
        (void)tiku_bt_send(cmd, sizeof cmd);
        TIKU_BT_PRINTF("p14.smp: LE LTK Request -> Negative Reply "
                        "(no bond, handle=0x%04x)\n", handle);
    }
}

/*---------------------------------------------------------------------------*/
/* GAP advertising (phase 7) helpers                                         */
/*---------------------------------------------------------------------------*/
/*
 * Bluetooth Core Spec, Vol 6 Part B 2.3.1 (Advertising Channel PDU
 * payload) caps the AD records at 31 bytes for legacy advertising.
 * Each AD record is `[len][type][data...]` where `len` covers `type`
 * + `data`. The advertising data therefore lays out as:
 *
 *   [02][AD_TYPE_FLAGS][AD_FLAGS_LE_GENERAL_DISC]
 *   [N+1][AD_TYPE_COMPLETE_LOCAL_NAME][name × N]
 *
 * That's 3 + 2 + N bytes; the chip pads the remaining bytes to 31
 * with zeros (the full 31 go out in the LE_Set_Advertising_Data
 * payload anyway).
 */

/** Length of the local-name string, capped at the advertising limit. */
static uint8_t bt_strlen_capped(const char *s, uint8_t cap)
{
    uint8_t n = 0U;
    while (n < cap && s[n] != '\0') ++n;
    return n;
}

/**
 * @brief Issue the three-step LE advertising-bring-up command sequence
 *
 * Each command's Command Complete event is awaited and the status
 * byte checked; a non-zero status anywhere aborts the chain. Caller
 * should leave bt_state.advertising clear when this returns non-OK.
 *
 * @param name      Local name to embed in the Complete Local Name AD
 *                  record. Need NOT be NUL-terminated; @p name_len is
 *                  authoritative.
 * @param name_len  Length of @p name in bytes (1..26).
 * @return TIKU_DRV_OK on success, TIKU_DRV_ERR_INVALID if any of the
 *         three Command Complete events reports a non-zero status,
 *         or a transport error from bt_hci_cmd_response().
 */
static int bt_advertise_setup(const char *name, uint8_t name_len)
{
    int     rc;
    int     n;
    uint8_t evt[64];

    /* Step 0: belt-and-braces LE_Set_Advertising_Enable(0). The chip
     * returns "Command Disallowed" (status 0x0C) on Set_Adv_Params
     * if its internal adv state machine is mid-transition (most
     * commonly: right after a connection drop, where the
     * link-layer cleanup hasn't fully released the adv state yet).
     * Disabling first puts the chip into a known state. Ignore
     * the rc -- it'll fail harmlessly if adv was already off. */
    {
        uint8_t en0 = 0U;
        (void)bt_hci_cmd_response(HCI_OP_LE_SET_ADV_ENABLE, &en0, 1U,
                                  evt, sizeof evt, &n);
    }

    /* Step 1: LE_Set_Advertising_Parameters (15 bytes).
     *   interval_min = 0x00A0 (100 ms, 0.625 ms units)
     *   interval_max = 0x00F0 (150 ms)
     *   adv_type     = 0x00 ADV_IND (connectable, scannable, undirected)
     *   own_addr_type= 0x00 public (chip's BD_ADDR)
     *   peer_*       = 0 (only used for directed advertising)
     *   channel_map  = 0x07 (all three advertising channels 37/38/39)
     *   filter       = 0x00 accept all
     */
    {
        const uint8_t params[15] = {
            0xA0U, 0x00U,                     /* interval_min LE */
            0xF0U, 0x00U,                     /* interval_max LE */
            0x00U,                            /* adv type ADV_IND */
            0x00U,                            /* own_addr_type public */
            0x00U,                            /* peer_addr_type */
            0x00U, 0x00U, 0x00U, 0x00U,
            0x00U, 0x00U,                     /* peer_addr (zeros) */
            0x07U,                            /* channel map all */
            0x00U,                            /* filter policy */
        };
        rc = bt_hci_cmd_response(HCI_OP_LE_SET_ADV_PARAMS,
                                 params, sizeof params,
                                 evt, sizeof evt, &n);
        if (rc != TIKU_DRV_OK || n < 7 || evt[6] != 0x00U) {
            TIKU_BT_PRINTF("p7.A: LE_Set_Adv_Params FAIL rc=%d "
                            "status=0x%02x\n", rc,
                            (n >= 7) ? evt[6] : 0xFFU);
            return (rc == TIKU_DRV_OK) ? TIKU_DRV_ERR_INVALID : rc;
        }
        TIKU_BT_PRINTF("p7.A: LE_Set_Adv_Params OK "
                        "(100..150ms, ADV_IND)\n");
    }

    /* Step 2: LE_Set_Advertising_Data (32 bytes: 1 len + 31 data). */
    {
        uint8_t  payload[32] = {0};
        uint8_t  data[31]    = {0};
        uint8_t  off         = 0U;

        /* Flags AD record (3 bytes): len, type, value. */
        data[off++] = 0x02U;
        data[off++] = AD_TYPE_FLAGS;
        data[off++] = AD_FLAGS_LE_GENERAL_DISC;

        /* Complete Local Name AD record (2 + name_len bytes). */
        data[off++] = (uint8_t)(1U + name_len);
        data[off++] = AD_TYPE_COMPLETE_LOCAL_NAME;
        {
            uint8_t i;
            for (i = 0U; i < name_len; ++i) data[off + i] = (uint8_t)name[i];
            off = (uint8_t)(off + name_len);
        }

        payload[0] = off;                /* Advertising_Data_Length */
        {
            uint8_t i;
            for (i = 0U; i < 31U; ++i) payload[1U + i] = data[i];
        }

        rc = bt_hci_cmd_response(HCI_OP_LE_SET_ADV_DATA,
                                 payload, sizeof payload,
                                 evt, sizeof evt, &n);
        if (rc != TIKU_DRV_OK || n < 7 || evt[6] != 0x00U) {
            TIKU_BT_PRINTF("p7.B: LE_Set_Adv_Data FAIL rc=%d "
                            "status=0x%02x\n", rc,
                            (n >= 7) ? evt[6] : 0xFFU);
            return (rc == TIKU_DRV_OK) ? TIKU_DRV_ERR_INVALID : rc;
        }
        TIKU_BT_PRINTF("p7.B: LE_Set_Adv_Data OK (%u B used "
                        "of 31)\n", off);
    }

    /* Step 3: LE_Set_Advertising_Enable(1). */
    {
        uint8_t en = 1U;
        rc = bt_hci_cmd_response(HCI_OP_LE_SET_ADV_ENABLE,
                                 &en, 1U, evt, sizeof evt, &n);
        if (rc != TIKU_DRV_OK || n < 7 || evt[6] != 0x00U) {
            TIKU_BT_PRINTF("p7.C: LE_Set_Adv_Enable(1) FAIL rc=%d "
                            "status=0x%02x\n", rc,
                            (n >= 7) ? evt[6] : 0xFFU);
            return (rc == TIKU_DRV_OK) ? TIKU_DRV_ERR_INVALID : rc;
        }
        /* Cache for the ATT layer's Device Name response. */
        {
            uint8_t i;
            for (i = 0U; i < name_len; ++i) bt_state.adv_name[i] = name[i];
            bt_state.adv_name_len = name_len;
        }
        /* TIKU_PRINTF doesn't support %.*s; copy into a NUL-terminated
         * local so %s renders properly. name_len is capped at
         * TIKU_BT_ADV_NAME_MAX (26) so the buffer fits. */
        {
            char nz[TIKU_BT_ADV_NAME_MAX + 1];
            uint8_t i;
            for (i = 0U; i < name_len; ++i) nz[i] = name[i];
            nz[name_len] = '\0';
            TIKU_BT_PRINTF("p7.C: LE_Set_Adv_Enable(1) OK *** "
                            "advertising as \"%s\" ***\n", nz);
        }
    }
    return TIKU_DRV_OK;
}

/*---------------------------------------------------------------------------*/
/* GAP scanning (phase 8) helpers                                            */
/*---------------------------------------------------------------------------*/

/** Convert milliseconds to chip 0.625 ms units, clamped to the LE
 *  HCI 16-bit valid range [0x0004 .. 0x4000] (= 2.5 ms .. 10.24 s). */
static uint16_t bt_ms_to_chip_units(uint16_t ms)
{
    uint32_t v = ((uint32_t)ms * 1000UL) / 625UL;
    if (v < 0x0004UL) v = 0x0004UL;
    if (v > 0x4000UL) v = 0x4000UL;
    return (uint16_t)v;
}

/*
 * The AD payload uses the standard `[len][type][data]` records; the
 * scan walks them until it finds a Complete (0x09) or Shortened
 * (0x08) Local Name record. The output is NOT NUL-terminated --
 * caller uses the returned length.
 */

/**
 * @brief Extract the local name AD record (type 0x08 or 0x09) into out
 *
 * @param ad       Pointer to the AD payload bytes (post-header,
 * the record stream)
 * @param ad_len   Length of @p ad in bytes (0..31 for legacy adv)
 * @param out      Destination buffer for the extracted name
 * @param out_max  Capacity of @p out; the name is truncated to fit
 * @return Number of name bytes written (0 if no name AD record found)
 */
static uint8_t bt_extract_name(const uint8_t *ad, uint8_t ad_len,
                               char *out, uint8_t out_max)
{
    uint8_t i = 0U;
    while (i < ad_len) {
        uint8_t rec_len = ad[i];
        uint8_t type;
        if (rec_len == 0U) break;
        if (i + 1U + rec_len > ad_len) break;       /* truncated */
        type = ad[i + 1U];
        if (type == AD_TYPE_COMPLETE_LOCAL_NAME
            || type == AD_TYPE_INCOMPLETE_LOCAL_NAME) {
            uint8_t name_len = (uint8_t)(rec_len - 1U);
            uint8_t copy     = (name_len < out_max) ? name_len : out_max;
            uint8_t k;
            for (k = 0U; k < copy; ++k) out[k] = (char)ad[i + 2U + k];
            return copy;
        }
        i = (uint8_t)(i + 1U + rec_len);
    }
    return 0U;
}

/** Look up @p addr in the scan cache; return its index or -1 if absent. */
static int bt_scan_find(const uint8_t addr[6])
{
    uint8_t i, k;
    for (i = 0U; i < bt_state.scan_count; ++i) {
        int eq = 1;
        for (k = 0U; k < 6U; ++k) {
            if (bt_state.scan[i].addr[k] != addr[k]) { eq = 0; break; }
        }
        if (eq) return (int)i;
    }
    return -1;
}

/**
 * @brief Insert or update an entry from an LE Advertising Report
 *
 * @p addr_le is six bytes in the on-wire little-endian order (LSB
 * first); they are reversed into MSB-first display order before
 * caching so callers (the `bt list` shell and tests) don't have to
 * re-reverse. RSSI / name are overwritten on every sighting -- later
 * beacons are more accurate than the first; SCAN_RSP usually carries
 * the long name where ADV_IND only had the short form.
 *
 * @param evt_type   HCI LE Advertising Report event_type
 *                   (0 ADV_IND ... 4 SCAN_RSP)
 * @param addr_type  0 = public, 1 = random
 * @param addr_le    BD_ADDR in wire order (LSB first)
 * @param ad         AD payload bytes from the report
 * @param ad_len     Length of @p ad in bytes
 * @param rssi       Received signal strength in dBm (signed)
 */
static void bt_scan_cache_add(uint8_t evt_type, uint8_t addr_type,
                              const uint8_t addr_le[6],
                              const uint8_t *ad, uint8_t ad_len,
                              int8_t rssi)
{
    uint8_t addr_msb[6];
    int     idx;
    int     k;
    tiku_bt_scan_entry_t *e;

    for (k = 0; k < 6; ++k) addr_msb[k] = addr_le[5 - k];

    idx = bt_scan_find(addr_msb);
    if (idx < 0) {
        if (bt_state.scan_count >= TIKU_BT_SCAN_MAX) return;
        idx = (int)bt_state.scan_count;
        bt_state.scan_count = (uint8_t)(bt_state.scan_count + 1U);
        e = &bt_state.scan[idx];
        for (k = 0; k < 6; ++k) e->addr[k] = addr_msb[k];
        e->addr_type = addr_type;
        e->name_len  = 0U;
    } else {
        e = &bt_state.scan[idx];
    }

    e->evt_type = evt_type;
    e->rssi_dbm = rssi;
    {
        char    namebuf[TIKU_BT_SCAN_NAME_MAX];
        uint8_t n = bt_extract_name(ad, ad_len, namebuf, sizeof namebuf);
        if (n > 0U) {                          /* never erase a known name */
            uint8_t i;
            e->name_len = n;
            for (i = 0U; i < n; ++i) e->name[i] = namebuf[i];
        }
    }
}

/*
 * Layout starting at the type byte returned by tiku_bt_recv:
 * [0]    0x04        HCI event packet type
 * [1]    0x3E        Event code (LE_Meta_Event)
 * [2]    plen        bytes remaining after this byte
 * [3]    subevent    e.g. 0x02 = LE_Advertising_Report
 * [4..]  subevent-specific data
 * For LE_Advertising_Report (Core Spec Vol 4 Part E 7.7.65.2):
 * [4]    num_reports
 * per report:
 * [.]  event_type (1)
 * [.]  addr_type  (1)
 * [.]  addr       (6, LSB first on the wire)
 * [.]  data_len   (1)
 * [.]  data       (data_len)
 * [.]  rssi       (1, signed)
 * num_reports is almost always 1 on the CYW43439 (the chip rarely
 * bundles in legacy 1M-PHY mode), but the spec allows >1 so this loops.
 */

/**
 * @brief Decode one HCI_LE_Meta_Event packet
 *
 * @param pkt  Event packet bytes including the type byte (offset 0)
 * @param len  Length of @p pkt in bytes
 */
static void bt_handle_le_meta(const uint8_t *pkt, int len)
{
    if (len < 5) return;
    if (pkt[1] != HCI_EVT_LE_META) return;

    /* LE Connection Complete (Core Spec Vol 4 Part E 7.7.65.1).
     * Payload layout (after pkt[3] subevent byte):
     *   [4]    status                (1)
     *   [5..6] handle                (2 LE)
     *   [7]    role                  (1)
     *   [8]    peer_addr_type        (1)
     *   [9..14] peer_addr            (6 LSB-first)
     *   [15..16] conn_interval       (2 LE, 1.25 ms units)
     *   [17..18] conn_latency        (2 LE)
     *   [19..20] supervision_timeout (2 LE, 10 ms units)
     *   [21]   master_clock_accuracy (1) -- unused here
     */
    if (pkt[3] == LE_SUBEVT_CONNECTION_COMPLETE && len >= 22) {
        uint8_t  status = pkt[4];
        uint16_t handle = (uint16_t)(pkt[5] | ((uint16_t)pkt[6] << 8));
        int      idx;

        if (status != 0x00U) {
            TIKU_BT_PRINTF("p9: LE Connection Complete FAIL "
                            "status=0x%02x handle=0x%04x\n", status, handle);
            return;
        }
        idx = bt_conn_alloc();
        if (idx < 0) {
            TIKU_BT_PRINTF("p9: LE Connection Complete -- no free "
                            "slot, dropping (handle=0x%04x)\n", handle);
            return;
        }
        {
            tiku_bt_connection_t *c = &bt_state.conns[idx].info;
            int k;
            c->handle              = handle;
            c->role                = pkt[7];
            c->peer_addr_type      = pkt[8];
            for (k = 0; k < 6; ++k) c->peer_addr[k] = pkt[14 - k];
            c->conn_interval_units =
                (uint16_t)(pkt[15] | ((uint16_t)pkt[16] << 8));
            c->conn_latency        =
                (uint16_t)(pkt[17] | ((uint16_t)pkt[18] << 8));
            c->supv_timeout_units  =
                (uint16_t)(pkt[19] | ((uint16_t)pkt[20] << 8));
        }
        bt_state.conns[idx].in_use  = 1U;
        bt_state.conns[idx].att_mtu = ATT_MTU_DEFAULT;
        /* When advertising as ADV_IND, the chip auto-stops on
         * connection (per Core Spec). Reflect that in the local state. */
        bt_state.advertising = 0U;
        {
            const tiku_bt_connection_t *c = &bt_state.conns[idx].info;
            TIKU_BT_PRINTF("p9: *** connected to "
                            "%02x:%02x:%02x:%02x:%02x:%02x role=%s "
                            "handle=0x%04x ***\n",
                            c->peer_addr[0], c->peer_addr[1],
                            c->peer_addr[2], c->peer_addr[3],
                            c->peer_addr[4], c->peer_addr[5],
                            c->role == 1 ? "peripheral" : "central",
                            c->handle);
        }
        /* Phase 14: as the peripheral, kick the central into starting
         * pairing/encryption by sending an SMP Security Request. iOS
         * never auto-pairs without this; without a Pairing Request
         * the LE-SC machinery is never exercised. AuthReq = SC
         * + Bonding + no-MITM (0x09) to match what Pairing Response
         * will accept. */
        if (bt_state.conns[idx].info.role == 1U /* peripheral */) {
            uint8_t sec_req[2];
            sec_req[0] = SMP_OP_SECURITY_REQUEST;
            sec_req[1] = (uint8_t)(SMP_AUTHREQ_SC
                                  | 0x01U /* Bonding */);
            (void)bt_send_acl(bt_state.conns[idx].info.handle,
                              L2CAP_CID_SMP, sec_req, sizeof sec_req);
            TIKU_BT_PRINTF("p14.smp: Security Request sent "
                           "(SC+Bonding) — awaiting Pairing Request\n");
        }
        return;
    }

    /* Phase 14: chip-side P-256 pubkey readback complete. Payload:
     *   [4]    status
     *   [5..68] 64-byte public key (LE byte order, X || Y) */
    if (pkt[3] == LE_SUBEVT_READ_LOCAL_P256_PUBKEY_CPL && len >= 69) {
        if (pkt[4] != 0x00U) {
            TIKU_BT_PRINTF("p14.smp: P256_PubKey_Complete status=0x%02x\n",
                            pkt[4]);
            return;
        }
        /* Walk all sessions waiting for a pubkey; deliver to the one
         * still pending. With TIKU_BT_CONN_MAX=1 today this is a
         * single-element loop. */
        {
            uint8_t i;
            for (i = 0U; i < TIKU_BT_CONN_MAX; ++i) {
                if (!bt_state.conns[i].in_use)             continue;
                if (bt_state.smp[i].have_pubkey)           continue;
                if (bt_state.smp[i].state == SMP_IDLE)     continue;
                {
                    uint8_t k;
                    for (k = 0U; k < 64U; ++k) {
                        bt_state.smp[i].local_pubkey[k] = pkt[5U + k];
                    }
                }
                bt_state.smp[i].have_pubkey = 1U;
                /* If the peer pubkey is already in hand, the local
                 * pubkey + Pairing Confirm are owed now. */
                bt_smp_try_send_pubkey_confirm((uint8_t)i);
                break;
            }
        }
        return;
    }

    /* Phase 14: chip-side DHKey computation complete. Payload:
     *   [4]    status
     *   [5..36] 32-byte DHKey */
    if (pkt[3] == LE_SUBEVT_GENERATE_DHKEY_COMPLETE && len >= 37) {
        if (pkt[4] != 0x00U) {
            TIKU_BT_PRINTF("p14.smp: DHKey_Complete status=0x%02x\n",
                            pkt[4]);
            return;
        }
        {
            uint8_t i;
            for (i = 0U; i < TIKU_BT_CONN_MAX; ++i) {
                if (!bt_state.conns[i].in_use)         continue;
                if (bt_state.smp[i].have_dhkey)        continue;
                if (bt_state.smp[i].state == SMP_IDLE) continue;
                {
                    uint8_t k;
                    for (k = 0U; k < 32U; ++k) {
                        bt_state.smp[i].dhkey[k] = pkt[5U + k];
                    }
                }
                bt_state.smp[i].have_dhkey = 1U;
                /* Derivation is only valid once the local nonce and
                 * the peer's are both set, which is the
                 * SMP_WAITING_DHCHECK state. Otherwise just cache. */
                (void)bt_smp_try_derive_keys((uint8_t)i);
                break;
            }
        }
        return;
    }

    /* Phase 14: encryption-restart trigger -- the chip asks for
     * the LTK for an active link. */
    if (pkt[3] == LE_SUBEVT_LONG_TERM_KEY_REQUEST) {
        bt_handle_le_ltk_request(pkt, len);
        return;
    }

    if (pkt[3] != LE_SUBEVT_ADVERTISING_REPORT) return;

    {
        uint8_t num_reports = pkt[4];
        int     off         = 5;
        uint8_t r;
        for (r = 0U; r < num_reports; ++r) {
            uint8_t        evt_type;
            uint8_t        addr_type;
            const uint8_t *addr;
            uint8_t        data_len;
            const uint8_t *data;
            int8_t         rssi;

            if (off + 9 > len) return;       /* per-report fixed prefix */
            evt_type  = pkt[off + 0];
            addr_type = pkt[off + 1];
            addr      = &pkt[off + 2];
            data_len  = pkt[off + 8];
            if (off + 9 + (int)data_len + 1 > len) return;
            data      = &pkt[off + 9];
            rssi      = (int8_t)pkt[off + 9 + data_len];

            bt_scan_cache_add(evt_type, addr_type, addr,
                              data, data_len, rssi);

            off += 9 + (int)data_len + 1;
        }
    }
}

/*
 * Disconnection Complete (event 0x05) is the only non-LE-Meta event
 * handled today. LE Meta (0x3E) is delegated to
 * bt_handle_le_meta which handles Connection Complete + Advertising
 * Report. Everything else is logged at info level and dropped.
 */

/**
 * @brief Top-level HCI event dispatcher (definition for forward decl)
 *
 * @param pkt  Event bytes including the type byte at offset 0
 * @param len  Length of @p pkt in bytes
 */
static void bt_handle_hci_event(const uint8_t *pkt, int len)
{
    if (len < 3 || pkt[0] != HCI_PKT_TYPE_EVENT) return;

    if (pkt[1] == HCI_EVT_DISCONNECTION_COMPLETE && len >= 6) {
        /* Layout: status(1) + handle(2 LE) + reason(1) */
        uint8_t  status = pkt[3];
        uint16_t handle = (uint16_t)(pkt[4] | ((uint16_t)pkt[5] << 8));
        uint8_t  reason = pkt[6];
        int      idx    = bt_conn_find(handle);
        if (idx >= 0) {
            bt_state.conns[idx].in_use = 0U;
            /* Phase 14: clear any in-flight SMP session for the link. */
            bt_state.smp[idx].state             = SMP_IDLE;
            bt_state.smp[idx].have_pubkey       = 0U;
            bt_state.smp[idx].have_dhkey        = 0U;
            bt_state.smp[idx].have_peer_pubkey  = 0U;
            bt_state.smp[idx].have_peer_random  = 0U;
            bt_state.smp[idx].pending_pubkey_tx = 0U;
            bt_state.smp[idx].pending_f5        = 0U;
            TIKU_BT_PRINTF("p9: *** disconnected handle=0x%04x "
                            "reason=0x%02x status=0x%02x ***\n",
                            handle, reason, status);
        }
        /* Auto-readvertise: ADV_IND advertising auto-stops on the
         * chip when the link comes up, so without this the device
         * disappears from scans after every connect/disconnect
         * cycle. Restart with whatever name was last in use; falls
         * back to "TikuPico" if advertise was never set up. */
        if (bt_state.adv_name_len > 0U && bt_any_connection() == 0) {
            char nz[TIKU_BT_ADV_NAME_MAX + 1];
            uint8_t i;
            for (i = 0U; i < bt_state.adv_name_len; ++i) nz[i] = bt_state.adv_name[i];
            nz[bt_state.adv_name_len] = '\0';
            TIKU_BT_PRINTF("p7: auto-readvertise after disconnect "
                           "(name=\"%s\")\n", nz);
            (void)tiku_bt_advertise_start(nz);
        }
        return;
    }
    /* Phase 14: Encryption Change (event 0x08).
     *   Layout: status(1) + handle(2 LE) + enabled(1)
     *     enabled 0 = OFF, 1 = AES-CCM, 2 = AES-CCM (LE Secure Conn). */
    if (pkt[1] == HCI_EVT_ENCRYPTION_CHANGE && len >= 7) {
        uint8_t  status  = pkt[3];
        uint16_t handle  = (uint16_t)(pkt[4] | ((uint16_t)pkt[5] << 8));
        uint8_t  enabled = pkt[6];
        int      idx     = bt_conn_find(handle);
        TIKU_BT_PRINTF("p14.smp: *** encryption change handle=0x%04x "
                        "status=0x%02x mode=%u ***\n",
                        handle, status, enabled);
        if (idx >= 0 && status == 0x00U && enabled != 0U) {
            bt_state.smp[idx].state = SMP_ENCRYPTED;
            TIKU_BT_PRINTF("p14.smp: *** link encrypted ***\n");
        }
        return;
    }
    /* Phase 14: Encryption Key Refresh Complete -- same outcome as
     * Encryption Change for this purpose; mark link encrypted. */
    if (pkt[1] == HCI_EVT_ENCRYPTION_KEY_REFRESH_COMPLETE && len >= 6) {
        uint8_t  status = pkt[3];
        uint16_t handle = (uint16_t)(pkt[4] | ((uint16_t)pkt[5] << 8));
        int      idx    = bt_conn_find(handle);
        TIKU_BT_PRINTF("p14.smp: encryption key refresh handle=0x%04x "
                        "status=0x%02x\n", handle, status);
        if (idx >= 0 && status == 0x00U) {
            bt_state.smp[idx].state = SMP_ENCRYPTED;
        }
        return;
    }
    if (pkt[1] == HCI_EVT_LE_META) {
        bt_handle_le_meta(pkt, len);
        return;
    }
    /* Drop quietly. Command Complete/Status events are consumed by
     * bt_hci_cmd_response which has its own discrimination logic. */
}

/* Forward declarations for the Phase 11 demo service so bt_init
 * can register it. Full definitions live further down (next to
 * tiku_bt_notify so the callback lifetime is obvious). */
static const tiku_bt_service_t bt_demo_service;
static void                    bt_demo_push_uptime(void);

/* Periodic notification timer. Armed once the first LE connection
 * comes up (in EVENT mode -- the runner's protothread reacts to
 * TIKU_EVENT_TIMER); stopped when the last connection goes away.
 * Replaces a per-tick "has a second boundary passed?" check
 * with a real one-shot-then-rearm event timer. */
static struct tiku_timer bt_demo_notify_timer;
static uint8_t           bt_demo_notify_armed;

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

/*
 * Called by the transport driver from its own init function, AFTER
 * the chip-side bring-up + tiku_bt_register_transport() succeed. The
 * stack does no chip-level work here -- just resets module state,
 * registers the demo service, and runs HCI_Reset / Read_Local_Version
 * / Read_BD_ADDR to (a) clear controller state and (b) cache the
 * chip's identity for the `bt status` shell command.
 */

/**
 * @brief Bring the protocol stack online once a transport is registered
 *
 * @return TIKU_DRV_OK once initialisation completes. Identity-query
 * failures (no Command Complete within ~1 s) are non-fatal:
 * bt_state.bd_addr stays zero and tiku_bt_addr() reports
 * TIKU_DRV_ERR_NOT_PRESENT until a re-init succeeds.
 */
int tiku_bt_init(void)
{
    bt_state.ready         = 0U;
    bt_state.advertising   = 0U;
    bt_state.adv_name_len  = 0U;
    bt_state.scanning      = 0U;
    bt_state.scan_count    = 0U;
    {
        uint8_t k;
        for (k = 0U; k < 6U; ++k) bt_state.bd_addr[k] = 0U;
    }
    {
        uint8_t k;
        for (k = 0U; k < TIKU_BT_CONN_MAX; ++k) {
            bt_state.conns[k].in_use  = 0U;
            bt_state.conns[k].att_mtu = ATT_MTU_DEFAULT;
        }
    }
    /* user_svc_count is not cleared on re-init -- services
     * registered at boot survive a re-init, which matches the
     * convention used by tiku_drv_registry. */
    {
        uint8_t k;
        for (k = 0U; k < TIKU_BT_CHAR_MAX; ++k) {
            bt_state.cccd_value[k] = 0U;
        }
    }
    /* Register the built-in demo service exactly once per boot. */
    if (bt_state.user_svc_count == 0U) {
        (void)tiku_bt_register_service(&bt_demo_service);
    }
    bt_demo_notify_armed = 0U;

    /* Arena-backed scratch (id=0xB1). Idempotent: re-init reuses the
     * backing buffer. The arena lets `ps` / region listings see BT's
     * memory budget rather than the buffer being an invisible
     * static .bss array (paired with the transport's id=0xB2 arena). */
    if (bt_scratch_cmd == (uint8_t *)0) {
        tiku_mem_err_t err = tiku_arena_create(&bt_arena, bt_arena_buf,
                              (tiku_mem_arch_size_t)sizeof bt_arena_buf,
                              /* id */ 0xB1U);
        if (err != TIKU_MEM_OK) {
            TIKU_BT_PRINTF("tiku_arena_create err=%d\n", (int)err);
            return TIKU_DRV_ERR_NOT_PRESENT;
        }
        bt_scratch_cmd = (uint8_t *)tiku_arena_alloc(&bt_arena,
                              BT_SCRATCH_CMD_SIZE);
        if (bt_scratch_cmd == (uint8_t *)0) {
            TIKU_BT_PRINTF("arena alloc FAIL (need %u B)\n",
                            (unsigned)BT_SCRATCH_CMD_SIZE);
            return TIKU_DRV_ERR_INVALID;
        }
        {
            tiku_mem_stats_t s;
            if (tiku_arena_stats(&bt_arena, &s) == TIKU_MEM_OK) {
                TIKU_BT_PRINTF("arena id=0xB1 used=%lu/%lu B\n",
                                (unsigned long)s.used_bytes,
                                (unsigned long)s.total_bytes);
            }
        }
    }

    /* Spawn the BT runner if it isn't already in the registry.
     * tiku_process_register kicks the process with TIKU_EVENT_INIT
     * which the runner's protothread treats as a wake-up. Idempotent
     * by design: a registered process keeps its slot. */
    (void)tiku_process_register("bt", &tiku_bt_runner);

    /* From here on the transport is up; let public-API guards through. */
    bt_state.ready = 1U;

    /* Phase 6.D: HCI_Reset, then Read_Local_Version + Read_BD_ADDR.
     * Reset clears the controller's state. The other two cache chip
     * identity for the `bt status` shell command (and confirm the
     * transport survives more than one round trip). */
    {
        uint8_t evt[64];
        int n;

        if (bt_hci_cmd_response(HCI_OP_RESET, (const uint8_t *)0, 0U,
                                evt, sizeof evt, &n) == TIKU_DRV_OK
            && n >= 7 && evt[6] == 0U) {
            TIKU_BT_PRINTF("p6.D: HCI_Reset OK (status=0x00)\n");
        }

        if (bt_hci_cmd_response(HCI_OP_READ_LOCAL_VERSION,
                                (const uint8_t *)0, 0U,
                                evt, sizeof evt, &n) == TIKU_DRV_OK
            && n >= 15 && evt[6] == 0U) {
            /* Cmd Complete payload after status (evt[6]):
             *   evt[7]  HCI_Version
             *   evt[8..9]  HCI_Revision LE
             *   evt[10] LMP_Version
             *   evt[11..12] Manufacturer LE
             *   evt[13..14] LMP_Subversion LE
             */
            bt_state.version.hci_version    = evt[7];
            bt_state.version.hci_revision   =
                (uint16_t)(evt[8]  | ((uint16_t)evt[9]  << 8));
            bt_state.version.lmp_version    = evt[10];
            bt_state.version.manufacturer   =
                (uint16_t)(evt[11] | ((uint16_t)evt[12] << 8));
            bt_state.version.lmp_subversion =
                (uint16_t)(evt[13] | ((uint16_t)evt[14] << 8));
            TIKU_BT_PRINTF("p6.D: Read_Local_Version hci=%u lmp=%u "
                            "mfr=0x%04x sub=0x%04x\n",
                            bt_state.version.hci_version,
                            bt_state.version.lmp_version,
                            bt_state.version.manufacturer,
                            bt_state.version.lmp_subversion);
        }

        if (bt_hci_cmd_response(HCI_OP_READ_BD_ADDR,
                                (const uint8_t *)0, 0U,
                                evt, sizeof evt, &n) == TIKU_DRV_OK
            && n >= 13 && evt[6] == 0U) {
            /* BD_ADDR is LE on the wire — reverse to MSB-first so
             * the cached form matches conventional MAC notation. */
            int i;
            for (i = 0; i < 6; ++i) bt_state.bd_addr[i] = evt[12 - i];
            TIKU_BT_PRINTF("p6.D: BD_ADDR = %02x:%02x:%02x:%02x:%02x:%02x\n",
                            bt_state.bd_addr[0], bt_state.bd_addr[1],
                            bt_state.bd_addr[2], bt_state.bd_addr[3],
                            bt_state.bd_addr[4], bt_state.bd_addr[5]);
        }
    }

#ifdef BT_SMP_SELFTEST
    /* Validate the AES-CMAC + f4/f5/f6 KDFs against the Core Spec
     * Vol 3 Part H 2.7 sample vectors before going on-air. Failing
     * any vector means the chip-side AES is broken or the byte-
     * ordering is off; surface it loudly. */
    (void)bt_smp_selftest();
#endif

    return TIKU_DRV_OK;
}

/** Return 1 once bring-up cached a non-zero BD_ADDR (= identity is real). */
static int bt_identity_ready(void)
{
    int i;
    for (i = 0; i < 6; ++i) {
        if (bt_state.bd_addr[i] != 0U) return 1;
    }
    return 0;
}

int tiku_bt_addr(uint8_t out[6])
{
    int i;
    if (!bt_identity_ready()) return TIKU_DRV_ERR_NOT_PRESENT;
    for (i = 0; i < 6; ++i) out[i] = bt_state.bd_addr[i];
    return TIKU_DRV_OK;
}

int tiku_bt_local_version(tiku_bt_version_t *out)
{
    if (!bt_identity_ready()) return TIKU_DRV_ERR_NOT_PRESENT;
    *out = bt_state.version;
    return TIKU_DRV_OK;
}

const char *tiku_bt_fw_version(void)
{
    /* Forward to the driver's getter -- the BTFW header string lives
     * in the transport's translation unit where the .S-baked blob is
     * parsed at upload time. */
    return cyw43_bt_fw_version();
}

/*---------------------------------------------------------------------------*/
/* GAP advertising — public API                                              */
/*---------------------------------------------------------------------------*/

int tiku_bt_advertise_start(const char *name)
{
    uint8_t name_len;
    int     rc;

    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;
    if (name == (const char *)0) return TIKU_DRV_ERR_INVALID;
    name_len = bt_strlen_capped(name, TIKU_BT_ADV_NAME_MAX);
    if (name_len == 0U) return TIKU_DRV_ERR_INVALID;

    /* If advertising was already running, stop first so the chip accepts
     * the new parameters cleanly. */
    if (bt_state.advertising) {
        (void)tiku_bt_advertise_stop();
    }

    rc = bt_advertise_setup(name, name_len);
    if (rc == TIKU_DRV_OK) bt_state.advertising = 1U;
    return rc;
}

int tiku_bt_advertise_stop(void)
{
    int     rc;
    int     n;
    uint8_t evt[64];
    uint8_t en = 0U;

    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;

    rc = bt_hci_cmd_response(HCI_OP_LE_SET_ADV_ENABLE, &en, 1U,
                             evt, sizeof evt, &n);
    bt_state.advertising = 0U;
    if (rc != TIKU_DRV_OK || n < 7) return rc;
    /* status byte non-zero usually means "already disabled" — fine. */
    return TIKU_DRV_OK;
}

int tiku_bt_is_advertising(void)
{
    return bt_state.advertising ? 1 : 0;
}

/*---------------------------------------------------------------------------*/
/* GAP scanning — public API                                                 */
/*---------------------------------------------------------------------------*/

/*
 * The runner TIKU_PROCESS_YIELDs while BT is fully idle (no scan,
 * no advert, no connection). Without an explicit wake, flipping
 * scan/advertise on from the shell process wouldn't re-dispatch
 * the runner. The event id is not interpreted by the runner --
 * any post unblocks the PT_YIELD.
 */

/**
 * @brief Wake the BT runner so it re-evaluates its loop condition
 */
static void bt_wake_runner(void)
{
    (void)tiku_process_post(&tiku_bt_runner, TIKU_EVENT_POLL, NULL);
}

int tiku_bt_scan_start(uint8_t active, uint16_t interval_ms,
                             uint16_t window_ms)
{
    int     rc;
    int     n;
    uint8_t evt[64];

    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;
    if (interval_ms < 3U)  interval_ms = 3U;
    if (window_ms   < 3U)  window_ms   = 3U;
    if (window_ms > interval_ms) window_ms = interval_ms;

    /* Reset the cache; otherwise stale entries from a prior scan
     * mingle with the new run and confuse the user. */
    bt_state.scan_count = 0U;

    /* LE_Set_Scan_Parameters: 7 bytes. */
    {
        uint16_t iv = bt_ms_to_chip_units(interval_ms);
        uint16_t wn = bt_ms_to_chip_units(window_ms);
        uint8_t params[7];
        params[0] = active ? 0x01U : 0x00U;          /* type */
        params[1] = (uint8_t)(iv & 0xFFU);
        params[2] = (uint8_t)((iv >> 8) & 0xFFU);
        params[3] = (uint8_t)(wn & 0xFFU);
        params[4] = (uint8_t)((wn >> 8) & 0xFFU);
        params[5] = 0x00U;                           /* own_addr public */
        params[6] = 0x00U;                           /* filter accept-all */
        rc = bt_hci_cmd_response(HCI_OP_LE_SET_SCAN_PARAMS, params, 7U,
                                 evt, sizeof evt, &n);
        if (rc != TIKU_DRV_OK || n < 7 || evt[6] != 0x00U) {
            TIKU_BT_PRINTF("p8.A: LE_Set_Scan_Params FAIL rc=%d "
                            "status=0x%02x\n", rc,
                            (n >= 7) ? evt[6] : 0xFFU);
            return (rc == TIKU_DRV_OK) ? TIKU_DRV_ERR_INVALID : rc;
        }
        TIKU_BT_PRINTF("p8.A: LE_Set_Scan_Params OK "
                        "(%s, interval %ums, window %ums)\n",
                        active ? "active" : "passive",
                        interval_ms, window_ms);
    }

    /* LE_Set_Scan_Enable: 2 bytes. Filter_Duplicates=1 lets the chip
     * deduplicate at the link layer; host-side dedup stays because
     * the chip's filter applies per scan window and clears on every
     * Set_Scan_Enable(0). */
    {
        uint8_t en[2] = { 0x01U, 0x01U };
        rc = bt_hci_cmd_response(HCI_OP_LE_SET_SCAN_ENABLE, en, 2U,
                                 evt, sizeof evt, &n);
        if (rc != TIKU_DRV_OK || n < 7 || evt[6] != 0x00U) {
            TIKU_BT_PRINTF("p8.A: LE_Set_Scan_Enable(1) FAIL rc=%d "
                            "status=0x%02x\n", rc,
                            (n >= 7) ? evt[6] : 0xFFU);
            return (rc == TIKU_DRV_OK) ? TIKU_DRV_ERR_INVALID : rc;
        }
        TIKU_BT_PRINTF("p8.A: LE_Set_Scan_Enable(1) OK *** "
                        "scan running ***\n");
    }
    bt_state.scanning = 1U;
    bt_wake_runner();
    return TIKU_DRV_OK;
}

int tiku_bt_scan_stop(void)
{
    int     rc;
    int     n;
    uint8_t evt[64];
    uint8_t en[2] = { 0x00U, 0x00U };

    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;
    bt_state.scanning = 0U;
    rc = bt_hci_cmd_response(HCI_OP_LE_SET_SCAN_ENABLE, en, 2U,
                             evt, sizeof evt, &n);
    if (rc == TIKU_DRV_OK) {
        TIKU_BT_PRINTF("p8.A: scan stopped (%u devices found)\n",
                        bt_state.scan_count);
    }
    bt_wake_runner();
    return rc;
}

int tiku_bt_is_scanning(void)
{
    return bt_state.scanning ? 1 : 0;
}

void tiku_bt_scan_clear(void)
{
    bt_state.scan_count = 0U;
}

uint8_t tiku_bt_scan_count(void)
{
    return bt_state.scan_count;
}

uint8_t tiku_bt_scan_results(tiku_bt_scan_entry_t *out,
                                   uint8_t max)
{
    uint8_t n;
    uint8_t i;
    if (out == (tiku_bt_scan_entry_t *)0) return 0U;
    n = (bt_state.scan_count < max) ? bt_state.scan_count : max;
    for (i = 0U; i < n; ++i) out[i] = bt_state.scan[i];
    return n;
}

/*---------------------------------------------------------------------------*/
/* Connection management — public API                                        */
/*---------------------------------------------------------------------------*/

uint8_t tiku_bt_connection_count(void)
{
    uint8_t i;
    uint8_t n = 0U;
    for (i = 0U; i < TIKU_BT_CONN_MAX; ++i) {
        if (bt_state.conns[i].in_use) ++n;
    }
    return n;
}

uint8_t tiku_bt_connections(tiku_bt_connection_t *out,
                                  uint8_t max)
{
    uint8_t i;
    uint8_t n = 0U;
    if (out == (tiku_bt_connection_t *)0) return 0U;
    for (i = 0U; i < TIKU_BT_CONN_MAX && n < max; ++i) {
        if (bt_state.conns[i].in_use) out[n++] = bt_state.conns[i].info;
    }
    return n;
}

int tiku_bt_disconnect(uint16_t handle)
{
    int     idx;
    int     rc;
    int     n;
    uint8_t evt[64];
    uint8_t params[3];

    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;

    /* Convenience: 0xFFFF = first active link. Useful for single-
     * connection demos where the caller doesn't track handles. */
    if (handle == 0xFFFFU) {
        uint8_t i;
        idx = -1;
        for (i = 0U; i < TIKU_BT_CONN_MAX; ++i) {
            if (bt_state.conns[i].in_use) {
                idx = (int)i;
                handle = bt_state.conns[i].info.handle;
                break;
            }
        }
        if (idx < 0) return TIKU_DRV_ERR_NOT_PRESENT;
    } else {
        idx = bt_conn_find(handle);
        if (idx < 0) return TIKU_DRV_ERR_INVALID;
    }

    /* HCI_Disconnect parameters: handle(2 LE) + reason(1).
     * Reason 0x13 = "Remote User Terminated Connection" -- the
     * spec-recommended value for an application-initiated tear-down. */
    params[0] = (uint8_t)(handle & 0xFFU);
    params[1] = (uint8_t)((handle >> 8) & 0xFFU);
    params[2] = 0x13U;

    /* HCI_Disconnect replies with Command STATUS (event 0x0F), not
     * Command Complete. bt_hci_cmd_response polls for CC, so
     * it will time out. Instead just queue the command and let the
     * runner observe the asynchronous Disconnection Complete event. */
    {
        uint8_t cmd[7];
        cmd[0] = HCI_PKT_TYPE_CMD;
        cmd[1] = (uint8_t)(HCI_OP_DISCONNECT & 0xFFU);
        cmd[2] = (uint8_t)((HCI_OP_DISCONNECT >> 8) & 0xFFU);
        cmd[3] = 3U;
        cmd[4] = params[0];
        cmd[5] = params[1];
        cmd[6] = params[2];
        rc = tiku_bt_send(cmd, sizeof cmd);
        if (rc != TIKU_DRV_OK) return rc;
    }
    (void)evt; (void)n;
    TIKU_BT_PRINTF("p9: disconnect requested for handle 0x%04x\n",
                    handle);
    return TIKU_DRV_OK;
}

/*---------------------------------------------------------------------------*/
/* GATT client (phase 13)                                                    */
/*---------------------------------------------------------------------------*/

int tiku_bt_connect_to(const uint8_t peer_addr[6],
                             uint8_t peer_addr_type)
{
    /* HCI_LE_Create_Connection has 25 bytes of parameters per Core
     * Spec Vol 4 Part E 7.8.12. Defaults chosen to match what most
     * sane BLE peripherals accept:
     *   scan_interval = 0x0060 (60 ms)
     *   scan_window   = 0x0030 (30 ms)
     *   conn_interval 24..40 (30..50 ms)
     *   latency       = 0
     *   supv_timeout  = 500 (5 s)
     *   ce_length     = 0
     */
    uint8_t params[25];
    int     rc;
    int     n;
    uint8_t evt[64];
    int     k;

    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;

    params[0]  = 0x60U; params[1]  = 0x00U;    /* scan_interval LE  */
    params[2]  = 0x30U; params[3]  = 0x00U;    /* scan_window LE    */
    params[4]  = 0x00U;                         /* filter_policy     */
    params[5]  = peer_addr_type;
    /* Reverse MSB-first display order to wire LSB-first. */
    for (k = 0; k < 6; ++k) params[6 + k] = peer_addr[5 - k];
    params[12] = 0x00U;                         /* own_addr_type pub */
    params[13] = 0x18U; params[14] = 0x00U;    /* conn_interval_min */
    params[15] = 0x28U; params[16] = 0x00U;    /* conn_interval_max */
    params[17] = 0x00U; params[18] = 0x00U;    /* latency           */
    params[19] = 0xF4U; params[20] = 0x01U;    /* supv_timeout 5s   */
    params[21] = 0x00U; params[22] = 0x00U;    /* min_ce_length     */
    params[23] = 0x00U; params[24] = 0x00U;    /* max_ce_length     */

    /* This command replies with Command Status, not Command
     * Complete -- so bt_hci_cmd_response would time out. Send raw
     * and let the Connection Complete event fire asynchronously. */
    {
        uint8_t cmd[4 + 25];
        cmd[0] = HCI_PKT_TYPE_CMD;
        cmd[1] = (uint8_t)(HCI_OP_LE_CREATE_CONNECTION & 0xFFU);
        cmd[2] = (uint8_t)((HCI_OP_LE_CREATE_CONNECTION >> 8) & 0xFFU);
        cmd[3] = 25U;
        {
            uint8_t i;
            for (i = 0U; i < 25U; ++i) cmd[4U + i] = params[i];
        }
        rc = tiku_bt_send(cmd, sizeof cmd);
    }
    (void)evt; (void)n;
    if (rc == TIKU_DRV_OK) {
        TIKU_BT_PRINTF("p13: LE_Create_Connection requested "
                        "(peer %02x:%02x:%02x:%02x:%02x:%02x "
                        "type=%s)\n",
                        peer_addr[0], peer_addr[1], peer_addr[2],
                        peer_addr[3], peer_addr[4], peer_addr[5],
                        peer_addr_type == 0U ? "public" : "random");
        bt_wake_runner();
    }
    return rc;
}

int tiku_bt_client_read(uint16_t conn_handle, uint16_t attr_handle)
{
    uint8_t pdu[3];
    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;
    if (bt_conn_find(conn_handle) < 0) return TIKU_DRV_ERR_INVALID;
    pdu[0] = ATT_OP_READ_REQ;
    pdu[1] = (uint8_t)(attr_handle & 0xFFU);
    pdu[2] = (uint8_t)((attr_handle >> 8) & 0xFFU);
    TIKU_BT_PRINTF("p13: client Read handle 0x%04x\n", attr_handle);
    return bt_send_acl(conn_handle, L2CAP_CID_ATT, pdu, 3U);
}

int tiku_bt_client_write(uint16_t conn_handle, uint16_t attr_handle,
                               const uint8_t *value, uint16_t len)
{
    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;
    if (bt_conn_find(conn_handle) < 0) return TIKU_DRV_ERR_INVALID;
    if (value == (const uint8_t *)0) return TIKU_DRV_ERR_INVALID;
    if ((uint32_t)len + 3U > (uint32_t)ATT_MTU_DEFAULT) {
        return TIKU_DRV_ERR_INVALID;
    }
    {
        uint8_t  pdu[ATT_MTU_DEFAULT];
        uint16_t i;
        pdu[0] = ATT_OP_WRITE_REQ;
        pdu[1] = (uint8_t)(attr_handle & 0xFFU);
        pdu[2] = (uint8_t)((attr_handle >> 8) & 0xFFU);
        for (i = 0U; i < len; ++i) pdu[3U + i] = value[i];
        TIKU_BT_PRINTF("p13: client Write handle 0x%04x len=%u\n",
                        attr_handle, len);
        return bt_send_acl(conn_handle, L2CAP_CID_ATT, pdu,
                           (uint16_t)(3U + len));
    }
}

int tiku_bt_client_discover_services(uint16_t conn_handle)
{
    uint8_t pdu[7];
    if (bt_state.ready == 0U) return TIKU_DRV_ERR_NOT_PRESENT;
    if (bt_conn_find(conn_handle) < 0) return TIKU_DRV_ERR_INVALID;
    pdu[0] = ATT_OP_READ_BY_GROUP_TYPE_REQ;
    pdu[1] = 0x01U; pdu[2] = 0x00U;            /* start = 0x0001 */
    pdu[3] = 0xFFU; pdu[4] = 0xFFU;            /* end   = 0xFFFF */
    pdu[5] = (uint8_t)(UUID_PRIMARY_SERVICE & 0xFFU);
    pdu[6] = (uint8_t)((UUID_PRIMARY_SERVICE >> 8) & 0xFFU);
    TIKU_BT_PRINTF("p13: client discover services on handle 0x%04x\n",
                    conn_handle);
    return bt_send_acl(conn_handle, L2CAP_CID_ATT, pdu, 7U);
}

int tiku_bt_client_subscribe(uint16_t conn_handle,
                                   uint16_t cccd_handle)
{
    /* CCCD value: bit 0 = notify, bit 1 = indicate. This enables
     * notifications by default; callers wanting indications can use
     * tiku_bt_client_write with 0x02. */
    static const uint8_t subscribe_value[2] = { 0x01U, 0x00U };
    return tiku_bt_client_write(conn_handle, cccd_handle,
                                      subscribe_value, 2U);
}

/*---------------------------------------------------------------------------*/
/* GATT server registration (phase 11) + notifications (phase 12)            */
/*---------------------------------------------------------------------------*/

int tiku_bt_register_service(const tiku_bt_service_t *svc)
{
    if (svc == (const tiku_bt_service_t *)0) return TIKU_DRV_ERR_INVALID;
    if (svc->char_count > TIKU_BT_CHAR_MAX) return TIKU_DRV_ERR_INVALID;
    if (bt_state.user_svc_count >= TIKU_BT_SVC_MAX) {
        return TIKU_DRV_ERR_INVALID;
    }
    bt_state.user_svc[bt_state.user_svc_count] = svc;
    bt_state.user_svc_count =
        (uint8_t)(bt_state.user_svc_count + 1U);
    TIKU_BT_PRINTF("p11: service 0x%04x registered (%u chars)\n",
                    svc->uuid, svc->char_count);
    return TIKU_DRV_OK;
}

int tiku_bt_notify(uint16_t char_uuid, const uint8_t *value,
                         uint16_t len)
{
    bt_att_entry_t table[ATT_HANDLE_MAX];
    uint8_t        n_attrs;
    int            char_idx;
    int            cccd_slot = -1;
    uint8_t        conn_idx;
    uint8_t        k;

    if (bt_state.ready == 0U || value == (const uint8_t *)0) {
        return TIKU_DRV_ERR_INVALID;
    }
    /* The PDU is opcode(1) + value_handle(2) + value(N); must fit in
     * the negotiated ATT MTU. Today this caps at the default MTU. */
    if ((uint16_t)len + 3U > (uint16_t)ATT_MTU_DEFAULT) {
        return TIKU_DRV_ERR_INVALID;
    }

    n_attrs = bt_att_snapshot(table);
    char_idx = bt_att_find_char_value(table, n_attrs, char_uuid);
    if (char_idx < 0) return TIKU_DRV_ERR_INVALID;

    for (k = 0U; k < bt_state.cccd_count; ++k) {
        if (bt_state.cccd_char_uuid[k] == char_uuid) {
            cccd_slot = (int)k;
            break;
        }
    }
    /* No CCCD allocated for this char (it wasn't NOTIFY/INDICATE
     * capable, or no client read it yet to trigger allocation).
     * Treat as "no subscriber" -- silent no-op. */
    if (cccd_slot < 0) return TIKU_DRV_OK;
    if ((bt_state.cccd_value[cccd_slot] & 0x0001U) == 0U) {
        return TIKU_DRV_OK;
    }

    /* Broadcast to every connection (today CONN_MAX=1, loop is forward-
     * compatible). CCCD is per-char today; per-{char, conn} is a
     * Phase 13+ refinement once multi-link bonding is supported. */
    for (conn_idx = 0U;
         conn_idx < TIKU_BT_CONN_MAX;
         ++conn_idx) {
        uint8_t  pdu[ATT_MTU_DEFAULT];
        uint16_t value_handle;
        uint16_t i;
        if (bt_state.conns[conn_idx].in_use == 0U) continue;
        value_handle = table[char_idx].handle;
        pdu[0] = ATT_OP_HANDLE_VALUE_NOTIFY;
        pdu[1] = (uint8_t)(value_handle & 0xFFU);
        pdu[2] = (uint8_t)((value_handle >> 8) & 0xFFU);
        for (i = 0U; i < len; ++i) pdu[3U + i] = value[i];
        (void)bt_send_acl(bt_state.conns[conn_idx].info.handle,
                          L2CAP_CID_ATT, pdu, (uint16_t)(3U + len));
    }
    return TIKU_DRV_OK;
}

/*---------------------------------------------------------------------------*/
/* Built-in "Tiku Stats" demo service                                        */
/*---------------------------------------------------------------------------*/
/*
 * Single characteristic exposing the uptime in seconds (uint32 LE).
 * Properties: READ + NOTIFY. The runner pushes a notification once
 * per second when any peer has CCCD bit 0 set; otherwise the value
 * is still readable on demand.
 */

static int bt_demo_uptime_read(void *user, uint8_t *out,
                               uint16_t out_max, uint16_t *out_len)
{
    uint32_t s;
    (void)user;
    if (out_max < 4U) return 1;
    s = tiku_clock_seconds();
    out[0] = (uint8_t)(s & 0xFFU);
    out[1] = (uint8_t)((s >> 8) & 0xFFU);
    out[2] = (uint8_t)((s >> 16) & 0xFFU);
    out[3] = (uint8_t)((s >> 24) & 0xFFU);
    *out_len = 4U;
    return 0;
}

static const tiku_bt_char_t bt_demo_chars[] = {
    {
        .uuid             = UUID_TIKU_UPTIME_CHAR,
        .properties       = (uint8_t)(TIKU_BT_PROP_READ
                                    | TIKU_BT_PROP_NOTIFY),
        .static_value     = (const uint8_t *)0,
        .static_value_len = 0U,
        .on_read          = bt_demo_uptime_read,
        .on_write         = (tiku_bt_char_write_t)0,
        .user             = (void *)0,
    },
};

static const tiku_bt_service_t bt_demo_service = {
    .uuid       = UUID_TIKU_STATS_SERVICE,
    .chars      = bt_demo_chars,
    .char_count = 1U,
};

/**
 * @brief Push the current uptime as a NOTIFY PDU
 *
 * Driven by the BT runner when the periodic 1 s timer fires (see
 * tiku_bt_runner thread). No-op when no peer has subscribed; the
 * CCCD bit check is inside tiku_bt_notify.
 */
static void bt_demo_push_uptime(void)
{
    uint32_t now = tiku_clock_seconds();
    uint8_t  buf[4];
    buf[0] = (uint8_t)(now & 0xFFU);
    buf[1] = (uint8_t)((now >> 8) & 0xFFU);
    buf[2] = (uint8_t)((now >> 16) & 0xFFU);
    buf[3] = (uint8_t)((now >> 24) & 0xFFU);
    (void)tiku_bt_notify(UUID_TIKU_UPTIME_CHAR, buf, 4U);
}

void tiku_bt_poll(void)
{
    /* Bound the per-tick drain: a busy room can produce dozens of
     * adverts per second; each tick caps at 8 packets so the runner
     * is not monopolised. The runner ticks at ~128 Hz so the
     * effective throughput is ~1024 packets/s -- well above what a
     * legacy LE radio can deliver. */
    if (bt_state.ready == 0U) return;
    if (bt_state.scanning == 0U
        && bt_state.advertising == 0U
        && bt_any_connection() == 0) {
        /* Nothing on-air that could produce events. The empty-ring
         * recv() costs one bp_read32 (~30 us) plus the runner-loop
         * overhead -- skip entirely. */
        return;
    }
    {
        uint8_t pkt[260];
        uint8_t i;
        for (i = 0U; i < 8U; ++i) {
            int n = tiku_bt_recv(pkt, sizeof pkt);
            if (n <= 0) break;
            switch (pkt[0]) {
            case HCI_PKT_TYPE_EVENT:
                bt_handle_hci_event(pkt, n);
                break;
            case HCI_PKT_TYPE_ACL:
                bt_handle_acl_pkt(pkt, n);
                break;
            default:
                TIKU_BT_PRINTF("poll: dropping unknown packet type "
                                "0x%02x (%d B)\n", pkt[0], n);
                break;
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
/* BT runner protothread                                                     */
/*---------------------------------------------------------------------------*/
/*
 * Replaces the old "tiku_bt_poll() inside WHD's runner" coupling.
 * Wake policy mirrors the WHD runner: a 1-tick wait while BT is
 * active (scan / advert / connection up), plain YIELD when idle.
 * The 1 s notification cadence is driven by a tiku_timer in EVENT
 * mode so tiku_clock_seconds() is not sampled 128x per second
 * just to detect a boundary crossing.
 */
TIKU_PROCESS_THREAD(tiku_bt_runner, ev, data)
{
    static struct tiku_timer rx_drain_timer;

    TIKU_PROCESS_BEGIN();

    (void)data;

    while (1) {
        /* Active = need to service the chip's BT2H ring on every
         * tick (scan delivers adverts, advert/conn delivers ACL).
         * Idle = nothing inbound is possible until shell or peer
         * flips state, so plain YIELD is the right power posture. */
        if (bt_state.ready
            && (bt_state.scanning
                || bt_state.advertising
                || bt_any_connection())) {
            PT_WAIT_UNTIL_TIMEOUT(process_pt, &rx_drain_timer, 0, 1U);
        } else {
            TIKU_PROCESS_WAIT_EVENT();
        }

        /* Drain pending events / ACL from the BT2H ring. Cheap
         * (one bp_read32) when nothing is queued. */
        tiku_bt_poll();

        /* Connection-presence drives the periodic uptime push. Arm
         * the timer the first time an active link appears; stop
         * it the first time they are all gone. The CCCD-subscriber
         * check lives inside bt_demo_push_uptime / tiku_bt_notify, so
         * even an armed timer is harmless without a real client. */
        if (bt_any_connection()) {
            if (bt_demo_notify_armed == 0U) {
                tiku_timer_set_event(&bt_demo_notify_timer,
                                     TIKU_CLOCK_SECOND);
                bt_demo_notify_armed = 1U;
            } else if (ev == TIKU_EVENT_TIMER
                       && (struct tiku_timer *)data
                          == &bt_demo_notify_timer) {
                bt_demo_push_uptime();
                tiku_timer_set_event(&bt_demo_notify_timer,
                                     TIKU_CLOCK_SECOND);
            }
        } else if (bt_demo_notify_armed) {
            tiku_timer_stop(&bt_demo_notify_timer);
            bt_demo_notify_armed = 0U;
        }
    }

    TIKU_PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/* SMP bonding store (phase 14 stub)                                         */
/*---------------------------------------------------------------------------*/
/*
 * One 32-byte fixed-width slot in .persistent SRAM (FRAM-backed on
 * MSP430, flash-backed on RP2350) so the next Phase 14 SMP change
 * can drop in real LTK/IRK material without a schema migration.
 *
 * Layout mirrors how whd.c places wifi_cred_record: tagged with
 * .persistent so the linker scripts back it with non-volatile
 * storage, and gated on a magic field that distinguishes "real
 * record" from "uninitialised NVM bytes". No checksum today (the
 * stub doesn't carry secret material); Phase 14 should add one
 * alongside real key storage.
 */
static __attribute__((section(".persistent")))
    tiku_bt_bond_record_t bt_bond_nvm[TIKU_BT_BOND_MAX];

int tiku_bt_bond_save(uint8_t slot, const tiku_bt_bond_record_t *rec)
{
    if (slot >= TIKU_BT_BOND_MAX || rec == (const tiku_bt_bond_record_t *)0) {
        return TIKU_DRV_ERR_INVALID;
    }
    {
        uint16_t mpu_saved = tiku_mpu_unlock_nvm();
        bt_bond_nvm[slot]       = *rec;
        bt_bond_nvm[slot].magic = TIKU_BT_BOND_MAGIC;
        tiku_mpu_lock_nvm(mpu_saved);
    }
    return TIKU_DRV_OK;
}

int tiku_bt_bond_load(uint8_t slot, tiku_bt_bond_record_t *out)
{
    uint8_t i;
    if (slot >= TIKU_BT_BOND_MAX || out == (tiku_bt_bond_record_t *)0) {
        return TIKU_DRV_ERR_INVALID;
    }
    if (bt_bond_nvm[slot].magic == TIKU_BT_BOND_MAGIC) {
        *out = bt_bond_nvm[slot];
    } else {
        /* Empty slot -- return a zeroed record with magic=0 so the
         * caller can distinguish "no bond" from "real bond" without
         * a separate is_present() probe. */
        out->magic          = 0UL;
        out->peer_addr_type = 0U;
        for (i = 0U; i < 6U; ++i) out->peer_addr[i] = 0U;
        out->_pad           = 0U;
        for (i = 0U; i < 16U; ++i) out->ltk[i] = 0U;
        out->flags          = 0UL;
    }
    return TIKU_DRV_OK;
}

int tiku_bt_bond_clear(uint8_t slot)
{
    if (slot >= TIKU_BT_BOND_MAX) return TIKU_DRV_ERR_INVALID;
    {
        uint16_t mpu_saved = tiku_mpu_unlock_nvm();
        uint8_t  i;
        bt_bond_nvm[slot].magic          = 0UL;
        bt_bond_nvm[slot].peer_addr_type = 0U;
        for (i = 0U; i < 6U; ++i) bt_bond_nvm[slot].peer_addr[i] = 0U;
        bt_bond_nvm[slot]._pad           = 0U;
        for (i = 0U; i < 16U; ++i) bt_bond_nvm[slot].ltk[i] = 0U;
        bt_bond_nvm[slot].flags          = 0UL;
        tiku_mpu_lock_nvm(mpu_saved);
    }
    return TIKU_DRV_OK;
}

/*
 * Linear scan over the bond slots. The stored peer_addr is in
 * MSB-first display order; the caller's @p addr_le is taken in LE
 * wire order so the comparison is straightforward (reverse on the
 * way in). Used by the LE LTK Request handler to recover the LTK
 * from NVM when an already-paired peer reconnects.
 */

/**
 * @brief Find a bond record by peer addr (Phase 14 reconnect path)
 *
 * @param addr_type  0 public, 1 random
 * @param addr_le    6 bytes in LE wire order (matches HCI / SMP)
 * @param out        Destination record on hit
 * @return TIKU_DRV_OK on hit, TIKU_DRV_ERR_NOT_PRESENT on miss
 */
static int bt_bond_find_by_addr(uint8_t addr_type, const uint8_t addr_le[6],
                                tiku_bt_bond_record_t *out)
{
    uint8_t slot;
    for (slot = 0U; slot < TIKU_BT_BOND_MAX; ++slot) {
        if (bt_bond_nvm[slot].magic != TIKU_BT_BOND_MAGIC) continue;
        if (bt_bond_nvm[slot].peer_addr_type != addr_type) continue;
        {
            int  match = 1;
            uint8_t k;
            for (k = 0U; k < 6U; ++k) {
                if (bt_bond_nvm[slot].peer_addr[k] != addr_le[5U - k]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                *out = bt_bond_nvm[slot];
                return TIKU_DRV_OK;
            }
        }
    }
    return TIKU_DRV_ERR_NOT_PRESENT;
}
