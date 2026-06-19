/*
 * camera_main.c — PawSync Behavior Camera firmware (ESP32-S3, ESP-IDF)
 *
 * Runs on-device computer vision to classify pet behavior from OV5640
 * camera at 5fps, and vocalization classification from 6-mic I2S array.
 * All inference is on-device (privacy-first). Events (not video) are
 * sent to the hub/cloud via WiFi.
 *
 * Behavior classes (0-5):
 *   0=resting, 1=pacing, 2=vocalizing, 3=destructive, 4=elimination, 5=playing
 *
 * Vocalization classes (0-6):
 *   0=none, 1=pain, 2=anxiety, 3=alert, 4=play, 5=attention, 6=distress
 *
 * Separation anxiety episode: pacing + vocalizing + destruction >5 min
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/uart.h"
#include "paw_protocol.h"

static const char *TAG = "pawsync-cam";

/* ---- Pin definitions (ESP32-S3) ---- */
#define I2S_WS_PIN    4
#define I2S_BCK_PIN   5
#define I2S_DATA_PIN  6
#define IR_LED_PIN    38
#define SHUTTER_PIN   39
#define UART_TX_PIN   43
#define UART_RX_PIN   44

/* ---- Camera configuration ---- */
#define CAM_XCLK_FREQ  20000000  /* 20MHz */
#define CAM_WIDTH       320
#define CAM_HEIGHT      240
#define CAM_FPS         5

/* ---- Audio configuration ---- */
#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_WINDOW_S     2
#define AUDIO_WINDOW_SIZE  (AUDIO_SAMPLE_RATE * AUDIO_WINDOW_S)  /* 32000 */

/* ---- Anxiety episode detection ---- */
#define ANXIETY_EPISODE_DURATION_S  300  /* 5 min */
#define ANXIETY_PACING_THRESHOLD    0.5f /* 50% of time pacing */
#define ANXIETY_VOCAL_THRESHOLD     3    /* 3 vocalizations in window */

/* ---- Globals ---- */
static bool privacy_shutter_closed = false;
static bool wifi_connected = false;
static uint8_t current_behavior = 0;
static uint8_t current_vocalization = 0;
static uint8_t current_confidence = 0;

/* Anxiety tracking */
static uint32_t anxiety_start_time = 0;
static uint32_t pacing_count = 0;
static uint32_t vocal_count = 0;
static bool anxiety_episode_active = false;

/* ---- Behavior model (declared in behavior_model.c) ---- */
extern void behavior_classify_frame(const uint8_t *frame, int w, int h,
                                    uint8_t *class_out, uint8_t *conf_out);

/* ---- Vocalization classifier (declared in vocalization.c) ---- */
extern void vocalization_classify(const int16_t *audio, int n,
                                  uint8_t *class_out, uint8_t *conf_out);

/* ---- Privacy shutter check (declared in privacy.c) ---- */
extern bool privacy_is_shutter_closed(void);
extern void privacy_init(void);

/* ---- WiFi event handler ---- */
static void wifi_event_handler(void *ctx, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ESP_LOGI(TAG, "WiFi connected");
    }
}

static void wifi_init_sta(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_sta_wifi();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               wifi_event_handler, NULL);
    /* SSID/password from NVS or hardcoded for dev */
    wifi_config_t wifi_cfg = {
        .sta = { .ssid = "PawSync", .password = "pawsync123" },
    };
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_connect();
}

/* ---- I2S mic array initialization ---- */
static i2s_chan_handle_t rx_handle;

static void mic_array_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                            I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .bclk = I2S_BCK_PIN,
            .ws = I2S_WS_PIN,
            .dout = -1,
            .din = I2S_DATA_PIN,
        },
    };
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);
}

static int mic_read_audio(int16_t *buf, int max_samples)
{
    size_t bytes_read = 0;
    i2s_channel_read(rx_handle, buf, max_samples * sizeof(int16_t),
                     &bytes_read, portMAX_DELAY);
    return bytes_read / sizeof(int16_t);
}

/* ---- Camera frame capture (stub — in production: esp_camera) ---- */
static uint8_t frame_buffer[CAM_WIDTH * CAM_HEIGHT * 2];  /* RGB565 */

static int camera_capture(uint8_t *buf, int max_size)
{
    /* In production: esp_camera_fb_get() returns a frame buffer */
    /* For now: stub returns noise */
    memset(buf, 0, max_size);
    return CAM_WIDTH * CAM_HEIGHT * 2;
}

/* ---- Send behavior event via WiFi (MQTT) ---- */
static void send_behavior_event(uint8_t behavior, uint8_t vocalization,
                                uint8_t confidence, uint32_t clip_ref)
{
    if (!wifi_connected) return;

    paw_behavior_payload_t bp = {0};
    bp.type            = PAW_MSG_BEHAVIOR;
    bp.node_id         = PAW_NODE_ID_CAMERA;
    bp.behavior_class  = behavior;
    bp.vocalization    = vocalization;
    bp.confidence      = confidence;
    if (anxiety_episode_active)
        bp.flags |= PAW_ALERT_ANXIETY;
    bp.duration_s      = 0;  /* set by caller */
    bp.clip_ref        = clip_ref;
    paw_pack_crc(&bp, sizeof(bp) - 2);

    /* In production: publish via MQTT */
    ESP_LOGI(TAG, "Behavior: %s, Vocal: %s, conf=%d%%",
             PAW_BEHAVIOR_NAMES[behavior],
             PAW_VOCAL_NAMES[vocalization],
             confidence);
}

