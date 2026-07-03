/**
 * DriveSync Wi-Fi + MQTT Client — ESP32-S3
 *
 * Uses esp_wifi + esp_mqtt (ESP-IDF).
 * License: MIT
 */

#include "wifi_mqtt.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "drivesync_wifi";

#define WIFI_CONNECTED_BIT  BIT0
#define MQTT_CONNECTED_BIT  BIT1

static EventGroupHandle_t s_event_group;
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static mqtt_cmd_cb_t s_cmd_callback = NULL;
static bool s_initialized = false;
static bool s_mqtt_connected = false;

/* ── Wi-Fi Event Handler ─────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Wi-Fi disconnected, retrying...");
            esp_wifi_connect();
            xEventGroupClearBits(s_event_group, WIFI_CONNECTED_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Wi-Fi connected, IP obtained");
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ── MQTT Event Handler ──────────────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;

    switch (id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        s_mqtt_connected = true;
        xEventGroupSetBits(s_event_group, MQTT_CONNECTED_BIT);

        /* Subscribe to command topics */
        esp_mqtt_client_subscribe(s_mqtt_client, "drivesync/cmd/mode", 1);
        esp_mqtt_client_subscribe(s_mqtt_client, "drivesync/cmd/end_trip", 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        s_mqtt_connected = false;
        xEventGroupClearBits(s_event_group, MQTT_CONNECTED_BIT);
        break;

    case MQTT_EVENT_DATA:
        if (s_cmd_callback != NULL) {
            char topic[64];
            snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
            s_cmd_callback(topic, (const uint8_t *)event->data, (uint8_t)event->data_len);
        }
        break;

    default:
        break;
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

void wifi_mqtt_init(void)
{
    if (s_initialized) return;

    /* Initialize event group */
    s_event_group = xEventGroupCreate();

    /* Initialize Wi-Fi */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "DrivesyncSetup",     /* Set via mobile app */
            .password = "setup123",
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    s_initialized = true;
    ESP_LOGI(TAG, "Wi-Fi + MQTT initialized");
}

void wifi_mqtt_set_cmd_callback(mqtt_cmd_cb_t callback)
{
    s_cmd_callback = callback;
}

void wifi_mqtt_connect(const char *client_id)
{
    /* Wait for Wi-Fi */
    xEventGroupWaitBits(s_event_group, WIFI_CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    /* Configure MQTT */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://broker.drivesync.cloud:8883",
        .credentials.client_id = client_id,
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                    mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);

    ESP_LOGI(TAG, "MQTT client started (ID: %s)", client_id);
}

void mqtt_publish(const char *topic, const char *payload, uint16_t len)
{
    if (s_mqtt_client == NULL || !s_mqtt_connected) return;
    esp_mqtt_client_publish(s_mqtt_client, topic, payload, len, 1, 0);
}

void mqtt_publish_binary(const char *topic, const uint8_t *data, uint16_t len)
{
    /* Base64 encode and publish as string */
    if (s_mqtt_client == NULL || !s_mqtt_connected) return;

    static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char b64[400];
    uint16_t b64_len = 0;
    uint16_t i;

    for (i = 0; i + 2 < len && b64_len < 396; i += 3) {
        uint32_t n = (data[i] << 16) | (data[i+1] << 8) | data[i+2];
        b64[b64_len++] = b64_table[(n >> 18) & 0x3F];
        b64[b64_len++] = b64_table[(n >> 12) & 0x3F];
        b64[b64_len++] = b64_table[(n >> 6) & 0x3F];
        b64[b64_len++] = b64_table[n & 0x3F];
    }
    b64[b64_len] = '\0';
    esp_mqtt_client_publish(s_mqtt_client, topic, b64, b64_len, 1, 0);
}

bool wifi_mqtt_is_connected(void)
{
    return s_mqtt_connected;
}