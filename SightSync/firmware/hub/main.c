/**
 * SightSync Vision Hub — Main Firmware
 *
 * ESP32-S3 · Wi-Fi · BLE 5.0 · Sub-GHz 868 MHz (CC1101)
 * Edge ML (Visual Fatigue Index), 20-20-20 timer, blink anomaly,
 * e-ink display, speaker, haptic, LED ring.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#include "../common/protocol.h"
#include "../common/crypto.h"
#include "wifi_mqtt.h"
#include "ble_central.h"
#include "subghz_radio.h"
#include "eink_display.h"
#include "fatigue_engine.h"
#include "alert_engine.h"

static const char *TAG = "sightsync_hub";

/* ── Runtime State ─────────────────────────────────────────────────── */

typedef struct {
    /* Blink rate from Eye Tag (BLE) */
    uint8_t  blink_rate_bpm;
    uint8_t  blink_confidence;

    /* Viewing distance from Desk Sentinel (Sub-GHz) */
    uint16_t viewing_distance_mm;
    uint32_t near_work_minutes_today;

    /* Ambient light from Desk Sentinel (Sub-GHz) */
    uint16_t ambient_lux;
    uint16_t blue_light_mw;
    uint8_t  ambient_quality;

    /* Blue light dose */
    uint16_t blue_dose_mj_cm2;

    /* Head posture from Eye Tag (BLE) */
    int16_t  pitch_centi;
    uint8_t  forward_head_flag;
    uint8_t  posture_risk;

    /* Periocular temp from Eye Tag (BLE) */
    int16_t  periocular_temp_centi;
    int16_t  periocular_temp_delta;

    /* Edge ML outputs */
    uint8_t  fatigue_score;
    uint8_t  blink_anomaly;
    uint8_t  dry_eye_risk;

    /* 20-20-20 timer */
    uint32_t last_break_epoch;
    uint32_t minutes_since_break;
    uint8_t  break_overdue;

    /* Mode */
    uint8_t  mode;  /* 0=work, 1=rest, 2=child, 3=sleep */

    /* Lamp command */
    uint16_t lamp_target_cct;
    uint8_t  lamp_brightness;
} hub_state_t;

static hub_state_t s_state;

/* ── BLE RX callback (from Eye Tag) ───────────────────────────────── */

static void on_ble_rx(const sightsync_header_t *hdr, const uint8_t *payload)
{
    switch (hdr->msg_type) {
    case MSG_TYPE_DATA_BLINK: {
        const payload_blink_t *b = (const payload_blink_t *)payload;
        s_state.blink_rate_bpm   = b->blinks_per_min;
        s_state.blink_confidence = b->blink_confidence;
        break;
    }
    case MSG_TYPE_DATA_POSTURE: {
        const payload_posture_t *p = (const payload_posture_t *)payload;
        s_state.pitch_centi       = p->pitch_centi;
        s_state.forward_head_flag = p->forward_head_flag;
        s_state.posture_risk      = p->posture_risk;
        break;
    }
    case MSG_TYPE_DATA_TEMP: {
        const payload_temp_t *t = (const payload_temp_t *)payload;
        s_state.periocular_temp_centi  = t->temp_centi;
        s_state.periocular_temp_delta  = t->temp_delta_centi;
        break;
    }
    case MSG_TYPE_STATUS: {
        /* handle status heartbeat */
        break;
    }
    default:
        break;
    }
}

/* ── Sub-GHz RX callback (from Desk Sentinel & Lamp Node) ──────────── */

static void on_subghz_rx(const sightsync_header_t *hdr, const uint8_t *payload)
{
    switch (hdr->msg_type) {
    case MSG_TYPE_DATA_DISTANCE: {
        const payload_distance_t *d = (const payload_distance_t *)payload;
        s_state.viewing_distance_mm    = d->distance_mm;
        s_state.near_work_minutes_today = d->near_work_minutes;
        break;
    }
    case MSG_TYPE_DATA_LIGHT: {
        const payload_light_t *l = (const payload_light_t *)payload;
        s_state.ambient_lux    = l->ambient_lux;
        s_state.blue_light_mw  = l->blue_light_mw;
        s_state.ambient_quality = l->ambient_quality;
        s_state.blue_dose_mj_cm2 = l->blue_dose_today << 6;
        break;
    }
    case MSG_TYPE_DATA_BLUE_DOSE: {
        const payload_blue_dose_t *b = (const payload_blue_dose_t *)payload;
        s_state.blue_dose_mj_cm2 = b->dose_mj_cm2;
        break;
    }
    case MSG_TYPE_STATUS: {
        /* node heartbeat */
        break;
    }
    default:
        break;
    }
}

