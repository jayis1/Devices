#ifndef DRIVESYNC_WIFI_MQTT_H
#define DRIVESYNC_WIFI_MQTT_H

#include <stdint.h>

/**
 * Wi-Fi + MQTT client for DriveSync Dash Hub.
 * Connects to user's phone hotspot or home Wi-Fi.
 * Publishes trip data and receives cloud commands.
 */

typedef void (*mqtt_cmd_cb_t)(const char *topic, const uint8_t *payload, uint8_t len);

/**
 * Initialize Wi-Fi and MQTT client.
 */
void wifi_mqtt_init(void);

/**
 * Set callback for cloud commands.
 */
void wifi_mqtt_set_cmd_callback(mqtt_cmd_cb_t callback);

/**
 * Connect to Wi-Fi and MQTT broker.
 */
void wifi_mqtt_connect(const char *client_id);

/**
 * Publish a message to an MQTT topic.
 */
void mqtt_publish(const char *topic, const char *payload, uint16_t len);

/**
 * Publish binary data (base64 encoded).
 */
void mqtt_publish_binary(const char *topic, const uint8_t *data, uint16_t len);

/**
 * Check if connected to cloud.
 */
bool wifi_mqtt_is_connected(void);

#endif /* DRIVESYNC_WIFI_MQTT_H */