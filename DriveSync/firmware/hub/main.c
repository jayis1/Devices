/**
 * DriveSync Dash Hub — Main Firmware
 *
 * ESP32-S3-WROOM-1-N8R8
 * Coordinates Steering Wheel Node (BLE), Seat Belt Tag (BLE),
 * OBD-II Dongle (BLE), and Cloud (Wi-Fi/MQTT).
 * Runs edge ML inference for eye-closure/head-pose detection and
 * fuses all modalities into a unified drowsiness risk score.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "protocol.h"
#include "camera_driver.h"
#include "edge_inference.h"
#include "ble_central.h"
#include "wifi_mqtt.h"

static const char *TAG = "drivesync_hub";

/* ── Global State ─────────────────────────────────────────────────── */

typedef struct {
    uint16_t    node_ids[8];        /* Registered node IDs */
    uint8_t     node_count;
    uint16_t    seq_counter;

    /* Latest sensor data */
    payload_camera_t    camera;     /* From local inference */
    payload_steering_t  steering;   /* From wheel node */
    payload_ppg_t       ppg;        /* From belt tag */
    payload_body_imu_t  body_imu;   /* From belt tag */
    payload_obd_t       obd;        /* From OBD dongle */
    payload_hub_imu_t   hub_imu;    /* From local IMU */

    /* Risk fusion state */
    uint8_t     risk_score;         /* 0-100 (fused) */
    uint8_t     perclos_risk;       /* 0-100 (vision sub-score) */
    uint8_t     steering_risk;      /* 0-100 (steering sub-score) */
    uint8_t     hrv_risk;           /* 0-100 (HRV sub-score) */
    uint8_t     body_sway_risk;     /* 0-100 (body sway sub-score) */

    /* Trip state */
    bool        driving;            /* True when vehicle speed > 5 km/h */
    uint32_t    trip_start_ms;
    uint32_t    last_break_ms;

    QueueHandle_t event_queue;
} hub_state_t;

static hub_state_t g_state = {0};

/* ── Event Types ─────────────────────────────────────────────────── */

typedef enum {
    EVENT_CAMERA_FRAME   = 0x01,
    EVENT_STEERING_DATA   = 0x02,
    EVENT_PPG_DATA        = 0x03,
    EVENT_BODY_IMU_DATA   = 0x04,
    EVENT_OBD_DATA        = 0x05,
    EVENT_HUB_IMU_DATA    = 0x06,
    EVENT_ALERT_CMD       = 0x10,
    EVENT_CLOUD_CMD        = 0x20,
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
    drivesync_header_t header;
    const uint8_t *payload;

    if (!drivesync_decode(data, len, &header, &payload)) {
        ESP_LOGW(TAG, "BLE: invalid packet from 0x%04X", sender_id);
        return;
    }

    hub_event_t event = {0};
    event.type = (hub_event_type_t)header.msg_type;
    event.sender_id = header.sender_id;
    event.data_len = header.payload_len;
    if (header.payload_len > 0 && header.payload_len <= DS_MAX_PAYLOAD) {
        memcpy(event.data, payload, header.payload_len);
    }

    xQueueSend(g_state.event_queue, &event, pdMS_TO_TICKS(10));
}

/* ── Camera Callback ────────────────────────────────────────────── */

static void camera_frame_callback(const camera_features_t *features)
{
    /* Called from camera task after edge inference */
    hub_event_t event = {0};
    event.type = EVENT_CAMERA_FRAME;
    event.data_len = sizeof(payload_camera_t);

    payload_camera_t cam = {0};
    cam.perclos = features->perclos;
    cam.blink_rate = features->blink_rate;
    cam.avg_blink_dur = features->avg_blink_dur;
    cam.head_pitch = features->head_pitch;
    cam.head_yaw = features->head_yaw;
    cam.head_roll = features->head_roll;
    cam.head_bob_count = features->head_bob_count;
    cam.confidence = features->confidence;
    cam.timestamp = esp_timer_get_time() / 1000;

    memcpy(event.data, &cam, sizeof(cam));
    xQueueSend(g_state.event_queue, &event, pdMS_TO_TICKS(10));
}

/* ── Hub IMU Timer (local LSM6DSO) ─────────────────────────────── */

static void hub_imu_timer_callback(void *arg)
{
    /* Read local LSM6DSO — vehicle inertial data */
    payload_hub_imu_t imu = {0};
    /* TODO: read from LSM6DSO via I2C */
    imu.timestamp = esp_timer_get_time() / 1000;

    hub_event_t event = {0};
    event.type = EVENT_HUB_IMU_DATA;
    event.data_len = sizeof(imu);
    memcpy(event.data, &imu, sizeof(imu));
    xQueueSend(g_state.event_queue, &event, pdMS_TO_TICKS(5));
}

