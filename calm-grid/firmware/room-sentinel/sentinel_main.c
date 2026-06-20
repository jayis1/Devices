/*
 * sentinel_main.c — CalmGrid Room Sentinel firmware (ESP32-S3, ESP-IDF)
 *
 * Monitors voice prosody stress (via 6-mic array) and ambient environment
 * (light, temperature, humidity, noise). All prosody analysis is on-device
 * — no speech is transcribed, stored, or transmitted. Only acoustic feature
 * vectors and a stress classification leave the device.
 *
 * Prosody classes (0-3):
 *   0=calm, 1=neutral, 2=elevated, 3=high-stress
 *
 * Privacy: physical mic mute switch + active LED indicator.
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
#include "driver/i2c.h"
#include "driver/uart.h"
#include "calm_protocol.h"

static const char *TAG = "calmgrid-sentinel";

/* ---- Pin definitions (ESP32-S3) ---- */
#define I2S_WS_PIN    4
#define I2S_BCK_PIN   5
#define I2S_DATA_PIN  6
#define I2C_SDA_PIN   8
#define I2C_SCL_PIN   9
#define IR_LED_PIN    38
#define MIC_MUTE_PIN  39
#define ACTIVE_LED_PIN 40

/* ---- Audio configuration ---- */
#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_WINDOW_S     2
#define AUDIO_WINDOW_SIZE  (AUDIO_SAMPLE_RATE * AUDIO_WINDOW_S)
#define VAD_THRESHOLD      500   /* RMS threshold for voice activity */
#define VAD_MIN_FRAMES     3     /* min 2s frames with voice to classify */

/* ---- Prosody reporting interval ---- */
#define PROSODY_REPORT_S   60    /* report prosody every 60s */
#define ENV_REPORT_S       60    /* report environment every 60s */

/* ---- VEML7700 + SHT40 I2C addresses ---- */
#define VEML7700_ADDR  0x10
#define SHT40_ADDR     0x44

/* ---- Globals ---- */
static int16_t audio_buffer[AUDIO_WINDOW_SIZE];
static uint8_t mic_muted = 0;
static uint8_t prosody_class = 1;  /* default: neutral */
static uint8_t prosody_confidence = 0;
static uint16_t speech_minutes = 0;
static int16_t f0_deviation = 0;
static uint8_t env_flags = 0;

/* ---- Function declarations ---- */
extern uint8_t classify_prosody(const int16_t *audio, int n,
                                 uint8_t *confidence, int16_t *f0_dev);
extern void    extract_prosody_features(const int16_t *audio, int n,
                                        float *features, int *n_features);
extern int     vad_detect(const int16_t *audio, int n);

/* ---- I2S configuration ---- */
static i2s_chan_handle_t rx_handle;

static void i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                            I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;
    chan_cfg.auto_clear = true;
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .bclk = I2S_BCK_PIN,
            .ws   = I2S_WS_PIN,
            .dout = -1,
            .din  = I2S_DATA_PIN,
        },
    };
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);
}

