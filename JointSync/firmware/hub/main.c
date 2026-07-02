/**
 * JointSync Hub — Main Firmware
 *
 * ESP32-S3-WROOM-1-N8R8
 * Coordinates Joint Tags (BLE), Compression Sleeve (Sub-GHz), Scanner (BLE),
 * and Cloud (Wi-Fi/MQTT). Runs edge ML inference for inflammation detection.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "protocol.h"
#include "ble_central.h"
#include "subghz_coord.h"
#include "wifi_mqtt.h"
#include "edge_inference.h"
#include "display.h"

static const char *TAG = "jointsync_hub";

/* ── Global State ─────────────────────────────────────────────────── */

typedef struct {
    uint16_t    node_ids[16];       /* Registered node IDs */
    uint8_t     node_count;
    uint16_t    seq_counter;
    QueueHandle_t event_queue;       /* Inter-task events */
} hub_state_t;

static hub_state_t g_state = {0};

/* ── Event Types ─────────────────────────────────────────────────── */

typedef enum {
    EVENT_IMU_DATA       = 0x01,
    EVENT_TEMP_DATA      = 0x02,
    EVENT_PPG_DATA       = 0x03,
    EVENT_THERMAL_CHUNK  = 0x04,
    EVENT_PRESSURE_DATA  = 0x06,
    EVENT_THERAPY_CMD    = 0x10,
    EVENT_CLOUD_ALERT    = 0x20,
} hub_event_type_t;

typedef struct {
    hub_event_type_t type;
    uint16_t    sender_id;
    uint8_t     data[245];
    uint8_t     data_len;
} hub_event_t;

/* ── BLE Callback ────────────────────────────────────────────────── */

static void ble_data_callback(uint16_t sender_id, const uint8_t *data, uint8_t len)
{
    jointsync_header_t header;
    const uint8_t *payload;

    if (!jointsync_decode(data, len, &header, &payload)) {
        ESP_LOGW(TAG, "BLE: invalid packet from 0x%04X", sender_id);
        return;
    }

    hub_event_t event = {0};
    event.type = (hub_event_type_t)header.msg_type;
    event.sender_id = header.sender_id;
    event.data_len = header.payload_len;
    if (header.payload_len > 0 && header.payload_len <= JS_MAX_PAYLOAD) {
        memcpy(event.data, payload, header.payload_len);
    }

    xQueueSend(g_state.event_queue, &event, pdMS_TO_TICKS(10));
}

/* ── Sub-GHz Callback ────────────────────────────────────────────── */

static void subghz_data_callback(uint16_t sender_id, const uint8_t *data, uint8_t len)
{
    /* Same processing as BLE */
    ble_data_callback(sender_id, data, len);
}

/* ── Cloud MQTT Callback ─────────────────────────────────────────── */

static void cloud_cmd_callback(const char *topic, const uint8_t *payload, uint8_t len)
{
    if (strstr(topic, "cmd/therapy") != NULL && len >= 5) {
        /* Forward therapy command to Sleeve via Sub-GHz */
        uint8_t packet[JS_MAX_PACKET_LEN];
        uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                           MSG_TYPE_CMD_THERAPY,
                                           JS_HUB_ID,
                                           g_state.seq_counter++,
                                           0,
                                           payload, len);
        subghz_send_packet(packet, pkt_len);
        ESP_LOGI(TAG, "Therapy command forwarded to Sleeve");
    } else if (strstr(topic, "cmd/scan") != NULL) {
        /* Forward scan command to Scanner via BLE */
        uint8_t packet[JS_MAX_PACKET_LEN];
        uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                           MSG_TYPE_CMD_SCAN,
                                           JS_HUB_ID,
                                           g_state.seq_counter++,
                                           0,
                                           payload, len > 0 ? payload : NULL, len);
        ble_send_to_scanner(packet, pkt_len);
        ESP_LOGI(TAG, "Scan command forwarded to Scanner");
    }
}

/* ── Event Processing Task ──────────────────────────────────────── */

