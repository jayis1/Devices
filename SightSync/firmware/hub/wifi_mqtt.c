/**
 * SightSync Vision Hub — Wi-Fi + MQTT Client Implementation
 *
 * ESP32-S3 Wi-Fi 4 (2.4 GHz) + MQTT over TLS to SightSync cloud.
 *
 * License: MIT
 */

#include "wifi_mqtt.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

static const char *TAG = "wifi_mqtt";
static esp_mqtt_client_handle_t s_mqtt = NULL;
static bool s_connected = false;

#define BROKER_URL "mqtts://sightsync.cloud:8883"

/* ── Wi-Fi event handler ──────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        /* Start MQTT client */
        esp_mqtt_client_start(s_mqtt);
    }
}

/* ── MQTT event handler ───────────────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;
    switch (id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "MQTT connected to broker");
        /* Subscribe to lamp command + policy update topics */
        esp_mqtt_client_subscribe(s_mqtt, "sightsync/+/cloud/lamp_cmd", 1);
        esp_mqtt_client_subscribe(s_mqtt, "sightsync/+/cloud/policy", 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA:
        /* Handle incoming lamp command or policy update */
        ESP_LOGI(TAG, "MQTT data: topic=%.*s", event->topic_len, event->topic);
        break;
    default:
        break;
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

void wifi_mqtt_init(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize Wi-Fi */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t any_id, got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &wifi_event_handler, NULL, &any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &wifi_event_handler, NULL, &got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "SightSync",
            .password = "configureme123",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    /* Initialize MQTT client */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = BROKER_URL,
        .credentials.client_id = "sightsync_hub",
    };
    s_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ESP_LOGI(TAG, "Wi-Fi + MQTT initialized");
}

void wifi_mqtt_publish(const char *topic, const char *data, int len)
{
    if (s_connected && s_mqtt != NULL) {
        esp_mqtt_client_publish(s_mqtt, topic, data, len, 1, 0);
    }
}

bool wifi_mqtt_is_connected(void)
{
    return s_connected;
}