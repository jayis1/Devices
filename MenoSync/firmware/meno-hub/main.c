/*
 * MenoSync — Meno Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS, ESP-IDF v5.x
 *
 * The Hub is the central coordinator of the MenoSync menopause
 * management system. It:
 *   1. Manages BLE 5.0 connections to Wrist Band and Bed Mat
 *   2. Manages Sub-GHz 868 MHz TDMA mesh with Climate Nodes
 *   3. Runs local edge inference (TFLite-Micro):
 *      - HotFlashNet screening from skin temp + EDA + HRV trends
 *      - NightSweatDetect screening from bed mat data
 *   4. Dispatches pre-emptive cooling commands to Climate Nodes
 *      when hot flash risk is high
 *   5. Captures voice samples via I²S microphone for mood screening
 *   6. Drives 3.5" TFT display (symptoms, hot flash risk, sleep score)
 *   7. Provides audio guidance (cooling notifications, reminders)
 *   8. Bridges all data to cloud via Wi-Fi/MQTT
 *   9. Manages OTA firmware distribution to all nodes
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

#include "../common/protocol.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "MenoSync-Hub";

/* === Global state === */
static ms_mesh_ctx_t g_mesh;
static QueueHandle_t g_vitals_queue;     /* Vitals from Wrist Band */
static QueueHandle_t g_eda_queue;        /* EDA from Wrist Band */
static QueueHandle_t g_bcg_queue;        /* BCG from Bed Mat */
static QueueHandle_t g_sweat_queue;      /* Sweat data from Bed Mat */
static QueueHandle_t g_ambient_queue;    /* Ambient from Climate Nodes */
static QueueHandle_t g_alert_queue;      /* Alert commands */
static SemaphoreHandle_t g_radio_mutex;

/* Monitoring state */
static uint8_t g_monitoring_active = 0;
static char g_patient_id[16] = "patient_001";
static uint8_t g_patient_age = 52;
static char g_menopause_stage[16] = "perimenopause";

/* Latest vitals (from Wrist Band) */
static uint8_t  g_hr = 72;
static uint8_t  g_spo2 = 98;
static int16_t  g_skin_temp_cd = 3300;
static uint16_t g_hrv_rmssd = 45;
static uint8_t  g_activity_class = 0;
static uint8_t  g_band_battery = 100;

/* EDA state (from Wrist Band) */
static uint16_t g_eda_microsiemens = 5;
static uint16_t g_eda_std = 1;
static uint8_t  g_stress_level = 0;
static uint8_t  g_band_eda_battery = 100;

/* BCG state (from Bed Mat) */
static uint8_t  g_bcg_hr = 65;
static uint8_t  g_bcg_br = 14;
static uint8_t  g_motion_level = 0;
static uint8_t  g_sleep_stage = 0;
static uint8_t  g_mat_battery = 100;

/* Sweat state (from Bed Mat) */
static uint8_t  g_sweat_pct = 0;
static uint8_t  g_night_sweat_flag = 0;
static int16_t  g_bed_temp_cd = 2500;
static uint8_t  g_mat_sweat_battery = 100;

/* Ambient state (from Climate Nodes — bedroom node = 0x10) */
static int16_t  g_ambient_temp_cd = 2300;
static uint16_t g_humidity_pct = 45;
static int16_t  g_radiant_temp_cd = 2400;
static uint8_t  g_hvac_state = 0;
static uint8_t  g_shade_pct = 0;

/* Vitals history for hot flash risk (20 min @ 1 Hz = 1200 samples) */
#define VITALS_HISTORY_LEN 1200
static uint8_t  g_hr_history[VITALS_HISTORY_LEN];
static int16_t  g_temp_history[VITALS_HISTORY_LEN];
static uint16_t g_eda_history[VITALS_HISTORY_LEN];
static int g_vitals_hist_idx = 0;
static int g_vitals_hist_count = 0;

/* EDA history (4 Hz, 20 min = 4800 samples → subsample to 120 at 10s) */
#define EDA_HIST_LEN 120
static uint16_t g_eda_hist[EDA_HIST_LEN];
static int g_eda_hist_idx = 0;
static int g_eda_hist_count = 0;

/* Risk scores (from edge inference) */
static uint8_t g_hotflash_risk = 0;
static uint8_t g_nightsweat_risk = 0;
static uint8_t g_sleep_quality = 75;
static uint8_t g_mood_risk = 0;
static uint8_t g_bone_risk = 0;
static uint8_t g_overall_risk = 0;
static uint8_t g_alert_level = 0;

