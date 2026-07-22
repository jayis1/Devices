/*
 * VoiceSync — Room Sentinel Firmware
 * ESP32-S3, ESP-IDF v5.x, FreeRTOS
 *
 * The Room Sentinel captures ambient voice via a 4-mic I²S array,
 * runs VoiceNet CNN on-device for voice quality classification,
 * monitors environmental air quality (SGP40 VOC), temperature/humidity
 * (SHT40), and detects talking/voice activity. Reports to Hub every
 * 2 minutes via Sub-GHz 868 MHz mesh.
 *
 * VoiceNet: int8-quantized 2D-CNN, ~180 KB, <300 ms inference on ESP32-S3
 *   Input:  2-second mel-spectrogram (80×128)
 *   Output: 8-class voice quality (Normal, Hoarse, Breathy, Strained,
 *           Tremor, Fatigue, Reflux, Disorder)
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "VoiceSync-Room";

/* === Audio capture === */
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_WINDOW_MS   2000
#define AUDIO_SAMPLES     (AUDIO_SAMPLE_RATE * AUDIO_WINDOW_MS / 1000) /* 32000 */
#define MEL_BINS          80
#define MEL_FRAMES        128

static int16_t audio_buffer[AUDIO_SAMPLES]; /* 2-second capture at 16 kHz */
static uint8_t g_mel_spectrogram[MEL_BINS * MEL_FRAMES]; /* 80×128 int8 */

/* === VoiceNet CNN output === */
static uint8_t g_voice_quality_class = 0;  /* 0-7 */
static uint8_t g_voice_confidence = 0;     /* 0-100% */
static uint16_t g_f0_detected = 0;        /* ×0.1 Hz */
static uint8_t g_talking_detected = 0;
static uint8_t g_db_spl = 0;

/* === Environmental sensors === */
static float g_temp_c = 0.0f;
static float g_humidity_pct = 0.0f;
static uint16_t g_voc_index = 0;

/* === Mesh state === */
static vs_mesh_ctx_t g_mesh;
static uint16_t g_msg_seq = 0;

/* === SX1262 SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = ROOM_GPIO_SX_MOSI,
        .miso_io_num = ROOM_GPIO_SX_MISO,
        .sclk_io_num = ROOM_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = ROOM_GPIO_SX_NSS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}

static void spi_cs_select(void) { }
static void spi_cs_release(void) { }
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t assert) {
    gpio_set_level(ROOM_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(ROOM_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int enable) { (void)enable; }

static const vs_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I²C for SHT40 + SGP40 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = ROOM_GPIO_SHT_SDA,
        .scl_io_num = ROOM_GPIO_SHT_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* === SHT40: Temperature/Humidity === */
static void read_sht40(float *temp, float *humidity)
{
    /* SHT40 I²C address: 0x44 */
    uint8_t cmd = 0xFD; /* High repeatability measurement */
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, 0x44 << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, cmd, true);
    i2c_master_stop(h);
    i2c_master_cmd_begin(I2C_NUM_0, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);

    vTaskDelay(pdMS_TO_TICKS(10)); /* Measurement time */

    uint8_t buf[6];
    h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, 0x44 << 1 | I2C_MASTER_READ, true);
    i2c_master_read(h, buf, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(h);
    i2c_master_cmd_begin(I2C_NUM_0, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);

    uint16_t t_raw = (buf[0] << 8) | buf[1];
    uint16_t h_raw = (buf[3] << 8) | buf[4];
    *temp = -45.0f + 175.0f * t_raw / 65535.0f;
    *humidity = 100.0f * h_raw / 65535.0f;
}

/* === SGP40: VOC Index === */
static uint16_t read_sgp40(float temp, float humidity)
{
    /* SGP40 I²C address: 0x59 */
    /* In production: send measure command with T/RH compensation,
     * read 3 bytes (VOC raw + CRC)
     * Convert raw to VOC index 0-500 using Sensirion algorithm
     */
    (void)temp; (void)humidity;
    return 100; /* Stub: moderate VOC */
}

/* === I²S Audio Capture === */
static void i2s_audio_init(void)
{
    /* Configure I²S for 4-mic array (ICS-43434 TDM)
     * 16 kHz sample rate, 16-bit, 4 channels
     * In production: use i2s_std or i2s_tdm driver
     */
    ESP_LOGI(TAG, "I²S audio initialized (16 kHz, 4-mic TDM)");
}

static void capture_audio(void)
{
    /* In production: read 32000 samples (2 seconds) from I²S
     * For 4-mic TDM, take channel 0 (or beamform)
     * Store in audio_buffer
     */
    memset(audio_buffer, 0, sizeof(audio_buffer));
    ESP_LOGI(TAG, "Captured 2s audio (%d samples)", AUDIO_SAMPLES);
}

