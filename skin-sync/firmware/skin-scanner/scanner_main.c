/*
 * scanner_main.c — SkinSync Skin Scanner Node firmware (ESP32-S3, ESP-IDF)
 *
 * Captures multispectral skin images (white + UV + NIR + polarized, 4 shots
 * in 3 seconds), runs on-device condition CNN (25 conditions, TFLite Micro)
 * + melanoma ABCDE pre-screen, tracks lesions with IMU-guided angle
 * reproduction, uploads to cloud for high-res analysis, and displays
 * results on the SH1106 OLED.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "skin_protocol.h"

static const char *TAG = "skin_scanner";

/* ---- Hardware pins ---- */
#define PIN_BTN_CAPTURE   0
#define PIN_BTN_MODE      1
#define PIN_BTN_LESION    2
#define PIN_BTN_IDENTIFY  3
#define PIN_LED_WHITE     4
#define PIN_LED_UV        5
#define PIN_LED_NIR       6
#define PIN_LED_POLARIZED 7
#define PIN_OLED_SDA      8
#define PIN_OLED_SCL      9
#define PIN_IMU_SDA       10
#define PIN_IMU_SCL       11

/* ---- Camera config (OV5640 on ESP32-S3 DVP bus) ---- */
static esp_err_t init_camera(void)
{
    camera_config_t config = {
        .pin_d0       = 21,
        .pin_d1       = 22,
        .pin_d2       = 23,
        .pin_d3       = 24,
        .pin_d4       = 25,
        .pin_d5       = 26,
        .pin_d6       = 27,
        .pin_d7       = 28,
        .pin_xclk     = 34,
        .pin_pclk     = 29,
        .pin_vsync    = 30,
        .pin_href     = 31,
        .pin_sccb_sda = 33,
        .pin_sccb_scl = 32,
        .pin_pwdn     = 35,
        .pin_reset    = 36,
        .xclk_freq_hz = 20000000,
        .frame_size   = FRAMESIZE_UXGA,   /* 1600x1200 */
        .pixel_format = PIXFORMAT_JPEG,
        .jpeg_quality = 10,
        .fb_count     = 2,
    };
    return esp_camera_init(&config);
}

/* ---- LED ring control ---- */
typedef enum {
    LED_MODE_WHITE = 0,
    LED_MODE_UV,
    LED_MODE_NIR,
    LED_MODE_POLARIZED,
    LED_MODE_COUNT
} led_mode_t;

static const char *mode_names[] = {"White", "UV 365nm", "NIR 850nm", "Polarized"};
static led_mode_t current_mode = LED_MODE_WHITE;

static void led_set(led_mode_t mode)
{
    gpio_set_level(PIN_LED_WHITE,     mode == LED_MODE_WHITE     ? 1 : 0);
    gpio_set_level(PIN_LED_UV,        mode == LED_MODE_UV        ? 1 : 0);
    gpio_set_level(PIN_LED_NIR,       mode == LED_MODE_NIR       ? 1 : 0);
    gpio_set_level(PIN_LED_POLARIZED, mode == LED_MODE_POLARIZED ? 1 : 0);
}

static void led_all_off(void)
{
    gpio_set_level(PIN_LED_WHITE, 0);
    gpio_set_level(PIN_LED_UV, 0);
    gpio_set_level(PIN_LED_NIR, 0);
    gpio_set_level(PIN_LED_POLARIZED, 0);
}

/* ---- SH1106 OLED display (simplified) ---- */
static void oled_init(void) { /* I2C init + SH1106 init sequence */ }
static void oled_clear(void) { /* clear display */ }
static void oled_show_text(int line, const char *text) { (void)line; (void)text; }
static void oled_show_result(const char *condition, uint8_t conf, uint8_t abcde)
{
    char buf[32];
    oled_clear();
    oled_show_text(0, "Skin Scan Result");
    snprintf(buf, sizeof(buf), "Cond: %s", condition);
    oled_show_text(1, buf);
    snprintf(buf, sizeof(buf), "Conf: %u%%", conf);
    oled_show_text(2, buf);
    if (abcde > 0) {
        snprintf(buf, sizeof(buf), "ABCDE: %u/100", abcde);
        oled_show_text(3, buf);
        if (abcde > 50)
            oled_show_text(4, "SEE DERMATOLOGIST");
    }
}

/* ---- LSM6DSL IMU for angle guidance (simplified) ---- */
typedef struct { float roll; float pitch; } imu_angle_t;

static void imu_init(void) { /* I2C init + LSM6DSL init */ }
static imu_angle_t imu_read_angles(void)
{
    /* In production: read accel + gyro, compute roll/pitch.
     * Used for lesion tracking: reproduce same angle for comparison scans. */
    imu_angle_t a = { 0.0f, 0.0f };
    return a;
}

/* ---- TFLite Micro model interface (stub) ---- */
typedef struct { void *handle; } TFLiteModel;