/* Hot flash prediction */
static uint8_t g_hotflash_prob = 0;
static uint8_t g_hotflash_minutes_to_onset = 0;
static uint8_t g_hotflash_severity_pred = 0;
static uint8_t g_cooling_recommended = 0;
static uint8_t g_cooling_active = 0;

/* Connected sensors */
static uint8_t g_band_connected = 0;
static uint8_t g_mat_connected = 0;
static uint8_t g_climate_count = 0;

/* === Forward declarations === */
static void ble_coordinator_task(void *arg);
static void vitals_processor_task(void *arg);
static void eda_processor_task(void *arg);
static void bcg_processor_task(void *arg);
static void sweat_processor_task(void *arg);
static void ambient_processor_task(void *arg);
static void hotflash_screening_task(void *arg);
static void cooling_controller_task(void *arg);
static void subghz_coordinator_task(void *arg);
static void voice_capture_task(void *arg);
static void cloud_bridge_task(void *arg);
static void display_task(void *arg);
static void audio_feedback_task(void *arg);
static void ota_manager_task(void *arg);

/* Edge inference stubs */
static uint8_t edge_hotflash_risk(const uint8_t *hr_hist,
                                   const int16_t *temp_hist,
                                   const uint16_t *eda_hist,
                                   int count);
static uint8_t edge_night_sweat(uint8_t sweat_pct, int16_t bed_temp,
                                 uint8_t motion, uint8_t hr);

/* === I²C init === */
static esp_err_t init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = HUB_GPIO_I2C_SDA,
        .scl_io_num = HUB_GPIO_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
        .clk_flags = 0,
    };
    esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* === Display update (ILI9488) === */
static void update_display(void)
{
    /* In production: SPI to ILI9488, render:
     * - Top: Hot Flash Risk meter (colored bar + % + minutes to onset)
     * - Middle: Current symptoms (skin temp, EDA stress, HR, sleep stage)
     * - Middle: Sleep quality score, night sweat status
     * - Bottom: Cooling status (HVAC mode, shade %), ambient temp
     * - Bottom: Sensor connection status, battery levels
     */
    ESP_LOGI(TAG, "Display: HF_risk=%d%% (onset in %d min) SkinT=%.1f°C EDA=%dµS "
             "stress=%d HR=%d sleep=%d cool=%s ambient=%.1f°C",
             g_hotflash_prob, g_hotflash_minutes_to_onset,
             g_skin_temp_cd / 100.0f, g_eda_microsiemens, g_stress_level,
             g_hr, g_sleep_stage, g_cooling_active ? "ON" : "off",
             g_ambient_temp_cd / 100.0f);
}

/* === Audio feedback === */
static void play_audio_message(const char *msg_id)
{
    /* I²S MAX98357A, play pre-encoded PCM messages:
     * "cooling_starting"   → "Cooling activated. A hot flash may be approaching."
     * "hotflash_warning"   → "Hot flash warning. Moving to a cooler area may help."
     * "night_sweat"        → "Night sweat detected. Your sleep was disrupted."
     * "med_reminder"       → "Remember to take your calcium and vitamin D."
     * "voice_prompt"       → "Please say a few words about how you're feeling today."
     * "vitals_normal"      → "Your vitals look good today."
     * "stress_high"        → "Your stress level is elevated. Consider a short break."
     */
    ESP_LOGI(TAG, "Audio: %s", msg_id);
}

/* === Haptic alert === */
static void trigger_haptic(uint8_t pattern)
{
    /* DRV2605L patterns:
     * 0 = single tap (info — cooling started)
     * 1 = double pulse (warning — hot flash approaching)
     * 2 = triple burst (urgent — hot flash detected)
     * 3 = continuous (critical — system alert)
     */
    ESP_LOGI(TAG, "Haptic: pattern %d", pattern);
}

