/*
 * Hub — WiFi + MQTT Client
 * firmware/hub/wifi_mqtt.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>
#include <stdio.h>

#include "psp_protocol.h"
#include "sensor_types.h"

static const char *TAG = "WIFI_MQTT";

extern QueueHandle_t get_detection_queue(void);

#define WIFI_SSID      CONFIG_PESTSYNC_WIFI_SSID
#define WIFI_PASS      CONFIG_PESTSYNC_WIFI_PASS
#define MQTT_BROKER    "mqtt://broker.pestsync.local"
#define USER_ID        "default"

static esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;
static bool wifi_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *event_data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ESP_LOGI(TAG, "WiFi connected, IP obtained");
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (id) {
    case MQTT_EVENT_CONNECTED:
        mqtt_connected = true;
        ESP_LOGI(TAG, "MQTT connected to broker");
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = false;
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA:
        /* Handle incoming commands from cloud */
        ESP_LOGI(TAG, "MQTT message: topic=%.*s", event->topic_len, event->topic);
        break;
    default:
        break;
    }
}

static void publish_telemetry(void)
{
    if (!mqtt_connected) return;

    char topic[128];
    char payload[512];

    /* Publish sentinel data */
    if (g_sentinel_data.node_id != 0) {
        snprintf(topic, sizeof(topic), "pestsync/%s/0x%04X/detection", USER_ID, g_sentinel_data.node_id);
        snprintf(payload, sizeof(payload),
            "{\"node\":\"0x%04X\",\"pest\":\"%s\",\"conf\":%d,\"count\":%d,"
            "\"thermal\":%.1f,\"ir\":%d,\"bat\":%d,\"alerts\":%d}",
            g_sentinel_data.node_id,
            pest_class_name(g_sentinel_data.pest_class),
            g_sentinel_data.confidence,
            g_sentinel_data.count_since_last,
            g_sentinel_data.thermal_max_c / 10.0f,
            g_sentinel_data.ir_illumination,
            g_sentinel_data.battery_pct,
            g_sentinel_data.alerts);
        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    }

    /* Publish trap data */
    if (g_trap_data.node_id != 0) {
        snprintf(topic, sizeof(topic), "pestsync/%s/0x%04X/trap", USER_ID, g_trap_data.node_id);
        snprintf(payload, sizeof(payload),
            "{\"node\":\"0x%04X\",\"status\":\"%s\",\"weight\":%d,"
            "\"bait\":%d,\"catch\":%d,\"bat\":%d,\"alerts\":%d}",
            g_trap_data.node_id,
            trap_status_name(g_trap_data.trap_status),
            g_trap_data.catch_weight_g,
            g_trap_data.bait_level,
            g_trap_data.catch_class,
            g_trap_data.battery_pct,
            g_trap_data.alerts);
        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    }

    /* Publish deterrent data */
    if (g_deterrent_data.node_id != 0) {
        snprintf(topic, sizeof(topic), "pestsync/%s/0x%04X/deterrent", USER_ID, g_deterrent_data.node_id);
        snprintf(payload, sizeof(payload),
            "{\"node\":\"0x%04X\",\"us\":%d,\"strobe\":%d,\"diff\":%d,"
            "\"oil\":%d,\"bat\":%d,\"alerts\":%d}",
            g_deterrent_data.node_id,
            g_deterrent_data.ultrasonic_active,
            g_deterrent_data.strobe_active,
            g_deterrent_data.diffuser_active,
            g_deterrent_data.oil_level,
            g_deterrent_data.battery_pct,
            g_deterrent_data.alerts);
        esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 1);
    }
}

void wifi_mqtt_task(void *pvParameters)
{
    /* WiFi init */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t wifi_cfg = { 0 };
    strcpy((char *)wifi_cfg.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_cfg.sta.password, WIFI_PASS);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    /* MQTT init */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.uri = MQTT_BROKER,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    ESP_LOGI(TAG, "WiFi + MQTT task started");

    /* Telemetry publishing loop */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000)); /* 30 second interval */
        if (wifi_connected && mqtt_connected) {
            publish_telemetry();
        }
    }
}