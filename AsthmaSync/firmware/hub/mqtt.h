/**
 * AsthmaSync — Hub MQTT Header
 *
 * License: MIT
 */

#ifndef MQTT_H
#define MQTT_H

#include "../common/protocol.h"
#include <stdint.h>
#include <stddef.h>

typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR,
} mqtt_state_t;

/** Initialize MQTT client with TLS. */
int mqtt_init(void);

/** Publish a message. If offline, enqueue in PSRAM buffer. */
int mqtt_publish(const char *topic, const char *payload, size_t len, int qos);

/** Enqueue a message in the offline buffer. */
int mqtt_enqueue(const char *topic, const char *payload, size_t len);

/** Flush all queued messages (called when connection restored). */
int mqtt_flush_queue(void);

/** Get current MQTT connection state. */
mqtt_state_t mqtt_get_state(void);

/** Build JSON envelope from protocol packet + TLV payload. */
int mqtt_build_telemetry_json(const pkt_header_t *hdr,
                              const uint8_t *payload, uint16_t plen,
                              char *out_json, size_t out_size);

#endif /* MQTT_H */