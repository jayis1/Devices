/* SeizureSync — MQTT + Wi-Fi stub (ESP-IDF) */
#include "mqtt_client.h"
#include "esp_log.h"
#include <stdio.h>
static const char *TAG = "MQTT";

void wifi_init(void) {
    ESP_LOGI(TAG, "Wi-Fi init (production: esp_wifi_start + esp_mqtt_client)");
}
void mqtt_init(void) {
    ESP_LOGI(TAG, "MQTT init (production: esp_mqtt_client_start to mqtt://broker)");
}
void mqtt_publish(const char *topic, const char *payload) {
    ESP_LOGI(TAG, "MQTT pub %s: %s", topic, payload);
}
void mqtt_publish_event(const void *ev) { (void)ev; }
void mqtt_publish_aura(const void *a) { (void)a; }
void mqtt_publish_sudep(const void *s) { (void)s; }