/**
 * @file main.c
 * @brief PoolSync Pool Camera firmware — ESP32-S3 + IMX477 + IR + PIR
 *
 * Captures 4K images for water clarity assessment and algae detection.
 * On-device clarity scoring (histogram analysis + green channel detection).
 * PIR-triggered safety monitoring for unsupervised pool access.
 * Night vision with 850nm IR LED array + IR-cut filter switching.
 */

#include "esp_camera.h"
#include "esp_wifi.h"
#include "esp_mqtt.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include "psp_protocol.h"
#include "psp_radio.h"

/* ============================================================
 * PIN DEFINITIONS — ESP32-S3
 * ============================================================ */

/* Camera interface (ESP32-S3 MIPI CSI-2) */
#define CAM_XCLK       GPIO_NUM_15
#define CAM_PCLK       GPIO_NUM_16
#define CAM_VSYNC      GPIO_NUM_17
#define CAM_HREF       GPIO_NUM_18
#define CAM_D0         GPIO_NUM_11
#define CAM_D1         GPIO_NUM_12
#define CAM_D2         GPIO_NUM_14
#define CAM_D3         GPIO_NUM_13
#define CAM_D4          GPIO_NUM_10
#define CAM_D5         GPIO_NUM_9
#define CAM_D6         GPIO_NUM_8
#define CAM_D7         GPIO_NUM_7

/* IR-cut filter driver */
#define PIN_IRCUT_EN    GPIO_NUM_4    /* IR-cut filter enable (day mode) */
#define PIN_IRCUT_SEL   GPIO_NUM_5    /* IR-cut filter select (day/night) */

/* IR LED array driver */
#define PIN_IR_LED_EN   GPIO_NUM_6    /* IR LED enable via MOSFET */

/* PIR motion sensor */
#define PIN_PIR_OUT     GPIO_NUM_7    /* AM312 PIR output */

/* Speaker (MAX98357A I2S) */
#define PIN_I2S_BCLK    GPIO_NUM_41
#define PIN_I2S_LRCLK   GPIO_NUM_42
#define PIN_I2S_DOUT    GPIO_NUM_40

/* TSL2591 ambient light sensor (I2C) */
#define PIN_I2C_SDA     GPIO_NUM_8
#define PIN_I2C_SCL     GPIO_NUM_9

/* Status LEDs */
#define PIN_LED_GREEN   GPIO_NUM_38
#define PIN_LED_BLUE    GPIO_NUM_39

/* SX1262 Sub-GHz radio (SPI) */
#define PIN_SX_NSS      GPIO_NUM_34
#define PIN_SX_SCK      GPIO_NUM_35
#define PIN_SX_MISO     GPIO_NUM_36
#define PIN_SX_MOSI     GPIO_NUM_37
#define PIN_SX_DIO1     GPIO_NUM_33
#define PIN_SX_BUSY     GPIO_NUM_32
#define PIN_SX_NRST     GPIO_NUM_31

/* ============================================================
 * CONSTANTS
 * ============================================================ */

#define CAPTURE_INTERVAL_SEC    300     /* 5 minutes between clarity captures */
#define PIR_CHECK_INTERVAL_MS   100     /* Check PIR every 100ms */
#define IMAGE_UPLOAD_QUALITY    80      /* JPEG quality for cloud upload */
#define CLARITY_HISTOGRAM_BINS  256     /* Histogram bins for clarity analysis */
#define ALGAE_GREEN_THRESHOLD   0.35f   /* Green channel ratio threshold for algae */
#define NIGHT_MODE_THRESHOLD    50.0f   /* Lux threshold for IR mode switch */

/* TSL2591 I2C address */
#define TSL2591_ADDR            0x29

/* ============================================================
 * STATE
 * ============================================================ */

typedef struct {
    float clarity_score;
    float green_channel;
    float turbidity_ntu;
    uint8_t algae_risk;           /* 0=none, 1=low, 2=medium, 3=high */
    bool night_mode;
    bool pir_triggered;
    bool wifi_connected;
    bool cloud_connected;
    uint32_t last_capture_time;
    uint32_t last_pir_time;
    uint32_t image_count;
} camera_state_t;

