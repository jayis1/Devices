/**
 * SightSync Desk Sentinel — Main Firmware
 *
 * ESP32-S3 · Sub-GHz 868 MHz (CC1101)
 * VL53L1X ToF distance sensor, VEML7700 ambient light,
 * TCS34725 RGBC color sensor, APDS9306 blue light, SSD1306 OLED.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"

#include "../common/protocol.h"
#include "../common/crc8.h"
#include "subghz_radio.h"
#include "sensors.h"

static const char *TAG = "desk_sentinel";

/* ── Sensor data ─────────────────────────────────────────────────── */

typedef struct {
    uint16_t distance_mm;
    uint8_t  distance_quality;
    uint16_t ambient_lux;
    uint16_t blue_light_mw;
    uint16_t cct_estimate;
    uint8_t  ambient_quality;
    uint8_t  blue_dose_today;
    uint32_t near_work_minutes;
    uint32_t near_work_start_epoch;
    bool     near_work_active;
} desk_state_t;

static desk_state_t s_state;

/* ── Sensor reading task (1 Hz) ──────────────────────────────────── */

static void sensor_task(void *arg)
{
    uint16_t seq = 0;
    uint32_t last_below_300 = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* Read VL53L1X distance */
        s_state.distance_mm = sensors_read_distance(&s_state.distance_quality);

        /* Read VEML7700 ambient lux */
        s_state.ambient_lux = sensors_read_lux();

        /* Read TCS34725 RGBC → estimate CCT */
        uint16_t r, g, b, c;
        sensors_read_rgbc(&r, &g, &b, &c);
        s_state.cct_estimate = sensors_estimate_cct(r, g, b, c);

        /* Read APDS9306 blue light */
        s_state.blue_light_mw = sensors_read_blue_light();

        /* Ambient quality assessment (ISO 8995) */
        if (s_state.ambient_lux < 300) {
            s_state.ambient_quality = 0;  /* insufficient */
        } else if (s_state.ambient_lux < 500) {
            s_state.ambient_quality = 1;  /* adequate */
        } else {
            s_state.ambient_quality = 2;  /* good */
        }

        /* Accumulate blue-light dose (simplified: mW/m² × 1s = mJ/m²) */
        s_state.blue_dose_today += (uint8_t)(s_state.blue_light_mw / 1000);
        if (s_state.blue_dose_today > 255) s_state.blue_dose_today = 255;

        /* Near-work tracking: if distance <300mm for >5 min continuous */
        uint32_t now = esp_timer_get_time() / 1000000;
        if (s_state.distance_mm > 0 && s_state.distance_mm < 300) {
            if (last_below_300 == 0) {
                last_below_300 = now;
            } else if ((now - last_below_300) > 300) {  /* 5 minutes */
                if (!s_state.near_work_active) {
                    s_state.near_work_active = true;
                    s_state.near_work_start_epoch = now;
                }
                s_state.near_work_minutes = (now - s_state.near_work_start_epoch) / 60;
            }
        } else {
            if (s_state.near_work_active && s_state.distance_mm > 1000) {
                /* Break detected (distance >1000 mm) */
                s_state.near_work_active = false;
                last_below_300 = 0;
            } else if (s_state.distance_mm == 0 || s_state.distance_mm >= 300) {
                last_below_300 = 0;
            }
        }

        ESP_LOGI(TAG, "dist=%dmm lux=%d cct=%dK blue=%dmW near=%lumin",
                 s_state.distance_mm, s_state.ambient_lux,
                 s_state.cct_estimate, s_state.blue_light_mw,
                 (unsigned long)s_state.near_work_minutes);

        /* Send distance data to hub (every second) */
        payload_distance_t dist_pkt = {
            .distance_mm        = s_state.distance_mm,
            .distance_quality   = s_state.distance_quality,
            .near_work_flag     = s_state.near_work_active ? 1 : 0,
            .near_work_minutes  = s_state.near_work_minutes,
            .timestamp          = now,
        };
        uint8_t pkt[32];
        uint8_t len = sightsync_encode(pkt, sizeof(pkt),
            MSG_TYPE_DATA_DISTANCE, SS_DESK_ID_BASE, seq++, 0,
            (const uint8_t *)&dist_pkt, sizeof(dist_pkt));
        subghz_radio_send(SS_HUB_ID, pkt, len);
    }
}

/* ── Light data task (every 30 seconds) ───────────────────────────── */

static void light_task(void *arg)
{
    uint16_t seq = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        uint32_t now = esp_timer_get_time() / 1000000;

        payload_light_t light_pkt = {
            .ambient_lux     = s_state.ambient_lux,
            .blue_light_mw   = s_state.blue_light_mw,
            .cct_estimate    = s_state.cct_estimate,
            .blue_dose_today = s_state.blue_dose_today,
            .blue_dose_pct   = (uint8_t)(s_state.blue_dose_today * 100 / 255),
            .ambient_quality = s_state.ambient_quality,
            .timestamp       = now,
        };
        uint8_t pkt[32];
        uint8_t len = sightsync_encode(pkt, sizeof(pkt),
            MSG_TYPE_DATA_LIGHT, SS_DESK_ID_BASE, seq++, 0,
            (const uint8_t *)&light_pkt, sizeof(light_pkt));
        subghz_radio_send(SS_HUB_ID, pkt, len);

        ESP_LOGI(TAG, "light data sent: lux=%d cct=%dK blue_dose=%d",
                 s_state.ambient_lux, s_state.cct_estimate, s_state.blue_dose_today);
    }
}

/* ── Heartbeat task (every 60 seconds) ────────────────────────────── */

static void heartbeat_task(void *arg)
{
    uint16_t seq = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        payload_status_t status = {
            .battery_pct = 100,  /* USB-C powered */
            .state = 1,          /* active */
            .error_code = 0,
        };
        uint8_t pkt[16];
        uint8_t len = sightsync_encode(pkt, sizeof(pkt),
            MSG_TYPE_STATUS, SS_DESK_ID_BASE, seq++, 0,
            (const uint8_t *)&status, sizeof(status));
        subghz_radio_send(SS_HUB_ID, pkt, len);
    }
}

/* ── Main ─────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "SightSync Desk Sentinel starting...");
    memset(&s_state, 0, sizeof(s_state));

    /* Initialize I²C bus */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    /* Initialize sensors */
    sensors_init();

    /* Initialize Sub-GHz radio */
    subghz_radio_init(NULL);

    /* Create tasks */
    xTaskCreate(sensor_task, "sensor", 4096, NULL, 5, NULL);
    xTaskCreate(light_task, "light", 4096, NULL, 4, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Desk Sentinel running.");
}