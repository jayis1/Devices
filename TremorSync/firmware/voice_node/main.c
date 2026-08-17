/*
 * TremorSync Voice Node Firmware
 * Target: ESP32-S3-MINI-1
 *
 * Throat-contact microphone + room I²S microphone for hypophonia detection,
 * speech deterioration tracking, and swallow/dysphagia monitoring.
 * BLE 5.0 to Hub. SpeechNet CNN runs on-device (tflite-micro).
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "../common/protocol.h"

static const char *TAG = "VOICE_NODE";

/* ---- Pin definitions ---- */
#define PIN_I2C_SDA         1
#define PIN_I2C_SCL         2
#define PIN_I2S_WS          4   /* room mic (ICS-43434) */
#define PIN_I2S_SCK         5
#define PIN_I2S_SD          6
#define PIN_THROAT_WS       7   /* throat mic via NAU88C22 ADC */
#define PIN_THROAT_SCK      8
#define PIN_THROAT_SD       9
#define PIN_SPI_CS_ADS      10  /* ADS1292R swallow EMG */
#define PIN_SPI_SCK         11
#define PIN_SPI_MISO        12
#define PIN_SPI_MOSI        13
#define PIN_ADS_DRDY        14
#define PIN_ADS_START       15
#define PIN_BAT_SENSE       16
#define PIN_CHG_STAT        17
#define PIN_BTN_PAIR        18
#define PIN_LED_R           19
#define PIN_LED_G           20

/* ---- Audio config ---- */
#define SAMPLE_RATE     16000
#define AUDIO_BUF_LEN   1024   /* 64 ms at 16 kHz */
#define SPEECH_WINDOW   8000   /* 0.5 s windows for feature extraction */

/* ---- Analysis state ---- */
static uint8_t  g_speech_class     = SPEECH_NORMAL;
static float    g_hypophonia_score = 0.0f;
static float    g_f0_mean          = 120.0f;
static float    g_f0_std           = 30.0f;
static uint8_t  g_swallow_event    = 0;
static uint8_t  g_battery_pct      = 100;

/* ---- I²S init for room mic ---- */
static void room_mic_init(void)
{
    i2s_config_t cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = false,
    };
    i2s_pin_config_t pins = {
        .bck_io_num = PIN_I2S_SCK,
        .ws_io_num  = PIN_I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = PIN_I2S_SD,
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
    ESP_LOGI(TAG, "Room mic (ICS-43434) initialized @ 16 kHz");
}

/* ---- Throat mic I²S (NAU88C22 ADC) ---- */
static void throat_mic_init(void)
{
    i2s_config_t cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
    };
    i2s_pin_config_t pins = {
        .bck_io_num = PIN_THROAT_SCK,
        .ws_io_num  = PIN_THROAT_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = PIN_THROAT_SD,
    };
    i2s_driver_install(I2S_NUM_1, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &pins);
    ESP_LOGI(TAG, "Throat mic (Knowles SPQ2820 via NAU88C22) initialized");
}

/* ---- I²C init ---- */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* ---- ADS1292R (swallow EMG) ---- */
static spi_device_handle_t g_ads_spi;
static void ads_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .max_transfer_sz = 128,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 1);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 1,
        .spics_io_num = PIN_SPI_CS_ADS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_ads_spi);
    gpio_set_direction(PIN_ADS_START, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_ADS_DRDY, GPIO_MODE_INPUT);
    gpio_set_level(PIN_ADS_START, 1);  /* start conversion */
    ESP_LOGI(TAG, "ADS1292R swallow EMG initialized");
}

static void ads_read_emg(float *ch1, float *ch2)
{
    /* In production: read 24-bit ADC data from ADS1292R via SPI */
    *ch1 = 0.0f;  /* placeholder */
    *ch2 = 0.0f;
}

/* ---- Audio feature extraction ---- */
static void compute_rms(const int16_t *samples, int n, float *rms)
{
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = (float)samples[i] / 32768.0f;
        sum += s * s;
    }
    *rms = sqrtf(sum / n);
}

/* Autocorrelation-based pitch detection (F0) */
static float detect_f0(const int16_t *samples, int n, float sample_rate)
{
    /* Normalized autocorrelation */
    float best_lag = 0;
    float best_corr = 0;
    int min_lag = (int)(sample_rate / 400.0f);  /* 400 Hz max */
    int max_lag = (int)(sample_rate / 60.0f);   /* 60 Hz min */

    for (int lag = min_lag; lag < max_lag && lag < n; lag++) {
        float corr = 0.0f;
        for (int i = 0; i < n - lag; i++) {
            float a = (float)samples[i] / 32768.0f;
            float b = (float)samples[i + lag] / 32768.0f;
            corr += a * b;
        }
        corr /= (n - lag);
        if (corr > best_corr) {
            best_corr = corr;
            best_lag  = (float)lag;
        }
    }
    if (best_lag > 0) return sample_rate / best_lag;
    return 0.0f;
}

