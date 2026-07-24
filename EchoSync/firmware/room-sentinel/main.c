/*
 * EchoSync — Room Sentinel Firmware
 * ESP32-S3, ESP-IDF, FreeRTOS
 *
 * The Room Sentinel continuously monitors ambient audio through a 4-mic
 * I²S array, runs the on-device SoundNet CNN for 20-class environmental
 * sound classification, estimates direction-of-arrival via TDOA
 * beamforming, and reports sound events to the Hub via Sub-GHz 868 MHz.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_system.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

static const char *TAG = "EchoSync-Sentinel";

/* === Audio Buffer === */
#define SAMPLE_RATE   ES_AUDIO_SAMPLE_RATE
#define NUM_MICS      4
#define AUDIO_CHUNK   1024
#define SPECTRO_FRAMES 126
#define MEL_BINS       64

static int16_t g_audio_buffer[NUM_MICS][AUDIO_CHUNK];
static uint8_t g_mel_spectrogram[MEL_BINS * SPECTRO_FRAMES];

/* === SoundNet Model (simulated — TFLite-Micro in production) === */
static const char *sound_names[20] = ES_SOUND_CLASS_NAMES;

typedef struct {
    uint8_t sound_class;
    uint8_t confidence;
    uint16_t direction_az;
    int8_t direction_el;
    uint16_t duration_ms;
    uint8_t db_spl;
} sound_event_t;

static sound_event_t g_last_event;
static uint8_t g_last_detected_class = 0xFF;
static uint32_t g_last_detection_time = 0;
static uint16_t g_event_counter = 0;

/* === SX1262 SPI Interface === */
static spi_device_handle_t g_spi_dev;

static void spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = SENTINEL_GPIO_SX_MOSI,
        .miso_io_num = SENTINEL_GPIO_SX_MISO,
        .sclk_io_num = SENTINEL_GPIO_SX_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000, .mode = 0,
        .spics_io_num = SENTINEL_GPIO_SX_NSS, .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi_dev);
}
static void spi_cs_select(void) {}
static void spi_cs_release(void) {}
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    spi_transaction_t t = { .tx_buffer = &byte, .rx_buffer = &rx, .length = 8 };
    spi_device_polling_transmit(g_spi_dev, &t);
    return rx;
}
static void spi_reset(uint8_t a) { gpio_set_level(SENTINEL_GPIO_SX_RST, a ? 0 : 1); }
static void spi_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static int spi_dio1_read(void) { return gpio_get_level(SENTINEL_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int e) { (void)e; }

static const es_spi_interface_t g_spi_iface = {
    .init = spi_init, .cs_select = spi_cs_select, .cs_release = spi_cs_release,
    .transfer = spi_transfer, .reset = spi_reset, .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read, .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I²S Microphone Array Init === */
static i2s_chan_handle_t g_rx_chan;

static void i2s_mic_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    i2s_new_channel(&chan_cfg, NULL, &g_rx_chan);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .bclk = SENTINEL_GPIO_I2S_BCLK,
            .ws   = SENTINEL_GPIO_I2S_LRCLK,
            .dout = -1,
            .din  = SENTINEL_GPIO_I2S_DATA,
            .mclk = -1,
        },
    };
    i2s_channel_init_std_mode(g_rx_chan, &std_cfg);
    i2s_channel_enable(g_rx_chan);
}

/* === Mel-Spectrogram Computation === */
static void compute_mel_spectrogram(const int16_t *audio, size_t len,
                                     uint8_t *mel_out)
{
    /* In production: full FFT → mel filterbank → log compression → quantize */
    /* Simplified: compute energy in bands */
    for (int frame = 0; frame < SPECTRO_FRAMES; frame++) {
        int offset = frame * (SAMPLE_RATE * 2 / SPECTRO_FRAMES);
        if (offset >= len) offset = len - 1;
        for (int bin = 0; bin < MEL_BINS; bin++) {
            /* Energy in mel band (simplified) */
            int energy = 0;
            int band_size = (len - offset) / MEL_BINS;
            if (band_size < 1) band_size = 1;
            for (int s = 0; s < band_size && offset + bin * band_size + s < len; s++) {
                int16_t sample = audio[offset + bin * band_size + s];
                energy += (int)sample * sample;
            }
            energy = energy / (band_size > 0 ? band_size : 1);
            /* Quantize to 0-255 */
            int val = energy >> 8;
            if (val > 255) val = 255;
            mel_out[frame * MEL_BINS + bin] = (uint8_t)val;
        }
    }
}

/* === SoundNet Inference (simulated) ===
 * In production: TFLite-Micro int8 quantized CNN
 * Architecture:
 *   Input: 64×126 mel-spectrogram (int8)
 *   Conv2D(32, 3×3) → ReLU → MaxPool(2)
 *   Conv2D(64, 3×3) → ReLU → MaxPool(2)
 *   Conv2D(128, 3×3) → ReLU → MaxPool(2)
 *   Flatten → Dense(128) → Dense(20, softmax)
 *   Size: ~220 KB, Inference: <200 ms on ESP32-S3 @ 240 MHz
 */