/* === Send cooling command via Sub-GHz to Climate Node === */
static void send_cooling_cmd(uint8_t climate_node_id, uint8_t action,
                              int16_t target_temp, uint8_t hvac_mode,
                              uint8_t shade_pct)
{
    ms_cooling_cmd_t cmd = {
        .action = action,
        .target_temp_cd = target_temp,
        .hvac_mode = hvac_mode,
        .shade_pct = shade_pct,
    };

    uint8_t msg[MS_MAX_MSG];
    size_t len = ms_encode(msg, sizeof(msg),
                           0x01, climate_node_id,
                           MS_MSG_COOLING_CMD, 0,
                           0, (uint8_t *)&cmd, sizeof(cmd));
    if (len > 0) {
        /* In production: send via RFM69HCW Sub-GHz to climate node */
        ESP_LOGI(TAG, "Sub-GHz → Climate Node 0x%02X: action=%d target=%.1f°C "
                 "HVAC=%d shade=%d%%",
                 climate_node_id, action, target_temp / 100.0f,
                 hvac_mode, shade_pct);
    }
}

/* === BLE Coordinator Task === */
static void ble_coordinator_task(void *arg)
{
    ESP_LOGI(TAG, "BLE coordinator started");
    uint8_t seq = 0;

    while (1) {
        if (g_band_connected || g_mat_connected) {
            uint8_t msg[MS_MAX_MSG];
            uint8_t payload[2] = {0x01, g_monitoring_active};
            size_t len = ms_encode(msg, sizeof(msg),
                                   0x01, MS_BROADCAST,
                                   MS_MSG_COMMAND, MS_CMD_START_MONITORING,
                                   seq++, payload, 2);
            (void)len;
        }
        vTaskDelay(pdMS_TO_TICKS(MS_HEARTBEAT_INTERVAL_S * 1000));
    }
}

/* === Vitals Processor Task === */
static void vitals_processor_task(void *arg)
{
    ms_vitals_t vitals;

    while (1) {
        if (xQueueReceive(g_vitals_queue, &vitals, portMAX_DELAY) == pdTRUE) {
            g_hr = vitals.heart_rate;
            g_spo2 = vitals.spo2;
            g_skin_temp_cd = vitals.skin_temp_cd;
            g_hrv_rmssd = vitals.hrv_rmssd_ms;
            g_activity_class = vitals.activity_class;
            g_band_battery = vitals.battery_pct;

            /* Store in history buffer */
            g_hr_history[g_vitals_hist_idx] = vitals.heart_rate;
            g_temp_history[g_vitals_hist_idx] = vitals.skin_temp_cd;
            g_vitals_hist_idx = (g_vitals_hist_idx + 1) % VITALS_HISTORY_LEN;
            if (g_vitals_hist_count < VITALS_HISTORY_LEN)
                g_vitals_hist_count++;

            /* Threshold alerts */
            if (vitals.skin_temp_cd > MS_SKIN_TEMP_HOT_CD) {
                ESP_LOGW(TAG, "Skin temp elevated: %.1f°C (hot flash indicator)",
                         vitals.skin_temp_cd / 100.0f);
            }
            if (vitals.heart_rate > 110) {
                ESP_LOGW(TAG, "Elevated HR: %d bpm", vitals.heart_rate);
            }
        }
    }
}

/* === EDA Processor Task === */
static void eda_processor_task(void *arg)
{
    ms_eda_t eda;

    while (1) {
        if (xQueueReceive(g_eda_queue, &eda, portMAX_DELAY) == pdTRUE) {
            g_eda_microsiemens = eda.eda_microsiemens;
            g_eda_std = eda.eda_std;
            g_stress_level = eda.stress_level;

            /* Store EDA in subsampled history (every sample → 10s bucket) */
            g_eda_hist[g_eda_hist_idx] = eda.eda_microsiemens;
            g_eda_hist_idx = (g_eda_hist_idx + 1) % EDA_HIST_LEN;
            if (g_eda_hist_count < EDA_HIST_LEN)
                g_eda_hist_count++;

            /* High stress alert */
            if (g_stress_level >= 3) {
                ESP_LOGW(TAG, "High stress detected: EDA=%d µS", g_eda_microsiemens);
                trigger_haptic(1);
                play_audio_message("stress_high");
            }
        }
    }
}

/* === BCG Processor Task === */
static void bcg_processor_task(void *arg)
{
    ms_bcg_t bcg;

    while (1) {
        if (xQueueReceive(g_bcg_queue, &bcg, portMAX_DELAY) == pdTRUE) {
            g_bcg_hr = bcg.hr_bpm;
            g_bcg_br = bcg.br_bpm;
            g_motion_level = bcg.motion_level;
            g_sleep_stage = bcg.sleep_stage;
            g_mat_battery = bcg.battery_pct;
        }
    }
}

