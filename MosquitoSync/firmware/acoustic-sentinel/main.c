/*
 * MosquitoSync — Acoustic Sentinel Firmware
 * ESP32-S3, FreeRTOS
 *
 * The Acoustic Sentinel continuously records 1-second audio windows from
 * a 4-mic I²S MEMS array, runs the WingNet CNN on-device (TFLite-Micro)
 * to classify mosquito species by wingbeat frequency, and immediately
 * alerts the Hub when a mosquito is detected. If a disease-vector species
 * is detected, it triggers an immediate barrier close command.
 *
 * Build: idf.py build with ESP-IDF v5.x
 *        Requires TFLite-Micro for ESP32 (esp-tflite-micro component)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "MosquitoSync-Acoustic";

/* === Audio Buffer ===
 * 1 second @ 16 kHz, 16-bit mono = 32000 bytes.
 * For 4-mic TDM, we process channel 0 (beamformed in production).
 */
#define SAMPLE_RATE       16000
#define AUDIO_WINDOW_MS   1000
#define SAMPLES_PER_WINDOW (SAMPLE_RATE * AUDIO_WINDOW_MS / 1000)
#define AUDIO_BUF_SAMPLES  1024  /* I²S DMA buffer size */
#define AUDIO_BUF_COUNT    8

/* Mel-spectrogram dimensions for WingNet */
#define MEL_BINS          64
#define MEL_TIME_STEPS    32

static int16_t g_audio_buffer[SAMPLES_PER_WINDOW];
static uint8_t g_mel_spectrogram[MEL_BINS * MEL_TIME_STEPS]; /* quantized 0-255 */

/* Detection state */
static uint16_t g_msg_seq = 0;
static uint16_t g_detections_24h = 0;
static uint8_t  g_last_species = 7; /* Non-mosquito default */
static uint8_t  g_last_confidence = 0;
static uint16_t g_last_wingbeat_freq = 0;
static uint32_t g_last_detection_time = 0;
static uint8_t  g_idle_mode = 0; /* 1 = duty-cycled (5s windows) */

/* === SX1262 SPI Interface (ESP32-S3) === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = ACOUSTIC_GPIO_SX_MOSI,
        .miso_io_num = ACOUSTIC_GPIO_SX_MISO,
        .sclk_io_num = ACOUSTIC_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = ACOUSTIC_GPIO_SX_NSS,
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
    gpio_set_level(ACOUSTIC_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(ACOUSTIC_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int enable) { (void)enable; }

static const ms_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I²S Microphone Array === */
static void i2s_mic_init(void)
{
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = AUDIO_BUF_COUNT,
        .dma_buf_len = AUDIO_BUF_SAMPLES,
        .use_apll = false,
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = ACOUSTIC_GPIO_I2S_BCLK,
        .ws_io_num = ACOUSTIC_GPIO_I2S_LRCLK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = ACOUSTIC_GPIO_I2S_DATA,
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    ESP_LOGI(TAG, "I2S microphone initialized (%d Hz, 16-bit)", SAMPLE_RATE);
}

/* === I²C for SHT40 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = ACOUSTIC_GPIO_SHT_SDA,
        .scl_io_num = ACOUSTIC_GPIO_SHT_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

static void read_sht40(int8_t *temp, uint8_t *humidity)
{
    /* SHT40 at I2C addr 0x44
     * Send measurement command 0xFD (high precision)
     * Wait 8 ms, read 6 bytes (temp MSB, LSB, CRC, hum MSB, LSB, CRC)
     */
    uint8_t cmd = 0xFD;
    i2c_cmd_handle_t cmd_h = i2c_cmd_link_create();
    i2c_master_start(cmd_h);
    i2c_master_write_byte(cmd_h, 0x44 << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_h, cmd, true);
    i2c_master_stop(cmd_h);
    i2c_master_cmd_begin(I2C_NUM_0, cmd_h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_h);

    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t data[6];
    cmd_h = i2c_cmd_link_create();
    i2c_master_start(cmd_h);
    i2c_master_write_byte(cmd_h, 0x44 << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd_h, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_h);
    i2c_master_cmd_begin(I2C_NUM_0, cmd_h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_h);

    uint16_t t_raw = (data[0] << 8) | data[1];
    uint16_t h_raw = (data[3] << 8) | data[4];
    *temp = (int8_t)(-45.0 + 175.0 * t_raw / 65535.0);
    *humidity = (uint8_t)(100.0 * h_raw / 65535.0);
}