/* ---- Anxiety episode detection ---- */
static void check_anxiety_episode(uint8_t behavior, uint8_t vocalization,
                                    uint32_t timestamp_s)
{
    if (behavior == 1) pacing_count++;      /* pacing */
    if (vocalization > 0) vocal_count++;    /* any vocalization */
    if (behavior == 3) {                     /* destructive */
        vocal_count += 2;  /* destruction is significant */
    }

    /* Check for anxiety episode start */
    if (!anxiety_episode_active) {
        if (pacing_count > 50 || vocal_count > ANXIETY_VOCAL_THRESHOLD) {
            anxiety_start_time = timestamp_s;
            anxiety_episode_active = true;
            ESP_LOGW(TAG, "Anxiety episode STARTED (pacing=%lu, vocal=%lu)",
                     pacing_count, vocal_count);
            send_behavior_event(behavior, vocalization, 90, 0);
        }
    } else {
        /* Check for anxiety episode end (>2 min without pacing/vocalizing) */
        if (pacing_count == 0 && vocal_count == 0) {
            uint32_t duration = timestamp_s - anxiety_start_time;
            if (duration > 120) {  /* >2 min calm → episode ended */
                anxiety_episode_active = false;
                ESP_LOGI(TAG, "Anxiety episode ENDED (duration=%lus)", duration);
                pacing_count = 0;
                vocal_count = 0;
            }
        }
        /* Auto-trigger enrichment on sustained anxiety */
        if (timestamp_s - anxiety_start_time > ANXIETY_EPISODE_DURATION_S) {
            ESP_LOGW(TAG, "Anxiety sustained >5min — requesting enrichment");
            /* Send alert to hub for enrichment trigger */
            paw_alert_payload_t ap = {0};
            ap.type = PAW_MSG_ALERT;
            ap.node_id = PAW_NODE_ID_CAMERA;
            ap.flags = PAW_ALERT_ANXIETY;
            ap.value = (uint16_t)(timestamp_s - anxiety_start_time);
            paw_pack_crc(&ap, sizeof(ap) - 2);
            /* mesh_send or WiFi MQTT publish */
        }
    }

    /* Reset per-window counters (called every 5s) */
    pacing_count = pacing_count > 0 ? pacing_count - 1 : 0;
}

/* ---- IR LED control for night vision ---- */
static void ir_led_set(bool on)
{
    gpio_set_level(IR_LED_PIN, on ? 1 : 0);
}

static bool is_night(void)
{
    /* Simple: check ambient light via camera exposure
     * In production: use APDS-9301 ambient light sensor */
    return false;  /* stub */
}

/* ---- Main tasks ---- */
static void camera_task(void *arg)
{
    uint32_t frame_count = 0;
    while (1) {
        if (privacy_is_shutter_closed()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* Night vision */
        if (is_night()) ir_led_set(true);
        else ir_led_set(false);

        /* Capture frame */
        int frame_size = camera_capture(frame_buffer, sizeof(frame_buffer));
        if (frame_size > 0) {
            /* Run behavior classification */
            uint8_t behavior = 0, confidence = 0;
            behavior_classify_frame(frame_buffer, CAM_WIDTH, CAM_HEIGHT,
                                     &behavior, &confidence);
            current_behavior = behavior;
            current_confidence = confidence;

            /* Check anxiety */
            uint32_t now_s = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
            check_anxiety_episode(behavior, current_vocalization, now_s);

            /* Send event every 30s or on anxiety episode */
            if (frame_count % 150 == 0 || anxiety_episode_active) {
                send_behavior_event(behavior, current_vocalization,
                                    confidence, 0);
            }
            frame_count++;
        }
        vTaskDelay(pdMS_TO_TICKS(1000 / CAM_FPS));  /* 5fps */
    }
}

static void audio_task(void *arg)
{
    static int16_t audio_buf[AUDIO_WINDOW_SIZE];
    while (1) {
        if (privacy_is_shutter_closed()) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int n = mic_read_audio(audio_buf, AUDIO_WINDOW_SIZE);
        if (n > 0) {
            /* Run vocalization classification */
            uint8_t vocal_class = 0, conf = 0;
            vocalization_classify(audio_buf, n, &vocal_class, &conf);
            current_vocalization = vocal_class;

            /* Alert on pain/distress vocalizations */
            if (vocal_class == 1 || vocal_class == 6) {
                ESP_LOGW(TAG, "Pain/distress vocalization detected: %s",
                         PAW_VOCAL_NAMES[vocal_class]);
                paw_alert_payload_t ap = {0};
                ap.type = PAW_MSG_ALERT;
                ap.node_id = PAW_NODE_ID_CAMERA;
                ap.flags = PAW_ALERT_ANXIETY;
                ap.value = vocal_class;
                paw_pack_crc(&ap, sizeof(ap) - 2);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));  /* 10Hz polling */
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "PawSync Behavior Camera starting");

    /* Init GPIO */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << IR_LED_PIN) | (1ULL << SHUTTER_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(IR_LED_PIN, 0);

    /* Init privacy shutter */
    privacy_init();

    /* Init WiFi */
    wifi_init_sta();

    /* Init mic array */
    mic_array_init();

    /* Create tasks */
    xTaskCreate(camera_task, "camera", 8192, NULL, 5, NULL);
    xTaskCreate(audio_task, "audio", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "All tasks started");
}