/* === Sweat Processor Task === */
static void sweat_processor_task(void *arg)
{
    ms_sweat_t sweat;

    while (1) {
        if (xQueueReceive(g_sweat_queue, &sweat, portMAX_DELAY) == pdTRUE) {
            g_sweat_pct = sweat.sweat_pct;
            g_night_sweat_flag = sweat.night_sweat_flag;
            g_bed_temp_cd = sweat.bed_temp_cd;
            g_mat_sweat_battery = sweat.battery_pct;

            /* Night sweat detection */
            g_nightsweat_risk = edge_night_sweat(sweat.sweat_pct, sweat.bed_temp_cd,
                                                  g_motion_level, g_bcg_hr);

            if (g_night_sweat_flag >= 2) {
                ESP_LOGW(TAG, "Night sweat detected: moisture=%d%% (severe)",
                         sweat.sweat_pct);
                play_audio_message("night_sweat");
            }
        }
    }
}

/* === Ambient Processor Task === */
static void ambient_processor_task(void *arg)
{
    ms_ambient_t ambient;

    while (1) {
        if (xQueueReceive(g_ambient_queue, &ambient, portMAX_DELAY) == pdTRUE) {
            g_ambient_temp_cd = ambient.ambient_temp_cd;
            g_humidity_pct = ambient.humidity_pct;
            g_radiant_temp_cd = ambient.radiant_temp_cd;
            g_hvac_state = ambient.hvac_state;
            g_shade_pct = ambient.shade_pct;

            /* Check if ambient temp is a trigger */
            if (g_ambient_temp_cd > MS_AMBIENT_TEMP_HIGH * 10) {
                ESP_LOGW(TAG, "Ambient temp high: %.1f°C (hot flash trigger risk)",
                         g_ambient_temp_cd / 100.0f);
            }
        }
    }
}

/* === Hot Flash Screening Task === */
/* Runs every 30 seconds, analyzes 20-minute vitals + EDA window
 * for hot flash risk indicators:
 * - Skin temp rising > 0.3°C in 10 min → +30
 * - EDA spike (> 2.5 µS above baseline) → +25
 * - HR rising > 12 bpm in 15 min → +15
 * - Ambient temp > 26°C → +15
 * - High stress level → +15
 */
static void hotflash_screening_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (g_vitals_hist_count >= 30) {
            g_hotflash_risk = edge_hotflash_risk(g_hr_history,
                                                  g_temp_history,
                                                  g_eda_hist,
                                                  g_vitals_hist_count);

            /* Determine if cooling is recommended */
            g_cooling_recommended = g_hotflash_risk > MS_COOLING_TRIGGER_RISK ? 1 : 0;

            /* Estimate minutes to onset (simplified) */
            if (g_hotflash_risk > 80) {
                g_hotflash_prob = g_hotflash_risk;
                g_hotflash_minutes_to_onset = 5 + (g_hotflash_risk % 5);
                g_hotflash_severity_pred = 2;  /* severe */
            } else if (g_hotflash_risk > 60) {
                g_hotflash_prob = g_hotflash_risk;
                g_hotflash_minutes_to_onset = 10 + (g_hotflash_risk % 5);
                g_hotflash_severity_pred = 1;  /* moderate */
            } else if (g_hotflash_risk > 30) {
                g_hotflash_prob = g_hotflash_risk;
                g_hotflash_minutes_to_onset = 15 + (g_hotflash_risk % 5);
                g_hotflash_severity_pred = 0;  /* mild */
            } else {
                g_hotflash_prob = g_hotflash_risk;
                g_hotflash_minutes_to_onset = 0;
                g_hotflash_severity_pred = 0;
            }

            /* Determine overall alert level */
            uint8_t max_risk = g_hotflash_risk;
            if (g_nightsweat_risk > max_risk) max_risk = g_nightsweat_risk;
            if (g_mood_risk > max_risk) max_risk = g_mood_risk;
            g_overall_risk = max_risk;

            if (max_risk >= 80) {
                g_alert_level = 3;
                trigger_haptic(2);
                play_audio_message("hotflash_warning");
            } else if (max_risk >= 60) {
                g_alert_level = 2;
                trigger_haptic(1);
            } else if (max_risk >= 30) {
                g_alert_level = 1;
            } else {
                g_alert_level = 0;
            }

            update_display();
        }
    }
}