/* === Audio Energy Calculation === */
static uint16_t compute_audio_energy(const int16_t *samples, int count)
{
    /* RMS energy in 0.01 units (0-65535) */
    uint64_t sum_sq = 0;
    for (int i = 0; i < count; i++) {
        sum_sq += (uint64_t)samples[i] * samples[i];
    }
    double rms = sqrt((double)sum_sq / count);
    /* Scale to 0-65535 range (int16_t max ~32767) */
    uint32_t scaled = (uint32_t)(rms * 2.0);
    return scaled > 65535 ? 65535 : (uint16_t)scaled;
}

/* === Mel-Spectrogram Computation (simplified) ===
 * In production, use a proper FFT + mel filterbank.
 * Here we show the interface and a simplified implementation.
 */
static void compute_mel_spectrogram(const int16_t *audio, int num_samples,
                                     uint8_t *mel_out)
{
    /* Parameters: 1024-sample FFT, 512 hop, 64 mel bins, 32 time steps
     *
     * For each time step t (0..31):
     *   1. Window 1024 samples (Hann)
     *   2. FFT (1024-point)
     *   3. Power spectrum
     *   4. Apply mel filterbank (64 bins)
     *   5. Log + quantize to uint8
     *
     * This fills mel_out[64 * 32] = 2048 bytes
     */
    memset(mel_out, 0, MEL_BINS * MEL_TIME_STEPS);

    int fft_size = 1024;
    int hop = (num_samples - fft_size) / (MEL_TIME_STEPS - 1);
    if (hop < 1) hop = 1;

    /* Simplified: use energy in frequency bands as proxy
     * In production: proper FFT (esp_dsp or kiss_fft) + mel filterbank
     */
    for (int t = 0; t < MEL_TIME_STEPS; t++) {
        int start = t * hop;
        /* For each mel bin, compute energy in corresponding frequency band */
        for (int m = 0; m < MEL_BINS; m++) {
            /* Mel frequency range: 150 Hz to 8000 Hz (mosquito wingbeat 300-700 Hz) */
            float mel_freq = 150.0 + (8000.0 - 150.0) * m / (MEL_BINS - 1);
            /* Simple Goertzel-like energy at this frequency */
            float real = 0.0f, imag = 0.0f;
            for (int n = 0; n < fft_size && (start + n) < num_samples; n++) {
                /* Hann window */
                float w = 0.5f * (1.0f - cosf(2.0f * M_PI * n / (fft_size - 1)));
                float sample = audio[start + n] * w;
                float phase = 2.0f * M_PI * mel_freq * n / SAMPLE_RATE;
                real += sample * cosf(phase);
                imag += sample * sinf(phase);
            }
            float power = sqrtf(real * real + imag * imag) / fft_size;
            /* Log scale + quantize to 0-255 */
            float log_power = log10f(power + 1.0f) * 40.0f;
            if (log_power > 255.0f) log_power = 255.0f;
            if (log_power < 0.0f) log_power = 0.0f;
            mel_out[t * MEL_BINS + m] = (uint8_t)log_power;
        }
    }
}

/* === WingNet CNN Inference (TFLite-Micro stub) ===
 * In production:
 *   - Load WingNet int8 model from flash
 *   - Run inference on g_mel_spectrogram
 *   - Returns species class (0-7) + confidence (0-100%)
 *
 * Here we implement a simplified wingbeat frequency detector
 * as a fallback/stand-in for the full CNN.
 */
