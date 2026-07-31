/*
 * BloomSync — Bloom Hub / Gateway Firmware
 * ESP32-S3, FreeRTOS, ESP-IDF v5.x
 *
 * The Hub is the central coordinator of the BloomSync postpartum
 * maternal health system. It:
 *   1. Manages BLE 5.0 connections to Recovery Band, Nursing Sensor, Wound Patch
 *   2. Runs local edge inference (TFLite-Micro):
 *      - HemorrhageRisk screening from vitals trends
 *      - WoundInfect screening from wound sensor data
 *      - MastitisDetect screening from bilateral breast temp
 *   3. Captures voice samples via I²S microphone for PPD prosody extraction
 *   4. Drives 3.5" TFT display (vitals, recovery timeline, nursing log)
 *   5. Provides audio guidance (reminders, coaching, alerts)
 *   6. Bridges all data to cloud via Wi-Fi/MQTT
 *   7. Manages OTA firmware distribution to all nodes
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

static const char *TAG = "BloomSync-Hub";

/* === Global state === */
static bs_mesh_ctx_t g_mesh;
static QueueHandle_t g_vitals_queue;     /* Vitals from Recovery Band */
static QueueHandle_t g_nursing_queue;    /* Nursing data from Nursing Sensor */
static QueueHandle_t g_wound_queue;      /* Wound data from Wound Patch */
static QueueHandle_t g_alert_queue;      /* Alert commands */
static SemaphoreHandle_t g_radio_mutex;

/* Postpartum recovery state */
static uint8_t g_monitoring_active = 0;
static uint32_t g_recovery_start_time = 0;
static uint8_t g_recovery_day = 1;       /* Day 1-42 of postpartum period */

/* Patient profile */
static char g_patient_id[16] = "patient_001";
static char g_delivery_type[16] = "cesarean";  /* "vaginal" or "cesarean" */
static uint8_t g_patient_age = 32;

/* Latest vitals (from Recovery Band) */
static uint8_t  g_hr = 75;
static uint8_t  g_spo2 = 98;
static int16_t  g_skin_temp_cd = 3680;   /* 36.80°C */
static uint16_t g_hrv_rmssd = 45;
static uint8_t  g_activity_class = 0;
static uint8_t  g_band_battery = 100;

/* Vitals history for hemorrhage risk (30-min window @ 1 Hz = 180 samples) */
#define VITALS_HISTORY_LEN 180
static uint8_t g_hr_history[VITALS_HISTORY_LEN];
static uint8_t g_spo2_history[VITALS_HISTORY_LEN];
static int16_t g_temp_history[VITALS_HISTORY_LEN];
static int g_vitals_history_idx = 0;
static int g_vitals_history_count = 0;

/* Nursing state (from Nursing Sensor) */
static int16_t g_temp_left_cd = 3650;
static int16_t g_temp_right_cd = 3650;
static int16_t g_breast_asym_cd = 0;
static uint8_t g_nursing_active = 0;
static uint8_t g_nursing_side = 0;       /* 0=idle, 1=left, 2=right */
static uint32_t g_nursing_session_start = 0;
static uint16_t g_nursing_session_count = 0;
static uint8_t g_nursing_battery = 100;

/* Wound state (from Wound Patch) */
static int16_t g_wound_temp_cd = 3680;
static uint8_t g_wound_moisture_pct = 30;
static uint8_t g_wound_ph = 68;         /* pH 6.8 (normal) */
static uint8_t g_wound_infection_risk = 0;
static uint8_t g_wound_battery = 100;

/* Wound history for infection screening (12h window @ 0.1Hz = 4320 samples) */
#define WOUND_HISTORY_LEN 720  /* 12h at 0.1 Hz (every 10s → 6/min → 720 in 2h) */
static int16_t g_wound_temp_hist[WOUND_HISTORY_LEN];
static uint8_t g_wound_moist_hist[WOUND_HISTORY_LEN];
static uint8_t g_wound_ph_hist[WOUND_HISTORY_LEN];
static int g_wound_hist_idx = 0;
static int g_wound_hist_count = 0;