/* === Cooling Controller Task === */
/* When hot flash risk is high, dispatches pre-emptive cooling commands
 * to Climate Nodes. The CoolingOptimizer DQN (cloud) learns the optimal
 * cooling strategy; the Hub executes the commands.
 */
static void cooling_controller_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  /* Every 10s */

        if (!g_monitoring_active)
            continue;

        if (g_cooling_recommended && !g_cooling_active) {
            /* Start pre-emptive cooling */
            ESP_LOGI(TAG, "Pre-emptive cooling: hot flash risk %d%%, onset in ~%d min",
                     g_hotflash_prob, g_hotflash_minutes_to_onset);

            /* Send cooling command to bedroom Climate Node (0x10) */
            send_cooling_cmd(0x10, 1, MS_COOLING_TEMP_TARGET_CD, 1, /* cool mode */
                             g_hotflash_prob > 50 ? 80 : 40);

            /* If sunlight is a trigger (radiant temp high), close shades more */
            if (g_radiant_temp_cd > g_ambient_temp_cd + 200) {
                send_cooling_cmd(0x10, 3, 0, 0, 90);  /* Close shades 90% */
            }

            g_cooling_active = 1;
            play_audio_message("cooling_starting");
            trigger_haptic(0);

        } else if (!g_cooling_recommended && g_cooling_active) {
            /* Stop cooling */
            ESP_LOGI(TAG, "Cooling deactivated — hot flash risk subsided");
            send_cooling_cmd(0x10, 0, 0, 0, 0);
            g_cooling_active = 0;
        }
    }
}

/* === Sub-GHz Coordinator Task === */
/* Manages TDMA mesh with Climate Nodes.
 * In production: use RFM69HCW via SPI, TDMA slot scheduling,
 * receive ambient data and heartbeats, send cooling commands.
 */
static void subghz_coordinator_task(void *arg)
{
    ESP_LOGI(TAG, "Sub-GHz 868 MHz coordinator started");
    uint8_t rx_buf[MS_MAX_MSG];

    while (1) {
        /* In production: receive from RFM69HCW, parse protocol messages,
         * route to appropriate queue.
         */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* === Voice Capture Task === */
/* Captures 30-second voice samples via I²S microphone (ICS-43434)
 * for mood/brain fog screening. Runs 2×/day: morning and evening.
 */
static void voice_capture_task(void *arg)
{
    uint32_t capture_count = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(12 * 60 * 60 * 1000));  /* Every 12h (2×/day) */

        if (!g_monitoring_active)
            continue;

        ESP_LOGI(TAG, "Voice capture #%lu starting (30s)", capture_count++);

        /* In production:
         * 1. Play audio prompt: "Please say a few words about how you're feeling today."
         * 2. Record 30s of I²S audio at 16 kHz
         * 3. Extract prosody features on-device (ESP32-S3 DSP)
         * 4. Pack into ms_prosody_t (128 bytes)
         * 5. Send to cloud via MQTT for MoodStress CNN inference
         * 6. NO raw audio stored or transmitted
         */
        play_audio_message("voice_prompt");

        /* Simulated prosody extraction */
        ms_prosody_t prosody = {
            .f0_mean = 180.0f + (capture_count % 10) * 2.0f,
            .f0_std = 20.0f + (capture_count % 5) * 1.5f,
            .f0_range = 65.0f,
            .jitter_local = 1.1f + (capture_count % 3) * 0.1f,
            .jitter_ppq5 = 0.7f,
            .shimmer_local = 7.5f,
            .shimmer_apq11 = 5.5f,
            .hnr_db = 20.0f + (capture_count % 4) * 0.5f,
            .speech_rate = 3.5f + (capture_count % 7) * 0.1f,
            .pause_ratio = 0.30f + (capture_count % 5) * 0.02f,
            .mean_intensity_db = 62.0f,
            .intensity_var = 7.0f,
            .spectral_slope = -0.040f,
            .spectral_flux = 0.10f,
            .mfcc_1 = -1.8f,
            .mfcc_2 = 1.5f,
            .mfcc_3 = -0.4f,
            .mfcc_4 = 0.2f,
            .breathiness = 0.12f,
            .roughness = 0.06f,
            .pitch_declination = -0.015f,
            .voiced_ratio = 0.75f,
            .energy_mean = 0.040f,
            .energy_std = 0.010f,
            .dur_phoneme_mean = 0.16f,
            .dur_pause_mean = 0.30f,
            .dur_pause_std = 0.12f,
            .f0_cv = 0.11f,
            .intensity_cv = 0.16f,
            .spectral_centroid = 1750.0f,
            .spectral_spread = 1100.0f,
            .prosody_score = 0.12f + (capture_count % 4) * 0.05f,
        };

        ESP_LOGI(TAG, "Voice capture complete — F0=%.1f Hz, jitter=%.2f%%, HNR=%.1f dB",
                 prosody.f0_mean, prosody.jitter_local, prosody.hnr_db);
    }
}

