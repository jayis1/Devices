#ifndef GLUCOSYNC_WIFI_MQTT_H
#define GLUCOSYNC_WIFI_MQTT_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*mqtt_cmd_cb)(const char *topic, const uint8_t *payload, uint8_t len);

void wifi_mqtt_init(void);
void wifi_mqtt_set_cmd_callback(mqtt_cmd_cb callback);
void wifi_mqtt_connect(const char *client_id);
bool wifi_mqtt_is_connected(void);
void mqtt_publish(const char *topic, const char *data, int len);

#endif