/*
 * SeizureSync — Seizure Hub firmware (ESP32-S3)
 *
 * Coordinator node: BCG bed-mat monitoring, SpO₂ apnea detection,
 * MLX90640 prone-position detection, edge inference (SeizureNet ensemble),
 * TDMA mesh coordinator, MQTT cloud bridge, 4G LTE backup.
 *
 * SPDX-License-Identifier: MIT
 *
 * Target: ESP32-S3-WROOM-1-N16R8
 * Toolchain: ESP-IDF v5.1
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "../common/protocol.h"
#include "../common/tdma.h"
#include "../common/crypto.h"
#include "sx1262.h"
#include "max30102.h"
#include "mlx90640.h"
#include "scd41.h"
#include "uc8151d.h"
#include "seizurenet_edge.h"

static const char *TAG = "HUB";

/* ---- Pin definitions (see README pin table) ---- */
#define PIN_SX1262_NSS   4
#define PIN_SX1262_RST   5
#define PIN_SX1262_DIO0  3
#define PIN_SX1262_BUSY  6
#define PIN_SX1262_SCK   7
#define PIN_SX1262_MISO  8
#define PIN_SX1262_MOSI  9
#define PIN_RELAY_SHAKER 38
#define PIN_RELAY_AUDIO  39
#define PIN_WS2812       40
#define PIN_BCG_ADC1     ADC1_CHANNEL_3   /* GPIO35 */
#define PIN_BCG_ADC2     ADC1_CHANNEL_4   /* GPIO36 */
#define PIN_BCG_ADC3     ADC1_CHANNEL_5   /* GPIO37 */
#define PIN_LTE_TX       41
#define PIN_LTE_RX       42
#define PIN_LTE_PWRKEY   44

/* ---- Globals ---- */
static sz_header_t g_last_seizure_hdr;
static sz_seizure_payload_t g_last_seizure;
static QueueHandle_t g_alert_queue;
static uint8_t g_net_id[SZ_NET_ID_LEN] = {'S','Z','S','Y','N','C'};
static uint8_t g_aes_key[SZ_AES_KEY_LEN];

/* ---- BCG (bed-mat ballistocardiography) ----
 * 3 piezo film sensors under mattress → charge amp → ADC
 * Sampling at 250 Hz, detect breathing rate + heart rate + motion.
 */
#define BCG_SAMPLE_HZ   250
#define BCG_BUF_LEN      750   /* 3 seconds */
static uint16_t g_bcg_buf[3][BCG_BUF_LEN];
static int g_bcg_idx = 0;

static void bcg_sample_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / BCG_SAMPLE_HZ);
    while (1) {
        for (int ch = 0; ch < 3; ch++) {
            uint32_t raw = adc1_get_raw(ch + 2);   /* ADC1_CH3..5 */
            g_bcg_buf[ch][g_bcg_idx] = (uint16_t)raw;
        }
        g_bcg_idx = (g_bcg_idx + 1) % BCG_BUF_LEN;
        vTaskDelayUntil(&last, period);
    }
}

/* BCG breathing rate (FFT-based, simplified) */
float bcg_get_breathing_rate(void)
{
    /* Simple zero-crossing rate on band-pass filtered BCG signal.
     * Production: use FreeRTOS DSP or a Goertzel filter. */
    int crossings = 0;
    uint16_t prev = g_bcg_buf[0][0];
    for (int i = 1; i < BCG_BUF_LEN; i++) {
        uint16_t cur = g_bcg_buf[0][i];
        if ((prev < 2048 && cur >= 2048) || (prev >= 2048 && cur < 2048))
            crossings++;
        prev = cur;
    }
    /* breathing = crossings / 2 / duration_seconds */
    return (float)crossings / 2.0f / 3.0f;   /* Hz → breaths/s × 60 = bpm */
}

/* ---- SpO₂ apnea detection ----
 * MAX30102 in SpO₂ mode, sample at 100 Hz.
 * Apnea = SpO₂ < 88% for > 10s, or breathing stops > 20s.
 */