/* Risk scores (from edge inference) */
static uint8_t g_hemorrhage_risk = 0;
static uint8_t g_preeclampsia_risk = 0;
static uint8_t g_wound_risk = 0;
static uint8_t g_mastitis_risk = 0;
static uint8_t g_ppd_risk = 0;
static uint8_t g_overall_risk = 0;
static uint8_t g_alert_level = 0;       /* 0=normal,1=watch,2=warning,3=critical */

/* Connected sensors */
static uint8_t g_band_connected = 0;
static uint8_t g_nursing_connected = 0;
static uint8_t g_wound_connected = 0;

/* === Forward declarations === */
static void ble_coordinator_task(void *arg);
static void vitals_processor_task(void *arg);
static void nursing_processor_task(void *arg);
static void wound_processor_task(void *arg);
static void hemorrhage_screening_task(void *arg);
static void voice_capture_task(void *arg);
static void cloud_bridge_task(void *arg);
static void display_task(void *arg);
static void audio_feedback_task(void *arg);
static void ota_manager_task(void *arg);

/* Edge inference stubs (TFLite-Micro in production) */
static uint8_t edge_hemorrhage_risk(const uint8_t *hr_hist, const uint8_t *spo2_hist,
                                     const int16_t *temp_hist, int count);
static uint8_t edge_wound_infection(const int16_t *temp_hist, const uint8_t *moist_hist,
                                     const uint8_t *ph_hist, int count);
static uint8_t edge_mastitis_detect(int16_t temp_left, int16_t temp_right,
                                     int16_t asym);

/* === I²C init (ESP32-S3) === */
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

/* === BME280 read (ambient temp/humidity) === */
static float read_ambient_temp(void)
{
    /* In production: read BME280 via I²C at 0x76 */
    return 22.5f;  /* placeholder */
}

/* === Display update (ILI9488) === */
static void update_display(void)
{
    /* In production: SPI to ILI9488, render:
     * - Top: HR, SpO₂, skin temp, HRV (large font)
     * - Middle: Recovery day X/42, risk meters (colored bars)
     * - Middle: Nursing log (last session, total count, next reminder)
     * - Bottom: Wound status, alerts, sensor connection status
     */
    ESP_LOGI(TAG, "Display: HR=%d bpm  SpO2=%d%%  Temp=%.1f°C  HRV=%dms  "
             "Risk: Hem=%d%%  PPH=%d%%  Wound=%d%%  Mast=%d%%  PPD=%d%%  "
             "Level=%d  Day=%d/42",
             g_hr, g_spo2, g_skin_temp_cd / 100.0f, g_hrv_rmssd,
             g_hemorrhage_risk, g_preeclampsia_risk, g_wound_risk,
             g_mastitis_risk, g_ppd_risk, g_alert_level, g_recovery_day);
}

/* === Audio feedback === */
static void play_audio_message(const char *msg_id)
{
    /* In production: I²S MAX98357A, play pre-encoded PCM messages:
     * "time_to_nurse"  → "It's time to nurse your baby."
     * "med_reminder"   → "Remember to take your iron supplement."
     * "vitals_normal"  → "Your vital signs look good today."
     * "risk_elevated"  → "Please rest and monitor your symptoms. "
     *                     "Your healthcare provider has been notified."
     * "emergency"      → "Emergency alert sent. Please seek help immediately."
     */
    ESP_LOGI(TAG, "Audio: %s", msg_id);
}

/* === Haptic alert === */
static void trigger_haptic(uint8_t pattern)
{
    /* DRV2605L patterns:
     * 0 = single tap (info)
     * 1 = double pulse (warning)
     * 2 = triple burst (urgent)
     * 3 = continuous (critical)
     */
    ESP_LOGI(TAG, "Haptic: pattern %d", pattern);
}

/* === BLE Coordinator Task === */
/* Manages BLE 5.0 connections to all wearable nodes.
 * In production: use NimBLE or Bluedroid stack.
 * - Advertise as central/coordinator
 * - Accept connections from Recovery Band, Nursing Sensor, Wound Patch
 * - Parse incoming GATT notifications into protocol messages
 * - Forward to appropriate processing queue
 */