extern const unsigned char *condition_model_data;
extern unsigned int condition_model_len;
extern const unsigned char *abcde_model_data;
extern unsigned int abcde_model_len;

TFLiteModel *tflm_load(const unsigned char *data, unsigned int len)
{
    (void)data; (void)len;
    return (TFLiteModel *)1;
}

typedef struct {
    uint8_t condition_class;
    uint8_t confidence;
    uint8_t abcde_score;
    uint8_t skin_age;
} scan_result_t;

static scan_result_t run_inference(camera_fb_t *fb_white,
                                   camera_fb_t *fb_uv,
                                   camera_fb_t *fb_nir,
                                   camera_fb_t *fb_polar)
{
    scan_result_t r = { 0, 0, 0, 0 };

    /* In production:
     * 1. Preprocess: resize each frame to 224x224, normalize
     * 2. Stack 4 channels: [white, UV, NIR, polarized]
     * 3. Run condition CNN (25 classes) → condition_class + confidence
     * 4. Run ABCDE detector on white frame (lesion analysis)
     * 5. Run skin age regressor on NIR + polarized frames
     *
     * Condition CNN: EfficientNet-Lite (int8, ~4MB on flash)
     * ABCDE: YOLOv8-nano lesion detect + custom CNN ABCDE scoring
     * Skin age: MobileNetV3 regressor
     */

    /* Heuristic stub: */
    r.condition_class = SS_COND_NORMAL;
    r.confidence = 85;
    r.abcde_score = 0;
    r.skin_age = 35;

    return r;
}

/* ---- Capture multispectral image set ---- */
typedef struct {
    camera_fb_t *white;
    camera_fb_t *uv;
    camera_fb_t *nir;
    camera_fb_t *polar;
    imu_angle_t angle;
    uint32_t timestamp;
} multispectral_set_t;

static multispectral_set_t capture_multispectral(void)
{
    multispectral_set_t set;
    memset(&set, 0, sizeof(set));
    set.timestamp = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;

    /* Capture 4 images with different LED modes (3 seconds total) */

    /* 1. White light (surface texture, pores) */
    led_set(LED_MODE_WHITE);
    vTaskDelay(pdMS_TO_TICKS(200));
    set.white = esp_camera_fb_get();
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 2. UV 365nm (bacterial fluorescence) */
    led_set(LED_MODE_UV);
    vTaskDelay(pdMS_TO_TICKS(200));
    set.uv = esp_camera_fb_get();
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 3. NIR 850nm (sub-surface inflammation, moisture) */
    led_set(LED_MODE_NIR);
    vTaskDelay(pdMS_TO_TICKS(200));
    set.nir = esp_camera_fb_get();
    vTaskDelay(pdMS_TO_TICKS(100));

    /* 4. Polarized white (true skin tone, lesion borders) */
    led_set(LED_MODE_POLARIZED);
    vTaskDelay(pdMS_TO_TICKS(200));
    set.polar = esp_camera_fb_get();

    led_all_off();

    /* Record IMU angle for lesion tracking */
    set.angle = imu_read_angles();

    return set;
}

static void free_multispectral(multispectral_set_t *set)
{
    if (set->white)  esp_camera_fb_return(set->white);
    if (set->uv)     esp_camera_fb_return(set->uv);
    if (set->nir)    esp_camera_fb_return(set->nir);
    if (set->polar)  esp_camera_fb_return(set->polar);
}

/* ---- Save to SD card ---- */
static void save_to_sd(multispectral_set_t *set, scan_result_t *result,
                       uint8_t body_location, uint16_t lesion_id)
{
    /* In production: mount SD via SPI, write 4 JPEGs + metadata JSON:
     *   /scans/20260622_143022_white.jpg
     *   /scans/20260622_143022_uv.jpg
     *   /scans/20260622_143022_nir.jpg
     *   /scans/20260622_143022_polar.jpg
     *   /scans/20260622_143022_meta.json
     * Metadata: timestamp, body_location, lesion_id, IMU angle, results */
    ESP_LOGI(TAG, "Saved scan to SD (loc=%u lesion=%u)", body_location, lesion_id);
}

/* ---- Upload to cloud via WiFi ---- */
static void upload_to_cloud(multispectral_set_t *set, scan_result_t *result)
{
    /* In production: POST 4 JPEGs + metadata to cloud API endpoint.
     * Cloud runs high-res condition CNN + dermatologist report generator. */
    ESP_LOGI(TAG, "Uploading multispectral set to cloud...");
    /* esp_http_client_post(...) */
}

/* ---- WiFi init ---- */
static void wifi_init(void)
{
    /* In production: WiFi6 connect via ESP32-S3 netif + event group */
    ESP_LOGI(TAG, "WiFi initialized (stub)");
}

/* ---- Lesion tracking state ---- */
static uint16_t current_lesion_id = 0;  /* set by lesion mark button */
static uint8_t  current_body_location = 0; /* set by app or button cycle */