static void event_processor_task(void *arg)
{
    hub_event_t event;

    while (1) {
        if (xQueueReceive(g_state.event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
            case EVENT_IMU_DATA: {
                /* Compute joint angle using edge ML */
                if (event.data_len >= sizeof(payload_imu_t)) {
                    payload_imu_t *imu = (payload_imu_t *)event.data;
                    float joint_angle = edge_compute_joint_angle(
                        imu->accel_x, imu->accel_y, imu->accel_z,
                        imu->gyro_x, imu->gyro_y, imu->gyro_z);

                    /* Send to cloud */
                    char json[128];
                    snprintf(json, sizeof(json),
                             "{\"joint\":%d,\"ax\":%d,\"ay\":%d,\"az\":%d,"
                             "\"gx\":%d,\"gy\":%d,\"gz\":%d,\"angle\":%.1f,\"ts\":%lu}",
                             event.sender_id,
                             imu->accel_x, imu->accel_y, imu->accel_z,
                             imu->gyro_x, imu->gyro_y, imu->gyro_z,
                             joint_angle, (unsigned long)imu->timestamp);
                    mqtt_publish("jointsync/data/imu", (const char *)json, strlen(json));
                }
                break;
            }

            case EVENT_TEMP_DATA: {
                if (event.data_len >= sizeof(payload_temp_t)) {
                    payload_temp_t *temp = (payload_temp_t *)event.data;
                    float temp_c = temp->temp_centi / 100.0f;

                    /* Check bilateral delta for inflammation */
                    float inflammation_prob = edge_detect_inflammation(
                        temp_c, temp->sensor_id, event.sender_id);

                    if (inflammation_prob > 0.7f) {
                        /* Send inflammation alert */
                        uint8_t alert_payload[2];
                        alert_payload[0] = event.sender_id & 0xFF;
                        alert_payload[1] = (uint8_t)(inflammation_prob * 100);

                        uint8_t packet[JS_MAX_PACKET_LEN];
                        uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                                            MSG_TYPE_ALERT_INFLAME,
                                                            JS_HUB_ID,
                                                            g_state.seq_counter++,
                                                            JS_FLAG_ACK_REQ,
                                                            alert_payload, 2);
                        ble_send_to_all(packet, pkt_len);

                        mqtt_publish("jointsync/alerts/inflammation",
                                     (const char *)alert_payload, 2);
                        ESP_LOGW(TAG, "Inflammation detected on node 0x%04X (p=%.2f)",
                                 event.sender_id, inflammation_prob);
                    }

                    /* Update display */
                    display_update_temp(event.sender_id, temp_c, inflammation_prob);

                    /* Send to cloud */
                    char json[96];
                    snprintf(json, sizeof(json),
                             "{\"joint\":%d,\"temp\":%.2f,\"inflam\":%.2f,\"ts\":%lu}",
                             event.sender_id, temp_c, inflammation_prob,
                             (unsigned long)temp->timestamp);
                    mqtt_publish("jointsync/data/temp", json, strlen(json));
                }
                break;
            }

            case EVENT_PPG_DATA: {
                if (event.data_len >= sizeof(payload_ppg_t)) {
                    payload_ppg_t *ppg = (payload_ppg_t *)event.data;

                    char json[128];
                    snprintf(json, sizeof(json),
                             "{\"joint\":%d,\"hr\":%d,\"hrv\":%d,\"spo2\":%d,\"conf\":%d}",
                             event.sender_id, ppg->hr, ppg->hrv_ms, ppg->spo2, ppg->confidence);
                    mqtt_publish("jointsync/data/ppg", json, strlen(json));
                }
                break;
            }

            case EVENT_THERMAL_CHUNK: {
                /* Forward thermal chunks to cloud as base64 */
                if (event.data_len >= sizeof(payload_thermal_chunk_t)) {
                    mqtt_publish_binary("jointsync/data/thermal",
                                        event.data, event.data_len);
                }
                break;
            }

            case EVENT_PRESSURE_DATA: {
                if (event.data_len >= sizeof(payload_pressure_t)) {
                    payload_pressure_t *p = (payload_pressure_t *)event.data;
                    float pressure_mmhg = p->pressure_centi / 100.0f;

                    char json[96];
                    snprintf(json, sizeof(json),
                             "{\"sleeve\":%d,\"pressure\":%.2f,\"pump\":%d,\"ts\":%lu}",
                             event.sender_id, pressure_mmhg, p->pump_state,
                             (unsigned long)p->timestamp);
                    mqtt_publish("jointsync/data/pressure", json, strlen(json));

                    display_update_pressure(pressure_mmhg, p->pump_state);
                }
                break;
            }

            case EVENT_THERAPY_CMD: {
                /* Forward to sleeve */
                uint8_t packet[JS_MAX_PACKET_LEN];
                uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                                    MSG_TYPE_CMD_THERAPY,
                                                    JS_HUB_ID,
                                                    g_state.seq_counter++,
                                                    0,
                                                    event.data, event.data_len);
                subghz_send_packet(packet, pkt_len);
                break;
            }

            default:
                ESP_LOGD(TAG, "Unhandled event type 0x%02X", event.type);
                break;
            }
        }
    }
}

