/* SeizureSync — MQTT client + Wi-Fi init (ESP-IDF) */
#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H
void mqtt_init(void);
void mqtt_publish(const char *topic, const char *payload);
void mqtt_publish_event(const struct sz_seizure_payload_s *ev);
void mqtt_publish_aura(const void *a);
void mqtt_publish_sudep(const void *s);
void wifi_init(void);
#endif