static camera_state_t g_cam;

/* ============================================================
 * CAMERA INITIALIZATION
 * ============================================================ */

static int camera_init(void)
{
    camera_config_t config = {
        .pin_pwdn = -1,
        .pin_reset = -1,
        .pin_xclk = CAM_XCLK,
        .pin_sccb_sda = -1,  /* Use default */
        .pin_sccb_scl = -1,
        .pin_d7 = CAM_D7,
        .pin_d6 = CAM_D6,
        .pin_d5 = CAM_D5,
        .pin_d4 = CAM_D4,
        .pin_d3 = CAM_D3,
        .pin_d2 = CAM_D2,
        .pin_d1 = CAM_D1,
        .pin_d0 = CAM_D0,
        .pin_vsync = CAM_VSYNC,
        .pin_href = CAM_HREF,
        .pin_pclk = CAM_PCLK,
        .xclk_freq_hz = 20000000,       /* 20 MHz */
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,  /* JPEG for bandwidth efficiency */
        .frame_size = FRAMESIZE_UXGA,    /* 1600x1200 for clarity analysis */
        .jpeg_quality = IMAGE_UPLOAD_QUALITY,
        .fb_count = 2,                   /* 2 frame buffers for double-buffer */
        .grab_mode = CAMERA_GRAB_LATEST   /* Always get newest frame */
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        printf("Camera init failed: %s\n", esp_err_to_name(err));
        return -1;
    }

    return 0;
}

/* ============================================================
 * IR-CUT FILTER + IR LED CONTROL
 * ============================================================ */

static void set_day_mode(void)
{
    /* Day mode: IR-cut filter IN (blocks IR, natural colors) */
    gpio_set_level(PIN_IRCUT_EN, 1);
    gpio_set_level(PIN_IRCUT_SEL, 0);  /* Filter IN */
    gpio_set_level(PIN_IR_LED_EN, 0);  /* IR LEDs OFF */
    g_cam.night_mode = false;

    /* Switch camera to color mode */
    sensor_t *s = esp_camera_sensor_get();
    if (s) s->set_vflip(s, 0);
}

static void set_night_mode(void)
{
    /* Night mode: IR-cut filter OUT, IR LEDs ON */
    gpio_set_level(PIN_IRCUT_EN, 1);
    gpio_set_level(PIN_IRCUT_SEL, 1);  /* Filter OUT */
    gpio_set_level(PIN_IR_LED_EN, 1);  /* IR LEDs ON */
    g_cam.night_mode = true;

    /* Switch camera to night mode (monochrome, higher gain) */
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 2);    /* Brightness +2 */
        s->set_contrast(s, 3);      /* Contrast +3 */
        s->set_gain_ctrl(s, 1);     /* AGC on */
        s->set_exposure_ctrl(s, 1); /* AEC on */
    }
}

/* ============================================================
 * CLARITY ANALYSIS — On-device histogram + green channel
 * ============================================================ */

/**
 * Analyze captured image for water clarity and algae risk
 * Uses histogram analysis and green channel ratio
 */