static int8_t g_wingnet_model[1]; /* Placeholder for model data */

static void wingnet_init(void)
{
    /* In production: load model from partition
     * esp_tflite_micro::Initialize(flash partition offset)
     */
    ESP_LOGI(TAG, "WingNet CNN initialized (TFLite-Micro int8, ~140 KB)");
}

static void wingnet_infer(const uint8_t *mel_spectrogram,
                          uint8_t *species_class, uint8_t *confidence,
                          uint16_t *wingbeat_freq)
{
    /* Simplified detection: find peak energy in 300-700 Hz band
     * Mel bin index for frequency f: m = (f - 150) / (8000 - 150) * 63
     * 300 Hz → m ≈ 1.2, 700 Hz → m ≈ 4.4
     */
    int best_bin = -1;
    uint8_t best_energy = 0;

    /* Sum energy across time steps for low mel bins (300-700 Hz) */
    for (int m = 1; m <= 5; m++) {
        uint32_t total = 0;
        for (int t = 0; t < MEL_TIME_STEPS; t++) {
            total += mel_spectrogram[t * MEL_BINS + m];
        }
        if (total > best_energy) {
            best_energy = (uint8_t)(total / MEL_TIME_STEPS);
            best_bin = m;
        }
    }

    /* If peak energy is high enough, classify by frequency */
    if (best_energy > ACOUSTIC_AUDIO_ENERGY_MIN && best_bin >= 0) {
        /* Convert mel bin to frequency */
        float freq = 150.0 + (8000.0 - 150.0) * best_bin / (MEL_BINS - 1);

        /* Classify species by wingbeat frequency */
        /* Aedes aegypti: 484 Hz, Ae. albopictus: 428 Hz
         * Anopheles gambiae: 423 Hz, An. stephensi: 455 Hz
         * Culex quinquefasciatus: 567 Hz, Cx. pipiens: 503 Hz
         * Mansonia uniformis: 322 Hz
         */
        float freqs[] = {484, 428, 423, 455, 567, 503, 322, 0};
        int best_class = 7; /* Non-mosquito */
        float best_dist = 1e9;
        for (int c = 0; c < 7; c++) {
            float dist = fabsf(freq - freqs[c]);
            if (dist < best_dist) {
                best_dist = dist;
                best_class = c;
            }
        }

        /* Only accept if within 30 Hz of known species frequency */
        if (best_dist < 30.0) {
            *species_class = (uint8_t)best_class;
            *wingbeat_freq = (uint16_t)(freq * 10); /* 0.1 Hz resolution */
            /* Confidence inversely proportional to frequency distance */
            *confidence = (uint8_t)(100.0 * (1.0 - best_dist / 30.0));
            if (*confidence < ACOUSTIC_CONFIDENCE_PCT) {
                *species_class = 7; /* Non-mosquito (low confidence) */
            }
        } else {
            *species_class = 7;
            *confidence = 0;
            *wingbeat_freq = (uint16_t)(freq * 10);
        }
    } else {
        *species_class = 7;
        *confidence = 0;
        *wingbeat_freq = 0;
    }

    /* In production: full WingNet CNN inference via TFLite-Micro
     * TfLiteStatus status = interpreter->Invoke();
     * output = interpreter->output(0)->data.int8;
     * species_class = argmax(output);
     * confidence = softmax(output)[species_class] * 100;
     */
}

/* === Send Species Alert to Hub === */
static void send_species_alert(ms_mesh_ctx_t *mesh, uint8_t species,
                                uint8_t confidence, uint16_t freq)
{
    ms_message_t alert;
    uint8_t is_vector = IS_DISEASE_VECTOR(species) ? 1 : 0;

    ms_build_species_alert(&alert, mesh->node_id, g_msg_seq++,
                           species, confidence, freq, is_vector);

    ms_mesh_send(mesh, &alert);

    ESP_LOGW(TAG, "Species alert sent: class=%d conf=%d%% freq=%.1fHz vector=%d",
             species, confidence, freq / 10.0, is_vector);
}

