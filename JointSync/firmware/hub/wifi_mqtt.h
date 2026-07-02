/**
 * JointSync Hub — Wi-Fi + MQTT Interface
 *
 * License: MIT
 */

#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*mqtt_cmd_cb_t)(const char *topic, const uint8_t *payload, uint8_t len);

void wifi_mqtt_init(void);
void wifi_mqtt_set_cmd_callback(mqtt_cmd_cb_t callback);
void wifi_mqtt_connect(const char *client_id);
void mqtt_publish(const char *topic, const char *data, int len);
void mqtt_publish_binary(const char *topic, const uint8_t *data, int len);
bool wifi_mqtt_is_connected(void);

#endif /* WIFI_MQTT_H */