/**
 * GlucoSync Metabolic Hub — Main Firmware
 *
 * ESP32-S3-WROOM-1-N8R2
 * Coordinates CGM (BLE), Meal Scanner (BLE), Activity Band (BLE),
 * Insulin Pen Tag (BLE), and Cloud (Wi-Fi/MQTT).
 * Runs edge ML for 30/60-min glucose forecasting and hypoglycemia warning.
 * Displays glucose + trend on e-ink display. Progressive alerts.
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
#include "cgm_ble.h"
#include "glucose_forecast.h"
#include "eink_display.h"
#include "ble_central.h"
#include "wifi_mqtt.h"
#include "alert_engine.h"

static const char *TAG = "glucosync_hub";

/* ── Global State ─────────────────────────────────────────────────── */

typedef struct {
    uint16_t    node_ids[8];
    uint8_t     node_count;
    uint16_t    seq_counter;

    /* Latest sensor data */
    payload_cgm_t       cgm;
    payload_meal_t      meal;
    payload_activity_t  activity;
    payload_insulin_t   insulin;
    payload_hub_imu_t   hub_imu;
    payload_forecast_t  forecast;

    /* History buffer for forecasting (60 minutes) */
    uint16_t    glucose_history[60];
    int16_t     trend_history[60];
    uint8_t     hist_idx;
    uint8_t     hist_count;

    /* Insulin on board (units) */
    float       insulin_on_board;
    uint32_t    last_insulin_time;

    /* Carbs on board (grams) */
    float       carbs_on_board;
    uint32_t    last_meal_time;

    /* Alert state */
    uint8_t     last_alert_level;
    uint32_t    last_alert_time;

    QueueHandle_t event_queue;
} hub_state_t;

static hub_state_t g_state = {0};

/* ── Event Types ─────────────────────────────────────────────────── */

typedef enum {
    EVENT_CGM_DATA       = 0x01,
    EVENT_MEAL_DATA      = 0x02,
    EVENT_ACTIVITY_DATA  = 0x03,
    EVENT_INSULIN_DATA   = 0x04,
    EVENT_HUB_IMU_DATA   = 0x05,
    EVENT_FORECAST_TICK   = 0x06,
    EVENT_CLOUD_CMD       = 0x20,
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
    glucosync_header_t header;
    const uint8_t *payload;

    if (!glucosync_decode(data, len, &header, &payload)) {
        ESP_LOGW(TAG, "BLE: invalid packet from 0x%04X", sender_id);
        return;
    }

    hub_event_t event = {0};
    event.type = (hub_event_type_t)header.msg_type;
    event.sender_id = header.sender_id;
    event.data_len = header.payload_len;
    if (header.payload_len > 0 && header.payload_len <= GS_MAX_PAYLOAD) {
        memcpy(event.data, payload, header.payload_len);
    }

    xQueueSend(g_state.event_queue, &event, pdMS_TO_TICKS(10));
}

/* ── CGM Callback ────────────────────────────────────────────────── */

static void cgm_data_callback(const payload_cgm_t *reading)
{
    hub_event_t event = {0};
    event.type = EVENT_CGM_DATA;
    event.data_len = sizeof(payload_cgm_t);
    memcpy(event.data, reading, sizeof(payload_cgm_t));
    xQueueSend(g_state.event_queue, &event, pdMS_TO_TICKS(10));
}

/* ── Hub IMU Timer ──────────────────────────────────────────────── */

static void hub_imu_timer_callback(void *arg)
{
    payload_hub_imu_t imu = {0};
    /* TODO: read from LSM6DSO via I2C, detect taps */
    imu.timestamp = esp_timer_get_time() / 1000;

    hub_event_t event = {0};
    event.type = EVENT_HUB_IMU_DATA;
    event.data_len = sizeof(imu);
    memcpy(event.data, &imu, sizeof(imu));
    xQueueSend(g_state.event_queue, &event, pdMS_TO_TICKS(5));
}

/* ── Insulin On Board (IOB) Calculation ─────────────────────────── */

