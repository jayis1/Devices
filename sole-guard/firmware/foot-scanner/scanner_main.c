/*
 * scanner_main.c — SoleGuard Foot Scanner node firmware (ESP32-S3, ESP-IDF)
 *
 * Daily plantar + interdigital foot imaging with white/IR/UV-A LED ring,
 * on-device TFLite wound detection, HX711 weight measurement, and WiFi
 * upload to the cloud + hub coordination.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "tflite_micro.h"
#include "sole_protocol.h"

static const char *TAG = "scanner";

#define NODE_ID SOLE_NODE_ID_SCANNER

/* LED GPIOs (active-high MOSFET-driven) */
#define LED_WHITE_PIN  GPIO_NUM_38
#define LED_IR_PIN     GPIO_NUM_39
#define LED_UV_PIN     GPIO_NUM_40
#define TOUCH_PIN      GPIO_NUM_41   /* capacitive touch button */
#define TFT_CS         GPIO_NUM_42

/* Camera pins (OV5640 on ESP32-S3 DVP interface) */
#define CAM_PWDN    -1
#define CAM_RESET   -1
#define CAM_XCLK    GPIO_NUM_15
#define CAM_SIOD    GPIO_NUM_4
#define CAM_SIOC    GPIO_NUM_5
#define CAM_Y9      GPIO_NUM_16
#define CAM_Y8      GPIO_NUM_17
#define CAM_Y7      GPIO_NUM_18
#define CAM_Y6      GPIO_NUM_12
#define CAM_Y5      GPIO_NUM_10
#define CAM_Y4      GPIO_NUM_8
#define CAM_Y3      GPIO_NUM_9
#define CAM_Y2      GPIO_NUM_11
#define CAM_Y3_PIN  GPIO_NUM_9
#define CAM_VSYNC   GPIO_NUM_6
#define CAM_HREF    GPIO_NUM_7
#define CAM_PCLK    GPIO_NUM_13

/* HX711 load cell */
#define HX711_SCK   GPIO_NUM_44
#define HX711_DOUT  GPIO_NUM_43

/* Wound classes */
static const char *wound_class_names[] = {
    "normal", "callus", "blister", "fissure", "ulcer", "fungal", "maceration"
};

static camera_config_t camera_config = {
    .pin_pwdn     = CAM_PWDN,
    .pin_reset    = CAM_RESET,
    .pin_xclk     = CAM_XCLK,
    .pin_sccb_sda = CAM_SIOD,
    .pin_sccb_scl = CAM_SIOC,
    .pin_y9 = CAM_Y9, .pin_y8 = CAM_Y8, .pin_y7 = CAM_Y7, .pin_y6 = CAM_Y6,
    .pin_y5 = CAM_Y5, .pin_y4 = CAM_Y4, .pin_y3 = CAM_Y3, .pin_y2 = CAM_Y2,
    .pin_vsync = CAM_VSYNC, .pin_href = CAM_HREF, .pin_pclk = CAM_PCLK,
    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size   = FRAMESIZE_QXGA,   /* 2048x1536 — downscale for model */
    .jpeg_quality = 12,
    .fb_count     = 2,
    .fb_location  = CAMERA_FB_IN_PSRAM,
};

static void leds_on(uint8_t mask) {
    gpio_set_level(LED_WHITE_PIN, mask & 1);
    gpio_set_level(LED_IR_PIN,    mask & 2);
    gpio_set_level(LED_UV_PIN,    mask & 4);
}

static void leds_init(void) {
    gpio_config_t io = { .pin_bit_mask = (1ULL<<LED_WHITE_PIN)|(1ULL<<LED_IR_PIN)|(1ULL<<LED_UV_PIN),
                         .mode = GPIO_MODE_OUTPUT, .pull_up_en = 0, .pull_down_en = 0, .intr_type = GPIO_INTR_DISABLE };
    gpio_config(&io);
    leds_on(0);
}

static esp_err_t camera_init_hw(void) {
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
    return err;
}

/* ---- TFLite wound detection ---- */
static TfLiteMicroModel *wound_model;
static uint8_t wound_arena[96 * 1024];

extern const unsigned char wound_detect_model_data[];
extern const size_t wound_detect_model_len;

static void wound_model_init(void) {
    wound_model = tflite_micro_load(wound_detect_model_data, wound_detect_model_len,
                                    wound_arena, sizeof(wound_arena));
    if (!wound_model) ESP_LOGE(TAG, "Wound model load failed");
}

/* Run wound detection on a captured frame.
 * Returns class index (0=normal..6) and confidence 0-100. */
