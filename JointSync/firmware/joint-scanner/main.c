/**
 * JointSync Joint Scanner — Main Firmware
 *
 * ESP32-S3-WROOM-1-N8R8
 * MLX90640 thermal array + OV5640 multispectral camera + 3 LED illuminators
 * BLE 5.0 to Hub.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "protocol.h"
#include "mlx90640_driver.h"
#include "ov5640_driver.h"
#include "thermal_process.h"
#include "led_driver.h"
#include "ble_scanner.h"

static const char *TAG = "joint_scanner";

#define JS_SCANNER_NODE_ID  0x0200

/* ── State ───────────────────────────────────────────────────────── */

typedef struct {
    uint16_t node_id;
    uint16_t seq_counter;
    bool    connected;
    bool    scanning;
    int16_t thermal_frame[768];  /* 32×24 pixels (centi-degrees C) */
    uint8_t multispectral_image[320 * 240];  /* Thumbnail */
} scanner_state_t;

static scanner_state_t g_state = {0};

/* ── BLE Command Handler ──────────────────────────────────────────── */

static void ble_cmd_handler(uint16_t sender_id, const uint8_t *data, uint8_t len)
{
    jointsync_header_t header;
    const uint8_t *payload;

    if (!jointsync_decode(data, len, &header, &payload)) {
        return;
    }

    switch (header.msg_type) {
    case MSG_TYPE_CMD_SCAN:
        if (!g_state.scanning) {
            g_state.scanning = true;
            ESP_LOGI(TAG, "Scan command received, starting scan...");
            /* Scan will be performed in the scan_task */
        }
        break;

    case MSG_TYPE_HEARTBEAT:
        ESP_LOGD(TAG, "Hub heartbeat");
        break;

    default:
        break;
    }
}

/* ── Scan Task ───────────────────────────────────────────────────── */

static void scan_task(void *arg)
{
    while (1) {
        if (!g_state.scanning) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        ESP_LOGI(TAG, "Starting joint scan...");

        /* Step 1: Capture thermal frame (MLX90640) */
        if (mlx90640_read_frame(g_state.thermal_frame) != ESP_OK) {
            ESP_LOGE(TAG, "MLX90640 read failed");
            g_state.scanning = false;
            continue;
        }

        /* Process thermal data */
        thermal_result_t result = thermal_process_frame(g_state.thermal_frame);
        ESP_LOGI(TAG, "Thermal: max=%.1f°C mean=%.1f°C asymmetry=%.2f°C",
                 result.max_temp, result.mean_temp, result.thermal_asymmetry);

        /* Send thermal data to Hub in chunks */
        for (int chunk = 0; chunk < JS_THERMAL_CHUNKS; chunk++) {
            payload_thermal_chunk_t payload;
            payload.chunk_idx = chunk;
            payload.total_chunks = JS_THERMAL_CHUNKS;

            int offset = chunk * JS_THERMAL_PIXELS_PER_CHUNK;
            int remaining = 768 - offset;
            int pixels = (remaining < JS_THERMAL_PIXELS_PER_CHUNK) ?
                         remaining : JS_THERMAL_PIXELS_PER_CHUNK;

            memcpy(payload.pixels, &g_state.thermal_frame[offset],
                   pixels * sizeof(int16_t));

            /* Zero-fill remaining pixels in chunk */
            if (pixels < JS_THERMAL_PIXELS_PER_CHUNK) {
                memset(&payload.pixels[pixels], 0,
                       (JS_THERMAL_PIXELS_PER_CHUNK - pixels) * sizeof(int16_t));
            }

            uint8_t packet[JS_MAX_PACKET_LEN];
            uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                                MSG_TYPE_DATA_THERMAL,
                                                g_state.node_id,
                                                g_state.seq_counter++,
                                                0,
                                                (uint8_t *)&payload, sizeof(payload));
            ble_scanner_send(packet, pkt_len);
            vTaskDelay(pdMS_TO_TICKS(20));  /* Small delay between chunks */
        }

        /* Step 2: Capture multispectral images */
        /* White light image */
        led_driver_set(LED_WHITE, 255);
        vTaskDelay(pdMS_TO_TICKS(100));
        ov5640_capture_qvga(g_state.multispectral_image, sizeof(g_state.multispectral_image));
        led_driver_off();

        /* UV image */
        led_driver_set(LED_UV, 200);
        vTaskDelay(pdMS_TO_TICKS(100));
        ov5640_capture_qvga(g_state.multispectral_image, sizeof(g_state.multispectral_image));
        led_driver_off();

        /* NIR image */
        led_driver_set(LED_NIR, 200);
        vTaskDelay(pdMS_TO_TICKS(100));
        ov5640_capture_qvga(g_state.multispectral_image, sizeof(g_state.multispectral_image));
        led_driver_off();

        ESP_LOGI(TAG, "Scan complete. Thermal + multispectral images captured.");

        /* Send image thumbnail to Hub (simplified — sends metadata only) */
        uint8_t img_metadata[8];
        img_metadata[0] = (uint8_t)(result.max_temp * 2);  /* Max temp × 2 */
        img_metadata[1] = (uint8_t)(result.mean_temp * 2); /* Mean temp × 2 */
        img_metadata[2] = (uint8_t)((result.thermal_asymmetry + 128) & 0xFF);
        img_metadata[3] = result.swelling_grade;
        img_metadata[4] = 320 & 0xFF;  /* Image width low */
        img_metadata[5] = (320 >> 8);   /* Image width high */
        img_metadata[6] = 240 & 0xFF;  /* Image height low */
        img_metadata[7] = 0;           /* Image height high (240 < 256) */

        uint8_t packet[JS_MAX_PACKET_LEN];
        uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                            MSG_TYPE_DATA_IMAGE,
                                            g_state.node_id,
                                            g_state.seq_counter++,
                                            0,
                                            img_metadata, sizeof(img_metadata));
        ble_scanner_send(packet, pkt_len);

        g_state.scanning = false;
    }
}