/* ── Fatigue Engine Task (5-second cycle) ──────────────────────────── */

static void fatigue_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        /* Compute Visual Fatigue Index from fused inputs */
        fatigue_inputs_t inputs = {
            .blink_rate           = s_state.blink_rate_bpm,
            .viewing_distance_mm  = s_state.viewing_distance_mm,
            .ambient_lux          = s_state.ambient_lux,
            .blue_dose_mj_cm2     = s_state.blue_dose_mj_cm2,
            .posture_angle_centi  = s_state.pitch_centi,
            .minutes_since_break  = s_state.minutes_since_break,
        };
        s_state.fatigue_score = fatigue_engine_compute(&inputs);

        /* Blink anomaly detection (isolation forest) */
        s_state.blink_anomaly = fatigue_engine_blink_anomaly(s_state.blink_rate_bpm);

        /* Dry-eye risk fusion */
        s_state.dry_eye_risk = fatigue_engine_dry_eye_risk(
            s_state.blink_rate_bpm,
            s_state.periocular_temp_delta,
            s_state.ambient_lux);

        ESP_LOGI(TAG, "fatigue=%d blink=%d/min dist=%dmm lux=%d posture_risk=%d dry_eye=%d",
                 s_state.fatigue_score, s_state.blink_rate_bpm,
                 s_state.viewing_distance_mm, s_state.ambient_lux,
                 s_state.posture_risk, s_state.dry_eye_risk);

        /* Alert engine evaluates thresholds */
        alert_engine_evaluate(&s_state);

        /* Update e-ink display */
        eink_display_update(&s_state);

        /* Send fatigue + forecast to cloud */
        if (wifi_mqtt_is_connected()) {
            char json[256];
            snprintf(json, sizeof(json),
                "{\"fatigue\":%d,\"blink\":%d,\"distance\":%d,\"lux\":%d,"
                "\"blue_dose\":%d,\"posture_risk\":%d,\"dry_eye\":%d,"
                "\"minutes_since_break\":%lu}",
                s_state.fatigue_score, s_state.blink_rate_bpm,
                s_state.viewing_distance_mm, s_state.ambient_lux,
                s_state.blue_dose_mj_cm2, s_state.posture_risk,
                s_state.dry_eye_risk, s_state.minutes_since_break);
            wifi_mqtt_publish("sightsync/hub/fatigue", json, strlen(json));
        }
    }
}

/* ── 20-20-20 Timer Task ───────────────────────────────────────────── */

static void break_timer_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  /* check every 10 s */

        uint32_t now = esp_timer_get_time() / 1000000;  /* epoch seconds */
        s_state.minutes_since_break = (now - s_state.last_break_epoch) / 60;

        /* Reset if user looked away (distance >1000 mm for >15 s) */
        static uint32_t far_start = 0;
        if (s_state.viewing_distance_mm > 1000) {
            if (far_start == 0) far_start = now;
            else if ((now - far_start) > 15) {
                s_state.last_break_epoch = now;
                s_state.minutes_since_break = 0;
                s_state.break_overdue = 0;
                far_start = 0;
                ESP_LOGI(TAG, "20-20-20 break detected");
            }
        } else {
            far_start = 0;
        }

        /* Alert if 20 minutes elapsed */
        if (s_state.minutes_since_break >= 20 && s_state.break_overdue == 0) {
            s_state.break_overdue = 1;
            alert_engine_break_reminder();
        }
    }
}

/* ── Lamp Control Task ──────────────────────────────────────────────── */