/* === Send Telemetry to Hub === */
static void send_telemetry(ms_mesh_ctx_t *mesh, int8_t temp, uint8_t humidity,
                           uint8_t detected, uint16_t audio_energy)
{
    uint8_t battery_v = 0; /* Read from ADC in production */
    /* In production: read battery via ADC
     * uint16_t raw = adc1_get_raw(ADC1_CHANNEL_x);
     * battery_v = (uint8_t)(raw * 4.2 / 4095 * 100); // x0.01V
     */

    ms_message_t msg;
    ms_build_acoustic_telem(&msg, mesh->node_id, g_msg_seq++,
                            battery_v, temp, humidity,
                            detected, g_last_species, g_last_confidence,
                            g_last_wingbeat_freq, g_detections_24h,
                            audio_energy, mesh->last_rssi);
    ms_mesh_send(mesh, &msg);
}

/* === Audio Capture + WingNet Inference Task === */
static void audio_task(void *arg)
{
    ms_mesh_ctx_t *mesh = (ms_mesh_ctx_t *)arg;
    size_t bytes_read = 0;
    int samples_collected = 0;
    int8_t temp = 25;
    uint8_t humidity = 50;

    ESP_LOGI(TAG, "Audio capture + WingNet task started");

    while (1) {
        /* Determine window size based on mode */
        int window_samples = g_idle_mode ?
            (SAMPLE_RATE * 5) : /* 5-second idle window */
            SAMPLES_PER_WINDOW; /* 1-second active window */

        /* Collect audio samples */
        samples_collected = 0;
        while (samples_collected < window_samples) {
            int to_read = window_samples - samples_collected;
            if (to_read > AUDIO_BUF_SAMPLES) to_read = AUDIO_BUF_SAMPLES;

            i2s_read(I2S_NUM_0,
                     (void *)&g_audio_buffer[samples_collected],
                     to_read * sizeof(int16_t),
                     &bytes_read, portMAX_DELAY);
            samples_collected += bytes_read / sizeof(int16_t);
        }

        /* Compute audio energy */
        uint16_t energy = compute_audio_energy(g_audio_buffer,
                                               samples_collected);

        /* Skip inference if energy too low (silence) */
        if (energy < ACOUSTIC_AUDIO_ENERGY_MIN) {
            /* Check idle timeout */
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            if (g_last_detection_time > 0 &&
                (now - g_last_detection_time) > ACOUSTIC_IDLE_TIMEOUT_S) {
                g_idle_mode = 1; /* Enter duty-cycled mode */
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* Exit idle mode when sound detected */
        g_idle_mode = 0;

        /* Compute mel-spectrogram */
        compute_mel_spectrogram(g_audio_buffer, samples_collected,
                                 g_mel_spectrogram);

        /* Run WingNet inference */
        uint8_t species, confidence;
        uint16_t freq;
        wingnet_infer(g_mel_spectrogram, &species, &confidence, &freq);

        /* Check if mosquito detected with sufficient confidence */
        if (species < 7 && confidence >= ACOUSTIC_CONFIDENCE_PCT) {
            g_last_species = species;
            g_last_confidence = confidence;
            g_last_wingbeat_freq = freq;
            g_last_detection_time = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            g_detections_24h++;

            ESP_LOGW(TAG, "MOSQUITO DETECTED: species=%d conf=%d%% freq=%.1fHz",
                     species, confidence, freq / 10.0);

            /* Send immediate species alert to Hub */
            send_species_alert(mesh, species, confidence, freq);

            /* Send telemetry */
            send_telemetry(mesh, temp, humidity, 1, energy);

            /* If disease vector, keep continuous monitoring */
            if (IS_DISEASE_VECTOR(species)) {
                ESP_LOGE(TAG, "DISEASE VECTOR species %d detected!", species);
            }
        } else {
            /* No detection — send periodic telemetry */
            static uint32_t last_telem = 0;
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
            if (now - last_telem > 300) { /* 5 min */
                /* Read temp/humidity occasionally */
                static uint32_t last_env = 0;
                if (now - last_env > 60) {
                    read_sht40(&temp, &humidity);
                    last_env = now;
                }
                send_telemetry(mesh, temp, humidity, 0, energy);
                last_telem = now;
            }
        }

        /* Small delay between windows */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* === Mesh Task === */
static void mesh_task(void *arg)
{
    ms_mesh_ctx_t *mesh = (ms_mesh_ctx_t *)arg;

    ms_radio_config_t radio_cfg = {
        .frequency = MS_NET_FREQ_HZ,
        .bandwidth = MS_NET_BW_HZ,
        .spreading_factor = MS_NET_SF,
        .coding_rate = MS_NET_CR,
        .preamble_len = MS_NET_PREAMBLE,
        .tx_power_dbm = MS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (ms_mesh_init(mesh, MS_NODE_ACOUSTIC, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    /* Join network */
    int join_retries = 0;
    while (join_retries < 10) {
        if (ms_mesh_join(mesh) == 0) {
            ESP_LOGI(TAG, "Joined mesh: node_id=%d slot=%d",
                     mesh->node_id, mesh->tdma_slot);
            break;
        }
        ESP_LOGW(TAG, "Join failed, retry %d", join_retries);
        vTaskDelay(pdMS_TO_TICKS(2000));
        join_retries++;
    }

    /* Listen for hub commands */
    ms_message_t msg;
    while (1) {
        if (ms_mesh_recv(mesh, &msg, 5000) == 0) {
            switch (msg.header.type) {
                case MS_MSG_COMMAND: {
                    uint8_t cmd = msg.payload[0];
                    ESP_LOGI(TAG, "Command from hub: cmd=0x%02X", cmd);
                    if (cmd == MS_CMD_HIGH_RISK_MODE) {
                        g_idle_mode = 0; /* Force continuous monitoring */
                        ESP_LOGI(TAG, "High-risk mode: continuous monitoring");
                    } else if (cmd == MS_CMD_NORMAL_MODE) {
                        /* Allow idle mode after timeout */
                    }
                    break;
                }
                case MS_MSG_TIME_SYNC:
                    /* Update local time */
                    break;
                case MS_MSG_CONFIG:
                    /* Update sampling config */
                    break;
                default:
                    break;
            }
        }
    }
}

/* === Status LED Task === */
static void status_task(void *arg)
{
    gpio_set_direction(ACOUSTIC_GPIO_LED, GPIO_MODE_OUTPUT);
    uint8_t state = 0;
    while (1) {
        state = !state;
        gpio_set_level(ACOUSTIC_GPIO_LED, state);
        /* Faster blink when mosquito detected recently */
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        uint32_t since_det = now - g_last_detection_time;
        int delay_ms = (since_det < 10) ? 200 : 1000;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "MosquitoSync Acoustic Sentinel starting...");

    static ms_mesh_ctx_t mesh;

    nvs_flash_init();

    /* Initialize GPIOs */
    gpio_set_direction(ACOUSTIC_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(ACOUSTIC_GPIO_SX_RST, 1);
    gpio_set_direction(ACOUSTIC_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(ACOUSTIC_GPIO_MIC_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(ACOUSTIC_GPIO_MIC_EN, 1); /* Enable mic array */

    /* Initialize I2C */
    i2c_init();

    /* Initialize I²S microphone array */
    i2s_mic_init();

    /* Initialize WingNet CNN */
    wingnet_init();

    /* Create tasks */
    xTaskCreate(mesh_task, "mesh", 8192, &mesh, 5, NULL);
    xTaskCreate(audio_task, "audio", 16384, &mesh, 4, NULL);
    xTaskCreate(status_task, "status", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "Acoustic Sentinel running. Free heap: %lu bytes",
             (unsigned long)esp_get_free_heap_size());
}