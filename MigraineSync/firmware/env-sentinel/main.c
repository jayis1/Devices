/**
 * MigraineSync — Env Sentinel Main
 * =================================
 * ESP32-S3 running ESP-IDF.
 *
 * Polls all environmental sensors at 1 Hz, computes 3-hour
 * barometric pressure delta, and transmits to Hub via Sub-GHz
 * every 30 seconds.
 *
 * License: MIT
 */

#include "config.h"
#include "sensors.h"
#include "../common/protocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "migrainesync_env";

/* ── Sub-GHz transmit (uses same SX1262 driver pattern as hub) ── */
extern int subghz_init(void);
extern int subghz_send(const uint8_t *data, size_t len);

static uint8_t s_seq = 0;

/* ── Build ENVIRONMENT TLV ──────────────────────────────── */
static size_t build_env_frame(const env_data_t *env, uint8_t *buf, size_t max_len)
{
    /* Build TLV payload: type(1) + len(1) + value(25) = 27 bytes */
    uint8_t value[25];
    encode_f32(&value[0],  env->pressure_hpa);
    encode_f32(&value[4],  env->pressure_delta_3h);
    encode_f32(&value[8],  env->light_lux);
    encode_f32(&value[12], env->temp_c);
    encode_f32(&value[16], env->humidity_pct);
    encode_u16(&value[20], (uint16_t)env->voc_index);
    encode_u16(&value[22], env->co2_ppm);
    value[24] = env->noise_db;

    tlv_t tlv = {
        .type = MSG_ENVIRONMENT,
        .len = 25,
        .value = value,
    };

    frame_t frame;
    size_t total = frame_build(&frame, s_seq++, &tlv, 1);
    if (total > max_len)
        return 0;

    memcpy(buf, &frame, total);
    return total;
}

/* ── Sensor task (1 Hz polling) ─────────────────────────── */
static void sensor_task(void *arg)
{
    env_data_t env;

    while (1) {
        if (sensors_read(&env) == 0) {
            ESP_LOGI(TAG, "P=%.1fhPa ΔP=%.2f L=%.0flux T=%.1f°C RH=%.1f%% "
                          "VOC=%.0f CO2=%u noise=%udB",
                     env.pressure_hpa, env.pressure_delta_3h,
                     env.light_lux, env.temp_c, env.humidity_pct,
                     env.voc_index, env.co2_ppm, env.noise_db);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  /* 1 Hz */
    }
}

/* ── Sub-GHz TX task (every 30s) ────────────────────────── */
static void subghz_tx_task(void *arg)
{
    env_data_t env;
    uint8_t tx_buf[64];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));  /* 30 seconds */

        /* Get latest sensor data */
        if (sensors_read(&env) != 0) {
            ESP_LOGW(TAG, "Sensor read failed, skipping TX");
            continue;
        }

        size_t len = build_env_frame(&env, tx_buf, sizeof(tx_buf));
        if (len > 0) {
            int sent = subghz_send(tx_buf, len);
            if (sent > 0)
                ESP_LOGI(TAG, "Sub-GHz TX: %u bytes (seq=%u)", (unsigned)len, s_seq - 1);
            else
                ESP_LOGW(TAG, "Sub-GHz TX failed");
        }
    }
}

/* ── app_main ───────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "MigraineSync Env Sentinel v%s starting...", FIRMWARE_VERSION);

    /* Initialize sensors */
    if (sensors_init() != 0) {
        ESP_LOGE(TAG, "Sensor init failed!");
        return;
    }

    /* Initialize Sub-GHz */
    subghz_init();

    /* Start tasks */
    xTaskCreate(sensor_task,     "sensors",   4096, NULL, 5, NULL);
    xTaskCreate(subghz_tx_task,  "subghz_tx", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Env Sentinel running (1 Hz sensor poll, 30s Sub-GHz TX)");
}