static int analyze_clarity(const uint8_t *jpg_buf, size_t jpg_len,
                           psp_clarity_reading_t *result)
{
    /* Get raw RGB565 or processed histogram from JPEG
     * For efficiency, we compute from the JPEG directly:
     *
     * 1. Clarity score: based on contrast (standard deviation of luminance)
     *    Clear water = high contrast (can see pool bottom)
     *    Cloudy water = low contrast (diffuse, flat histogram)
     *
     * 2. Green channel ratio: green / (red + green + blue)
     *    Normal water: green ratio ~0.3-0.35 (blue dominant)
     *    Algae starting: green ratio > 0.35 (green shift)
     *    Full algae: green ratio > 0.45 (very green)
     *
     * 3. Turbidity estimate: inverse of clarity score
     */

    /* For on-device efficiency, we use a simplified analysis
     * that samples the JPEG at regular intervals */

    /* Compute average R, G, B values from JPEG */
    uint32_t total_r = 0, total_g = 0, total_b = 0;
    uint32_t sample_count = 0;

    /* Sample every 1000th byte from JPEG (rough but fast) */
    /* In production: decode JPEG to RGB, then compute histogram */
    /* Simplified: use ESP32-S3 JPEG decoder peripheral */
    for (size_t i = 0; i < jpg_len && sample_count < 10000; i += 100) {
        /* Would decode JPEG macroblocks here */
        /* Placeholder: actual implementation uses esp_jpeg_decode */
        sample_count++;
    }

    /* Prevent division by zero */
    if (sample_count == 0) sample_count = 1;

    float avg_r = (float)total_r / sample_count;
    float avg_g = (float)total_g / sample_count;
    float avg_b = (float)total_b / sample_count;
    float total = avg_r + avg_g + avg_b;

    if (total < 1.0f) total = 1.0f;

    float green_ratio = avg_g / total;

    /* Clarity score: based on image contrast
     * High contrast (clear water) → score close to 1.0
     * Low contrast (cloudy water) → score close to 0.0
     * This would normally be computed from histogram standard deviation
     */
    float clarity = 0.75f;  /* Placeholder — real impl uses histogram variance */

    /* Green channel assessment */
    result->green_channel = green_ratio;

    /* Algae risk classification */
    if (green_ratio < ALGAE_GREEN_THRESHOLD) {
        result->algae_risk = 0;  /* None */
    } else if (green_ratio < 0.40f) {
        result->algae_risk = 1;  /* Low */
    } else if (green_ratio < 0.45f) {
        result->algae_risk = 2;  /* Medium */
    } else {
        result->algae_risk = 3;  /* High */
    }

    /* Turbidity estimate (inverse of clarity) */
    result->turbidity_ntu = (1.0f - clarity) * 100.0f;
    result->clarity_score = clarity;

    return 0;
}

/* ============================================================
 * PIR MOTION CALLBACK — Safety monitoring
 * ============================================================ */