/* ── Risk Fusion ────────────────────────────────────────────────── */

/**
 * Fuse all modalities into a unified 0-100 drowsiness risk score.
 * Uses weighted combination of sub-scores.
 */
static uint8_t compute_fused_risk(void)
{
    /* PERCLOS risk: PERCLOS > 0.15 is drowsy threshold (NHTSA) */
    float perclos = g_state.camera.perclos;
    g_state.perclos_risk = (uint8_t)(perclos * 100.0f * 4.0f);  /* scale */
    if (g_state.perclos_risk > 100) g_state.perclos_risk = 100;

    /* Head-bob risk */
    if (g_state.camera.head_bob_count > 3) {
        g_state.perclos_risk = (uint8_t)(g_state.perclos_risk * 1.3f);
        if (g_state.perclos_risk > 100) g_state.perclos_risk = 100;
    }

    /* Steering risk: high jerk_count when alert, low when drowsy */
    /* Drowsy drivers make fewer micro-corrections (jerkiness decay) */
    if (g_state.driving) {
        float jerk = g_state.steering.jerk_count;
        /* Low jerk + high speed = drowsy */
        if (jerk < 2 && g_state.obd.speed_kmh > 60) {
            g_state.steering_risk = 60;
        } else if (jerk < 5) {
            g_state.steering_risk = 30;
        } else {
            g_state.steering_risk = 10;
        }
    } else {
        g_state.steering_risk = 0;
    }

    /* HRV risk: low RMSSD (< 25 ms) suggests drowsiness */
    if (g_state.ppg.hrv_rmssd > 0) {
        if (g_state.ppg.hrv_rmssd < 20) {
            g_state.hrv_risk = 70;
        } else if (g_state.ppg.hrv_rmssd < 30) {
            g_state.hrv_risk = 40;
        } else if (g_state.ppg.hrv_rmssd < 40) {
            g_state.hrv_risk = 20;
        } else {
            g_state.hrv_risk = 5;
        }
    } else {
        g_state.hrv_risk = 0;  /* no PPG data */
    }

    /* Body sway risk: high sway amplitude = fatigue */
    if (g_state.body_imu.sway_amp > 500) {
        g_state.body_sway_risk = 60;
    } else if (g_state.body_imu.sway_amp > 300) {
        g_state.body_sway_risk = 30;
    } else {
        g_state.body_sway_risk = 10;
    }

    /* Weighted fusion (weights from trained fusion model) */
    float fused = 0.35f * g_state.perclos_risk
                + 0.25f * g_state.steering_risk
                + 0.20f * g_state.hrv_risk
                + 0.15f * g_state.body_sway_risk
                + 0.05f * 0;  /* OBD context modifier */

    /* Time-since-break amplifier */
    uint32_t now_ms = esp_timer_get_time() / 1000;
    uint32_t since_break = now_ms - g_state.last_break_ms;
    if (since_break > 7200000) {  /* > 2 hours */
        fused *= 1.2f;
    } else if (since_break > 3600000) {  /* > 1 hour */
        fused *= 1.1f;
    }

    if (fused > 100.0f) fused = 100.0f;
    return (uint8_t)fused;
}

/* ── Alert Logic ────────────────────────────────────────────────── */