static int wound_classify(camera_fb_t *fb, uint8_t *confidence) {
    if (!wound_model || !fb) { *confidence = 0; return 0; }
    /* Downscale RGB565 frame to 224x224 float input */
    float input[224 * 224 * 3];
    /* Nearest-neighbor downscale from fb->width x fb->height to 224x224 */
    for (int y = 0; y < 224; y++) {
        for (int x = 0; x < 224; x++) {
            int sx = (x * fb->width) / 224;
            int sy = (y * fb->height) / 224;
            uint16_t px = ((uint16_t *)fb->buf)[sy * fb->width + sx];
            uint8_t r = (px >> 11) & 0x1F;
            uint8_t g = (px >> 5) & 0x3F;
            uint8_t b = px & 0x1F;
            input[(y*224+x)*3+0] = r / 31.0f;
            input[(y*224+x)*3+1] = g / 63.0f;
            input[(y*224+x)*3+2] = b / 31.0f;
        }
    }
    float output[7];
    if (tflm_model_invoke(wound_model, input, 224*224*3, output, 7) != 0) {
        *confidence = 0; return 0;
    }
    int best = 0; float bestv = output[0];
    for (int i = 1; i < 7; i++) if (output[i] > bestv) { bestv = output[i]; best = i; }
    *confidence = (uint8_t)(bestv * 100.0f);
    return best;
}

/* ---- HX711 weight scale ---- */
extern int hx711_read_grams(int32_t *grams);

static int32_t measure_weight(void) {
    int32_t g = 0;
    if (hx711_read_grams(&g) == 0)
        return g;
    return 0;
}

/* ---- WiFi + upload (simplified) ---- */
extern esp_err_t wifi_connect(void);
extern esp_err_t cloud_upload_scan(const uint8_t *jpeg, size_t len,
                                    uint8_t wound_class, uint8_t confidence,
                                    int32_t weight_grams, uint8_t foot_side);

/* ---- Scan sequence ---- */
static void do_scan(uint8_t foot_side) {
    ESP_LOGI(TAG, "Scanning %s foot", foot_side == 0 ? "left" : "right");

    /* Capture 3 images: white, IR, UV */
    camera_fb_t *fbs[3] = {0};
    uint8_t masks[3] = { 0x01, 0x02, 0x04 };  /* white, IR, UV */

    for (int i = 0; i < 3; i++) {
        leds_on(masks[i]);
        vTaskDelay(pdMS_TO_TICKS(120)); /* let LEDs + auto-exposure settle */
        fbs[i] = esp_camera_fb_get();
        leds_on(0);
        if (!fbs[i]) { ESP_LOGE(TAG, "Capture %d failed", i); goto done; }
    }

    /* Classify on the white-light image (primary); IR/UV uploaded for clinician */
    uint8_t conf = 0;
    int cls = wound_classify(fbs[0], &conf);
    ESP_LOGI(TAG, "Wound class: %s (%d%%)", wound_class_names[cls], conf);

    int32_t weight = measure_weight();
    ESP_LOGI(TAG, "Weight: %ld g", (long)weight);

    /* Convert primary frame to JPEG for upload (camera may already be JPEG; here RGB565) */
    /* For brevity: upload the raw RGB565 buffer (cloud converts). In production, encode JPEG. */
    cloud_upload_scan(fbs[0]->buf, fbs[0]->len, (uint8_t)cls, conf, weight, foot_side);

    /* If wound detected (class > 0), send alert to hub via WiFi */
    if (cls > 0 && conf > 60) {
        sole_scan_payload_t sp = {0};
        sp.type = SOLE_MSG_SCAN_RESULT;
        sp.node_id = NODE_ID;
        sp.flags = SOLE_ALERT_WOUND;
        sp.wound_class = (uint8_t)cls;
        sp.confidence = conf;
        sp.image_ref = 0; /* filled by cloud */
        sp.weight_dag = (int16_t)(weight / 100); /* decagrams */
        sole_pack_crc(&sp, sizeof(sp) - 2);
        /* Send to hub over WiFi (hub's REST endpoint) */
        extern esp_err_t hub_send_scan_result(const void *payload, size_t len);
        hub_send_scan_result(&sp, sizeof(sp));
    }

done:
    for (int i = 0; i < 3; i++)
        if (fbs[i]) esp_camera_fb_return(fbs[i]);
}

/* ---- Touch button (polling) ---- */
static uint8_t foot_side_select = 0; /* 0=left, 1=right */

static void touch_task(void *arg) {
    int debounce = 0;
    while (1) {
        if (gpio_get_level(TOUCH_PIN) == 0) {
            debounce++;
            if (debounce == 20) { /* 200ms held */
                do_scan(foot_side_select);
                foot_side_select = 1 - foot_side_select;
            }
        } else {
            debounce = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "SoleGuard Foot Scanner starting");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    leds_init();
    gpio_config_t touch_cfg = { .pin_bit_mask = (1ULL<<TOUCH_PIN),
                                .mode = GPIO_MODE_INPUT, .pull_up_en = 1 };
    gpio_config(&touch_cfg);

    camera_init_hw();
    wound_model_init();
    wifi_connect();

    xTaskCreate(touch_task, "touch", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Ready — touch button to scan left foot");
}