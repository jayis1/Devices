/**
 * GlucoSync Meal Scanner — Main Firmware
 *
 * ESP32-S3-WROOM-1-N8R2
 * Handheld multispectral food scanner. Captures 5-band images (white/470/660/850/940 nm),
 * runs on-device MobileNetV3-tiny CNN for food classification + carb regression,
 * sends results to Hub via BLE 5.0.
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
#include "camera_driver.h"
#include "food_inference.h"
#include "bme280.h"
#include "ble_periph.h"

static const char *TAG = "glucosync_scanner";

/* ── Scan State ─────────────────────────────────────────────────── */

typedef enum {
    SCAN_STATE_IDLE = 0,
    SCAN_STATE_CAPTURING = 1,
    SCAN_STATE_INFERRING = 2,
    SCAN_STATE_SENDING = 3,
} scan_state_t;

typedef struct {
    scan_state_t state;
    uint16_t seq_counter;
    uint8_t spectral_bands_captured;  /* bitmask */
    uint8_t band_idx;
    camera_frame_t frames[5];  /* one per spectral band */
    QueueHandle_t scan_queue;
} scanner_state_t;

static scanner_state_t g_state = {0};

/* ── BLE RX callback (commands from hub) ────────────────────────── */

static void ble_rx_callback(const uint8_t *data, uint8_t len)
{
    glucosync_header_t header;
    const uint8_t *payload;

    if (!glucosync_decode(data, len, &header, &payload)) {
        return;
    }

    if (header.msg_type == MSG_TYPE_CMD_MODE) {
        payload_mode_t mode;
        memcpy(&mode, payload, sizeof(mode));
        ESP_LOGI(TAG, "Mode change: %d", mode.mode);
    }
}

/* ── Scan Button Interrupt ──────────────────────────────────────── */

static void scan_button_handler(void *arg)
{
    if (g_state.state == SCAN_STATE_IDLE) {
        uint8_t trigger = 1;
        xQueueSendFromISR(g_state.scan_queue, &trigger, NULL);
    }
}

/* ── Scan Task ──────────────────────────────────────────────────── */

static void scan_task(void *arg)
{
    uint8_t trigger;

    while (1) {
        if (xQueueReceive(g_state.scan_queue, &trigger, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Scan initiated");

            g_state.state = SCAN_STATE_CAPTURING;
            g_state.spectral_bands_captured = 0;
            g_state.band_idx = 0;

            /* Capture 5 spectral band images in sequence */
            const uint8_t bands[5] = {
                BAND_WHITE, BAND_470NM, BAND_660NM, BAND_850NM, BAND_940NM
            };

            for (int i = 0; i < 5; i++) {
                /* Turn on LED for this band */
                camera_set_led(bands[i], true);
                vTaskDelay(pdMS_TO_TICKS(50));  /* LED warmup */

                /* Capture frame */
                camera_frame_t *frame = &g_state.frames[i];
                if (camera_capture(frame) == 0) {
                    g_state.spectral_bands_captured |= (1 << i);
                    ESP_LOGD(TAG, "Captured band %d", bands[i]);
                }

                /* Turn off LED */
                camera_set_led(bands[i], false);
                vTaskDelay(pdMS_TO_TICKS(20));  /* cool down */
            }

            if (g_state.spectral_bands_captured != 0x1F) {
                ESP_LOGW(TAG, "Incomplete capture: 0x%02X", g_state.spectral_bands_captured);
                g_state.state = SCAN_STATE_IDLE;
                continue;
            }

            /* Run food inference */
            g_state.state = SCAN_STATE_INFERRING;

            food_inference_result_t result;
            float ambient_temp, ambient_humidity;
            bme280_read(&ambient_temp, &ambient_humidity);

            if (food_inference_predict(g_state.frames, &result)) {
                ESP_LOGI(TAG, "Food: class=%d (%d%%), carbs=%ug, portion=%ug, GI=%d",
                         result.food_class_id, result.food_confidence,
                         result.carb_grams, result.portion_grams,
                         result.glycemic_index);

                /* Send to hub via BLE */
                g_state.state = SCAN_STATE_SENDING;

                payload_meal_t meal = {0};
                meal.food_class_id = result.food_class_id;
                meal.food_confidence = result.food_confidence;
                meal.carb_grams = result.carb_grams;
                meal.portion_grams = result.portion_grams;
                meal.glycemic_index = result.glycemic_index;
                meal.spectral_bands = g_state.spectral_bands_captured;
                meal.timestamp = esp_timer_get_time() / 1000;

                uint8_t packet[GS_MAX_PACKET_LEN];
                uint8_t pkt_len = glucosync_encode(packet, sizeof(packet),
                                                    MSG_TYPE_DATA_MEAL,
                                                    GS_SCANNER_ID_BASE,
                                                    g_state.seq_counter++,
                                                    GS_FLAG_ENCRYPTED,
                                                    (uint8_t *)&meal, sizeof(meal));
                ble_periph_send(packet, pkt_len);

                /* Status LED: green = success */
                camera_set_status_led(0x00FF00);
            } else {
                ESP_LOGE(TAG, "Food inference failed");
                camera_set_status_led(0xFF0000);  /* red = error */
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
            camera_set_status_led(0x000000);
            g_state.state = SCAN_STATE_IDLE;
        }
    }
}

/* ── Main ────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "GlucoSync Meal Scanner starting...");

    nvs_flash_init();

    g_state.state = SCAN_STATE_IDLE;
    g_state.seq_counter = 0;
    g_state.scan_queue = xQueueCreate(4, sizeof(uint8_t));

    /* Initialize camera */
    camera_init();
    camera_set_resolution(640, 480);

    /* Initialize BME280 (ambient correction) */
    bme280_init();

    /* Initialize food inference model */
    food_inference_init();

    /* Initialize BLE peripheral */
    ble_periph_init(ble_rx_callback);
    ble_periph_start_advertising("GlucoSync-Scanner");

    /* Create scan task */
    xTaskCreate(scan_task, "scan_task", 8192, NULL, 5, NULL);

    /* Main loop — heartbeat + status */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        /* Send heartbeat to hub */
        payload_status_t status = {0};
        status.battery_pct = 100;  /* TODO: read LiPo ADC */
        status.state = (g_state.state == SCAN_STATE_IDLE) ? 0 : 1;

        uint8_t packet[GS_MAX_PACKET_LEN];
        uint8_t pkt_len = glucosync_encode(packet, sizeof(packet),
                                            MSG_TYPE_STATUS,
                                            GS_SCANNER_ID_BASE,
                                            g_state.seq_counter++,
                                            0,
                                            (uint8_t *)&status, sizeof(status));
        ble_periph_send(packet, pkt_len);

        ESP_LOGI(TAG, "Heartbeat — state: %d — bat: 100%%", g_state.state);
    }
}