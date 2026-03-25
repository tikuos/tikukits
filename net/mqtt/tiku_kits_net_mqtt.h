/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_net_mqtt.h - MQTT 3.1.1 client
 *
 * Provides an event-driven MQTT 3.1.1 client for TikuOS embedded
 * networking.  Supports QoS 0 and QoS 1 publish/subscribe over
 * TCP, with optional TLS 1.3 PSK transport (MQTTS).
 *
 * Design constraints for ultra-low-power embedded targets:
 *   - Static allocation only (no heap)
 *   - Single connection (one broker at a time)
 *   - QoS 0 and QoS 1 only (no QoS 2)
 *   - Clean session only (no persistent session state)
 *   - FRAM-backed TX/RX buffers
 *   - Event-driven via callbacks (non-blocking)
 *
 * Memory budget (defaults):
 *   SRAM:  ~60 bytes (context + parser)
 *   FRAM:  TX_BUF + RX_BUF + credentials + will
 *          = 192 + 192 + ~73 + ~65 = ~522 bytes
 *
 * Usage:
 *   @code
 *     tiku_kits_net_mqtt_init();
 *
 *     uint8_t broker[] = {172, 16, 7, 1};
 *     tiku_kits_net_mqtt_set_server(broker, 1883);
 *     tiku_kits_net_mqtt_set_credentials("my_device",
 *         NULL, NULL);
 *
 *     tiku_kits_net_mqtt_connect(my_msg_cb, my_event_cb);
 *     // In my_event_cb(EVT_CONNECTED):
 *     tiku_kits_net_mqtt_subscribe("cmd/#", 1);
 *     tiku_kits_net_mqtt_publish("sensors/temp",
 *         data, len, 0, 0);
 *
 *     // Call periodically (~once per second):
 *     tiku_kits_net_mqtt_periodic();
 *   @endcode
 *
 * For TLS (MQTTS):
 *   @code
 *     // Enable at compile time:
 *     //   #define TIKU_KITS_NET_MQTT_TLS_ENABLE 1
 *     // Set PSK before connecting:
 *     tiku_kits_crypto_tls_set_psk(key, 16, "mqtt", 4);
 *     tiku_kits_net_mqtt_set_server(broker, 8883);
 *     tiku_kits_net_mqtt_connect(msg_cb, event_cb);
 *   @endcode
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.  See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_NET_MQTT_H_
#define TIKU_KITS_NET_MQTT_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_net.h"
#include <stdint.h>

/*---------------------------------------------------------------------------*/
/* COMPILE GUARD                                                             */
/*---------------------------------------------------------------------------*/

#ifndef TIKU_KITS_NET_MQTT_ENABLE
#define TIKU_KITS_NET_MQTT_ENABLE       0
#endif

/*---------------------------------------------------------------------------*/
/* TLS OPTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Enable TLS 1.3 PSK transport for MQTT (MQTTS).
 *
 * When enabled, the client performs a TLS handshake after TCP
 * connect and before sending the MQTT CONNECT packet.  The
 * application must call tiku_kits_crypto_tls_set_psk() before
 * tiku_kits_net_mqtt_connect().
 */
#ifndef TIKU_KITS_NET_MQTT_TLS_ENABLE
#define TIKU_KITS_NET_MQTT_TLS_ENABLE   0
#endif

/*---------------------------------------------------------------------------*/
/* CONFIGURATION (compile-time overrideable)                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief FRAM-backed TX buffer for outgoing MQTT packets.
 *
 * Must hold the largest MQTT packet the client will send.
 * CONNECT with credentials + will can reach ~120 bytes.
 * PUBLISH packets are limited to TX_BUF_SIZE minus overhead.
 */
#ifndef TIKU_KITS_NET_MQTT_TX_BUF_SIZE
#define TIKU_KITS_NET_MQTT_TX_BUF_SIZE      192
#endif

/**
 * @brief FRAM-backed RX buffer for incoming MQTT payloads.
 *
 * Must hold the variable header + payload of the largest
 * incoming MQTT packet (typically PUBLISH from the broker).
 */