static void ble_coordinator_task(void *arg)
{
    ESP_LOGI(TAG, "BLE coordinator started");
    uint8_t seq = 0;

    while (1) {
        /* Periodic heartbeat to all connected nodes */
        if (g_band_connected || g_nursing_connected || g_wound_connected) {
            uint8_t msg[BS_MAX_MSG];
            uint8_t payload[2] = {0x01, g_monitoring_active};
            size_t len = bs_encode(msg, sizeof(msg),
                                   0x01, BS_BROADCAST,
                                   BS_MSG_COMMAND, BS_CMD_START_MONITORING,
                                   seq++, payload, 2);
            if (len > 0) {
                /* In production: send via BLE GATT write to each connected node */
            }
        }
        vTaskDelay(pdMS_TO_TICKS(BS_HEARTBEAT_INTERVAL_S * 1000));
    }
}

/* === Vitals Processor Task === */
/* Receives vitals data from Recovery Band via BLE, stores in history,
 * updates display, checks simple threshold alerts.
 */
static void vitals_processor_task(void *arg)
{
    bs_vitals_t vitals;

    while (1) {
        if (xQueueReceive(g_vitals_queue, &vitals, portMAX_DELAY) == pdTRUE) {
            /* Update latest vitals */
            g_hr = vitals.heart_rate;
            g_spo2 = vitals.spo2;
            g_skin_temp_cd = vitals.skin_temp_cd;
            g_hrv_rmssd = vitals.hrv_rmssd;
            g_activity_class = vitals.activity_class;
            g_band_battery = vitals.battery_pct;

            /* Store in history buffer */
            g_hr_history[g_vitals_history_idx] = vitals.heart_rate;
            g_spo2_history[g_vitals_history_idx] = vitals.spo2;
            g_temp_history[g_vitals_history_idx] = vitals.skin_temp_cd;
            g_vitals_history_idx = (g_vitals_history_idx + 1) % VITALS_HISTORY_LEN;
            if (g_vitals_history_count < VITALS_HISTORY_LEN)
                g_vitals_history_count++;

            /* Threshold alerts */
            if (vitals.heart_rate > BS_HR_HIGH_THRESHOLD) {
                bs_risk_t risk = {
                    .hemorrhage_risk = g_hemorrhage_risk,
                    .preeclampsia_risk = g_preeclampsia_risk,
                    .wound_risk = g_wound_risk,
                    .mastitis_risk = g_mastitis_risk,
                    .ppd_risk = g_ppd_risk,
                    .recovery_progress = 0,
                    .overall_risk = g_overall_risk,
                    .alert_level = g_alert_level,
                };
                uint8_t alert_payload[sizeof(bs_risk_t) + 2];
                alert_payload[0] = BS_ALERT_VITAL_ABNORMAL;
                alert_payload[1] = BS_SEV_HIGH;
                memcpy(&alert_payload[2], &risk, sizeof(bs_risk_t));
                xQueueSend(g_alert_queue, alert_payload, 0);
                ESP_LOGW(TAG, "ALERT: Tachycardia HR=%d", vitals.heart_rate);
            }
            if (vitals.spo2 < BS_SPO2_LOW_THRESHOLD) {
                ESP_LOGW(TAG, "ALERT: Hypoxemia SpO2=%d%%", vitals.spo2);
                trigger_haptic(2);
                play_audio_message("risk_elevated");
            }
            if (vitals.skin_temp_cd > BS_TEMP_HIGH_THRESHOLD) {
                ESP_LOGW(TAG, "ALERT: Fever temp=%.1f°C",
                         vitals.skin_temp_cd / 100.0f);
            }
        }
    }
}

