/**
 * JointSync Hub — Wi-Fi + MQTT Cloud Client
 *
 * ESP32-S3 Wi-Fi station + MQTT client for cloud communication.
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

#define WIFI_SSID       "JointSync_Setup"
#define WIFI_PASS       "jointsync123"
#define MQTT_BROKER_URI "mqtt://broker.jointsync.local:1883"

static esp_mqtt_client_handle_t g_mqtt_client = NULL;
static mqtt_cmd_cb_t g_cmd_callback = NULL;
static bool g_wifi_connected = false;
static bool g_mqtt_connected = false;

/* ── Wi-Fi Event Handler ─────────────────────────────────────────── */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_connected = false;
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Wi-Fi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_connected = true;
        if (g_mqtt_client) {
            esp_mqtt_client_start(g_mqtt_client);
        }
    }
}

/* ── MQTT Event Handler ───────────────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;

    switch (id) {
    case MQTT_EVENT_CONNECTED:
        g_mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT connected to broker");
        /* Subscribe to command topics */
        esp_mqtt_client_subscribe(g_mqtt_client, "jointsync/hub/001/cmd/therapy", 1);
        esp_mqtt_client_subscribe(g_mqtt_client, "jointsync/hub/001/cmd/scan", 1);
        esp_mqtt_client_subscribe(g_mqtt_client, "jointsync/hub/001/cmd/mode", 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        g_mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected");
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "MQTT data: topic=%.*s, len=%d", event->topic_len, event->topic, event->data_len);
        if (g_cmd_callback) {
            g_cmd_callback(event->topic, (const uint8_t *)event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;

    default:
        break;
    }
}

/* ── Public API ───────────────────────────────────────────────────── */

void wifi_mqtt_init(void)
{
    /* Initialize Wi-Fi */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    /* Initialize MQTT */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = "jointsync_hub_001",
    };
    g_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(g_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    ESP_LOGI(TAG, "Wi-Fi + MQTT initialized");
}

void wifi_mqtt_set_cmd_callback(mqtt_cmd_cb_t callback)
{
    g_cmd_callback = callback;
}

void wifi_mqtt_connect(const char *client_id)
{
    if (g_mqtt_client) {
        esp_mqtt_client_start(g_mqtt_client);
    }
}

void mqtt_publish(const char *topic, const char *data, int len)
{
    if (g_mqtt_connected && g_mqtt_client) {
        esp_mqtt_client_publish(g_mqtt_client, topic, data, len, 1, 0);
    }
}

void mqtt_publish_binary(const char *topic, const uint8_t *data, int len)
{
    if (g_mqtt_connected && g_mqtt_client) {
        esp_mqtt_client_publish(g_mqtt_client, topic, (const char *)data, len, 1, 0);
    }
}

bool wifi_mqtt_is_connected(void)
{
    return g_mqtt_connected;
}