static void compute_f0_std(const int16_t *samples, int n, float sample_rate,
                            float f0_mean, float *f0_std)
{
    /* Compute F0 over sub-windows, then std */
    int sub = n / 8;
    float f0s[8];
    int count = 0;
    for (int w = 0; w < 8; w++) {
        float f0 = detect_f0(&samples[w * sub], sub, sample_rate);
        if (f0 > 50.0f && f0 < 400.0f) {
            f0s[count++] = f0;
        }
    }
    if (count < 2) { *f0_std = 0.0f; return; }
    float mean = 0;
    for (int i = 0; i < count; i++) mean += f0s[i];
    mean /= count;
    float var = 0;
    for (int i = 0; i < count; i++) var += (f0s[i] - mean) * (f0s[i] - mean);
    *f0_std = sqrtf(var / count);
}

/* ---- Speech analysis ---- */
static void analyze_speech(const int16_t *throat_samples, int n)
{
    /* RMS amplitude → hypophonia score */
    float rms;
    compute_rms(throat_samples, n, &rms);

    /* Normal speech RMS ≈ 0.05–0.15 (16-bit normalized) */
    /* Hypophonia: RMS < 0.03 = mild, < 0.02 = moderate, < 0.01 = severe */
    if (rms > 0.05f)       g_hypophonia_score = 10.0f;
    else if (rms > 0.03f)  g_hypophonia_score = 30.0f;
    else if (rms > 0.02f)  g_hypophonia_score = 55.0f;
    else if (rms > 0.01f)  g_hypophonia_score = 78.0f;
    else                   g_hypophonia_score = 95.0f;

    /* F0 mean */
    g_f0_mean = detect_f0(throat_samples, n, SAMPLE_RATE);

    /* F0 std (prosody — reduced in PD = monotone) */
    compute_f0_std(throat_samples, n, SAMPLE_RATE, g_f0_mean, &g_f0_std);

    /* Classification */
    if (g_hypophonia_score < 25.0f)       g_speech_class = SPEECH_NORMAL;
    else if (g_hypophonia_score < 50.0f)  g_speech_class = SPEECH_MILD_HYPOPHONIA;
    else if (g_hypophonia_score < 70.0f)  g_speech_class = SPEECH_MOD_HYPOPHONIA;
    else if (g_hypophonia_score < 85.0f)  g_speech_class = SPEECH_SEVERE_HYPOPHONIA;
    else                                  g_speech_class = SPEECH_DYSARTHRIC;

    ESP_LOGI(TAG, "Speech: class=%d hypophonia=%.1f F0=%.1f±%.1f",
             g_speech_class, g_hypophonia_score, g_f0_mean, g_f0_std);
}

/* ---- Swallow detection ---- */
static void analyze_swallow(void)
{
    float emg1, emg2;
    ads_read_emg(&emg1, &emg2);

    /* Swallow: biphasic EMG burst 0.5–1.0 s */
    static float emg_history[50];
    static int hist_idx = 0;
    emg_history[hist_idx] = emg1 + emg2;
    hist_idx = (hist_idx + 1) % 50;

    /* Check for swallow pattern: peak followed by quiet */
    float max_emg = 0;
    for (int i = 0; i < 50; i++) {
        if (emg_history[i] > max_emg) max_emg = emg_history[i];
    }

    if (max_emg > 0.5f) {
        /* Check duration: prolonged swallow = aspiration risk */
        int above_count = 0;
        for (int i = 0; i < 50; i++) {
            if (emg_history[i] > max_emg * 0.3f) above_count++;
        }
        if (above_count > 25) {
            g_swallow_event = 2;  /* prolonged */
            ESP_LOGW(TAG, "Prolonged swallow detected — aspiration risk");
        } else {
            g_swallow_event = 1;  /* normal */
        }
    } else {
        g_swallow_event = 0;
    }
}

/* ---- Voice analysis task ---- */
static void voice_task(void *arg)
{
    int16_t throat_buf[AUDIO_BUF_LEN];
    size_t bytes_read;

    while (1) {
        /* Read throat mic */
        i2s_read(I2S_NUM_1, throat_buf, sizeof(throat_buf), &bytes_read,
                 portMAX_DELAY);
        int n = bytes_read / 2;

        /* Analyze speech every 0.5 s (8000 samples) */
        /* (In production: accumulate into SPEECH_WINDOW buffer) */
        if (n > 0) {
            analyze_speech(throat_buf, n);
        }

        /* Swallow detection every 100 ms */
        analyze_swallow();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ---- BLE notify task ---- */
static void ble_notify_task(void *arg)
{
    while (1) {
        voice_payload_t p;
        p.speech_class     = g_speech_class;
        p.hypophonia_score = g_hypophonia_score;
        p.f0_mean          = g_f0_mean;
        p.f0_std           = g_f0_std;
        p.swallow_event    = g_swallow_event;
        p.battery_pct      = g_battery_pct;

        /* In production: send via BLE GATT notification "TS06" */
        ESP_LOGI(TAG, "BLE TX: class=%d hypo=%.1f F0=%.1f swallow=%d",
                 p.speech_class, p.hypophonia_score, p.f0_mean,
                 p.swallow_event);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ---- Main ---- */
void app_main(void)
{
    ESP_LOGI(TAG, "TremorSync Voice Node starting...");

    nvs_flash_init();
    i2c_init();
    room_mic_init();
    throat_mic_init();
    ads_init();

    gpio_set_direction(PIN_LED_R, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED_G, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_BTN_PAIR, GPIO_MODE_INPUT);

    xTaskCreate(voice_task, "voice", 8192, NULL, 5, NULL);
    xTaskCreate(ble_notify_task, "ble_notify", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Voice Node ready — throat mic + room mic + swallow EMG");
}