/* === Nursing Processor Task === */
static void nursing_processor_task(void *arg)
{
    bs_nursing_t nursing;

    while (1) {
        if (xQueueReceive(g_nursing_queue, &nursing, portMAX_DELAY) == pdTRUE) {
            g_temp_left_cd = nursing.temp_left_cd;
            g_temp_right_cd = nursing.temp_right_cd;
            g_breast_asym_cd = nursing.temp_asym_cd;
            g_nursing_battery = nursing.battery_pct;

            /* Detect nursing session start/stop */
            if (nursing.nursing_active > 0 && g_nursing_active == 0) {
                g_nursing_active = 1;
                g_nursing_side = nursing.nursing_active;
                g_nursing_session_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
                ESP_LOGI(TAG, "Nursing session started (side=%s)",
                         g_nursing_side == 1 ? "left" : "right");
            } else if (nursing.nursing_active == 0 && g_nursing_active == 1) {
                g_nursing_active = 0;
                uint32_t duration = (xTaskGetTickCount() * portTICK_PERIOD_MS) -
                                     g_nursing_session_start;
                g_nursing_session_count++;
                ESP_LOGI(TAG, "Nursing session ended (duration=%lu min, total=%d)",
                         duration / 60000, g_nursing_session_count);
            }

            /* Mastitis screening (edge inference) */
            g_mastitis_risk = edge_mastitis_detect(nursing.temp_left_cd,
                                                    nursing.temp_right_cd,
                                                    nursing.temp_asym_cd);
            if (g_mastitis_risk > 70) {
                ESP_LOGW(TAG, "ALERT: Mastitis risk %d%% (asym=%.1f°C)",
                         g_mastitis_risk, g_breast_asym_cd / 10.0f);
                trigger_haptic(1);
            }
        }
    }
}

/* === Wound Processor Task === */
static void wound_processor_task(void *arg)
{
    bs_wound_t wound;

    while (1) {
        if (xQueueReceive(g_wound_queue, &wound, portMAX_DELAY) == pdTRUE) {
            g_wound_temp_cd = wound.wound_temp_cd;
            g_wound_moisture_pct = wound.moisture_pct;
            g_wound_ph = wound.ph_value;
            g_wound_infection_risk = wound.infection_risk;
            g_wound_battery = wound.battery_pct;

            /* Store in wound history */
            g_wound_temp_hist[g_wound_hist_idx] = wound.wound_temp_cd;
            g_wound_moist_hist[g_wound_hist_idx] = wound.moisture_pct;
            g_wound_ph_hist[g_wound_hist_idx] = wound.ph_value;
            g_wound_hist_idx = (g_wound_hist_idx + 1) % WOUND_HISTORY_LEN;
            if (g_wound_hist_count < WOUND_HISTORY_LEN)
                g_wound_hist_count++;

            /* Wound infection screening (edge inference) */
            if (g_wound_hist_count >= 60) {  /* Need at least 10 min of data */
                g_wound_risk = edge_wound_infection(g_wound_temp_hist,
                                                     g_wound_moist_hist,
                                                     g_wound_ph_hist,
                                                     g_wound_hist_count);
                if (g_wound_risk > 65) {
                    ESP_LOGW(TAG, "ALERT: Wound infection risk %d%% (temp=%.1f°C pH=%.1f)",
                             g_wound_risk, g_wound_temp_cd / 100.0f,
                             g_wound_ph / 10.0f);
                    trigger_haptic(1);
                }
            }
        }
    }
}

/* === Hemorrhage Risk Screening Task === */
/* Runs every 30 seconds, analyzes 30-minute vitals window for hemorrhage
 * risk indicators: rising HR, falling SpO₂, dropping HRV, temp instability.
 */