/* ---- I2C for VEML7700 + SHT40 ---- */
static void i2c_init_sensors(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

/* ---- Read VEML7700 ambient light (lux * 10) ---- */
static uint16_t veml7700_read_lux(void)
{
    uint8_t data[2];
    i2c_master_write_read_device(I2C_NUM_0, VEML7700_ADDR,
                                 (uint8_t[]){0x04}, 1, data, 2, 100 / portTICK_PERIOD_MS);
    uint16_t raw = (data[1] << 8) | data[0];
    /* VEML7700: lux = raw * 0.0036 * gain_resolution_factor
     * With default config: ~0.0036 lux/count → *10 = 0.036 */
    return (uint16_t)(raw * 0.036f);
}

/* ---- Read VEML7700 white channel → estimate CCT ---- */
static uint16_t veml7700_read_cct(uint16_t lux)
{
    /* CCT approximation from lux + white channel ratio
     * In production: calibrate with reference light sources */
    if (lux < 50) return 2700;   /* dim → warm assumption */
    if (lux < 200) return 3000;
    if (lux < 500) return 4000;
    if (lux < 1000) return 5000;
    return 6500;                  /* bright → cool */
}

/* ---- Read SHT40 temperature + humidity ---- */
static void sht40_read(int16_t *temp_centic, uint16_t *humidity_centi)
{
    uint8_t cmd = 0xFD;  /* high precision measurement */
    i2c_master_write_to_device(I2C_NUM_0, SHT40_ADDR, &cmd, 1, 100 / portTICK_PERIOD_MS);
    vTaskDelay(10 / portTICK_PERIOD_MS);  /* wait for measurement */
    uint8_t data[6];
    i2c_master_read_from_device(I2C_NUM_0, SHT40_ADDR, data, 6, 100 / portTICK_PERIOD_MS);
    /* SHT40: temp = -45 + 175 * raw/65535, humidity = 100 * raw/65535 */
    uint16_t t_raw = (data[0] << 8) | data[1];
    uint16_t h_raw = (data[3] << 8) | data[4];
    float temp = -45.0f + 175.0f * (float)t_raw / 65535.0f;
    float hum  = 100.0f * (float)h_raw / 65535.0f;
    *temp_centic = (int16_t)(temp * 100.0f);
    *humidity_centi = (uint16_t)(hum * 100.0f);
}

/* ---- Compute noise level (dB) from audio RMS ---- */
static uint16_t compute_noise_db(const int16_t *audio, int n)
{
    float sum_sq = 0;
    for (int i = 0; i < n; i++)
        sum_sq += (float)audio[i] * audio[i];
    float rms = sqrtf(sum_sq / n);
    /* Convert 16-bit RMS to dB SPL (approximate, calibrated) */
    /* 0 dB SPL ref, mic sensitivity in datasheet → simplified */
    float db = 20.0f * log10f(rms / 32768.0f) + 94.0f;  /* rough calibration */
    if (db < 0) db = 0;
    return (uint16_t)(db * 10.0f);  /* dB * 10 */
}

/* ---- Mic mute switch interrupt ---- */
static void IRAM_ATTR mute_isr_handler(void *arg)
{
    mic_muted = gpio_get_level(MIC_MUTE_PIN) == 0 ? 1 : 0;
    gpio_set_level(ACTIVE_LED_PIN, mic_muted ? 0 : 1);
}

/* ---- Audio capture + prosody task ---- */
static void audio_task(void *arg)
{
    (void)arg;
    size_t bytes_read;
    uint32_t frames_with_voice = 0;
    uint32_t total_frames = 0;
    uint32_t speech_seconds = 0;

    while (1) {
        if (mic_muted) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            prosody_class = 0;  /* calm (no data) */
            prosody_confidence = 0;
            continue;
        }

        /* Read 2s of audio */
        i2s_channel_read(rx_handle, audio_buffer,
                         AUDIO_WINDOW_SIZE * 2, &bytes_read,
                         portMAX_DELAY);
        int samples = bytes_read / 2;

        /* Check VAD */
        int voice_present = vad_detect(audio_buffer, samples);
        total_frames++;

        if (voice_present) {
            frames_with_voice++;
            speech_seconds += 2;

            /* Extract prosody features + classify */
            uint8_t conf = 0;
            int16_t f0_dev = 0;
            prosody_class = classify_prosody(audio_buffer, samples, &conf, &f0_dev);
            prosody_confidence = conf;
            f0_deviation = f0_dev;

            gpio_set_level(ACTIVE_LED_PIN, 1);  /* active indicator */
        } else {
            gpio_set_level(ACTIVE_LED_PIN, 0);
        }

        /* Every 60s: compute speech minutes + report */
        if (total_frames >= 30) {  /* 30 frames × 2s = 60s */
            speech_minutes = speech_seconds / 60;
            if (speech_minutes == 0 && frames_with_voice > 0)
                speech_minutes = 1;  /* at least 1 min if any voice */

            /* Reset counters */
            frames_with_voice = 0;
            total_frames = 0;
            speech_seconds = 0;
        }
    }
}