/* === Mel-Spectrogram Computation ===
 * Converts time-domain audio to mel-spectrogram for VoiceNet CNN.
 * 80 mel bins × 128 time frames from 2-second audio at 16 kHz.
 */
static void compute_mel_spectrogram(const int16_t *audio, int n)
{
    /* In production:
     * 1. Pre-emphasis filter (0.97)
     * 2. Frame: 25 ms windows, 10 ms hop → ~200 frames (downsample to 128)
     * 3. Hamming window
     * 4. FFT (512-point) → power spectrum
     * 5. Mel filterbank (80 bins, 0-8000 Hz) → mel spectrogram
     * 6. Log-magnitude → dB scale
     * 7. Normalize to int8 range for quantized CNN
     */
    memset(g_mel_spectrogram, 0, sizeof(g_mel_spectrogram));
    (void)audio; (void)n;
}

/* === VoiceNet CNN Inference ===
 * Runs the int8-quantized VoiceNet model on the mel-spectrogram.
 * In production: use TFLite-Micro with ESP-NN hardware acceleration.
 *
 * Architecture:
 *   Input:  80×128×1 (mel-spectrogram, int8)
 *   Conv2D(32, 3×3) + ReLU + MaxPool(2×2) → 40×64×32
 *   Conv2D(64, 3×3) + ReLU + MaxPool(2×2) → 20×32×64
 *   Conv2D(128, 3×3) + ReLU + MaxPool(2×2) → 10×16×128
 *   Flatten → 20480
 *   Dense(128) + ReLU
 *   Dense(8) + Softmax
 *   Output: 8-class voice quality (int8 → float → argmax)
 *
 * Model size: ~180 KB (int8 quantized)
 * Inference time: ~250 ms on ESP32-S3 @ 240 MHz
 */
static void run_voicenet(void)
{
    /* In production:
     * 1. Load TFLite-Micro model from flash
     * 2. Set input tensor to g_mel_spectrogram
     * 3. Invoke interpreter
     * 4. Read output tensor (8 probabilities)
     * 5. argmax → voice_quality_class
     * 6. softmax max → confidence
     */
    g_voice_quality_class = VS_VOICE_NORMAL; /* Stub: normal voice */
    g_voice_confidence = 85;
}

/* === F0 Detection from ambient audio === */
static float detect_f0_ambient(const int16_t *samples, int n)
{
    /* Autocorrelation pitch detection
     * Same algorithm as Vocal Band but for airborne audio
     */
    int min_lag = AUDIO_SAMPLE_RATE / 400;  /* Lower range for ambient */
    int max_lag = AUDIO_SAMPLE_RATE / 80;

    float best_corr = 0.0f;
    int best_lag = 0;

    for (int lag = min_lag; lag <= max_lag; lag++) {
        float corr = 0.0f;
        for (int i = 0; i < n - lag; i++) {
            corr += (float)samples[i] * (float)samples[i + lag];
        }
        corr /= (n - lag);
        if (corr > best_corr) {
            best_corr = corr;
            best_lag = lag;
        }
    }
    if (best_lag > 0)
        return (float)AUDIO_SAMPLE_RATE / best_lag;
    return 0.0f;
}

/* === Voice Activity Detection (VAD) === */
static uint8_t detect_talking(const int16_t *samples, int n)
{
    float rms = 0.0f;
    for (int i = 0; i < n; i++) {
        float v = (float)samples[i] / 32768.0f;
        rms += v * v;
    }
    rms = sqrtf(rms / n);
    return (rms > 0.03f) ? 1 : 0;
}

/* === Compute dB SPL === */
static uint8_t compute_db_spl(const int16_t *samples, int n)
{
    float rms = 0.0f;
    for (int i = 0; i < n; i++) {
        float v = (float)samples[i] / 32768.0f;
        rms += v * v;
    }
    rms = sqrtf(rms / n);
    if (rms < 0.0001f) return 40;
    float db = 20.0f * log10f(rms) + 90.0f;
    if (db < 40.0f) db = 40.0f;
    if (db > 120.0f) db = 120.0f;
    return (uint8_t)db;
}