static void trigger_alerts(uint8_t risk)
{
    if (risk < 30) return;  /* no alert */

    payload_alert_t alert = {0};
    alert.risk_score = risk;

    if (risk >= 85) {
        /* Critical: urgent alarm + phone notification */
        alert.alert_level = 4;
        alert.duration_sec = 10;
        alert.source = 3;  /* fusion */

        /* Send haptic to belt tag */
        uint8_t packet[DS_MAX_PACKET_LEN];
        uint8_t pkt_len = drivesync_encode(packet, sizeof(packet),
                                            MSG_TYPE_ALERT_CRITICAL,
                                            DS_HUB_ID,
                                            g_state.seq_counter++,
                                            DS_FLAG_ACK_REQ,
                                            (uint8_t *)&alert, sizeof(alert));
        ble_send_to_belt(packet, pkt_len);

        /* Publish to cloud for emergency contact notification */
        char json[128];
        snprintf(json, sizeof(json),
                 "{\"risk\":%d,\"level\":\"critical\",\"ts\":%llu}",
                 risk, (unsigned long long)(esp_timer_get_time() / 1000));
        mqtt_publish("drivesync/alerts/critical", json, strlen(json));

        /* Play urgent alarm */
        camera_play_alert_sound(ALERT_URGENT);
    } else if (risk >= 70) {
        /* High: alarm + belt haptic */
        alert.alert_level = 3;
        alert.duration_sec = 5;
        alert.source = 3;

        uint8_t packet[DS_MAX_PACKET_LEN];
        uint8_t pkt_len = drivesync_encode(packet, sizeof(packet),
                                            MSG_TYPE_ALERT_DROWSY,
                                            DS_HUB_ID,
                                            g_state.seq_counter++,
                                            DS_FLAG_ACK_REQ,
                                            (uint8_t *)&alert, sizeof(alert));
        ble_send_to_belt(packet, pkt_len);
        camera_play_alert_sound(ALERT_HIGH);
    } else if (risk >= 50) {
        /* Moderate: audio chime + voice prompt */
        alert.alert_level = 2;
        alert.duration_sec = 3;
        camera_play_alert_sound(ALERT_MODERATE);
        camera_play_voice_prompt(VOICE_PULL_OVER);
    } else if (risk >= 30) {
        /* Low: subtle steering vibration */
        alert.alert_level = 1;
        alert.duration_sec = 1;

        uint8_t packet[DS_MAX_PACKET_LEN];
        uint8_t pkt_len = drivesync_encode(packet, sizeof(packet),
                                            MSG_TYPE_ALERT_DROWSY,
                                            DS_HUB_ID,
                                            g_state.seq_counter++,
                                            0,
                                            (uint8_t *)&alert, sizeof(alert));
        ble_send_to_wheel(packet, pkt_len);
    }
}

/* ── Event Processing Task ──────────────────────────────────────── */

static void event_processor_task(void *arg)
{
    hub_event_t event;

    while (1) {
        if (xQueueReceive(g_state.event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
            case EVENT_CAMERA_FRAME: {
                if (event.data_len >= sizeof(payload_camera_t)) {
                    memcpy(&g_state.camera, event.data, sizeof(payload_camera_t));
                }
                break;
            }

            case EVENT_STEERING_DATA: {
                if (event.data_len >= sizeof(payload_steering_t)) {
                    memcpy(&g_state.steering, event.data, sizeof(payload_steering_t));
                }
                break;
            }

            case EVENT_PPG_DATA: {
                if (event.data_len >= sizeof(payload_ppg_t)) {
                    memcpy(&g_state.ppg, event.data, sizeof(payload_ppg_t));
                }
                break;
            }

            case EVENT_BODY_IMU_DATA: {
                if (event.data_len >= sizeof(payload_body_imu_t)) {
                    memcpy(&g_state.body_imu, event.data, sizeof(payload_body_imu_t));
                }
                break;
            }

            case EVENT_OBD_DATA: {
                if (event.data_len >= sizeof(payload_obd_t)) {
                    memcpy(&g_state.obd, event.data, sizeof(payload_obd_t));
                    /* Update driving state */
                    g_state.driving = (g_state.obd.speed_kmh > 5);
                    if (g_state.driving && g_state.trip_start_ms == 0) {
                        g_state.trip_start_ms = esp_timer_get_time() / 1000;
                    }
                }
                break;
            }

            case EVENT_HUB_IMU_DATA: {
                if (event.data_len >= sizeof(payload_hub_imu_t)) {
                    memcpy(&g_state.hub_imu, event.data, sizeof(payload_hub_imu_t));
                }
                break;
            }

            case EVENT_CLOUD_CMD: {
                /* Handle cloud commands (mode change, etc.) */
                if (event.data_len >= 1) {
                    if (event.data[0] == 0x01) {
                        /* End trip */
                        g_state.driving = false;
                        g_state.trip_start_ms = 0;
                    }
                }
                break;
            }

            default:
                ESP_LOGD(TAG, "Unhandled event type 0x%02X", event.type);
                break;
            }

            /* Recompute fused risk every event (debounced to ~5 Hz) */
            static uint32_t last_risk_update = 0;
            uint32_t now = esp_timer_get_time() / 1000;
            if (now - last_risk_update > 200) {  /* 5 Hz */
                g_state.risk_score = compute_fused_risk();
                last_risk_update = now;

                /* Trigger alerts if needed */
                trigger_alerts(g_state.risk_score);

                /* Publish to cloud every 5 sec */
                static uint32_t last_cloud_pub = 0;
                if (now - last_cloud_pub > 5000) {
                    char json[256];
                    snprintf(json, sizeof(json),
                             "{\"risk\":%d,\"perclos\":%.3f,\"blink\":%d,"
                             "\"head_bob\":%d,\"hrv\":%d,\"hr\":%d,"
                             "\"speed\":%d,\"rpm\":%d,\"steering_risk\":%d,"
                             "\"hrv_risk\":%d,\"ts\":%llu}",
                             g_state.risk_score,
                             g_state.camera.perclos,
                             g_state.camera.blink_rate,
                             g_state.camera.head_bob_count,
                             g_state.ppg.hrv_rmssd,
                             g_state.ppg.hr,
                             g_state.obd.speed_kmh,
                             g_state.obd.rpm,
                             g_state.steering_risk,
                             g_state.hrv_risk,
                             (unsigned long long)now);
                    mqtt_publish("drivesync/data/fusion", json, strlen(json));
                    last_cloud_pub = now;
                }
            }
        }
    }
}