/* === Cloud Bridge Task === */
static void cloud_bridge_task(void *arg)
{
    uint32_t report_seq = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        if (!g_monitoring_active)
            continue;

        /* Build risk assessment */
        ms_risk_t risk = {
            .hotflash_risk = g_hotflash_risk,
            .nightsweat_risk = g_nightsweat_risk,
            .sleep_quality = g_sleep_quality,
            .mood_risk = g_mood_risk,
            .bone_risk = g_bone_risk,
            .overall_risk = g_overall_risk,
            .cooling_active = g_cooling_active,
            .alert_level = g_alert_level,
        };

        /* In production:
         * 1. Build JSON payload with all current vitals, EDA, BCG, sweat, ambient, risk
         * 2. Publish to MQTT topic: menosync/telemetry/{patient_id}
         * 3. If Wi-Fi unavailable, buffer to microSD
         * 4. Check for OTA update availability
         */
        report_seq++;
        ESP_LOGI(TAG, "Cloud report #%lu: risk={HF=%d%%, NS=%d%%, sleep=%d, mood=%d%%, "
                 "bone=%d%%} level=%d cooling=%s",
                 report_seq, risk.hotflash_risk, risk.nightsweat_risk,
                 risk.sleep_quality, risk.mood_risk, risk.bone_risk,
                 risk.alert_level, risk.cooling_active ? "ON" : "off");

        /* Emergency alert dispatch */
        if (g_alert_level >= 3) {
            ESP_LOGW(TAG, "CRITICAL: Dispatching alert to patient + healthcare provider");
        }
    }
}

/* === Display Task === */
static void display_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (g_monitoring_active) {
            update_display();
        }
    }
}

/* === Audio Feedback Task === */
static void audio_feedback_task(void *arg)
{
    uint32_t last_med_reminder = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        if (!g_monitoring_active)
            continue;

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* Calcium + Vitamin D reminder (every 12 hours — bone health) */
        if ((now - last_med_reminder) > 12 * 60 * 60 * 1000) {
            play_audio_message("med_reminder");
            last_med_reminder = now;
        }
    }
}

/* === OTA Manager Task === */
static void ota_manager_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3600 * 1000));
        /* In production: check cloud for firmware updates, distribute via BLE/Sub-GHz */
    }
}

/* === Edge Inference: Hot Flash Risk Screening === */
/* Lightweight on-device screening using multi-modal trends.
 * Full HotFlashNet LSTM runs in cloud; this is a fast pre-screen.
 *
 * Indicators:
 * - Skin temp rising > 0.3°C in 10 min → +30
 * - EDA spike (> 2.5 µS above recent average) → +25
 * - HR rising > 12 bpm in 15 min → +15
 * - Ambient temp > 26°C → +15
 * - High stress level (from EDA) → +15
 */
static uint8_t edge_hotflash_risk(const uint8_t *hr_hist,
                                   const int16_t *temp_hist,
                                   const uint16_t *eda_hist,
                                   int count)
{
    if (count < 30) return 0;
    uint8_t risk = 0;

    /* Skin temp trend (last 10 min = 600 samples at 1 Hz) */
    int lookback = count < 600 ? count : 600;
    int start = (g_vitals_hist_idx - lookback + VITALS_HISTORY_LEN) %
                VITALS_HISTORY_LEN;
    int16_t old_temp = temp_hist[start];
    int16_t new_temp = temp_hist[(g_vitals_hist_idx - 1 + VITALS_HISTORY_LEN) %
                                  VITALS_HISTORY_LEN];
    int16_t temp_rise = new_temp - old_temp;
    if (temp_rise > MS_SKIN_TEMP_RISE_CD) risk += 30;
    else if (temp_rise > 20) risk += 15;  /* > 0.2°C */

    /* EDA spike (compare recent to baseline average) */
    if (g_eda_hist_count > 10) {
        uint32_t eda_avg = 0;
        for (int i = 0; i < g_eda_hist_count; i++)
            eda_avg += g_eda_hist[i];
        eda_avg /= g_eda_hist_count;

        if (g_eda_microsiemens > eda_avg + MS_EDA_SPIKE_UV) risk += 25;
        else if (g_eda_microsiemens > eda_avg + 150) risk += 12;
    }

    /* HR rising trend (15 min = 900 samples) */
    int hr_lookback = count < 900 ? count : 900;
    int hr_start = (g_vitals_hist_idx - hr_lookback + VITALS_HISTORY_LEN) %
                   VITALS_HISTORY_LEN;
    uint8_t old_hr = hr_hist[hr_start];
    int hr_rise = (int)g_hr - (int)old_hr;
    if (hr_rise > MS_HR_RISE_THRESHOLD) risk += 15;
    else if (hr_rise > 8) risk += 7;

    /* Ambient temp trigger */
    if (g_ambient_temp_cd > MS_AMBIENT_TEMP_HIGH * 10) risk += 15;

    /* Stress level */
    if (g_stress_level >= 3) risk += 15;
    else if (g_stress_level >= 2) risk += 8;

    return risk > 100 ? 100 : risk;
}