static void hemorrhage_screening_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));  /* Every 30s */

        if (g_vitals_history_count >= 30) {  /* Need at least 30 samples */
            g_hemorrhage_risk = edge_hemorrhage_risk(g_hr_history,
                                                      g_spo2_history,
                                                      g_temp_history,
                                                      g_vitals_history_count);

            /* Simple preeclampsia screening: sustained tachycardia + low SpO₂ */
            if (g_vitals_history_count >= 60) {
                int high_hr_count = 0;
                int low_spo2_count = 0;
                int recent = g_vitals_history_count < 60 ? g_vitals_history_count : 60;
                int start = (g_vitals_history_idx - recent + VITALS_HISTORY_LEN) %
                            VITALS_HISTORY_LEN;
                for (int i = 0; i < recent; i++) {
                    int idx = (start + i) % VITALS_HISTORY_LEN;
                    if (g_hr_history[idx] > 100) high_hr_count++;
                    if (g_spo2_history[idx] < 95) low_spo2_count++;
                }
                if (high_hr_count > recent * 0.5 && low_spo2_count > recent * 0.3) {
                    g_preeclampsia_risk = 65 + (high_hr_count * 35 / recent);
                } else {
                    g_preeclampsia_risk = g_preeclampsia_risk > 5 ?
                                           g_preeclampsia_risk - 5 : 0;
                }
            }

            /* Determine overall alert level */
            uint8_t max_risk = g_hemorrhage_risk;
            if (g_preeclampsia_risk > max_risk) max_risk = g_preeclampsia_risk;
            if (g_wound_risk > max_risk) max_risk = g_wound_risk;
            if (g_mastitis_risk > max_risk) max_risk = g_mastitis_risk;
            g_overall_risk = max_risk;

            if (max_risk >= 80) {
                g_alert_level = 3;  /* critical */
                trigger_haptic(3);
                play_audio_message("emergency");
            } else if (max_risk >= 60) {
                g_alert_level = 2;  /* warning */
                trigger_haptic(2);
            } else if (max_risk >= 30) {
                g_alert_level = 1;  /* watch */
            } else {
                g_alert_level = 0;  /* normal */
            }

            update_display();
        }
    }
}

/* === Voice Capture Task === */
/* Captures 30-second voice samples via I²S microphone (ICS-43434)
 * for PPD screening. Extracts prosody features on-device (no transcription),
 * sends prosody vector to cloud for PPDetect inference.
 * Runs 3×/day: morning, afternoon, evening.
 */
static void voice_capture_task(void *arg)
{
    uint32_t capture_count = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(8 * 60 * 60 * 1000));  /* Every 8h (3×/day) */

        if (!g_monitoring_active)
            continue;

        ESP_LOGI(TAG, "Voice capture #%lu starting (30s)", capture_count++);

        /* In production:
         * 1. Play audio prompt: "Please say a few words about how you're feeling today."
         * 2. Record 30s of I²S audio at 16 kHz (ICS-43434)
         * 3. Extract prosody features on-device (ESP32-S3 DSP):
         *    - F0 (pitch) via autocorrelation
         *    - Jitter (period perturbation)
         *    - Shimmer (amplitude perturbation)
         *    - HNR (harmonics-to-noise ratio)
         *    - Speech rate (syllable detection)
         *    - Pause ratio
         *    - MFCC coefficients (4)
         *    - Spectral features (centroid, spread, slope, flux)
         * 4. Pack into bs_prosody_t (128 bytes)
         * 5. Send to cloud via MQTT for PPDetect CNN inference
         * 6. NO raw audio stored or transmitted — only prosody features
         */
        play_audio_message("voice_prompt");

        /* Simulated prosody extraction */
        bs_prosody_t prosody = {
            .f0_mean = 210.0f + (capture_count % 10) * 2.5f,
            .f0_std = 25.0f + (capture_count % 5) * 1.5f,
            .f0_range = 80.0f,
            .jitter_local = 1.2f + (capture_count % 3) * 0.1f,
            .jitter_ppq5 = 0.8f,
            .shimmer_local = 8.5f,
            .shimmer_apq11 = 6.2f,
            .hnr_db = 18.0f + (capture_count % 4) * 0.5f,
            .speech_rate = 3.2f + (capture_count % 7) * 0.1f,
            .pause_ratio = 0.35f + (capture_count % 5) * 0.02f,
            .mean_intensity_db = 65.0f,
            .intensity_var = 8.0f,
            .spectral_slope = -0.045f,
            .spectral_flux = 0.12f,
            .mfcc_1 = -2.1f,
            .mfcc_2 = 1.8f,
            .mfcc_3 = -0.5f,
            .mfcc_4 = 0.3f,
            .breathiness = 0.15f,
            .roughness = 0.08f,
            .pitch_declination = -0.02f,
            .voiced_ratio = 0.72f,
            .energy_mean = 0.045f,
            .energy_std = 0.012f,
            .dur_phoneme_mean = 0.18f,
            .dur_pause_mean = 0.35f,
            .dur_pause_std = 0.15f,
            .f0_cv = 0.12f,
            .intensity_cv = 0.18f,
            .spectral_centroid = 1850.0f,
            .spectral_spread = 1200.0f,
            .prosody_score = 0.15f + (capture_count % 4) * 0.05f,
        };

        /* Forward to cloud bridge for PPDetect inference */
        /* In production: xQueueSend to cloud_bridge with BS_MSG_VOICE_PROSODY */

        ESP_LOGI(TAG, "Voice capture complete — prosody extracted, "
                 "F0=%.1f Hz, jitter=%.2f%%, HNR=%.1f dB, speech_rate=%.1f syll/s",
                 prosody.f0_mean, prosody.jitter_local, prosody.hnr_db,
                 prosody.speech_rate);
    }
}