/**
 * Compute insulin on board using exponential decay model.
 * Rapid insulin: ~3 hour duration, 75% active at 1.5h.
 */
static float compute_iob(uint32_t now_sec)
{
    if (g_state.last_insulin_time == 0) return 0.0f;

    uint32_t elapsed = now_sec - g_state.last_insulin_time;
    if (elapsed > 10800) return 0.0f;  /* > 3 hours */

    /* Exponential decay: IOB = D * exp(-k*t), k ≈ ln(2)/90min */
    float t_min = elapsed / 60.0f;
    float k = 0.0077f;  /* ln(2)/90 ≈ 0.0077 per minute */
    float iob = g_state.insulin.estimated_units * expf(-k * t_min);
    return iob;
}

/* ── Carbs On Board (COB) Calculation ───────────────────────────── */

/**
 * Compute carbs on board using linear decay over ~2 hours.
 */
static float compute_cob(uint32_t now_sec)
{
    if (g_state.last_meal_time == 0) return 0.0f;

    uint32_t elapsed = now_sec - g_state.last_meal_time;
    if (elapsed > 7200) return 0.0f;  /* > 2 hours */

    /* Linear decay: COB = carbs * (1 - t/120min) */
    float t_min = elapsed / 60.0f;
    float cob = g_state.meal.carb_grams * (1.0f - t_min / 120.0f);
    if (cob < 0) cob = 0;
    return cob;
}

/* ── Forecast Tick (every 5 min) ────────────────────────────────── */

static void forecast_timer_callback(void *arg)
{
    hub_event_t event = {0};
    event.type = EVENT_FORECAST_TICK;
    xQueueSend(g_state.event_queue, &event, pdMS_TO_TICKS(10));
}

/* ── Event Processing Task ──────────────────────────────────────── */

