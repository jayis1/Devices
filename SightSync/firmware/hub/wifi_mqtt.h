/**
 * SightSync Vision Hub — Wi-Fi + MQTT Client
 * License: MIT
 */

#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include <stdbool.h>
#include <stdint.h>

void  wifi_mqtt_init(void);
void  wifi_mqtt_publish(const char *topic, const char *data, int len);
bool  wifi_mqtt_is_connected(void);

#endif /* WIFI_MQTT_H */