/* ---- Environment monitoring task ---- */
static void env_task(void *arg)
{
    (void)arg;
    while (1) {
        uint16_t lux = veml7700_read_lux();
        uint16_t cct = veml7700_read_cct(lux);
        int16_t temp_c;
        uint16_t hum_c;
        sht40_read(&temp_c, &hum_c);

        /* Noise from last audio buffer */
        uint16_t noise_db = mic_muted ? 0 : compute_noise_db(audio_buffer, 256);

        /* Environmental stress flags */
        env_flags = 0;
        if (noise_db > 600) env_flags |= CALM_ALERT_ENV_STRESS;  /* >60 dB */
        if (temp_c > 2700) env_flags |= CALM_ALERT_ENV_STRESS;   /* >27°C */

        /* Build environment payload */
        calm_environment_payload_t ep;
        ep.type = CALM_MSG_ENVIRONMENT;
        ep.node_id = CALM_NODE_ID_SENTINEL;
        ep.seq = 0;
        ep.flags = env_flags;
        ep.ambient_lux = lux;
        ep.cct_kelvin = cct;
        ep.temp_centic = temp_c;
        ep.humidity_centi = hum_c;
        ep.noise_db_tenth = noise_db;
        calm_pack_crc(&ep, sizeof(ep) - 2);

        /* In production: send via WiFi/MQTT to cloud + hub */
        ESP_LOGI(TAG, "ENV: lux=%u cct=%u temp=%d.%02d hum=%u.%02d noise=%u.%u",
                 lux, cct, temp_c/100, abs(temp_c)%100,
                 hum_c/100, hum_c%100, noise_db/10, noise_db%10);

        vTaskDelay(ENV_REPORT_S * 1000 / portTICK_PERIOD_MS);
    }
}

/* ---- Prosody reporting task ---- */
static void prosody_report_task(void *arg)
{
    (void)arg;
    while (1) {
        vTaskDelay(PROSODY_REPORT_S * 1000 / portTICK_PERIOD_MS);

        uint8_t flags = 0;
        if (prosody_class >= 2) flags |= CALM_ALERT_PROSODY_STRESS;

        calm_prosody_payload_t pp;
        pp.type = CALM_MSG_PROSODY;
        pp.node_id = CALM_NODE_ID_SENTINEL;
        pp.seq = 0;
        pp.flags = flags;
        pp.prosody_class = prosody_class;
        pp.confidence = prosody_confidence;
        pp.speech_minutes = speech_minutes;
        pp.f0_deviation = f0_deviation;
        calm_pack_crc(&pp, sizeof(pp) - 2);

        ESP_LOGI(TAG, "PROSODY: class=%u conf=%u speech=%u f0dev=%d",
                 prosody_class, prosody_confidence, speech_minutes, f0_deviation);
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "CalmGrid Room Sentinel starting...");

    /* NVS for WiFi */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO for mic mute + active LED */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MIC_MUTE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(MIC_MUTE_PIN, mute_isr_handler, NULL);

    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << ACTIVE_LED_PIN) | (1ULL << IR_LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);

    /* Initialize I2S + sensors */
    i2s_init();
    i2c_init_sensors();

    /* Initialize mic_muted state */
    mic_muted = (gpio_get_level(MIC_MUTE_PIN) == 0) ? 1 : 0;

    /* Create tasks */
    xTaskCreate(audio_task, "audio", 16384, NULL, 5, NULL);
    xTaskCreate(env_task, "env", 4096, NULL, 3, NULL);
    xTaskCreate(prosody_report_task, "prosody", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Sentinel ready. Mic muted: %s", mic_muted ? "YES" : "NO");
}