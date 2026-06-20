/*
 * scanner_main.c — GreenPulse Leaf Scanner Node firmware (ESP32-S3, ESP-IDF)
 *
 * Captures multispectral leaf images (white + UV + NIR, 3 shots in 2s),
 * runs the on-device species-ID CNN (TFLite Micro, ~4MB int8), pre-screens
 * for disease (binary healthy/suspect), uploads the full multispectral set
 * to the cloud for high-res disease/pest CNN, and displays results on the
 * 1.3" OLED. WiFi to hub/cloud.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi.h"
#include "driver/i2c.h"
#include "green_protocol.h"

static const char *TAG = "leaf_scanner";

/* ---- Hardware pins ---- */
#define PIN_OV5640_PWDN    2
#define PIN_OV5640_RESET  3
#define PIN_LED_WHITE     4   /* 5500K white LED ring */
#define PIN_LED_UV        5   /* 365nm UV LED */
#define PIN_LED_NIR       6   /* 850nm NIR LED */
#define PIN_BUTTON_CAPTURE 7
#define PIN_BUTTON_MODE   8
#define PIN_BUTTON_ID     9
#define PIN_SD_CS        10
#define PIN_OLED_DC     11

/* ---- OLED (SH1106, SPI) ---- */
#define OLED_WIDTH  128
#define OLED_HEIGHT 64

/* ---- TFLite Micro species-ID model (stub) ---- */
extern const unsigned char *species_model_data;
extern unsigned int species_model_len;

extern const unsigned char *disease_prescreen_data;
extern unsigned int disease_prescreen_len;

/* Image dimensions for on-device inference (downsampled) */
#define IMG_W 224
#define IMG_H 224
#define IMG_CH 3  /* white + UV + NIR */

typedef struct { void *handle; } TfLiteMicroModel;

TfLiteMicroModel *tflm_model_create(const unsigned char *data, unsigned int len)
{
    (void)data; (void)len;
    return (TfLiteMicroModel *)1;  /* stub */
}

int tflm_model_invoke_uint8(TfLiteMicroModel *m, const uint8_t *input,
                            int in_dim, uint8_t *output, int out_dim)
{
    (void)m; (void)input; (void)in_dim; (void)out_dim;
    /* Stub: return top-1 species = 1 (Monstera), confidence 72 */
    output[0] = 1;   /* species class */
    output[1] = 72;  /* confidence */
    return 0;
}

int tflm_model_invoke_disease(TfLiteMicroModel *m, const uint8_t *input,
                              int in_dim, uint8_t *output, int out_dim)
{
    (void)m; (void)input; (void)in_dim; (void)out_dim;
    /* Stub: return healthy=1, suspect=0 */
    output[0] = 0;  /* 0=healthy, 1=suspect */
    output[1] = 85; /* confidence */
    return 0;
}