/* === Cloud Bridge Task === */
/* Sends telemetry to cloud via MQTT, receives commands and OTA updates.
 * Buffers to microSD during Wi-Fi outage.
 */
static void cloud_bridge_task(void *arg)
{
    uint32_t report_seq = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  /* Every 10s */

        if (!g_monitoring_active)
            continue;

        /* Build risk assessment */
        bs_risk_t risk = {
            .hemorrhage_risk = g_hemorrhage_risk,
            .preeclampsia_risk = g_preeclampsia_risk,
            .wound_risk = g_wound_risk,
            .mastitis_risk = g_mastitis_risk,
            .ppd_risk = g_ppd_risk,
            .recovery_progress = (g_recovery_day * 100) / 42,
            .overall_risk = g_overall_risk,
            .alert_level = g_alert_level,
        };

        /* In production:
         * 1. Build JSON payload with all current vitals, nursing, wound, risk
         * 2. Publish to MQTT topic: bloom-sync/telemetry/{patient_id}
         * 3. If Wi-Fi unavailable, buffer to microSD
         * 4. Check for OTA update availability
         */
        report_seq++;
        ESP_LOGI(TAG, "Cloud report #%lu: risk={hem=%d%%, pph=%d%%, wound=%d%%, "
                 "mast=%d%%, ppd=%d%%} level=%d",
                 report_seq, risk.hemorrhage_risk, risk.preeclampsia_risk,
                 risk.wound_risk, risk.mastitis_risk, risk.ppd_risk,
                 risk.alert_level);

        /* Emergency alert dispatch */
        if (g_alert_level >= 3) {
            ESP_LOGW(TAG, "CRITICAL: Dispatching emergency alert to healthcare provider");
            /* In production: publish to bloom-sync/alerts/{patient_id}
             * with QoS 1, trigger SMS/email to OB-GYN via cloud service */
        }
    }
}

/* === Display Task === */
static void display_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));  /* 2 Hz refresh */
        if (g_monitoring_active) {
            update_display();
        }
    }
}

/* === Audio Feedback Task === */
static void audio_feedback_task(void *arg)
{
    uint32_t last_nursing_reminder = 0;
    uint32_t last_med_reminder = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));  /* Check every minute */
        if (!g_monitoring_active)
            continue;

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        /* Nursing reminder (every 3 hours during breastfeeding period) */
        if (g_nursing_connected && !g_nursing_active &&
            (now - last_nursing_reminder) > 3 * 60 * 60 * 1000) {
            play_audio_message("time_to_nurse");
            last_nursing_reminder = now;
        }

        /* Medication reminder (every 8 hours) */
        if ((now - last_med_reminder) > 8 * 60 * 60 * 1000) {
            play_audio_message("med_reminder");
            last_med_reminder = now;
        }
    }
}

/* === OTA Manager Task === */
static void ota_manager_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3600 * 1000));  /* Check every hour */
        /* In production:
         * 1. Check cloud for firmware updates for each node type
         * 2. Download firmware image
         * 3. Distribute via BLE to nodes (BS_MSG_OTA_BLOCK)
         * 4. Verify checksum, trigger reboot
         */
    }
}

/* === Edge Inference: Hemorrhage Risk Screening === */
/* Lightweight on-device screening using vital trends.
 * Full LSTM model runs in cloud; this is a fast pre-screen.
 * Indicators:
 * - HR rising > 15 bpm in 30 min → +30
 * - HR > 110 bpm sustained → +25
 * - SpO₂ < 92% sustained → +20
 * - HRV drop > 50% in 1h → +20
 * - Skin temp instability (> 0.5°C swing) → +10
 */
