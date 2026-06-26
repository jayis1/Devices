/*
 * HiveSync — Entrance Monitor Firmware
 * MCU: ESP32-S3
 * Peripherals: OV5640 camera, IR LEDs, ICS-43434 mic, SHT40, CC1101
 * Runs: YOLOv8-nano for bee counting, Varroa mite detection
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "cc1101_esp.h"
#include "sht40_esp.h"
#include "ics43434_esp.h"
#include "hivesync_proto.h"
#include "bee_counter_model.h"   /* TFLite Micro model */
#include "varroa_model.h"         /* TFLite Micro model */

#define TAG "ENTRANCE_MONITOR"
#define NODE_TYPE NODE_ENTRANCE_MONITOR

/* Camera config for OV5640 */
#define CAM_XRES       160
#define CAM_YRES       160
#define CAM_FPS        10

/* TDMA timing */
#define TX_SLOT_US     200000   /* 200ms slot for transmit */
#define CAPTURE_INTERVAL_S  30  /* Capture every 30s */

typedef struct {
    uint16_t node_id;
    int bees_in;
    int bees_out;
    float mite_count_per_bee;
    int mite_class;          /* 0=none, 1=low, 2=moderate, 3=high */
    float entrance_temp_c;
    float entrance_hum_pct;
    float spectral_centroid_hz;
    float rssi_dbm;
} entrance_data_t;

static entrance_data_t g_data;
static cc1101_dev_t g_radio;
static sht40_dev_t g_sht;
static uint8_t g_tx_buf[256];

/* ---- IR LED Control ---- */
static void ir_leds_on(void) {
    gpio_set_level(GPIO_NUM_4, 1);  /* IR LED enable */
}

static void ir_leds_off(void) {
    gpio_set_level(GPIO_NUM_4, 0);
}

/* ---- Camera Init ---- */
static esp_err_t init_camera(void) {
    camera_config_t config = {
        .pin_pwdn  = GPIO_NUM_32,
        .pin_reset = GPIO_NUM_33,
        .pin_xclk  = GPIO_NUM_15,
        .pin_sccb_sda = GPIO_NUM_21,
        .pin_sccb_scl = GPIO_NUM_22,
        .pin_d7    = GPIO_NUM_19,
        .pin_d6    = GPIO_NUM_36,
        .pin_d5    = GPIO_NUM_18,
        .pin_d4    = GPIO_NUM_5,
        .pin_d3    = GPIO_NUM_34,
        .pin_d2    = GPIO_NUM_35,
        .pin_d1    = GPIO_NUM_39,
        .pin_d0    = GPIO_NUM_38,
        .pin_vsync = GPIO_NUM_25,
        .pin_href  = GPIO_NUM_23,
        .pin_pclk  = GPIO_NUM_22,
        .xclk_freq_hz = 20000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_RGB565,
        .frame_size   = FRAMESIZE_QQVGA,  /* 160x160 */
        .jpeg_quality = 12,
        .fb_count     = 2,
    };
    return esp_camera_init(&config);
}

/* ---- Bee Counting (YOLOv8-nano via TFLite) ---- */
static void count_bees(uint8_t *frame, int w, int h, int *in_count, int *out_count) {
    /* Direction determined by bee position relative to tunnel center line */
    bee_detection_t detections[MAX_DETECTIONS];
    int num_det = bee_counter_detect(frame, w, h, detections);

    *in_count = 0;
    *out_count = 0;
    for (int i = 0; i < num_det; i++) {
        if (detections[i].direction == BEE_DIR_IN) (*in_count)++;
        else if (detections[i].direction == BEE_DIR_OUT) (*out_count)++;
    }
}

/* ---- Varroa Detection ---- */
static void detect_varroa(uint8_t *frame, int w, int h, float *mites_per_bee, int *mite_class) {
    varroa_result_t result = varroa_detect(frame, w, h);
    *mites_per_bee = result.mites_per_bee;
    *mite_class = result.mite_class;  /* 0=none,1=low,2=moderate,3=high */
}

/* ---- Main Task ---- */
static void entrance_monitor_task(void *pvParameters) {
    /* Init SHT40 */
    sht40_init(&g_sht, I2C_NUM_0);

    /* Init CC1101 */
    cc1101_esp_init(&g_radio, SPI2_HOST, GPIO_NUM_18, GPIO_NUM_5);
    cc1101_set_frequency(&g_radio, 868000000);

    /* Init TFLite models */
    bee_counter_model_init();
    varroa_model_init();

    /* Beacon to gateway */
    hivesync_send_beacon(&g_radio, g_data.node_id, NODE_ENTRANCE_MONITOR);

    int64_t last_capture = 0;

    while (1) {
        int64_t now = esp_timer_get_time() / 1000; /* ms */
        if (now - last_capture < CAPTURE_INTERVAL_S * 1000) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        last_capture = now;

        /* Capture frame */
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            continue;
        }

        /* Check if IR mode needed (night) */
        float temp;
        sht40_read(&g_sht, &temp, NULL);
        g_data.entrance_temp_c = temp;
        /* If hour suggests night, enable IR LEDs */
        /* (Simplified — production would use RTC time) */
        int is_night = (temp < 18.0f); /* Rough proxy */
        if (is_night) ir_leds_on();

        /* Run bee counter */
        int bees_in, bees_out;
        count_bees(fb->buf, fb->width, fb->height, &bees_in, &bees_out);
        g_data.bees_in = bees_in;
        g_data.bees_out = bees_out;

        /* Run Varroa detector */
        detect_varroa(fb->buf, fb->width, fb->height,
                       &g_data.mite_count_per_bee, &g_data.mite_class);

        esp_camera_fb_return(fb);
        if (is_night) ir_leds_off();

        /* Pack & transmit */
        uint16_t len = hivesync_pack_entrance(g_tx_buf,
            g_data.node_id, &g_data);
        cc1101_tx_packet(&g_radio, g_tx_buf, len);

        ESP_LOGI(TAG, "Bees: in=%d out=%d Mites: %.1f/bbee class=%d",
                 bees_in, bees_out, g_data.mite_count_per_bee, g_data.mite_class);

        /* Check for gateway commands */
        uint8_t rx_buf[64];
        uint16_t rx_len;
        if (cc1101_rx_packet(&g_radio, rx_buf, &rx_len, 100) == CC1101_OK) {
            hivesync_handle_command(rx_buf, rx_len);
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "HiveSync Entrance Monitor v1.0 starting...");

    /* Init GPIO for IR LEDs */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    /* Init camera */
    if (init_camera() != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed!");
    }

    /* Get node ID from EFUSE */
    g_data.node_id = 0x0010;  /* Default, override from EFUSE */

    xTaskCreatePinnedToCore(entrance_monitor_task, "monitor", 8192, NULL, 5, NULL, 1);
}