#ifndef TIKU_KITS_NET_MQTT_RX_BUF_SIZE
#define TIKU_KITS_NET_MQTT_RX_BUF_SIZE      192
#endif

/** Maximum client ID length (bytes). */
#ifndef TIKU_KITS_NET_MQTT_MAX_CLIENT_ID
#define TIKU_KITS_NET_MQTT_MAX_CLIENT_ID    24
#endif

/** Maximum username length (bytes). */
#ifndef TIKU_KITS_NET_MQTT_MAX_USERNAME
#define TIKU_KITS_NET_MQTT_MAX_USERNAME     24
#endif

/** Maximum password length (bytes). */
#ifndef TIKU_KITS_NET_MQTT_MAX_PASSWORD
#define TIKU_KITS_NET_MQTT_MAX_PASSWORD     24
#endif

/** Maximum will topic length (bytes). */
#ifndef TIKU_KITS_NET_MQTT_MAX_WILL_TOPIC
#define TIKU_KITS_NET_MQTT_MAX_WILL_TOPIC   32
#endif

/** Maximum will message length (bytes). */
#ifndef TIKU_KITS_NET_MQTT_MAX_WILL_MSG
#define TIKU_KITS_NET_MQTT_MAX_WILL_MSG     32
#endif

/**
 * @brief Maximum in-flight QoS 1 operations.
 *
 * Tracks unacknowledged PUBLISH (QoS 1), SUBSCRIBE, and
 * UNSUBSCRIBE packets.  Each slot costs 4 bytes SRAM.
 */
#ifndef TIKU_KITS_NET_MQTT_MAX_INFLIGHT
#define TIKU_KITS_NET_MQTT_MAX_INFLIGHT     4
#endif

/**
 * @brief Keepalive interval in seconds.
 *
 * Client sends PINGREQ if no packet has been sent within
 * this interval.  Broker disconnects if nothing received
 * within 1.5x.  Set to 0 to disable keepalive.
 *
 * Must satisfy: keepalive * TIKU_CLOCK_SECOND < 65536
 * (16-bit clock overflow constraint).
 */
#ifndef TIKU_KITS_NET_MQTT_KEEPALIVE_SEC
#define TIKU_KITS_NET_MQTT_KEEPALIVE_SEC    60
#endif

/** Default MQTT broker port (plaintext TCP). */
#ifndef TIKU_KITS_NET_MQTT_PORT
#define TIKU_KITS_NET_MQTT_PORT             1883
#endif

/** Default MQTT broker port (TLS). */
#ifndef TIKU_KITS_NET_MQTT_TLS_PORT
#define TIKU_KITS_NET_MQTT_TLS_PORT         8883
#endif

/** Local TCP source port for MQTT connections. */
#ifndef TIKU_KITS_NET_MQTT_LOCAL_PORT
#define TIKU_KITS_NET_MQTT_LOCAL_PORT       49600
#endif

/**
 * @brief PINGRESP timeout in seconds.
 *
 * If PINGRESP is not received within this many seconds
 * after sending PINGREQ, the connection is considered dead.
 */
#ifndef TIKU_KITS_NET_MQTT_PING_TIMEOUT_SEC
#define TIKU_KITS_NET_MQTT_PING_TIMEOUT_SEC 10
#endif

/**
 * @brief CONNACK timeout in seconds.
 *
 * Maximum wait for CONNACK after sending MQTT CONNECT.
 * If exceeded, the connection is aborted.
 */
#ifndef TIKU_KITS_NET_MQTT_CONNECT_TIMEOUT
#define TIKU_KITS_NET_MQTT_CONNECT_TIMEOUT  30
#endif