static void lamp_control_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));  /* evaluate every 30 s */

        /* Determine target CCT and brightness from time of day + fatigue */
        uint32_t hour = (esp_timer_get_time() / 1000000) % 86400 / 3600;

        uint16_t target_cct;
        uint8_t  target_brightness;

        if (hour >= 22 || hour < 6) {
            /* Night: warm dim */
            target_cct = 1800;
            target_brightness = 15;
        } else if (hour < 10) {
            /* Morning: warm medium */
            target_cct = 3000;
            target_brightness = 60;
        } else if (hour < 17) {
            /* Day: cool bright */
            target_cct = 5500;
            target_brightness = 80;
        } else if (hour < 20) {
            /* Evening: warm medium */
            target_cct = 3500;
            target_brightness = 55;
        } else {
            /* Late evening: warm dim */
            target_cct = 2200;
            target_brightness = 30;
        }

        /* Adjust for fatigue: if fatigue >60, boost brightness 10% */
        if (s_state.fatigue_score > 60) {
            target_brightness = (uint8_t)(target_brightness * 1.1f);
            if (target_brightness > 100) target_brightness = 100;
        }

        /* Adjust for ambient: if lux <300 and not night, boost */
        if (s_state.ambient_lux < 300 && hour >= 6 && hour < 22) {
            target_brightness = (uint8_t)(target_brightness * 1.2f);
            if (target_brightness > 100) target_brightness = 100;
        }

        if (target_cct != s_state.lamp_target_cct ||
            target_brightness != s_state.lamp_brightness) {

            s_state.lamp_target_cct = target_cct;
            s_state.lamp_brightness = target_brightness;

            payload_lamp_cmd_t cmd = {
                .target_cct       = target_cct,
                .brightness_pct   = target_brightness,
                .mode             = 0,  /* auto */
                .transition_sec   = 5,
                .reserved         = 0,
            };

            uint8_t pkt[32];
            uint8_t len = sightsync_encode(pkt, sizeof(pkt),
                MSG_TYPE_CMD_LAMP, SS_HUB_ID, 0, 0,
                (const uint8_t *)&cmd, sizeof(cmd));
            subghz_radio_send(SS_LAMP_ID_BASE, pkt, len);
            ESP_LOGI(TAG, "lamp cmd: CCT=%dK brightness=%d%%",
                     target_cct, target_brightness);
        }
    }
}

/* ── Heartbeat Task ─────────────────────────────────────────────────── */

static void heartbeat_task(void *arg)
{
    uint16_t seq = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint8_t pkt[16];
        uint8_t len = sightsync_encode(pkt, sizeof(pkt),
            MSG_TYPE_HEARTBEAT, SS_HUB_ID, seq++, 0, NULL, 0);
        subghz_radio_send(SS_BROADCAST_ID, pkt, len);
    }
}

/* ── Main ───────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "SightSync Vision Hub starting...");
    memset(&s_state, 0, sizeof(s_state));
    s_state.last_break_epoch = esp_timer_get_time() / 1000000;
    s_state.lamp_target_cct = 4000;
    s_state.lamp_brightness = 60;

    /* Init crypto (AES-128 key provisioned from NVS) */
    uint8_t aes_key[16] = {0};  /* TODO: load from NVS */
    sightsync_crypto_init(aes_key);

    /* Init e-ink display */
    eink_display_init();

    /* Init BLE central (connects to Eye Tag) */
    ble_central_init(on_ble_rx);
    ble_central_start_scan();

    /* Init Sub-GHz radio (CC1101 868 MHz) */
    subghz_radio_init(on_subghz_rx);

    /* Init Wi-Fi + MQTT */
    wifi_mqtt_init();

    /* Init fatigue engine (load tflite-micro models) */
    fatigue_engine_init();

    /* Init alert engine */
    alert_engine_init();

    /* Create tasks */
    xTaskCreate(fatigue_task, "fatigue", 8192, NULL, 5, NULL);
    xTaskCreate(break_timer_task, "break", 4096, NULL, 4, NULL);
    xTaskCreate(lamp_control_task, "lamp", 4096, NULL, 3, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "SightSync Vision Hub running.");
}