/**
 * MigraineSync — Hub Main
 * ========================
 * ESP32-S3 running ESP-IDF.
 *
 * Coordinates all nodes via Sub-GHz (Env Sentinel) and BLE 5.0
 * (Aura Band, Hydrate Tag). Runs edge ML inference (tflite-micro)
 * every 15 min. Publishes telemetry to cloud via WiFi/MQTT.
 * Stores 14-day rolling cache in PSRAM + microSD.
 *
 * License: MIT
 */

#include "config.h"
#include "../common/protocol.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/spi_master.h>

static const char *TAG = "migrainesync_hub";

/* ── State ──────────────────────────────────────────────── */
static QueueHandle_t s_rx_queue;       /* frames from Sub-GHz/BLE */
static QueueHandle_t s_tx_queue;       /* frames to cloud (MQTT) */
static uint8_t s_seq = 0;
static float s_current_risk = 0.0f;
static bool s_wifi_connected = false;

/* ── Frame buffer for received messages ─────────────────── */
typedef struct {
    frame_t frame;
    tlv_t   tlvs[MAX_TLVS];
    int     n_tlvs;
} rx_msg_t;

/* ── Forward declarations ───────────────────────────────── */
static void subghz_task(void *arg);
static void ble_central_task(void *arg);
static void edge_ml_task(void *arg);
static void mqtt_bridge_task(void *arg);
static void display_task(void *arg);
static void button_task(void *arg);

/* ── Alert LED + Buzzer ─────────────────────────────────── */
static void set_status_led(uint8_t r, uint8_t g, uint8_t b)
{
    /* WS2812 via RMT (simplified — real impl uses rmt_tx) */
    ESP_LOGI(TAG, "LED: R=%d G=%d B=%d", r, g, b);
}

static void buzz(uint16_t duration_ms, uint32_t freq_hz)
{
    /* PWM buzzer via ledc (simplified) */
    ESP_LOGI(TAG, "Buzz %dms @ %luHz", duration_ms, freq_hz);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
}

/* ── Process received frame ─────────────────────────────── */
static void process_rx(const rx_msg_t *msg)
{
    for (int i = 0; i < msg->n_tlvs; i++) {
        const tlv_t *tlv = &msg->tlvs[i];
        switch (tlv->type) {
        case MSG_ENVIRONMENT: {
            float pressure = decode_f32(&tlv->value[0]);
            float p_delta  = decode_f32(&tlv->value[4]);
            float light    = decode_f32(&tlv->value[8]);
            float temp     = decode_f32(&tlv->value[12]);
            float rh       = decode_f32(&tlv->value[16]);
            uint16_t voc   = decode_u16(&tlv->value[20]);
            uint16_t co2   = decode_u16(&tlv->value[22]);
            uint8_t noise  = tlv->value[24];

            ESP_LOGI(TAG, "ENV: P=%.1fhPa ΔP=%.2f lux=%.0f T=%.1f RH=%.1f "
                          "VOC=%u CO2=%u noise=%u",
                     pressure, p_delta, light, temp, rh, voc, co2, noise);

            /* Cache to PSRAM ring buffer + queue for MQTT */
        } break;

        case MSG_VITALS: {
            uint8_t hr     = tlv->value[0];
            float hrv      = decode_f32(&tlv->value[1]);
            uint8_t spo2   = tlv->value[5];
            float skin_t   = decode_f32(&tlv->value[6]);
            uint8_t act    = tlv->value[10];

            ESP_LOGI(TAG, "VITALS: HR=%u HRV=%.1fms SpO2=%u skinT=%.1f act=%u",
                     hr, hrv, spo2, skin_t, act);
        } break;

        case MSG_BAROMETRIC: {
            float pressure = decode_f32(&tlv->value[0]);
            float p_delta  = decode_f32(&tlv->value[4]);
            ESP_LOGI(TAG, "BARO: P=%.1f ΔP=%.2f hPa", pressure, p_delta);
        } break;

        case MSG_LIGHT_DOSE: {
            float lux = decode_f32(&tlv->value[0]);
            float cum = decode_f32(&tlv->value[4]);
            ESP_LOGI(TAG, "LIGHT: lux=%.0f cumulative=%.1f lux·min", lux, cum);
        } break;

        case MSG_HYDRATION: {
            float vol_ml   = decode_f32(&tlv->value[0]);
            uint8_t sips   = tlv->value[4];
            float weight_g = decode_f32(&tlv->value[5]);
            ESP_LOGI(TAG, "HYDRATION: vol=%.0fml sips=%u weight=%.0fg",
                     vol_ml, sips, weight_g);
        } break;

        case MSG_BATTERY: {
            uint8_t pct    = tlv->value[0];
            uint16_t volt  = decode_u16(&tlv->value[1]);
            ESP_LOGI(TAG, "BATT: %u%% %umV", pct, volt);
        } break;

        case MSG_MANUAL_EVENT: {
            uint8_t etype  = tlv->value[0];
            uint32_t ts    = (uint32_t)tlv->value[1] | ((uint32_t)tlv->value[2] << 8) |
                             ((uint32_t)tlv->value[3] << 16) | ((uint32_t)tlv->value[4] << 24);
            ESP_LOGI(TAG, "MANUAL EVENT: type=%u ts=%lu", etype, ts);
        } break;

        default:
            ESP_LOGW(TAG, "Unknown TLV type 0x%02X", tlv->type);
        }
    }
}

