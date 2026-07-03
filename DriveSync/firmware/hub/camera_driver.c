/**
 * DriveSync Camera Driver — OV5640 + 940nm IR Implementation
 *
 * ESP32-S3 DVP parallel camera capture + IR LED control + I2S audio.
 * Uses esp_camera component (ESP-IDF) for OV5640, MAX98357A for audio.
 *
 * License: MIT
 */

#include "camera_driver.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include <string.h>

static const char *TAG = "drivesync_cam";

static camera_config_t s_camera_config;
static camera_frame_cb_t s_frame_cb = NULL;
static bool s_ir_enabled = true;
static bool s_capturing = false;

/* ── IR LED PWM (LEDC) ────────────────────────────────────────────── */

static void ir_led_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch_cfg = {
        .channel = LEDC_CHANNEL_0,
        .duty = 128,  /* 50% duty */
        .gpio_num = IR_LED_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_cfg);
}

static void ir_led_set(bool on)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? 200 : 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* ── I2S Audio (MAX98357A) ────────────────────────────────────────── */

static i2s_chan_handle_t s_tx_chan = NULL;

static void i2s_audio_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_SAMPLE_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .bclk = 39,
            .ws = 40,
            .dout = 42,
            .din = -1,
            .mclk = -1,
        },
    };
    i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    i2s_channel_enable(s_tx_chan);
}

/* Alert sound waveforms (pre-computed 16-bit PCM) */
static const int16_t alert_tones[][800] = {
    /* ALERT_LOW: short 880 Hz beep (0.5s @ 16kHz) */
    {0},
    /* ALERT_MODERATE: double 660 Hz beep */
    {0},
    /* ALERT_HIGH: triple 440 Hz beep */
    {0},
    /* ALERT_URGENT: continuous 1000 Hz alarm */
    {0},
};

static void play_tone(int16_t freq_hz, uint16_t duration_ms)
{
    if (s_tx_chan == NULL) return;

    uint32_t sample_rate = 16000;
    uint32_t num_samples = (sample_rate * duration_ms) / 1000;
    int16_t *buffer = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (buffer == NULL) return;

    for (uint32_t i = 0; i < num_samples; i++) {
        buffer[i] = (int16_t)(16000.0f * sinf(2.0f * 3.14159f * freq_hz * i / sample_rate));
    }

    size_t bytes_written;
    i2s_channel_write(s_tx_chan, buffer, num_samples * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    free(buffer);
}

/* ── Camera Configuration ────────────────────────────────────────── */

void camera_init(void)
{
    s_camera_config.ledc_channel = LEDC_CHANNEL_1;
    s_camera_config.ledc_timer = LEDC_TIMER_1;
    s_camera_config.xclk_freq_hz = 20000000;  /* 20 MHz */
    s_camera_config.pin_d0 = 4;
    s_camera_config.pin_d1 = 5;
    s_camera_config.pin_d2 = 6;
    s_camera_config.pin_d3 = 7;
    s_camera_config.pin_d4 = 15;
    s_camera_config.pin_d5 = 16;
    s_camera_config.pin_d6 = 17;
    s_camera_config.pin_d7 = 18;
    s_camera_config.pin_pclk = 12;
    s_camera_config.pin_vsync = 13;
    s_camera_config.pin_href = 14;
    s_camera_config.pin_sccb_sda = 9;
    s_camera_config.pin_sccb_scl = 8;
    s_camera_config.pin_reset = 11;
    s_camera_config.pin_pwdn = -1;
    s_camera_config.frame_size = FRAMESIZE_VGA;  /* 640x480 */
    s_camera_config.pixel_format = PIXFORMAT_GRAYSCALE;
    s_camera_config.fb_count = 2;
    s_camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    s_camera_config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&s_camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(err));
        return;
    }

    /* Set OV5640 to night mode (long exposure + gain) */
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_gain_ctrl(s, 1);
        s->set_exposure_ctrl(s, 1);
        s->set_again_gain_ctrl(s, 1);
        s->set_aec_value(s, 600);  /* Higher exposure for IR */
    }

    ir_led_init();
    i2s_audio_init();

    ESP_LOGI(TAG, "Camera initialized (OV5640 + 940nm IR)");
}

/* ── Capture Task ────────────────────────────────────────────────── */

static void capture_task(void *arg)
{
    uint8_t fps = *(uint8_t *)arg;
    uint32_t interval_ms = 1000 / fps;

    while (s_capturing) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb == NULL) {
            ESP_LOGW(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* Ensure IR is on for night driving */
        if (s_ir_enabled) {
            ir_led_set(true);
        }

        /* Run edge inference on frame → features */
        if (s_frame_cb != NULL) {
            camera_features_t features;
            memset(&features, 0, sizeof(features));
            /* edge_inference_process_frame is called internally */
            /* For now, stub with zeros — edge_inference.c handles actual CNN */
            s_frame_cb(&features);
        }

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }

    ir_led_set(false);
    vTaskDelete(NULL);
}

void camera_start_capture(uint8_t fps)
{
    if (s_capturing) return;
    s_capturing = true;

    static uint8_t s_fps = 10;
    s_fps = fps;

    xTaskCreate(capture_task, "cam_capture", 8192, &s_fps, 4, NULL);
    ESP_LOGI(TAG, "Capture started at %d FPS", fps);
}

void camera_stop_capture(void)
{
    s_capturing = false;
}

void camera_set_frame_callback(camera_frame_cb_t cb)
{
    s_frame_cb = cb;
}

void camera_set_ir_enabled(bool enabled)
{
    s_ir_enabled = enabled;
    if (!enabled) {
        ir_led_set(false);
    }
}

void camera_play_alert_sound(alert_sound_t sound)
{
    switch (sound) {
    case ALERT_LOW:
        play_tone(880, 200);
        break;
    case ALERT_MODERATE:
        play_tone(660, 400);
        vTaskDelay(pdMS_TO_TICKS(100));
        play_tone(660, 400);
        break;
    case ALERT_HIGH:
        play_tone(440, 300);
        vTaskDelay(pdMS_TO_TICKS(80));
        play_tone(440, 300);
        vTaskDelay(pdMS_TO_TICKS(80));
        play_tone(440, 300);
        break;
    case ALERT_URGENT:
        for (int i = 0; i < 5; i++) {
            play_tone(1000, 300);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        break;
    }
    ESP_LOGI(TAG, "Alert sound played: %d", sound);
}

void camera_play_voice_prompt(voice_prompt_t prompt)
{
    /* In production: play pre-recorded WAV from flash (spiffs) */
    const char *prompts[] = {
        "You seem drowsy. Please pull over when it is safe to do so.",
        "It has been a while since your last break. Consider taking a rest.",
        "Please stay alert and keep your eyes on the road.",
    };

    ESP_LOGI(TAG, "Voice prompt: %s", prompts[prompt]);
    /* Stub: generate a brief attention tone */
    play_tone(800, 150);
    vTaskDelay(pdMS_TO_TICKS(50));
    play_tone(1000, 150);
}