static void event_processor_task(void *arg)
{
    hub_event_t event;

    while (1) {
        if (xQueueReceive(g_state.event_queue, &event, portMAX_DELAY) == pdTRUE) {
            uint32_t now = esp_timer_get_time() / 1000;

            switch (event.type) {
            case EVENT_CGM_DATA: {
                if (event.data_len >= sizeof(payload_cgm_t)) {
                    memcpy(&g_state.cgm, event.data, sizeof(payload_cgm_t));

                    /* Push to history buffer */
                    g_state.glucose_history[g_state.hist_idx] = g_state.cgm.glucose_mgdl;
                    g_state.trend_history[g_state.hist_idx] = g_state.cgm.trend_mgdl_min;
                    g_state.hist_idx = (g_state.hist_idx + 1) % 60;
                    if (g_state.hist_count < 60) g_state.hist_count++;

                    /* Update e-ink display */
                    eink_display_glucose(g_state.cgm.glucose_mgdl,
                                         g_state.cgm.trend_mgdl_min,
                                         g_state.forecast.glucose_30min);
                }
                break;
            }

            case EVENT_MEAL_DATA: {
                if (event.data_len >= sizeof(payload_meal_t)) {
                    memcpy(&g_state.meal, event.data, sizeof(payload_meal_t));
                    g_state.last_meal_time = now;
                    g_state.carbs_on_board = g_state.meal.carb_grams;

                    ESP_LOGI(TAG, "Meal: %ug carbs, GI=%d, class=%d",
                             g_state.meal.carb_grams, g_state.meal.glycemic_index,
                             g_state.meal.food_class_id);
                }
                break;
            }

            case EVENT_ACTIVITY_DATA: {
                if (event.data_len >= sizeof(payload_activity_t)) {
                    memcpy(&g_state.activity, event.data, sizeof(payload_activity_t));
                }
                break;
            }

            case EVENT_INSULIN_DATA: {
                if (event.data_len >= sizeof(payload_insulin_t)) {
                    memcpy(&g_state.insulin, event.data, sizeof(payload_insulin_t));
                    g_state.last_insulin_time = now;
                    g_state.insulin_on_board = g_state.insulin.estimated_units;

                    ESP_LOGI(TAG, "Insulin: %u units (%s)",
                             g_state.insulin.estimated_units,
                             g_state.insulin.pen_type == 0 ? "basal" : "bolus");
                }
                break;
            }

            case EVENT_HUB_IMU_DATA: {
                if (event.data_len >= sizeof(payload_hub_imu_t)) {
                    memcpy(&g_state.hub_imu, event.data, sizeof(payload_hub_imu_t));

                    /* Tap to dismiss alert */
                    if (g_state.hub_imu.tap_detected == 2 && g_state.last_alert_level > 0) {
                        alert_engine_dismiss();
                        g_state.last_alert_level = 0;
                        ESP_LOGI(TAG, "Alert dismissed by double-tap");
                    }
                }
                break;
            }

            case EVENT_FORECAST_TICK: {
                /* Update IOB and COB */
                g_state.insulin_on_board = compute_iob(now);
                g_state.carbs_on_board = compute_cob(now);

                /* Run glucose forecast LSTM if we have enough data */
                if (g_state.hist_count >= 10) {
                    glucose_forecast_result_t result;
                    glucose_forecast_predict(
                        g_state.glucose_history, g_state.trend_history, g_state.hist_count,
                        g_state.insulin_on_board,
                        g_state.carbs_on_board,
                        g_state.activity.hr,
                        g_state.activity.intensity,
                        &result);

                    g_state.forecast.glucose_30min = result.glucose_30min;
                    g_state.forecast.glucose_60min = result.glucose_60min;
                    g_state.forecast.hypo_risk_30 = result.hypo_risk_30;
                    g_state.forecast.hyper_risk_60 = result.hyper_risk_60;
                    g_state.forecast.risk_score = result.risk_score;
                    g_state.forecast.recommendation = result.recommendation;
                    g_state.forecast.timestamp = now;

                    /* Trigger alerts if needed */
                    uint8_t alert_level = alert_engine_evaluate(
                        result.risk_score,
                        result.glucose_30min,
                        result.hypo_risk_30);

                    if (alert_level > g_state.last_alert_level) {
                        alert_engine_trigger(alert_level, result.recommendation);
                        g_state.last_alert_level = alert_level;
                        g_state.last_alert_time = now;
                    } else if (alert_level == 0 && g_state.last_alert_level > 0) {
                        alert_engine_dismiss();
                        g_state.last_alert_level = 0;
                    }

                    /* Update e-ink with forecast */
                    eink_display_glucose(g_state.cgm.glucose_mgdl,
                                         g_state.cgm.trend_mgdl_min,
                                         result.glucose_30min);

                    /* Publish to cloud */
                    char json[256];
                    snprintf(json, sizeof(json),
                             "{\"glucose\":%d,\"trend\":%d,\"forecast_30\":%d,"
                             "\"forecast_60\":%d,\"hypo_risk\":%d,\"risk\":%d,"
                             "\"iob\":%.1f,\"cob\":%.1f,\"hr\":%d,\"activity\":%d,"
                             "\"ts\":%llu}",
                             g_state.cgm.glucose_mgdl,
                             g_state.cgm.trend_mgdl_min,
                             result.glucose_30min,
                             result.glucose_60min,
                             result.hypo_risk_30,
                             result.risk_score,
                             g_state.insulin_on_board,
                             g_state.carbs_on_board,
                             g_state.activity.hr,
                             g_state.activity.intensity,
                             (unsigned long long)now);
                    mqtt_publish("glucosync/data/forecast", json, strlen(json));
                }
                break;
            }

            case EVENT_CLOUD_CMD: {
                if (event.data_len >= 1) {
                    if (event.data[0] == 0x01) {
                        /* Manual glucose entry mode */
                        ESP_LOGI(TAG, "Cloud: manual entry mode");
                    } else if (event.data[0] == 0x02) {
                        /* Request AGP report */
                        ESP_LOGI(TAG, "Cloud: AGP report requested");
                    }
                }
                break;
            }

            default:
                ESP_LOGD(TAG, "Unhandled event type 0x%02X", event.type);
                break;
            }
        }
    }
}