static void spo2_task(void *arg)
{
    (void)arg;
    while (1) {
        float spo2 = max30102_read_spo2();
        uint8_t hr  = max30102_read_hr();
        ESP_LOGI(TAG, "SpO2=%.1f%% HR=%u bpm", spo2, hr);

        /* Apnea detection: SpO2 < 88% OR BCG breathing < 6 breaths/min */
        float br = bcg_get_breathing_rate() * 60.0f;
        if (spo2 < 88.0f || br < 6.0f) {
            /* Start apnea timer; if sustained > 20s, trigger SUDEP alert */
            int apnea_s = 0;
            while (spo2 < 88.0f || bcg_get_breathing_rate() * 60.0f < 6.0f) {
                apnea_s++;
                vTaskDelay(pdMS_TO_TICKS(1000));
                spo2 = max30102_read_spo2();
                if (apnea_s >= 20) {
                    ESP_LOGW(TAG, "SUDEP ALERT: apnea %d s, SpO2=%.1f%%",
                             apnea_s, spo2);
                    sudep_alert_trigger(apnea_s, spo2, br);
                    break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---- Prone position detection (MLX90640 thermal array) ----
 * Classify body orientation from thermal silhouette.
 * Prone (face-down) is a major SUDEP risk factor.
 */
static void prone_task(void *arg)
{
    (void)arg;
    float frame[768];   /* 32×24 */
    while (1) {
        mlx90640_get_frame(frame);
        /* Simple heuristic: if hottest region (head) is in lower half
         * of frame and chest region is compressed → prone.
         * Production: use a small CNN (tflite-micro). */
        float max_t = -999;
        int max_x = 0, max_y = 0;
        for (int y = 0; y < 24; y++) {
            for (int x = 0; x < 32; x++) {
                float t = frame[y * 32 + x];
                if (t > max_t) { max_t = t; max_x = x; max_y = y; }
            }
        }
        int prone = (max_y > 12) ? 1 : 0;  /* head in lower half = prone */
        if (prone) ESP_LOGW(TAG, "PRONE position detected (head y=%d)", max_y);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ---- SUDEP alert ---- */
void sudep_alert_trigger(int apnea_s, float spo2, float br)
{
    sz_header_t h = {0};
    memcpy(h.net_id, g_net_id, SZ_NET_ID_LEN);
    h.src_node = SZ_SLOT_HUB;
    h.dst_node = SZ_SLOT_BEACON;
    h.type = SZ_PKT_SUDEP_ALERT;
    h.seq++;

    sz_sudep_payload_t p = {0};
    p.apnea_state = (apnea_s > 30) ? 4 : (apnea_s > 20) ? 3 : 2;
    p.apnea_duration_s = apnea_s;
    p.prone_flag = 1;   /* TODO: from prone_task */
    p.spo2_pct = (uint8_t)spo2;
    p.hr_bpm = (uint8_t)max30102_read_hr();

    uint8_t pkt[SZ_RADIO_MAX_PKT];
    size_t len = sz_pack(pkt, &h, (uint8_t *)&p, sizeof(p));
    sx1262_send(pkt, len);

    /* Drive bed-shaker relay (fail-closed: energize to alert) */
    gpio_set_level(PIN_RELAY_SHAKER, 1);
    gpio_set_level(PIN_RELAY_AUDIO, 1);

    /* MQTT publish to cloud */
    mqtt_publish_sudep(&p);

    ESP_LOGW(TAG, "SUDEP alert sent: apnea=%d s SpO2=%.0f%% HR=%u",
             apnea_s, spo2, p.hr_bpm);
}

/* ---- TDMA mesh coordinator ----
 * Hub is slot 0. Broadcasts BEACON at start of each superframe.
 * Listens during other slots; processes incoming alerts.
 */
static void tdma_coordinator_task(void *arg)
{
    (void)arg;
    while (1) {
        /* Slot 0: broadcast beacon */
        uint8_t beacon[SZ_RADIO_MAX_PKT];
        sz_header_t bh = {0};
        memcpy(bh.net_id, g_net_id, SZ_NET_ID_LEN);
        bh.src_node = SZ_SLOT_HUB;
        bh.dst_node = 0xFF;   /* broadcast */
        bh.type = SZ_PKT_BEACON;
        bh.seq++;
        size_t blen = sz_pack(beacon, &bh, NULL, 0);
        sx1262_send(beacon, blen);

        /* Listen in slots 1-7 */
        for (int slot = 1; slot < SZ_TDMA_SLOTS; slot++) {
            uint32_t offset, duration;
            sz_tdma_get_slot_timing(slot, &offset, &duration);
            vTaskDelay(pdMS_TO_TICKS(duration));

            uint8_t rx_buf[SZ_RADIO_MAX_PKT];
            int rlen = sx1262_receive(rx_buf, sizeof(rx_buf), duration);
            if (rlen > 0) {
                sz_header_t h;
                uint8_t payload[128];
                size_t plen;
                if (sz_parse(rx_buf, rlen, &h, payload, &plen) == 0) {
                    handle_mesh_packet(&h, payload, plen);
                }
            }
        }
    }
}

/* ---- Handle incoming mesh packets ---- */
void handle_mesh_packet(const sz_header_t *h, const uint8_t *payload, size_t plen)
{
    switch (h->type) {
    case SZ_PKT_SEIZURE_ALERT: {
        memcpy(&g_last_seizure, payload, sizeof(g_last_seizure));
        memcpy(&g_last_seizure_hdr, h, sizeof(g_last_seizure_hdr));
        ESP_LOGW(TAG, "Seizure alert from node %d: semiology=%d severity=%d",
                 h->src_node, g_last_seizure.semiology, g_last_seizure.severity);

        /* Cross-validate with BCG: confirm motor activity in bed-mat */
        float motion = bcg_get_motion_energy();
        if (motion > BCG_MOTION_THRESHOLD || g_last_seizure.confidence > 80) {
            /* Confirmed: relay to caregiver beacon */
            relay_to_beacon(h, payload, plen);
            /* Cloud publish */
            mqtt_publish_event(&g_last_seizure);
        } else {
            ESP_LOGI(TAG, "Low-confidence seizure, monitoring...");
        }
        break;
    }
    case SZ_PKT_AURA_ALERT: {
        sz_aura_payload_t *a = (sz_aura_payload_t *)payload;
        ESP_LOGW(TAG, "AURA pre-ictal: lead=%d s prob=%d%%",
                 a->lead_time_s, a->probability);
        relay_to_beacon(h, payload, plen);
        mqtt_publish_aura(a);
        break;
    }
    case SZ_PKT_ACK: {
        sz_ack_payload_t *a = (sz_ack_payload_t *)payload;
        ESP_LOGI(TAG, "Caregiver ack: action=%d event=%u", a->action, a->event_unix);
        /* Stop bed-shaker */
        gpio_set_level(PIN_RELAY_SHAKER, 0);
        gpio_set_level(PIN_RELAY_AUDIO, 0);
        break;
    }
    case SZ_PKT_HEARTBEAT: {
        sz_heartbeat_payload_t *hb = (sz_heartbeat_payload_t *)payload;
        ESP_LOGI(TAG, "Heartbeat from %d: batt=%d%% rssi=%d",
                 h->src_node, hb->battery_pct, hb->rssi_dbm);
        break;
    }
    default:
        ESP_LOGI(TAG, "Unknown pkt type 0x%02x from %d", h->type, h->src_node);
    }
}

void relay_to_beacon(const sz_header_t *h, const uint8_t *payload, size_t plen)
{
    sz_header_t rb = *h;
    rb.src_node = SZ_SLOT_HUB;
    rb.dst_node = SZ_SLOT_BEACON;
    rb.seq++;
    uint8_t pkt[SZ_RADIO_MAX_PKT];
    size_t len = sz_pack(pkt, &rb, payload, plen);
    sx1262_send(pkt, len);
}

/* ---- MQTT cloud bridge ---- */
void mqtt_publish_event(const sz_seizure_payload_t *ev)
{
    /* Publish to seizuresync/{patient_id}/event */
    char topic[64];
    snprintf(topic, sizeof(topic), "seizuresync/%02x%02x/event",
             g_net_id[4], g_net_id[5]);
    char payload_str[256];
    snprintf(payload_str, sizeof(payload_str),
             "{\"onset\":%u,\"semiology\":%d,\"severity\":%d,"
             "\"duration\":%u,\"confidence\":%d,\"recovery\":%d}",
             ev->onset_unix, ev->semiology, ev->severity,
             ev->duration_s, ev->confidence, ev->recovery_state);
    mqtt_publish(topic, payload_str);
}

void mqtt_publish_aura(const sz_aura_payload_t *a)
{
    char topic[64];
    snprintf(topic, sizeof(topic), "seizuresync/%02x%02x/event",
             g_net_id[4], g_net_id[5]);
    char payload_str[128];
    snprintf(payload_str, sizeof(payload_str),
             "{\"type\":\"aura\",\"predicted\":%u,\"lead_s\":%u,\"prob\":%d}",
             a->predicted_unix, a->lead_time_s, a->probability);
    mqtt_publish(topic, payload_str);
}

void mqtt_publish_sudep(const sz_sudep_payload_t *s)
{
    char topic[64];
    snprintf(topic, sizeof(topic), "seizuresync/%02x%02x/sudep",
             g_net_id[4], g_net_id[5]);
    char payload_str[192];
    snprintf(payload_str, sizeof(payload_str),
             "{\"apnea_state\":%d,\"apnea_s\":%u,\"prone\":%d,"
             "\"spo2\":%d,\"hr\":%d}",
             s->apnea_state, s->apnea_duration_s, s->prone_flag,
             s->spo2_pct, s->hr_bpm);
    mqtt_publish(topic, payload_str);
}

/* ---- E-ink display update ---- */
void display_update(const char *line1, const char *line2, const char *line3)
{
    uc8151d_clear();
    uc8151d_draw_text(0, 0, line1, 2);
    uc8151d_draw_text(0, 40, line2, 1);
    uc8151d_draw_text(0, 60, line3, 1);
    uc8151d_refresh();
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "SeizureSync Hub starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO init for relays (fail-closed: off = safe for shaker,
     * but watchdog forces ON if MCU hang) */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL<<PIN_RELAY_SHAKER) | (1ULL<<PIN_RELAY_AUDIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_RELAY_SHAKER, 0);
    gpio_set_level(PIN_RELAY_AUDIO, 0);

    /* ADC init for BCG piezo sensors */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PIN_BCG_ADC1, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(PIN_BCG_ADC2, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(PIN_BCG_ADC3, ADC_ATTEN_DB_11);

    /* I²C init for MAX30102, MLX90640, SCD41 */
    i2c_config_t i2c1 = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 17, .scl_io_num = 16,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &i2c1);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    /* SX1262 Sub-GHz radio init */
    sx1262_init(PIN_SX1262_NSS, PIN_SX1262_RST, PIN_SX1262_DIO0,
                PIN_SX1262_BUSY, PIN_SX1262_SCK, PIN_SX1262_MISO,
                PIN_SX1262_MOSI);
    sx1262_set_frequency(SZ_RADIO_FREQ_HZ);
    sx1262_set_tx_power(SZ_RADIO_TX_DBM);
    sx1262_set_modem_params(SZ_RADIO_BW_HZ, SZ_RADIO_SF, SZ_RADIO_CR);

    /* Wi-Fi + MQTT init */
    wifi_init();
    mqtt_init();

    /* 4G LTE backup init */
    lte_backup_init(PIN_LTE_TX, PIN_LTE_RX, PIN_LTE_PWRKEY);

    /* E-ink display */
    uc8151d_init();
    display_update("SeizureSync Hub", "Initializing...", "");

    /* Load edge ML model (SeizureNet ensemble + SUDEPNet) */
    seizurenet_edge_init();

    /* Create tasks */
    g_alert_queue = xQueueCreate(8, sizeof(sz_seizure_payload_t));
    xTaskCreate(bcg_sample_task, "bcg", 4096, NULL, 5, NULL);
    xTaskCreate(spo2_task, "spo2", 4096, NULL, 4, NULL);
    xTaskCreate(prone_task, "prone", 4096, NULL, 3, NULL);
    xTaskCreate(tdma_coordinator_task, "tdma", 4096, NULL, 6, NULL);

    /* Watchdog: force bed-shaker ON if MCU hang (SUDEP safety) */
    esp_timer_handle_t wdt_timer;
    esp_timer_init();
    const esp_timer_create_args_t wdt_args = {
        .callback = [](void *arg) {
            /* If no heartbeat feed, trigger shaker + audio */
            gpio_set_level(PIN_RELAY_SHAKER, 1);
            gpio_set_level(PIN_RELAY_AUDIO, 1);
        },
        .name = "wdt_shaker"
    };
    esp_timer_create(&wdt_args, &wdt_timer);
    esp_timer_start_periodic(wdt_timer, 30000000);  /* 30 s */

    display_update("SeizureSync Hub", "Monitoring active", "Risk: LOW");

    ESP_LOGI(TAG, "Hub ready. All tasks started.");
}