static uint8_t edge_hemorrhage_risk(const uint8_t *hr_hist, const uint8_t *spo2_hist,
                                     const int16_t *temp_hist, int count)
{
    if (count < 30) return 0;
    uint8_t risk = 0;

    /* HR rising rate (last 30 min) */
    int recent = count < 30 ? count : 30;
    int start_idx = (g_vitals_history_idx - recent + VITALS_HISTORY_LEN) %
                    VITALS_HISTORY_LEN;
    uint8_t hr_start = hr_hist[start_idx];
    uint8_t hr_end = hr_hist[(g_vitals_history_idx - 1 + VITALS_HISTORY_LEN) %
                              VITALS_HISTORY_LEN];
    int hr_rise = (int)hr_end - (int)hr_start;
    if (hr_rise > 15) risk += 30;
    else if (hr_rise > 8) risk += 15;

    /* Sustained high HR */
    int high_hr = 0;
    for (int i = 0; i < recent; i++) {
        int idx = (start_idx + i) % VITALS_HISTORY_LEN;
        if (hr_hist[idx] > BS_HR_HIGH_THRESHOLD) high_hr++;
    }
    if (high_hr > recent * 0.5) risk += 25;
    else if (high_hr > recent * 0.3) risk += 12;

    /* Sustained low SpO₂ */
    int low_spo2 = 0;
    for (int i = 0; i < recent; i++) {
        int idx = (start_idx + i) % VITALS_HISTORY_LEN;
        if (spo2_hist[idx] < BS_SPO2_LOW_THRESHOLD) low_spo2++;
    }
    if (low_spo2 > recent * 0.4) risk += 20;
    else if (low_spo2 > recent * 0.2) risk += 10;

    /* Skin temp instability */
    int16_t temp_min = 9999, temp_max = -9999;
    for (int i = 0; i < recent; i++) {
        int idx = (start_idx + i) % VITALS_HISTORY_LEN;
        if (temp_hist[idx] < temp_min) temp_min = temp_hist[idx];
        if (temp_hist[idx] > temp_max) temp_max = temp_hist[idx];
    }
    if ((temp_max - temp_min) > 50) risk += 10;  /* > 0.5°C swing */

    return risk > 100 ? 100 : risk;
}

/* === Edge Inference: Wound Infection Screening === */
/* Checks wound temp trend, moisture, and pH for infection indicators.
 * Indicators:
 * - Wound temp > 37.9°C → +30
 * - Wound temp rising > 0.8°C in 12h → +25
 * - pH > 7.5 → +25
 * - Moisture > 80% → +20
 */
static uint8_t edge_wound_infection(const int16_t *temp_hist, const uint8_t *moist_hist,
                                     const uint8_t *ph_hist, int count)
{
    if (count < 10) return 0;
    uint8_t risk = 0;

    /* Current wound temp */
    int16_t current_temp = temp_hist[(g_wound_hist_idx - 1 + WOUND_HISTORY_LEN) %
                                      WOUND_HISTORY_LEN];
    if (current_temp > BS_WOUND_TEMP_HIGH_THRESHOLD) risk += 30;
    else if (current_temp > 3750) risk += 15;  /* > 37.5°C */

    /* Temp rising trend (compare first vs last in window) */
    int lookback = count < 60 ? count : 60;  /* Up to 10 min */
    int start = (g_wound_hist_idx - lookback + WOUND_HISTORY_LEN) % WOUND_HISTORY_LEN;
    int16_t old_temp = temp_hist[start];
    int16_t new_temp = current_temp;
    int temp_rise = new_temp - old_temp;
    if (temp_rise > BS_WOUND_TEMP_RISE_THRESHOLD) risk += 25;
    else if (temp_rise > 4) risk += 10;

    /* pH elevated */
    uint8_t current_ph = ph_hist[(g_wound_hist_idx - 1 + WOUND_HISTORY_LEN) %
                                  WOUND_HISTORY_LEN];
    if (current_ph > BS_WOUND_PH_HIGH_THRESHOLD) risk += 25;
    else if (current_ph > 72) risk += 10;  /* pH > 7.2 */

    /* Moisture elevated */
    uint8_t current_moist = moist_hist[(g_wound_hist_idx - 1 + WOUND_HISTORY_LEN) %
                                        WOUND_HISTORY_LEN];
    if (current_moist > BS_WOUND_MOISTURE_HIGH_THRESHOLD) risk += 20;
    else if (current_moist > 60) risk += 8;

    return risk > 100 ? 100 : risk;
}

