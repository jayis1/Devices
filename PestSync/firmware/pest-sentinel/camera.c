/*
 * Pest Sentinel — Camera (OV2640)
 * firmware/pest-sentinel/camera.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/gpio.h"
#include <string.h>

#include "psp_protocol.h"

static const char *TAG = "CAMERA";

#define PIN_CAM_D0      4
#define PIN_CAM_VSYNC    5
#define PIN_CAM_HREF     6
#define PIN_CAM_PCLK     7
#define PIN_CAM_XCLK     8
#define PIN_CAM_SDA      9
#define PIN_CAM_SCL      10
#define PIN_CAM_D2       11
#define PIN_CAM_D3       12
#define PIN_CAM_D4       13
#define PIN_CAM_D5       14
#define PIN_CAM_D6       15
#define PIN_CAM_D7       16

#define PIN_IR_LED       41

extern bool g_ir_illumination_on;

/* Frame buffer shared between camera and CNN tasks */
static SemaphoreHandle_t frame_mutex;
static camera_fb_t *latest_frame = NULL;
static bool new_frame_available = false;

camera_fb_t *camera_get_frame(void)
{
    xSemaphoreTake(frame_mutex, portMAX_DELAY);
    camera_fb_t *fb = latest_frame;
    new_frame_available = false;
    xSemaphoreGive(frame_mutex);
    return fb;
}

bool camera_frame_available(void)
{
    return new_frame_available;
}

static esp_err_t init_camera(void)
{
    camera_config_t config = {
        .pin_d0       = PIN_CAM_D0,
        .pin_d1       = PIN_CAM_D2,
        .pin_d2       = PIN_CAM_D3,
        .pin_d3       = PIN_CAM_D4,
        .pin_d4       = PIN_CAM_D5,
        .pin_d5       = PIN_CAM_D6,
        .pin_d6       = PIN_CAM_D7,
        .pin_xclk     = PIN_CAM_XCLK,
        .pin_pclk     = PIN_CAM_PCLK,
        .pin_vsync    = PIN_CAM_VSYNC,
        .pin_href     = PIN_CAM_HREF,
        .pin_sccb_sda = PIN_CAM_SDA,
        .pin_sccb_scl = PIN_CAM_SCL,
        .xclk_freq_hz = 20000000,
        .frame_size   = FRAMESIZE_QVGA,    /* 320×240 */
        .pixel_format = PIXFORMAT_RGB565,  /* CNN input format */
        .fb_count     = 2,
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_LATEST,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Get sensor handle and configure */
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 0);
        s->set_contrast(s, 0);
        s->set_saturation(s, 0);
        /* Disable auto-gain for night mode consistency */
        s->set_gain_ctrl(s, 0);
    }

    ESP_LOGI(TAG, "OV2640 camera initialized (320×240 RGB565)");
    return ESP_OK;
}

void camera_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Camera task started");

    frame_mutex = xSemaphoreCreateMutex();

    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed — task exiting");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        /* Capture frame — this blocks until PIR has triggered (signaled via flag) */
        /* In production: PIR interrupt sets a flag, camera_task waits on it */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* Enable IR illumination for night capture */
        if (g_ir_illumination_on) {
            gpio_set_level(PIN_IR_LED, 1);
            vTaskDelay(pdMS_TO_TICKS(50)); /* warm up IR LEDs */
        }

        camera_fb_t *fb = esp_camera_fb_get();
        gpio_set_level(PIN_IR_LED, 0); /* IR off after capture */

        if (fb) {
            xSemaphoreTake(frame_mutex, portMAX_DELAY);
            if (latest_frame) esp_camera_fb_return(latest_frame);
            latest_frame = fb;
            new_frame_available = true;
            xSemaphoreGive(frame_mutex);

            /* Notify CNN task */
            xTaskNotifyGive(xTaskGetHandle("cnn"));
        }
    }
}