static uint8_t soundnet_inference(const uint8_t *mel_spec, uint8_t *out_class)
{
    /* Placeholder: real implementation loads TFLite-Micro model */
    /* Simulate inference based on energy patterns */
    int total_energy = 0;
    for (int i = 0; i < MEL_BINS * SPECTRO_FRAMES; i++)
        total_energy += mel_spec[i];

    *out_class = 0;
    uint8_t confidence = 0;

    /* Simple heuristic for demonstration */
    if (total_energy > 500000) {
        *out_class = ES_SOUND_SMOKE_ALARM;
        confidence = 92;
    } else if (total_energy > 200000) {
        *out_class = ES_SOUND_DOORBELL;
        confidence = 88;
    } else if (total_energy > 100000) {
        *out_class = ES_SOUND_DOOR_KNOCK;
        confidence = 85;
    } else {
        return 0; /* No detection */
    }

    return confidence;
}

/* === Direction-of-Arrival (TDOA Beamforming) === */
static void estimate_direction(const int16_t audio_4ch[][AUDIO_CHUNK],
                                uint16_t *azimuth, int8_t *elevation)
{
    /* In production: SRP-PHAT with cross-correlation between mic pairs */
    /* Mic spacing: 50 mm square arrangement */
    /* Simplified: estimate from amplitude differences */
    int energy[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < AUDIO_CHUNK; j++)
            energy[i] += (int)audio_4ch[i][j] * audio_4ch[i][j];

    /* Amplitude ratio → approximate direction */
    float dx = (energy[1] - energy[0]) / (float)(energy[0] + energy[1] + 1);
    float dy = (energy[3] - energy[2]) / (float)(energy[2] + energy[3] + 1);
    *azimuth = (uint16_t)((atan2f(dy, dx) * 1800.0 / M_PI) + 3600) % 3600;
    *elevation = 0; /* Planar array → no elevation */
}

/* === SHT40 Temp/Humidity === */
static void read_sht40(float *temp, float *humidity)
{
    uint8_t cmd = 0xFD; /* Measure high resolution */
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, 0x44 << 1, true);
    i2c_master_write_byte(h, cmd, true);
    i2c_master_stop(h);
    i2c_master_cmd_begin(I2C_NUM_0, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t buf[6];
    h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, 0x44 << 1 | 1, true);
    i2c_master_read(h, buf, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(h);
    i2c_master_cmd_begin(I2C_NUM_0, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);

    uint16_t t_raw = (buf[0] << 8) | buf[1];
    uint16_t h_raw = (buf[3] << 8) | buf[4];
    *temp = -45 + 175 * (t_raw / 65535.0);
    *humidity = 100 * (h_raw / 65535.0);
}

/* === Sound Detection Task === */
static void sound_detection_task(void *arg)
{
    size_t bytes_read;
    int16_t raw_data[AUDIO_CHUNK * 2]; /* Stereo I²S */

    ESP_LOGI(TAG, "Sound detection task started (SoundNet CNN)");

    while (1) {
        /* Read 2 seconds of audio from I²S */
        int total_samples = 0;
        while (total_samples < (int)(SAMPLE_RATE * ES_AUDIO_BUFFER_SECONDS)) {
            i2s_channel_read(g_rx_chan, (char *)&raw_data[total_samples],
                            AUDIO_CHUNK * 2, &bytes_read, portMAX_DELAY);
            total_samples += bytes_read / 2;
        }

        /* Deinterleave 4-channel audio (TDM mode) */
        for (int i = 0; i < AUDIO_CHUNK && i < total_samples; i++) {
            g_audio_buffer[0][i] = raw_data[i * 4];
            g_audio_buffer[1][i] = raw_data[i * 4 + 1];
            g_audio_buffer[2][i] = raw_data[i * 4 + 2];
            g_audio_buffer[3][i] = raw_data[i * 4 + 3];
        }

        /* Compute mel-spectrogram from mic 0 (reference) */
        compute_mel_spectrogram(g_audio_buffer[0], AUDIO_CHUNK, g_mel_spectrogram);

        /* Run SoundNet inference */
        uint8_t detected_class;
        uint8_t confidence = soundnet_inference(g_mel_spectrogram, &detected_class);

        if (confidence >= ES_DETECTION_CONFIDENCE_MIN) {
            uint32_t now = esp_log_timestamp();
            uint32_t cooldown = IS_EMERGENCY_CLASS(detected_class)
                                 ? ES_EMERGENCY_COOLDOWN_MS
                                 : ES_DETECTION_COOLDOWN_MS;
            /* Check cooldown */
            if (detected_class != g_last_detected_class ||
                (now * 1000 - g_last_detection_time) > cooldown) {

                /* Estimate direction */
                uint16_t azimuth;
                int8_t elevation;
                estimate_direction(g_audio_buffer, &azimuth, &elevation);

                /* Compute SPL */
                float rms = 0;
                for (int i = 0; i < AUDIO_CHUNK; i++)
                    rms += (float)g_audio_buffer[0][i] * g_audio_buffer[0][i];
                rms = sqrtf(rms / AUDIO_CHUNK);
                uint8_t db_spl = (uint8_t)(20.0 * log10f(rms + 1) + 30);

                /* Get priority */
                uint8_t priority = es_class_priority[detected_class];

                /* Build and send telemetry */
                g_event_counter++;
                es_mesh_ctx_t *mesh = (es_mesh_ctx_t *)arg;

                /* Read temp/humidity */
                float temp, hum;
                read_sht40(&temp, &hum);

                es_message_t msg;
                es_build_sentinel_telem(&msg, mesh->node_id, mesh->msg_seq++,
                                        0xFF /* USB power */,
                                        detected_class, confidence,
                                        azimuth, elevation,
                                        (uint16_t)(ES_AUDIO_BUFFER_SECONDS * 1000),
                                        (int16_t)(temp * 10),
                                        (uint16_t)(hum * 10),
                                        db_spl, priority,
                                        g_event_counter, mesh->last_rssi);

                uint8_t buf[ES_MAX_MSG];
                size_t len = es_encode(&msg, buf, sizeof(buf));
                es_radio_tx(buf, (uint8_t)len);

                g_last_detected_class = detected_class;
                g_last_detection_time = now * 1000;

                ESP_LOGW(TAG, "DETECTED: %s conf=%d%% dir=%.1f° SPL=%ddB "
                         "priority=%d event=%d",
                         sound_names[detected_class], confidence,
                         azimuth / 10.0, db_spl, priority, g_event_counter);
            }
        }
    }
}