/* ── Button Handler Task ────────────────────────────────────────── */

static void button_task(void *arg)
{
    static bool prev_btn1 = false, prev_btn2 = false, prev_btn3 = false;

    while (1) {
        bool btn1 = display_get_button1();  /* Therapy */
        bool btn2 = display_get_button2();  /* Scan */
        bool btn3 = display_get_button3();  /* Menu */

        if (btn1 && !prev_btn1) {
            /* Trigger adaptive therapy on primary joint */
            payload_therapy_t therapy = {
                .mode = 3,        /* adaptive */
                .target_mmhg = 30,
                .duration_sec = 1800,
                .joint_id = 0      /* knee */
            };
            uint8_t packet[JS_MAX_PACKET_LEN];
            uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                                MSG_TYPE_CMD_THERAPY,
                                                JS_HUB_ID,
                                                g_state.seq_counter++,
                                                0,
                                                (uint8_t *)&therapy, sizeof(therapy));
            subghz_send_packet(packet, pkt_len);
            ESP_LOGI(TAG, "Therapy button pressed — adaptive mode sent");
        }

        if (btn2 && !prev_btn2) {
            /* Trigger scan */
            uint8_t scan_cmd = 0;
            uint8_t packet[JS_MAX_PACKET_LEN];
            uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                                MSG_TYPE_CMD_SCAN,
                                                JS_HUB_ID,
                                                g_state.seq_counter++,
                                                0,
                                                &scan_cmd, 1);
            ble_send_to_scanner(packet, pkt_len);
            ESP_LOGI(TAG, "Scan button pressed — scan triggered");
        }

        if (btn3 && !prev_btn3) {
            display_next_page();
        }

        prev_btn1 = btn1;
        prev_btn2 = btn2;
        prev_btn3 = btn3;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ── Main ────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "JointSync Hub starting...");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize state */
    g_state.node_count = 0;
    g_state.seq_counter = 0;
    g_state.event_queue = xQueueCreate(32, sizeof(hub_event_t));

    /* Initialize edge ML */
    edge_inference_init();

    /* Initialize display */
    display_init();

    /* Initialize BLE central */
    ble_central_init(ble_data_callback);
    ble_central_start_scan();

    /* Initialize Sub-GHz coordinator */
    subghz_coord_init(subghz_data_callback);
    subghz_coord_start();

    /* Initialize Wi-Fi + MQTT */
    wifi_mqtt_init();
    wifi_mqtt_set_cmd_callback(cloud_cmd_callback);
    wifi_mqtt_connect("jointsync/hub/001");

    /* Create tasks */
    xTaskCreate(event_processor_task, "event_proc", 8192, NULL, 5, NULL);
    xTaskCreate(button_task, "buttons", 4096, NULL, 3, NULL);

    /* Main loop — heartbeat + status */
    uint8_t heartbeat_counter = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));  /* 30 seconds */

        /* Send heartbeat to all nodes */
        uint8_t packet[JS_MAX_PACKET_LEN];
        uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                            MSG_TYPE_HEARTBEAT,
                                            JS_HUB_ID,
                                            g_state.seq_counter++,
                                            0, NULL, 0);
        ble_send_to_all(packet, pkt_len);
        subghz_send_packet(packet, pkt_len);

        /* Publish status to cloud */
        char status_json[128];
        uint8_t battery = 100;  /* TODO: read ADC */
        uint8_t uptime_h = (uint8_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 3600000);
        snprintf(status_json, sizeof(status_json),
                 "{\"hub\":\"001\",\"bat\":%d,\"uptime_h\":%d,\"nodes\":%d}",
                 battery, uptime_h, g_state.node_count);
        mqtt_publish("jointsync/status/hub", status_json, strlen(status_json));

        /* Update display */
        display_update_heartbeat(heartbeat_counter++);

        ESP_LOGI(TAG, "Heartbeat %d — nodes: %d", heartbeat_counter, g_state.node_count);
    }
}