/* ── Cloud MQTT Callback ─────────────────────────────────────────── */

static void cloud_cmd_callback(const char *topic, const uint8_t *payload, uint8_t len)
{
    if (strstr(topic, "cmd/mode") != NULL && len >= 1) {
        payload_mode_t mode = { .mode = payload[0] };
        uint8_t packet[GS_MAX_PACKET_LEN];
        uint8_t pkt_len = glucosync_encode(packet, sizeof(packet),
                                            MSG_TYPE_CMD_MODE,
                                            GS_HUB_ID,
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
    ESP_LOGI(TAG, "GlucoSync Metabolic Hub starting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    g_state.node_count = 0;
    g_state.seq_counter = 0;
    g_state.hist_idx = 0;
    g_state.hist_count = 0;
    g_state.insulin_on_board = 0;
    g_state.carbs_on_board = 0;
    g_state.last_alert_level = 0;
    g_state.event_queue = xQueueCreate(32, sizeof(hub_event_t));

    /* Initialize edge ML */
    glucose_forecast_init();

    /* Initialize e-ink display */
    eink_display_init();
    eink_display_boot_screen();

    /* Initialize CGM BLE bridge */
    cgm_ble_init(cgm_data_callback);
    cgm_ble_start_scan();  /* Scan for Dexcom/Libre/custom CGM */

    /* Initialize BLE central for nodes */
    ble_central_init(ble_data_callback);
    ble_central_start_scan();

    /* Initialize Wi-Fi + MQTT */
    wifi_mqtt_init();
    wifi_mqtt_set_cmd_callback(cloud_cmd_callback);
    wifi_mqtt_connect("glucosync/hub/001");

    /* Initialize alert engine */
    alert_engine_init();

    /* Create event processor task */
    xTaskCreate(event_processor_task, "event_proc", 8192, NULL, 5, NULL);

    /* Hub IMU timer (10 Hz for tap detection) */
    const esp_timer_create_args_t imu_timer_args = {
        .callback = hub_imu_timer_callback,
        .name = "hub_imu_timer"
    };
    esp_timer_handle_t imu_timer;
    esp_timer_create(&imu_timer_args, &imu_timer);
    esp_timer_start_periodic(imu_timer, 100000);  /* 100 ms = 10 Hz */

    /* Forecast timer (every 5 minutes) */
    const esp_timer_create_args_t forecast_timer_args = {
        .callback = forecast_timer_callback,
        .name = "forecast_timer"
    };
    esp_timer_handle_t forecast_timer;
    esp_timer_create(&forecast_timer_args, &forecast_timer);
    esp_timer_start_periodic(forecast_timer, 300000000);  /* 5 min */

    /* Main loop — heartbeat + status */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));  /* 30 seconds */

        /* Send heartbeat to all nodes */
        uint8_t packet[GS_MAX_PACKET_LEN];
        uint8_t pkt_len = glucosync_encode(packet, sizeof(packet),
                                            MSG_TYPE_HEARTBEAT,
                                            GS_HUB_ID,
                                            g_state.seq_counter++,
                                            0, NULL, 0);
        ble_send_to_all(packet, pkt_len);

        /* Publish status to cloud */
        char status_json[128];
        uint8_t battery = 100;  /* TODO: read ADC */
        snprintf(status_json, sizeof(status_json),
                 "{\"hub\":\"001\",\"bat\":%d,\"nodes\":%d,\"glucose\":%d,\"iob\":%.1f}",
                 battery, g_state.node_count,
                 g_state.cgm.glucose_mgdl,
                 g_state.insulin_on_board);
        mqtt_publish("glucosync/status/hub", status_json, strlen(status_json));

        ESP_LOGI(TAG, "Heartbeat — nodes: %d — glucose: %d — risk: %d — IOB: %.1f — COB: %.1f",
                 g_state.node_count,
                 g_state.cgm.glucose_mgdl,
                 g_state.forecast.risk_score,
                 g_state.insulin_on_board,
                 g_state.carbs_on_board);
    }
}