/* ---- OV5640 camera capture (simplified) ---- */
static int capture_image(uint8_t *buf, int max_bytes, uint8_t led_mode)
{
    /* In production:
     * 1. Turn on selected LED (white/UV/NIR)
     * 2. Wait 100ms for illumination to stabilize
     * 3. Trigger OV5640 single-frame capture (I2C config + DVP readout)
     * 4. Downsample to 224x224 if feeding to CNN, or save full 5MP to SD
     * 5. Turn off LED
     */
    gpio_set_level(led_mode, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(led_mode, 0);
    /* Stub: zero-fill the buffer */
    memset(buf, 0, max_bytes);
    return IMG_W * IMG_H;  /* single channel per shot */
}

/* ---- OLED display (SH1106) ---- */
static void oled_init(void)
{
    /* In production: SPI init, SH1106 init sequence */
}

static void oled_show_text(const char *line1, const char *line2,
                           const char *line3, const char *line4)
{
    ESP_LOGI(TAG, "OLED: [%s] [%s] [%s] [%s]", line1, line2, line3, line4);
    /* In production: render text to SH1106 framebuffer, SPI transfer */
}

/* ---- WiFi + cloud upload ---- */
static void wifi_init_and_connect(void)
{
    /* In production: esp_wifi_init, connect to configured SSID,
     * or use BLE to get credentials from hub/mobile app. */
}

static void upload_multispectral_set(uint8_t tag_id, const uint8_t *white,
                                     const uint8_t *uv, const uint8_t *nir,
                                     int img_bytes)
{
    /* In production: multipart POST to cloud /api/v1/scan/upload
     * with tag_id + 3 image channels.
     * Cloud runs full disease/pest CNN and returns results via MQTT. */
    (void)tag_id; (void)white; (void)uv; (void)nir; (void)img_bytes;
    ESP_LOGI(TAG, "Uploaded multispectral set for tag %u", tag_id);
}

/* ---- Capture + inference cycle ---- */
static uint8_t current_tag_id = 0;  /* set via button cycle */
static uint8_t capture_mode = 0;    /* 0=white 1=UV 2=NIR 3=multispectral */

static void do_capture_and_analyze(void)
{
    uint8_t white_img[IMG_W * IMG_H];
    uint8_t uv_img[IMG_W * IMG_H];
    uint8_t nir_img[IMG_W * IMG_H];

    oled_show_text("Scanning...", "", "", "");

    /* Capture multispectral set (white + UV + NIR) */
    int sz_w = capture_image(white_img, sizeof(white_img), PIN_LED_WHITE);
    int sz_u = capture_image(uv_img, sizeof(uv_img), PIN_LED_UV);
    int sz_n = capture_image(nir_img, sizeof(nir_img), PIN_LED_NIR);

    /* Run species-ID on white-light image */
    uint8_t species_out[2];
    TfLiteMicroModel *sm = tflm_model_create(species_model_data, species_model_len);
    tflm_model_invoke_uint8(sm, white_img, sz_w, species_out, 2);
    uint8_t species = species_out[0];
    uint8_t spec_conf = species_out[1];

    /* Run disease pre-screen (binary: healthy/suspect) on NIR image */
    uint8_t disease_out[2];
    TfLiteMicroModel *dm = tflm_model_create(disease_prescreen_data, disease_prescreen_len);
    tflm_model_invoke_disease(dm, nir_img, sz_n, disease_out, 2);
    uint8_t disease_suspect = disease_out[0];
    uint8_t disease_conf = disease_out[1];

    /* Upload full set to cloud for high-res disease/pest CNN */
    upload_multispectral_set(current_tag_id, white_img, uv_img, nir_img,
                             IMG_W * IMG_H);

    /* Display result on OLED */
    char line1[20], line2[24], line3[24], line4[20];
    snprintf(line1, sizeof(line1), "Tag %u", current_tag_id);
    snprintf(line2, sizeof(line2), "Species: %u (%u%%)", species, spec_conf);
    snprintf(line3, sizeof(line3), "%s (%u%%)",
             disease_suspect ? "SUSPECT" : "Healthy", disease_conf);
    snprintf(line4, sizeof(line4), "Cloud analyzing...");
    oled_show_text(line1, line2, line3, line4);

    /* Send result to hub via WiFi (not Sub-GHz — scanner is WiFi only) */
    uint8_t flags = 0;
    if (disease_suspect) flags |= GP_ALERT_DISEASE_SUSPECT;

    /* In production: POST to cloud, which forwards to hub via MQTT */
    ESP_LOGI(TAG, "Scan complete: tag=%u species=%u conf=%u disease=%s",
             current_tag_id, species, spec_conf,
             disease_suspect ? "SUSPECT" : "healthy");
}

/* ---- Button handling ---- */
static void handle_buttons(void)
{
    static uint8_t last_cap = 1, last_mode = 1, last_id = 1;

    uint8_t cap = gpio_get_level(PIN_BUTTON_CAPTURE);
    uint8_t mode = gpio_get_level(PIN_BUTTON_MODE);
    uint8_t id = gpio_get_level(PIN_BUTTON_ID);

    if (!cap && last_cap) {
        do_capture_and_analyze();
    }
    if (!mode && last_mode) {
        capture_mode = (capture_mode + 1) % 4;
        const char *modes[] = {"White", "UV", "NIR", "Multi"};
        char buf[20];
        snprintf(buf, sizeof(buf), "Mode: %s", modes[capture_mode]);
        oled_show_text("GreenPulse Scanner", buf, "", "");
    }
    if (!id && last_id) {
        current_tag_id = (current_tag_id + 1) % 64;
        char buf[20];
        snprintf(buf, sizeof(buf), "Tag: %u", current_tag_id);
        oled_show_text("GreenPulse Scanner", buf, "", "");
    }

    last_cap = cap; last_mode = mode; last_id = id;
}

/* ---- Main ---- */
void app_main(void)
{
    /* Initialize GPIO */
    gpio_set_direction(PIN_LED_WHITE, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED_UV, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED_NIR, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BUTTON_CAPTURE, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_BUTTON_MODE, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_BUTTON_ID, GPIO_MODE_INPUT);
    gpio_set_level(PIN_LED_WHITE, 0);
    gpio_set_level(PIN_LED_UV, 0);
    gpio_set_level(PIN_LED_NIR, 0);

    oled_init();
    wifi_init_and_connect();

    oled_show_text("GreenPulse Scanner", "Ready", "Tag: 0", "");

    while (1) {
        handle_buttons();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}