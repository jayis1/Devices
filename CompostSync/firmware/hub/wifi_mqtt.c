/*
 * Hub — WiFi + MQTT client
 * firmware/hub/wifi_mqtt.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "csp_protocol.h"
#include "sensor_types.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WIFI_MQTT";

extern QueueHandle_t telemetry_queue;

/* Event group for WiFi */
#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;

/* MQTT client handle */
static esp_mqtt_client_handle_t mqtt_client = NULL;

static uint8_t node_id_str[20];
static char topic_telemetry[64];
static char topic_status[64];
static char topic_alert[64];

/* User/device config (loaded from NVS) */
static char device_id[32] = "compost-hub-001";
static char mqtt_uri[128] = "mqtt://broker.compostsync.local:1883";

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                esp_mqtt_event_id_t id,
                                esp_mqtt_event_t *data)
{
    switch (id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        esp_mqtt_client_subscribe(mqtt_client, topic_status, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT msg topic=%.*s", data->topic_len, data->topic);
        /* Handle incoming commands */
        break;
    default:
        break;
    }
}

static void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    /* WiFi config — should be loaded from NVS */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "YOUR_WIFI",
            .password = "YOUR_PASSWORD",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA initialized");
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_uri,
        .client_id = device_id,
        .keepalive = 30,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_MQTT_EVENT_ANY,
                                    (esp_event_handler_t)mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

/* Format bin node telemetry as JSON and publish */
static void publish_telemetry(const bin_node_data_t *data)
{
    if (!mqtt_client) return;

    char json[512];
    snprintf(json, sizeof(json),
        "{\"node\":\"%04X\",\"uptime\":%lu,\"batt\":%d,"
        "\"t1\":%d,\"t2\":%d,\"t3\":%d,"
        "\"m1\":%d,\"m2\":%d,\"m3\":%d,"
        "\"co2\":%d,\"ch4\":%d,\"mass\":%d,"
        "\"vent\":%d,\"phase\":\"%s\",\"alerts\":%d}",
        data->node_id, (unsigned long)data->uptime_s, data->battery_pct,
        data->temp_c[0], data->temp_c[1], data->temp_c[2],
        data->moisture_pct[0], data->moisture_pct[1], data->moisture_pct[2],
        data->co2_ppm, data->methane_ppm, data->mass_grams,
        data->vent_position, csp_phase_name(data->phase), data->alerts);

    esp_mqtt_client_publish(mqtt_client, topic_telemetry, json, 0, 1, 0);
    ESP_LOGI(TAG, "Published telemetry: %s", json);
}

void wifi_mqtt_task(void *pvParameters)
{
    /* Init WiFi */
    wifi_init_sta();

    /* Wait for WiFi connection */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                         pdFALSE, pdTRUE, portMAX_DELAY);

    /* Init MQTT */
    snprintf(topic_telemetry, sizeof(topic_telemetry),
             "compostsync/%s/telemetry", device_id);
    snprintf(topic_status, sizeof(topic_status),
             "compostsync/%s/status", device_id);
    snprintf(topic_alert, sizeof(topic_alert),
             "compostsync/%s/alerts", device_id);

    mqtt_init();

    /* Process telemetry queue */
    bin_node_data_t data;
    while (1) {
        if (xQueueReceive(telemetry_queue, &data, portMAX_DELAY) == pdTRUE) {
            publish_telemetry(&data);

            /* Check for critical alerts */
            if (data.alerts & ALERT_METHANE_HIGH) {
                char alert_json[256];
                snprintf(alert_json, sizeof(alert_json),
                    "{\"type\":\"methane_high\",\"node\":\"%04X\",\"ch4\":%d,\"action\":\"TURN_PILE_NOW\"}",
                    data.node_id, data.methane_ppm);
                esp_mqtt_client_publish(mqtt_client, topic_alert,
                                        alert_json, 0, 2, 0);
            }
        }
    }
}