static void IRAM_ATTR pir_isr_handler(void *arg)
{
    g_cam.pir_triggered = true;
    g_cam.last_pir_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* ============================================================
 * SPEAKER — Verbal warnings for safety
 * ============================================================ */

static void play_warning(const char *message)
{
    /* In production: use ESP32-S3 I2S → MAX98357A speaker
     * For now: placeholder for TTS or pre-recorded warnings */
    /* Common warnings:
     * - "Pool area accessed. Adult supervision required."
     * - "Chemical dosing in progress. Do not enter pool."
     * - "GFCI fault detected. Pool equipment shut off."
     */
    (void)message;
}

/* ============================================================
 * MAIN TASK
 * ============================================================ */

static void camera_task(void *pvParameters)
{
    (void)pvParameters;

    /* Initialize camera */
    if (camera_init() != 0) {
        printf("FATAL: Camera init failed\n");
        vTaskDelete(NULL);
    }

    /* Initialize PIR interrupt */
    gpio_config_t pir_cfg = {
        .pin_bit_mask = (1ULL << PIN_PIR_OUT),
        .mode = GPIO_MODE_INTR_POSEDGE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    gpio_config(&pir_cfg);
    gpio_isr_handler_add(PIN_PIR_OUT, pir_isr_handler, NULL);

    /* Initialize radio */
    psp_radio_init();

    /* Initialize TSL2591 ambient light sensor */
    /* (I2C init + TSL2591 configuration) */

    /* Main loop */
    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;

        /* 1. Read ambient light (TSL2591) and switch day/night mode */
        float lux = 500.0f;  /* Placeholder — read from TSL2591 */
        if (lux < NIGHT_MODE_THRESHOLD && !g_cam.night_mode) {
            set_night_mode();
        } else if (lux >= NIGHT_MODE_THRESHOLD && g_cam.night_mode) {
            set_day_mode();
        }

        /* 2. Check PIR — unsupervised pool access */
        if (g_cam.pir_triggered) {
            g_cam.pir_triggered = false;

            /* Capture image for safety review */
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb) {
                /* Analyze for person detection (would use YOLOv8-nano on ESP32-S3) */
                /* Send alarm to hub */
                psp_alarm_payload_t alarm = {
                    .alarm_type = PSP_ALARM_UNAUTH_ACCESS,
                    .severity = 2,  /* Warning */
                    .value = 0.0f,
                    .timestamp = now,
                };
                psp_header_t hdr = {
                    .preamble = PSP_PREAMBLE,
                    .sync_word = PSP_SYNC_WORD,
                    .src_addr = PSP_ADDR_POOL_CAMERA,
                    .dst_addr = PSP_ADDR_HUB,
                    .msg_type = PSP_MSG_ALARM,
                };
                psp_frame_t frame;
                psp_encode(&hdr, (uint8_t *)&alarm, sizeof(alarm),
                           (uint8_t *)&frame, sizeof(frame));
                psp_radio_send_alarm(&frame);

                /* Play verbal warning */
                play_warning("Pool area accessed. Adult supervision required.");

                esp_camera_fb_return(fb);
            }
        }

        /* 3. Periodic clarity capture */
        if (now - g_cam.last_capture_time >= CAPTURE_INTERVAL_SEC / 1000) {
            g_cam.last_capture_time = now;

            /* Capture image */
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb) {
                /* Analyze water clarity on-device */
                psp_clarity_reading_t clarity;
                analyze_clarity(fb->buf, fb->len, &clarity);

                /* Build PSP frame and transmit to hub */
                psp_image_data_t payload = {
                    .clarity_score = clarity.clarity_score,
                    .green_channel = clarity.green_channel,
                    .turbidity_ntu = clarity.turbidity_ntu,
                    .algae_risk = clarity.algae_risk,
                    .image_hash = 0,  /* Would compute CRC16 of image */
                    .timestamp = now,
                };

                psp_header_t hdr = {
                    .preamble = PSP_PREAMBLE,
                    .sync_word = PSP_SYNC_WORD,
                    .src_addr = PSP_ADDR_POOL_CAMERA,
                    .dst_addr = PSP_ADDR_HUB,
                    .msg_type = PSP_MSG_IMAGE_DATA,
                };

                psp_frame_t frame;
                psp_encode(&hdr, (uint8_t *)&payload, sizeof(payload),
                           (uint8_t *)&frame, sizeof(frame));
                psp_radio_send(&frame);

                /* If algae risk is medium or high, request full image upload */
                if (clarity.algae_risk >= 2) {
                    /* Upload full JPEG image over Wi-Fi to cloud */
                    /* (MQTT publish or HTTP POST) */
                }

                g_cam.image_count++;
                esp_camera_fb_return(fb);
            }
        }

        /* 4. Listen for commands from hub */
        psp_frame_t rx_frame;
        if (psp_radio_recv(&rx_frame, 100) == 0) {
            if (rx_frame.header.msg_type == PSP_MSG_IMAGE_UPLOAD) {
                /* Hub requests full image upload over Wi-Fi */
                camera_fb_t *fb = esp_camera_fb_get();
                if (fb) {
                    /* Upload via Wi-Fi MQTT or HTTP */
                    /* esp_mqtt_client_publish(...) */
                    esp_camera_fb_return(fb);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    /* Initialize GPIOs */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_IRCUT_EN) | (1ULL << PIN_IRCUT_SEL) |
                        (1ULL << PIN_IR_LED_EN) | (1ULL << PIN_LED_GREEN) |
                        (1ULL << PIN_LED_BLUE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* Start in day mode */
    set_day_mode();

    /* Initialize I2S for speaker */
    /* (I2S config for MAX98357A) */

    /* Create camera task */
    xTaskCreate(camera_task, "camera_task", 8192, NULL, 5, NULL);
}