/* ── Cloud MQTT Callback ─────────────────────────────────────────── */

static void cloud_cmd_callback(const char *topic, const uint8_t *payload, uint8_t len)
{
    if (strstr(topic, "cmd/mode") != NULL && len >= 1) {
        payload_mode_t mode = { .mode = payload[0] };
        uint8_t packet[DS_MAX_PACKET_LEN];
        uint8_t pkt_len = drivesync_encode(packet, sizeof(packet),
                                            MSG_TYPE_CMD_MODE,
                                            DS_HUB_ID,
                                            g_state.seq_counter++,
                                            0,
                                            (uint8_t *)&mode, sizeof(mode));
        ble_send_to_all(packet, pkt_len);
        ESP_LOGI(TAG, "Mode command forwarded: %d", mode.mode);
    }
}

/* ── Main ────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "DriveSync Dash Hub starting...");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize state */
    g_state.node_count = 0;
    g_state.seq_counter = 0;
    g_state.risk_score = 0;
    g_state.driving = false;
    g_state.trip_start_ms = 0;
    g_state.last_break_ms = esp_timer_get_time() / 1000;
    g_state.event_queue = xQueueCreate(32, sizeof(hub_event_t));

    /* Initialize edge ML */
    edge_inference_init();

    /* Initialize camera with IR illumination */
    camera_init();
    camera_set_frame_callback(camera_frame_callback);
    camera_start_capture(10);  /* 10 FPS */

    /* Initialize BLE central */
    ble_central_init(ble_data_callback);
    ble_central_start_scan();

    /* Initialize Wi-Fi + MQTT */
    wifi_mqtt_init();
    wifi_mqtt_set_cmd_callback(cloud_cmd_callback);
    wifi_mqtt_connect("drivesync/hub/001");

    /* Create tasks */
    xTaskCreate(event_processor_task, "event_proc", 8192, NULL, 5, NULL);

    /* Hub IMU timer (10 Hz) */
    const esp_timer_create_args_t imu_timer_args = {
        .callback = hub_imu_timer_callback,
        .name = "hub_imu_timer"
    };
    esp_timer_handle_t imu_timer;
    esp_timer_create(&imu_timer_args, &imu_timer);
    esp_timer_start_periodic(imu_timer, 100000);  /* 100 ms = 10 Hz */

    /* Main loop — heartbeat + status */
    uint8_t heartbeat_counter = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));  /* 30 seconds */

        /* Send heartbeat to all nodes */
        uint8_t packet[DS_MAX_PACKET_LEN];
        uint8_t pkt_len = drivesync_encode(packet, sizeof(packet),
                                            MSG_TYPE_HEARTBEAT,
                                            DS_HUB_ID,
                                            g_state.seq_counter++,
                                            0, NULL, 0);
        ble_send_to_all(packet, pkt_len);

        /* Publish status to cloud */
        char status_json[128];
        uint8_t battery = 100;  /* TODO: read ADC */
        uint8_t uptime_h = (uint8_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 3600000);
        snprintf(status_json, sizeof(status_json),
                 "{\"hub\":\"001\",\"bat\":%d,\"uptime_h\":%d,\"nodes\":%d,\"driving\":%s}",
                 battery, uptime_h, g_state.node_count,
                 g_state.driving ? "true" : "false");
        mqtt_publish("drivesync/status/hub", status_json, strlen(status_json));

        ESP_LOGI(TAG, "Heartbeat %d — nodes: %d — risk: %d — driving: %s",
                 heartbeat_counter++, g_state.node_count,
                 g_state.risk_score, g_state.driving ? "yes" : "no");
    }
}