/* === Edge Inference: Mastitis Detection === */
/* Checks bilateral breast temperature asymmetry.
 * Clinical threshold: > 1.3°C asymmetry indicates mastitis.
 */
static uint8_t edge_mastitis_detect(int16_t temp_left, int16_t temp_right,
                                     int16_t asym)
{
    uint8_t risk = 0;

    /* Asymmetry threshold (asym is in centi-degrees × 10 = millidegrees) */
    /* temp_asym_cd is |left-right| in centi-degrees, threshold is 13 (1.3°C) */
    if (asym > BS_BREAST_TEMP_ASYM_THRESHOLD * 10) {
        risk = 70 + (asym - BS_BREAST_TEMP_ASYM_THRESHOLD * 10) * 3;
    } else if (asym > 100) {  /* > 1.0°C */
        risk = 40 + (asym - 100) * 10;
    } else if (asym > 50) {   /* > 0.5°C */
        risk = 15 + (asym - 50) * 5;
    } else {
        risk = asym / 4;  /* low baseline */
    }

    /* Absolute temp elevation */
    int16_t max_temp = temp_left > temp_right ? temp_left : temp_right;
    if (max_temp > BS_BREAST_TEMP_HIGH_THRESHOLD) {
        risk += 15;
    }

    return risk > 100 ? 100 : risk;
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "BloomSync Hub starting — postpartum maternal health monitor");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize I²C */
    init_i2c();

    /* Initialize GPIO for status LED, button, haptic */
    gpio_set_direction(HUB_GPIO_LED_STATUS, GPIO_MODE_OUTPUT);
    gpio_set_direction(HUB_GPIO_BUTTON, GPIO_MODE_INPUT);
    gpio_set_direction(HUB_GPIO_HAPTIC_EN, GPIO_MODE_OUTPUT);

    /* Create queues */
    g_vitals_queue  = xQueueCreate(32, sizeof(bs_vitals_t));
    g_nursing_queue = xQueueCreate(16, sizeof(bs_nursing_t));
    g_wound_queue   = xQueueCreate(16, sizeof(bs_wound_t));
    g_alert_queue   = xQueueCreate(8, sizeof(bs_risk_t) + 2);
    g_radio_mutex   = xSemaphoreCreateMutex();

    /* Initialize mesh context */
    bs_mesh_init(&g_mesh, 0x01);

    /* Start monitoring */
    g_monitoring_active = 1;
    g_recovery_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* Create tasks */
    xTaskCreate(ble_coordinator_task, "ble_coord", 4096, NULL, 5, NULL);
    xTaskCreate(vitals_processor_task, "vitals_proc", 4096, NULL, 6, NULL);
    xTaskCreate(nursing_processor_task, "nursing_proc", 4096, NULL, 5, NULL);
    xTaskCreate(wound_processor_task, "wound_proc", 4096, NULL, 5, NULL);
    xTaskCreate(hemorrhage_screening_task, "hem_screen", 4096, NULL, 7, NULL);
    xTaskCreate(voice_capture_task, "voice_cap", 6144, NULL, 4, NULL);
    xTaskCreate(cloud_bridge_task, "cloud_bridge", 4096, NULL, 4, NULL);
    xTaskCreate(display_task, "display", 4096, NULL, 3, NULL);
    xTaskCreate(audio_feedback_task, "audio_fb", 4096, NULL, 3, NULL);
    xTaskCreate(ota_manager_task, "ota_mgr", 4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "All tasks started — monitoring active for %s (delivery: %s, day %d/42)",
             g_patient_id, g_delivery_type, g_recovery_day);
}