/* === Edge Inference: Night Sweat Screening === */
static uint8_t edge_night_sweat(uint8_t sweat_pct, int16_t bed_temp,
                                 uint8_t motion, uint8_t hr)
{
    uint8_t risk = 0;

    if (sweat_pct > MS_SWEAT_MOISTURE_THRESHOLD) risk += 40;
    else if (sweat_pct > 20) risk += 20;

    if (bed_temp > MS_BED_TEMP_HIGH_CD) risk += 25;
    else if (bed_temp > 3600) risk += 10;

    if (motion >= 2) risk += 20;
    else if (motion == 1) risk += 8;

    if (hr > 80) risk += 15;
    else if (hr > 72) risk += 7;

    return risk > 100 ? 100 : risk;
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "MenoSync Hub starting — menopause management system");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    init_i2c();

    gpio_set_direction(HUB_GPIO_LED_STATUS, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_BUTTON, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_HAPTIC_EN, GPIO_MODE_OUTPUT);

    /* Create queues */
    g_vitals_queue  = xQueueCreate(32, sizeof(ms_vitals_t));
    g_eda_queue     = xQueueCreate(64, sizeof(ms_eda_t));
    g_bcg_queue     = xQueueCreate(16, sizeof(ms_bcg_t));
    g_sweat_queue   = xQueueCreate(16, sizeof(ms_sweat_t));
    g_ambient_queue = xQueueCreate(16, sizeof(ms_ambient_t));
    g_alert_queue   = xQueueCreate(8, sizeof(ms_risk_t) + 2);
    g_radio_mutex   = xSemaphoreCreateMutex();

    /* Initialize mesh context */
    ms_mesh_init(&g_mesh, 0x01);

    /* Start monitoring */
    g_monitoring_active = 1;

    /* Create tasks */
    xTaskCreate(ble_coordinator_task, "ble_coord", 4096, NULL, 5, NULL);
    xTaskCreate(vitals_processor_task, "vitals_proc", 4096, NULL, 6, NULL);
    xTaskCreate(eda_processor_task, "eda_proc", 4096, NULL, 6, NULL);
    xTaskCreate(bcg_processor_task, "bcg_proc", 4096, NULL, 5, NULL);
    xTaskCreate(sweat_processor_task, "sweat_proc", 4096, NULL, 5, NULL);
    xTaskCreate(ambient_processor_task, "ambient_proc", 4096, NULL, 5, NULL);
    xTaskCreate(hotflash_screening_task, "hf_screen", 4096, NULL, 7, NULL);
    xTaskCreate(cooling_controller_task, "cool_ctrl", 4096, NULL, 6, NULL);
    xTaskCreate(subghz_coordinator_task, "subghz_coord", 4096, NULL, 5, NULL);
    xTaskCreate(voice_capture_task, "voice_cap", 6144, NULL, 4, NULL);
    xTaskCreate(cloud_bridge_task, "cloud_bridge", 4096, NULL, 4, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 3, NULL);
    xTaskCreate(audio_feedback_task, "audio_fb", 4096, NULL, 3, NULL);
    xTaskCreate(ota_manager_task, "ota_mgr", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "All tasks started — monitoring active for %s (age %d, stage: %s)",
             g_patient_id, g_patient_age, g_menopause_stage);
}