/* ── Button Task ─────────────────────────────────────────────────── */

static void button_task(void *arg)
{
    static bool prev_btn = false;

    while (1) {
        bool btn = (gpio_get_level(39) == 0);  /* Active low */

        if (btn && !prev_btn && !g_state.scanning) {
            g_state.scanning = true;
            ESP_LOGI(TAG, "Scan button pressed");
        }

        prev_btn = btn;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ── Main ────────────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "JointSync Scanner starting...");

    nvs_flash_init();

    g_state.node_id = JS_SCANNER_NODE_ID;
    g_state.seq_counter = 0;
    g_state.scanning = false;

    /* Initialize GPIO */
    gpio_set_direction(39, GPIO_MODE_INPUT);  /* Scan button */
    gpio_pullup_en(39);
    gpio_set_direction(40, GPIO_MODE_INPUT);  /* Power button */
    gpio_pullup_en(40);
    gpio_set_direction(41, GPIO_MODE_OUTPUT); /* Status LED */

    /* Initialize LED drivers */
    led_driver_init();

    /* Initialize thermal sensor */
    mlx90640_init();

    /* Initialize camera */
    ov5640_init();

    /* Initialize BLE */
    ble_scanner_init(ble_cmd_handler);
    ble_scanner_advertise();

    /* Create tasks */
    xTaskCreate(scan_task, "scan", 8192, NULL, 5, NULL);
    xTaskCreate(button_task, "button", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "Scanner ready (node 0x%04X)", g_state.node_id);

    /* Main loop — heartbeat + status LED */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        /* Send heartbeat */
        payload_status_t status;
        status.battery_pct = 100;  /* TODO: read LiPo voltage */
        status.state = g_state.scanning ? 1 : 0;
        status.error_code = 0;

        uint8_t packet[JS_MAX_PACKET_LEN];
        uint8_t pkt_len = jointsync_encode(packet, sizeof(packet),
                                            MSG_TYPE_HEARTBEAT,
                                            g_state.node_id,
                                            g_state.seq_counter++,
                                            0,
                                            (uint8_t *)&status, sizeof(status));
        ble_scanner_send(packet, pkt_len);

        /* Blink status LED */
        gpio_set_level(41, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(41, 0);
    }
}