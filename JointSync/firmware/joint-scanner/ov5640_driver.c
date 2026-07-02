/**
 * JointSync Joint Scanner — OV5640 Camera Driver
 *
 * DVP (8-bit parallel) interface to OV5640 5MP camera.
 *
 * License: MIT
 */

#include "ov5640_driver.h"
#include "esp_log.h"
#include "esp_camera.h"

static const char *TAG = "ov5640";

static bool g_initialized = false;

esp_err_t ov5640_init(void)
{
    /* ESP32-S3 camera configuration using esp_camera component */
    camera_config_t config = {
        .pin_pwdn     = 18,   /* GPIO18 */
        .pin_reset    = 17,   /* GPIO17 */
        .pin_xclk     = 16,   /* GPIO16 */
        .pin_sscb_sda = 15,   /* GPIO15 (SCCB) */
        .pin_sscb_scl = 14,   /* GPIO14 (SCCB) */

        .pin_d7       = 3,
        .pin_d6       = 4,
        .pin_d5       = 5,
        .pin_d4       = 6,
        .pin_d3       = 7,
        .pin_d2       = 8,
        .pin_d1       = 9,
        .pin_d0       = 10,
        .pin_vsync    = 12,
        .pin_href     = 13,
        .pin_pclk    = 11,

        .xclk_freq_hz = 20000000,  /* 20 MHz */
        .ledc_timer   = LEDC_TIMER_1,
        .ledc_channel = LEDC_CHANNEL_1,

        .pixel_format  = PIXFORMAT_JPEG,
        .frame_size    = FRAMESIZE_QVGA,  /* 320×240 */
        .jpeg_quality  = 10,
        .fb_count      = 2,
        .fb_location   = CAMERA_FB_IN_PSRAM,
        .grab_mode     = CAMERA_GRAB_LATEST,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Get sensor handle and configure */
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        /* Configure for close-up joint imaging */
        s->set_brightness(s, 1);
        s->set_contrast(s, 1);
        s->set_saturation(s, 0);
        s->set_whitebal(s, 1);  /* Auto WB */
        s->set_gainceiling(s, GAINCEILING_4X);
    }

    g_initialized = true;
    ESP_LOGI(TAG, "OV5640 initialized (QVGA 320×240, JPEG, 20 MHz XCLK)");
    return ESP_OK;
}

esp_err_t ov5640_capture_qvga(uint8_t *buf, size_t buf_len)
{
    if (!g_initialized) return ESP_ERR_INVALID_STATE;

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL) {
        ESP_LOGE(TAG, "Camera capture failed");
        return ESP_FAIL;
    }

    /* Copy to buffer (up to buf_len bytes) */
    size_t copy_len = (fb->len < buf_len) ? fb->len : buf_len;
    memcpy(buf, fb->buf, copy_len);

    esp_camera_fb_return(fb);

    ESP_LOGD(TAG, "Captured %d bytes", copy_len);
    return ESP_OK;
}

esp_err_t ov5640_capture_raw(uint8_t *buf, size_t buf_len, size_t *actual_len)
{
    if (!g_initialized) return ESP_ERR_INVALID_STATE;

    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL) return ESP_FAIL;

    size_t copy_len = (fb->len < buf_len) ? fb->len : buf_len;
    memcpy(buf, fb->buf, copy_len);
    if (actual_len) *actual_len = copy_len;

    esp_camera_fb_return(fb);
    return ESP_OK;
}

void ov5640_set_exposure(int level)
{
    if (!g_initialized) return;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_exposure_ctrl(s, level);
    }
}