/* ---- Button task ---- */
static void button_task(void *arg)
{
    uint8_t prev_capture = 1, prev_mode = 1, prev_lesion = 1, prev_identify = 1;

    while (1) {
        uint8_t capture  = gpio_get_level(PIN_BTN_CAPTURE);
        uint8_t mode     = gpio_get_level(PIN_BTN_MODE);
        uint8_t lesion   = gpio_get_level(PIN_BTN_LESION);
        uint8_t identify = gpio_get_level(PIN_BTN_IDENTIFY);

        /* Capture button (falling edge) */
        if (!capture && prev_capture) {
            ESP_LOGI(TAG, "Capture initiated — mode: %s", mode_names[current_mode]);

            oled_show_text(0, "Capturing...");
            oled_show_text(1, mode_names[current_mode]);

            multispectral_set_t set = capture_multispectral();

            /* Run on-device inference */
            scan_result_t result = run_inference(set.white, set.uv,
                                                  set.nir, set.polar);

            /* Display result */
            const char *cond_name = ss_condition_name(result.condition_class);
            oled_show_result(cond_name, result.confidence, result.abcde_score);

            /* Save to SD */
            save_to_sd(&set, &result, current_body_location, current_lesion_id);

            /* Upload to cloud */
            upload_to_cloud(&set, &result);

            /* Send scan result to hub via WiFi (not Sub-GHz — image too large) */
            uint8_t flags = 0;
            if (result.abcde_score > 50)
                flags |= SS_ALERT_LESION;
            if (result.condition_class != SS_COND_NORMAL && result.confidence > 60)
                flags |= SS_ALERT_CONDITION;

            /* Build scan result payload and send via WiFi to hub */
            ss_scan_result_payload_t payload;
            memset(&payload, 0, sizeof(payload));
            payload.type = SS_MSG_SCAN_RESULT;
            payload.node_id = SS_NODE_ID_SCANNER;
            payload.flags = flags;
            payload.body_location = current_body_location;
            payload.condition_class = result.condition_class;
            payload.condition_conf = result.confidence;
            payload.abcde_score = result.abcde_score;
            payload.skin_age = result.skin_age;
            payload.lesion_id = current_lesion_id;
            ss_pack_crc(&payload, sizeof(payload) - 2);
            /* In production: send via WiFi/MQTT to hub */

            ESP_LOGI(TAG, "Scan complete: %s (%u%%) ABCDE=%u skin_age=%u",
                     cond_name, result.confidence, result.abcde_score,
                     result.skin_age);

            if (result.abcde_score > 50) {
                ESP_LOGW(TAG, "⚠ LESION SUSPECT — ABCDE score %u — see dermatologist",
                         result.abcde_score);
            }

            free_multispectral(&set);
        }

        /* Mode button (cycle through LED modes) */
        if (!mode && prev_mode) {
            current_mode = (current_mode + 1) % LED_MODE_COUNT;
            ESP_LOGI(TAG, "Mode: %s", mode_names[current_mode]);
            oled_show_text(0, "Mode:");
            oled_show_text(1, mode_names[current_mode]);
        }

        /* Lesion mark button */
        if (!lesion && prev_lesion) {
            current_lesion_id++;
            ESP_LOGI(TAG, "Lesion tracking: ID %u", current_lesion_id);
            char buf[32];
            snprintf(buf, sizeof(buf), "Lesion ID: %u", current_lesion_id);
            oled_show_text(0, "Lesion Track");
            oled_show_text(1, buf);
        }

        /* Identify button (trigger full on-device CNN) */
        if (!identify && prev_identify) {
            ESP_LOGI(TAG, "Identify: full condition analysis");
            oled_show_text(0, "Analyzing...");
            /* In production: capture + run full CNN inference */
        }

        prev_capture = capture;
        prev_mode = mode;
        prev_lesion = lesion;
        prev_identify = identify;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "SkinSync Skin Scanner starting...");

    /* Initialize GPIO for LED ring drivers */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_LED_WHITE) | (1ULL << PIN_LED_UV) |
                        (1ULL << PIN_LED_NIR) | (1ULL << PIN_LED_POLARIZED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    /* Initialize button GPIOs */
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << PIN_BTN_CAPTURE) | (1ULL << PIN_BTN_MODE) |
                        (1ULL << PIN_BTN_LESION) | (1ULL << PIN_BTN_IDENTIFY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn_conf);

    led_all_off();

    /* Initialize OLED */
    oled_init();
    oled_clear();
    oled_show_text(0, "SkinSync Scanner");
    oled_show_text(1, "Ready");

    /* Initialize IMU */
    imu_init();

    /* Initialize camera */
    esp_err_t err = init_camera();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        oled_show_text(1, "Camera FAIL");
        return;
    }
    ESP_LOGI(TAG, "Camera initialized (OV5640 UXGA)");

    /* Initialize WiFi */
    wifi_init();

    /* Start button task */
    xTaskCreate(button_task, "button_task", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "Scanner ready. Press CAPTURE to scan.");
}