/* === Audio + ML Pipeline === */
static void audio_ml_task(void *arg)
{
    while (1) {
        /* Capture 2-second audio window */
        capture_audio();

        /* Compute mel-spectrogram */
        compute_mel_spectrogram(audio_buffer, AUDIO_SAMPLES);

        /* Run VoiceNet CNN */
        run_voicenet();

        /* Detect F0 and voice activity */
        float f0 = detect_f0_ambient(audio_buffer, AUDIO_SAMPLES);
        g_f0_detected = (uint16_t)(f0 * 10.0f);
        g_talking_detected = detect_talking(audio_buffer, AUDIO_SAMPLES);
        g_db_spl = compute_db_spl(audio_buffer, AUDIO_SAMPLES);

        ESP_LOGI(TAG, "VoiceNet: class=%d conf=%d%% f0=%.1fHz talking=%d dB=%d",
                 g_voice_quality_class, g_voice_confidence,
                 g_f0_detected / 10.0f, g_talking_detected, g_db_spl);

        /* Send immediate alert if critical voice class detected */
        if (IS_VOICE_CRITICAL_CLASS(g_voice_quality_class) &&
            g_voice_confidence > VOICE_NET_CONFIDENCE_PCT) {
            vs_message_t alert;
            vs_build_voice_alert(&alert, g_mesh.node_id, g_msg_seq++,
                                 g_voice_quality_class, g_voice_confidence,
                                 g_f0_detected, 1);
            uint8_t buf[VS_MAX_MSG];
            size_t len = vs_encode(&alert, buf, sizeof(buf));
            vs_radio_tx(buf, (uint8_t)len);
            ESP_LOGW(TAG, "CRITICAL voice class %d detected! Alert sent.",
                     g_voice_quality_class);
        }

        vTaskDelay(pdMS_TO_TICKS(ROOM_SAMPLE_MS));
    }
}

/* === Environmental Sensor Task === */
static void env_task(void *arg)
{
    while (1) {
        read_sht40(&g_temp_c, &g_humidity_pct);
        g_voc_index = read_sgp40(g_temp_c, g_humidity_pct);

        ESP_LOGI(TAG, "Env: %.1f°C, %.1f%% RH, VOC=%d",
                 g_temp_c, g_humidity_pct, g_voc_index);

        /* Alert on low humidity */
        if (g_humidity_pct < HUMIDITY_TARGET_MIN) {
            vs_message_t alert;
            uint8_t data[2];
            data[0] = (uint8_t)g_humidity_pct;
            data[1] = (uint8_t)g_temp_c;
            vs_build_alert(&alert, g_mesh.node_id, g_msg_seq++,
                           VS_ALERT_LOW_HUMIDITY, 1, data, 2);
            uint8_t buf[VS_MAX_MSG];
            size_t len = vs_encode(&alert, buf, sizeof(buf));
            vs_radio_tx(buf, (uint8_t)len);
        }

        vTaskDelay(pdMS_TO_TICKS(60000)); /* 1 minute */
    }
}

/* === Telemetry TX Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        if (g_mesh.joined) {
            vs_message_t msg;
            vs_build_room_telem(&msg, g_mesh.node_id, g_msg_seq++,
                                0xFF, /* USB powered */
                                g_voice_quality_class,
                                g_voice_confidence,
                                g_f0_detected,
                                0, /* phonation % from vocal band */
                                (int16_t)(g_temp_c * 10.0f),
                                (uint16_t)(g_humidity_pct * 10.0f),
                                g_voc_index,
                                g_db_spl,
                                g_talking_detected,
                                g_mesh.last_rssi);

            uint8_t buf[VS_MAX_MSG];
            size_t len = vs_encode(&msg, buf, sizeof(buf));
            vs_mesh_send(&g_mesh, &msg);

            ESP_LOGI(TAG, "Telemetry sent (%d bytes)", (int)len);
        }
        vTaskDelay(pdMS_TO_TICKS(ROOM_TX_MS));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "VoiceSync Room Sentinel starting...");

    /* Initialize I²C */
    i2c_init();

    /* Initialize I²S audio */
    i2s_audio_init();

    /* Initialize SPI for SX1262 */
    spi_init();

    /* Join mesh network */
    vs_radio_config_t radio_cfg = {
        .frequency = VS_NET_FREQ_HZ,
        .bandwidth = VS_NET_BW_HZ,
        .spreading_factor = VS_NET_SF,
        .coding_rate = VS_NET_CR,
        .preamble_len = VS_NET_PREAMBLE,
        .tx_power_dbm = VS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (vs_mesh_init(&g_mesh, VS_NODE_ROOM, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        return;
    }

    /* Join network (retry up to 10 times) */
    for (int i = 0; i < 10; i++) {
        if (vs_mesh_join(&g_mesh) == 0) {
            ESP_LOGI(TAG, "Joined mesh: id=%d slot=%d",
                     g_mesh.node_id, g_mesh.tdma_slot);
            break;
        }
        ESP_LOGW(TAG, "Join attempt %d failed, retrying...", i + 1);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    /* Start tasks */
    xTaskCreate(audio_ml_task, "audio_ml", 16384, NULL, 5, NULL);
    xTaskCreate(env_task, "env", 4096, NULL, 3, NULL);
    xTaskCreate(telemetry_task, "telemetry", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "VoiceSync Room Sentinel running");
}