/* ── Edge ML task ───────────────────────────────────────── */
static void edge_ml_task(void *arg)
{
    ESP_LOGI(TAG, "Edge ML task started (interval %ds)", EDGE_ML_INTERVAL_S);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(EDGE_ML_INTERVAL_S * 1000));

        /* Gather last 6h of features from cache */
        /* In production: load quantized tflite-micro models from flash
         *   - prodrome_detector.tflite (1D-CNN, 180 KB)
         *   - onset_predictor.tflite   (LSTM, 420 KB)
         * Run inference → get risk score 0-100
         */

        /* Placeholder inference (real impl uses tflite-micro) */
        float risk = 0.0f;
        /* ... feature extraction + tflite micro inference ... */
        s_current_risk = risk;

        ESP_LOGI(TAG, "Edge ML: risk=%.1f%%", risk);

        /* Update status LED based on risk */
        if (risk >= RISK_THRESHOLD_HIGH) {
            set_status_led(255, 0, 0);  /* red */
            buzz(500, 3000);
            /* Send alert to Aura Band via BLE */
        } else if (risk >= RISK_THRESHOLD_MOD) {
            set_status_led(255, 100, 0);  /* orange */
        } else if (risk >= RISK_THRESHOLD_LOW) {
            set_status_led(255, 255, 0);  /* yellow */
        } else {
            set_status_led(0, 255, 0);  /* green */
        }
    }
}

/* ── Sub-GHz task ───────────────────────────────────────── */
static void subghz_task(void *arg)
{
    ESP_LOGI(TAG, "Sub-GHz task started (SX1262, 868.1 MHz, TDMA mesh coordinator)");

    /* In production: init SX1262 via HSPI
     * Configure as TDMA coordinator
     * Broadcast time-sync beacon every superframe
     * Listen for Env Sentinel data frames in assigned slots
     */

    rx_msg_t msg;
    while (1) {
        /* Simulated receive — real impl: SX1262 CAD + receive */
        if (xQueueReceive(s_rx_queue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE) {
            process_rx(&msg);
        }
    }
}

/* ── BLE Central task ───────────────────────────────────── */
static void ble_central_task(void *arg)
{
    ESP_LOGI(TAG, "BLE central task started (scanning for Aura Band + Hydrate Tag)");

    /* In production: init NimBLE stack
     * Scan for MigraineSync service UUID
     * Connect to Aura Band and Hydrate Tag
     * Subscribe to TX characteristic notifications
     * Forward frames to s_rx_queue
     */

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        /* Periodic re-scan if connections dropped */
    }
}

/* ── MQTT Bridge task ───────────────────────────────────── */
static void mqtt_bridge_task(void *arg)
{
    ESP_LOGI(TAG, "MQTT bridge task started");

    while (1) {
        if (!s_wifi_connected) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        /* In production: use esp-mqtt client
         * Batch telemetry every 5 min
         * Publish to migrainesync/telemetry
         * Handle backfill when WiFi restored
         */

        vTaskDelay(pdMS_TO_TICKS(300000));  /* 5 min */
    }
}

/* ── Display task ──────────────────────────────────────── */
static void display_task(void *arg)
{
    ESP_LOGI(TAG, "Display task started (ILI9341 2.4\" TFT)");

    while (1) {
        /* In production: render risk gauge, top trigger, time on ILI9341 */
        ESP_LOGI(TAG, "Display: risk=%.1f%%", s_current_risk);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ── Button task ────────────────────────────────────────── */
static void button_task(void *arg)
{
    ESP_LOGI(TAG, "Button task started");

    while (1) {
        if (gpio_get_level(BTN_ACK_PIN) == 0) {
            ESP_LOGI(TAG, "ACK button pressed — clearing alert");
            set_status_led(0, 255, 0);
            vTaskDelay(pdMS_TO_TICKS(300));  /* debounce */
        }
        if (gpio_get_level(BTN_SILENCE_PIN) == 0) {
            ESP_LOGI(TAG, "Silence button pressed");
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ── app_main ───────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "MigraineSync Hub v%s starting...", FIRMWARE_VERSION);

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES)
        ESP_ERROR_CHECK(nvs_flash_erase());

    /* Create queues */
    s_rx_queue = xQueueCreate(32, sizeof(rx_msg_t));
    s_tx_queue = xQueueCreate(64, sizeof(rx_msg_t));

    /* Init GPIO for buttons */
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BTN_ACK_PIN) | (1ULL << BTN_SILENCE_PIN) |
                        (1ULL << BTN_PAIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    /* Init WiFi */
    /* esp_netif_init(); esp_event_loop_create_default(); ... */

    /* Start tasks */
    xTaskCreate(subghz_task,       "subghz",    4096, NULL, 5, NULL);
    xTaskCreate(ble_central_task,  "ble",       4096, NULL, 5, NULL);
    xTaskCreate(edge_ml_task,      "edge_ml",   8192, NULL, 4, NULL);
    xTaskCreate(mqtt_bridge_task,  "mqtt",      4096, NULL, 3, NULL);
    xTaskCreate(display_task,      "display",   4096, NULL, 3, NULL);
    xTaskCreate(button_task,       "button",    2048, NULL, 6, NULL);

    ESP_LOGI(TAG, "All tasks started");
}