/* === Mesh Task === */
static void mesh_task(void *arg)
{
    es_radio_config_t radio_cfg = {
        .frequency = ES_NET_FREQ_HZ,
        .bandwidth = ES_NET_BW_HZ,
        .spreading_factor = ES_NET_SF,
        .coding_rate = ES_NET_CR,
        .preamble_len = ES_NET_PREAMBLE,
        .tx_power_dbm = ES_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    static es_mesh_ctx_t mesh;
    if (es_mesh_init(&mesh, ES_NODE_SENTINEL, &g_spi_iface, &radio_cfg) != 0) {
        ESP_LOGE(TAG, "Mesh init failed");
        vTaskDelete(NULL);
    }

    /* Join network */
    if (es_mesh_join(&mesh) != 0) {
        ESP_LOGE(TAG, "Mesh join failed");
        /* Continue in standalone mode */
    }
    ESP_LOGI(TAG, "Joined mesh: node_id=%d slot=%d", mesh.node_id, mesh.tdma_slot);

    /* Start sound detection with mesh context */
    xTaskCreate(sound_detection_task, "sound_task", 16384, &mesh, 5, NULL);

    /* Listen for commands from hub */
    es_message_t msg;
    while (1) {
        if (es_mesh_recv(&mesh, &msg, 5000) == 0) {
            if (msg.header.type == ES_MSG_COMMAND) {
                uint8_t cmd = msg.payload[0];
                ESP_LOGI(TAG, "Command from hub: cmd=%d", cmd);
                if (cmd == ES_CMD_START_ENROLL) {
                    ESP_LOGI(TAG, "Starting custom sound enrollment...");
                    /* In production: record 5s sample, send to hub */
                } else if (cmd == ES_CMD_CALIBRATE) {
                    ESP_LOGI(TAG, "Calibrating microphones...");
                } else if (cmd == ES_CMD_REBOOT) {
                    esp_restart();
                }
            }
        }
        /* Send heartbeat every 60 seconds */
        es_message_t hb;
        memset(&hb, 0, sizeof(hb));
        hb.header.sync[0] = ES_SYNC0;
        hb.header.sync[1] = ES_SYNC1;
        hb.header.src = mesh.node_id;
        hb.header.dst = 0x00;
        hb.header.type = ES_MSG_HEARTBEAT;
        hb.header.msg_id = mesh.msg_seq++;
        hb.payload[0] = 0xFF; /* USB power */
        hb.payload[1] = (uint8_t)mesh.last_rssi;
        hb.payload_len = 2;
        uint8_t buf[ES_MAX_MSG];
        size_t len = es_encode(&hb, buf, sizeof(buf));
        es_radio_tx(buf, (uint8_t)len);
    }
}

/* === I²C Init for SHT40 === */
static void i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SENTINEL_GPIO_SHT40_SDA,
        .scl_io_num = SENTINEL_GPIO_SHT40_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "EchoSync Room Sentinel starting...");

    /* GPIO setup for radio reset */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SENTINEL_GPIO_SX_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    i2c_init();
    i2s_mic_init();

    xTaskCreate(mesh_task, "mesh_task", 8192, NULL, 4, NULL);

    ESP_LOGI(TAG, "EchoSync Room Sentinel ready");
}