/*---------------------------------------------------------------------------*/
/* MQTT-SPECIFIC ERROR CODES                                                 */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_NET_MQTT_ERRORS MQTT Error Codes
 * @brief Extend the common net error range (after HTTP at -14).
 * @{ */

/** Broker rejected CONNECT (check get_connack_rc()). */
#define TIKU_KITS_NET_ERR_MQTT_CONNACK  (-15)

/** MQTT protocol violation from broker. */
#define TIKU_KITS_NET_ERR_MQTT_PROTO    (-16)

/** Wrong state for the requested operation. */
#define TIKU_KITS_NET_ERR_MQTT_STATE    (-17)

/** In-flight table full (too many pending QoS 1 ops). */
#define TIKU_KITS_NET_ERR_MQTT_INFLIGHT (-18)

/** TCP connection to broker failed. */
#define TIKU_KITS_NET_ERR_MQTT_TCP      (-19)

/** TLS handshake with broker failed. */
#define TIKU_KITS_NET_ERR_MQTT_TLS      (-20)

/** @} */

/*---------------------------------------------------------------------------*/
/* MQTT 3.1.1 PACKET TYPES                                                  */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_NET_MQTT_TYPES MQTT Packet Types
 * @brief Upper 4 bits of the fixed header first byte.
 * @{ */
#define TIKU_KITS_NET_MQTT_TYPE_CONNECT      1
#define TIKU_KITS_NET_MQTT_TYPE_CONNACK      2
#define TIKU_KITS_NET_MQTT_TYPE_PUBLISH      3
#define TIKU_KITS_NET_MQTT_TYPE_PUBACK       4
#define TIKU_KITS_NET_MQTT_TYPE_SUBSCRIBE    8
#define TIKU_KITS_NET_MQTT_TYPE_SUBACK       9
#define TIKU_KITS_NET_MQTT_TYPE_UNSUBSCRIBE 10
#define TIKU_KITS_NET_MQTT_TYPE_UNSUBACK    11
#define TIKU_KITS_NET_MQTT_TYPE_PINGREQ     12
#define TIKU_KITS_NET_MQTT_TYPE_PINGRESP    13
#define TIKU_KITS_NET_MQTT_TYPE_DISCONNECT  14
/** @} */

/*---------------------------------------------------------------------------*/
/* QOS LEVELS                                                                */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_NET_MQTT_QOS MQTT QoS Levels
 * @{ */
#define TIKU_KITS_NET_MQTT_QOS_0    0   /**< At most once */
#define TIKU_KITS_NET_MQTT_QOS_1    1   /**< At least once */
/** @} */

/*---------------------------------------------------------------------------*/
/* CONNACK RETURN CODES                                                      */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_NET_MQTT_CONNACK_RC CONNACK Return Codes
 * @brief Byte 2 of the CONNACK variable header (MQTT 3.1.1).
 * @{ */
#define TIKU_KITS_NET_MQTT_CONNACK_ACCEPTED       0
#define TIKU_KITS_NET_MQTT_CONNACK_REFUSED_PROTO  1
#define TIKU_KITS_NET_MQTT_CONNACK_REFUSED_ID     2
#define TIKU_KITS_NET_MQTT_CONNACK_REFUSED_SERVER 3
#define TIKU_KITS_NET_MQTT_CONNACK_REFUSED_AUTH   4
#define TIKU_KITS_NET_MQTT_CONNACK_REFUSED_NOAUTH 5
/** @} */

/*---------------------------------------------------------------------------*/
/* CONNECTION STATES                                                         */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_NET_MQTT_STATES Connection States
 * @{ */
#define TIKU_KITS_NET_MQTT_STATE_DISCONNECTED   0
#define TIKU_KITS_NET_MQTT_STATE_TCP_CONNECTING 1
#define TIKU_KITS_NET_MQTT_STATE_TLS_CONNECTING 2
#define TIKU_KITS_NET_MQTT_STATE_CONNECTING     3
#define TIKU_KITS_NET_MQTT_STATE_CONNECTED      4
#define TIKU_KITS_NET_MQTT_STATE_DISCONNECTING  5
/** @} */

/*---------------------------------------------------------------------------*/
/* EVENTS                                                                    */
/*---------------------------------------------------------------------------*/

/** @defgroup TIKU_KITS_NET_MQTT_EVENTS Connection Events
 * @brief Delivered to the application via event_cb.
 * @{ */
#define TIKU_KITS_NET_MQTT_EVT_CONNECTED    1
#define TIKU_KITS_NET_MQTT_EVT_DISCONNECTED 2
#define TIKU_KITS_NET_MQTT_EVT_ERROR        3
#define TIKU_KITS_NET_MQTT_EVT_PUBACK       4
#define TIKU_KITS_NET_MQTT_EVT_SUBACK       5
#define TIKU_KITS_NET_MQTT_EVT_UNSUBACK     6
/** @} */

/*---------------------------------------------------------------------------*/
/* CALLBACK TYPES                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Incoming PUBLISH message callback.
 *
 * Invoked when a PUBLISH is received from the broker.  The
 * topic and payload pointers reference the internal RX buffer
 * and are valid only for the duration of the callback -- copy
 * if needed.  For QoS 1, PUBACK is sent automatically after
 * the callback returns.
 *
 * @param topic        Topic string (NOT NUL-terminated)
 * @param topic_len    Topic length in bytes
 * @param payload      Message payload bytes
 * @param payload_len  Payload length in bytes
 * @param qos          QoS level (0 or 1)
 * @param retain       Retain flag from the broker
 */
typedef void (*tiku_kits_net_mqtt_msg_cb_t)(
    const char *topic,
    uint16_t topic_len,
    const uint8_t *payload,
    uint16_t payload_len,
    uint8_t qos,
    uint8_t retain);

/**
 * @brief Connection event callback.
 *
 * Invoked on state transitions: connection established,
 * unexpected disconnection, protocol error, or ACK received.
 *
 * @param event  One of TIKU_KITS_NET_MQTT_EVT_*
 */
typedef void (*tiku_kits_net_mqtt_event_cb_t)(
    uint8_t event);

/*---------------------------------------------------------------------------*/
/* PUBLIC API                                                                */
/*---------------------------------------------------------------------------*/

#if TIKU_KITS_NET_MQTT_ENABLE

/**
 * @brief Initialise the MQTT subsystem.
 *
 * Clears all state, buffers, and the in-flight table.
 * Call once during boot, after the network stack (TCP and
 * optionally TLS) is initialised.
 */
void tiku_kits_net_mqtt_init(void);

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Set the MQTT broker address and port.
 *
 * @param addr  Broker IPv4 address (4 bytes, network order)
 * @param port  Broker port in host byte order
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NULL if addr is NULL
 */
int8_t tiku_kits_net_mqtt_set_server(
    const uint8_t *addr,
    uint16_t port);

/**
 * @brief Set MQTT credentials.
 *
 * @param client_id  Client identifier (required, NUL-term)
 * @param username   Username (NULL to omit)
 * @param password   Password (NULL to omit)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_NULL if client_id is NULL,
 *         TIKU_KITS_NET_ERR_OVERFLOW if string too long
 */
int8_t tiku_kits_net_mqtt_set_credentials(
    const char *client_id,
    const char *username,
    const char *password);

/**
 * @brief Set the MQTT last will and testament.
 *
 * If configured, the broker publishes the will message when
 * the client disconnects unexpectedly.  Pass NULL topic to
 * clear a previously set will.
 *
 * @param topic    Will topic (NUL-terminated, NULL to clear)
 * @param msg      Will message payload
 * @param msg_len  Will message length
 * @param qos      Will QoS (0 or 1)
 * @param retain   Will retain flag
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_OVERFLOW if too long
 */
int8_t tiku_kits_net_mqtt_set_will(
    const char *topic,
    const uint8_t *msg,
    uint16_t msg_len,
    uint8_t qos,
    uint8_t retain);

/*---------------------------------------------------------------------------*/
/* CONNECTION                                                                */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initiate MQTT connection to the configured broker.
 *
 * Non-blocking.  Starts the TCP handshake (and optionally
 * TLS).  On completion, event_cb fires with EVT_CONNECTED
 * or EVT_ERROR.  Server and credentials must be configured
 * before calling.
 *
 * @param msg_cb    Incoming PUBLISH callback
 * @param event_cb  Connection event callback
 * @return TIKU_KITS_NET_OK if connect initiated,
 *         TIKU_KITS_NET_ERR_MQTT_STATE if not disconnected,
 *         TIKU_KITS_NET_ERR_MQTT_TCP if TCP connect failed
 */
int8_t tiku_kits_net_mqtt_connect(
    tiku_kits_net_mqtt_msg_cb_t msg_cb,
    tiku_kits_net_mqtt_event_cb_t event_cb);

/**
 * @brief Disconnect from the broker.
 *
 * Sends MQTT DISCONNECT and closes the TCP connection.
 * Does not fire the event callback (caller-initiated).
 *
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_MQTT_STATE if already disconnected
 */
int8_t tiku_kits_net_mqtt_disconnect(void);

/*---------------------------------------------------------------------------*/
/* PUBLISH                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Publish a message to a topic.
 *
 * @param topic     Topic string (NUL-terminated)
 * @param data      Payload bytes (NULL if data_len is 0)
 * @param data_len  Payload length
 * @param qos       QoS level (0 or 1)
 * @param retain    Retain flag
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_MQTT_STATE if not connected,
 *         TIKU_KITS_NET_ERR_OVERFLOW if exceeds TX buffer,
 *         TIKU_KITS_NET_ERR_MQTT_INFLIGHT if QoS 1 table full
 */
int8_t tiku_kits_net_mqtt_publish(
    const char *topic,
    const uint8_t *data,
    uint16_t data_len,
    uint8_t qos,
    uint8_t retain);

/*---------------------------------------------------------------------------*/
/* SUBSCRIBE / UNSUBSCRIBE                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Subscribe to a topic filter.
 *
 * @param topic  Topic filter (NUL-terminated, +/# wildcards)
 * @param qos    Maximum QoS (0 or 1)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_MQTT_STATE if not connected,
 *         TIKU_KITS_NET_ERR_MQTT_INFLIGHT if table full
 */
int8_t tiku_kits_net_mqtt_subscribe(
    const char *topic,
    uint8_t qos);

/**
 * @brief Unsubscribe from a topic filter.
 *
 * @param topic  Topic filter (NUL-terminated)
 * @return TIKU_KITS_NET_OK on success,
 *         TIKU_KITS_NET_ERR_MQTT_STATE if not connected,
 *         TIKU_KITS_NET_ERR_MQTT_INFLIGHT if table full
 */
int8_t tiku_kits_net_mqtt_unsubscribe(
    const char *topic);

/*---------------------------------------------------------------------------*/
/* PERIODIC / STATUS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Periodic housekeeping (call ~once per second).
 *
 * Manages keepalive PINGREQ/PINGRESP and CONNACK timeout.
 * Must be called regularly while the MQTT client is active.
 */
void tiku_kits_net_mqtt_periodic(void);

/**
 * @brief Check if the MQTT client is connected.
 *
 * @return 1 if state is CONNECTED, 0 otherwise
 */
uint8_t tiku_kits_net_mqtt_is_connected(void);

/**
 * @brief Get the current connection state.
 *
 * @return One of TIKU_KITS_NET_MQTT_STATE_*
 */
uint8_t tiku_kits_net_mqtt_get_state(void);

/**
 * @brief Get the CONNACK return code from the last connect.
 *
 * @return CONNACK return code (0=accepted, 1-5=refused),
 *         or 0xFF if no CONNACK received yet
 */
uint8_t tiku_kits_net_mqtt_get_connack_rc(void);

/**
 * @brief Get the SUBACK return code from the last subscribe.
 *
 * @return SUBACK return code (0=QoS0, 1=QoS1, 0x80=failure),
 *         or 0xFF if no SUBACK received yet
 */
uint8_t tiku_kits_net_mqtt_get_suback_rc(void);

#endif /* TIKU_KITS_NET_MQTT_ENABLE */

#endif /* TIKU_KITS_NET_MQTT_H_ */
