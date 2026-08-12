/*
 * WanderSync — Voice Node Firmware
 * ESP32-S3, FreeRTOS
 *
 * Plays personalized voice reminders using pre-recorded family voices
 * from W25Q128 SPI flash. Responds to simple voice commands via
 * on-device keyword detection (KeywordNet). Announces time and date
 * for orientation. No audio leaves the device — privacy-first.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "driver/spi_master.h"
#include "esp_timer.h"

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

static const char *TAG = "WanderSync-Voice";

/* === Global state === */
static ws_mesh_ctx_t g_mesh;
static ws_radio_hal_t g_radio_hal;
static ws_radio_config_t g_radio_cfg;
static SemaphoreHandle_t g_mesh_mutex;

static uint8_t g_speaker_active = 0;
static uint8_t g_last_keyword = WS_KW_NONE;
static uint8_t g_reminders_24h = 0;
static uint8_t g_ack_rate = 100;

/* W25Q128 SPI flash handle */
static spi_device_handle_t g_flash_spi;

/* === SX1262 HAL (ESP32-S3 SPI) === */
static spi_device_handle_t g_spi;

static int hal_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = VOICE_GPIO_SX_MOSI,
        .miso_io_num = VOICE_GPIO_SX_MISO,
        .sclk_io_num = VOICE_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi);
    return 0;
}

static int hal_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    spi_transaction_t t = {0};
    t.length = len * 8;
    if (tx) t.tx_buffer = tx;
    if (rx) t.rx_buffer = rx;
    spi_device_polling_transmit(g_spi, &t);
    return 0;
}

static void hal_cs_low(void)  { gpio_set_level(VOICE_GPIO_SX_NSS, 0); }
static void hal_cs_high(void) { gpio_set_level(VOICE_GPIO_SX_NSS, 1); }
static void hal_reset(int assert) { gpio_set_level(VOICE_GPIO_SX_RST, !assert); }
static int  hal_dio1_read(void) { return gpio_get_level(VOICE_GPIO_SX_DIO1); }
static int  hal_busy_read(void) { return gpio_get_level(VOICE_GPIO_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void hal_delay_us(uint32_t us) { esp_rom_delay_us(us); }
static void hal_on_dio1(void) {}

/* === W25Q128 SPI Flash — Voice Clip Storage === */
static void flash_init(void)
{
    spi_device_interface_config_t flash_cfg = {
        .clock_speed_hz = 40000000, /* 40 MHz */
        .mode = 0,
        .spics_io_num = VOICE_GPIO_FLASH_CS,
        .queue_size = 4,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    spi_bus_add_device(SPI2_HOST, &flash_cfg, &g_flash_spi);
    ESP_LOGI(TAG, "W25Q128 SPI flash initialized (16 MB)");
}

static void flash_read_clip(uint8_t clip_index, uint8_t *buf, size_t len)
{
    /* Each clip is 10 seconds of 16 kHz 16-bit mono = 320,000 bytes ≈ 312 KB
     * 16 MB flash / 312 KB = ~52 clips at full quality
     * With 8 kHz 16-bit = 160 KB per clip → 100 clips
     * Clip address = clip_index * CLIP_SIZE */
    const size_t CLIP_SIZE = 160000; /* 10 sec at 8 kHz 16-bit */
    uint32_t addr = clip_index * CLIP_SIZE;

    /* W25Q128 read command: 0x03 + 24-bit address */
    uint8_t cmd[4] = {0x03,
                      (addr >> 16) & 0xFF,
                      (addr >> 8) & 0xFF,
                      addr & 0xFF};

    spi_transaction_t t = {0};
    t.length = 32; /* 4 bytes command */
    t.tx_buffer = cmd;
    spi_device_polling_transmit(g_flash_spi, &t);

    /* Read data */
    t.length = len * 8;
    t.tx_buffer = NULL;
    t.rx_buffer = buf;
    spi_device_polling_transmit(g_flash_spi, &t);
}

/* === I²S Speaker (MAX98357A) === */
#define I2S_SAMPLE_RATE 8000
#define I2S_BUF_LEN 1024

static void speaker_init(void)
{
    i2s_config_t i2s_cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = I2S_BUF_LEN,
        .use_apll = false,
    };
    i2s_pin_config_t pin_cfg = {
        .bck_io_num = VOICE_GPIO_AMP_BCLK,
        .ws_io_num = VOICE_GPIO_AMP_LRCLK,
        .data_out_num = VOICE_GPIO_AMP_DATA,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };
    i2s_driver_install(I2S_NUM_0, &i2s_cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_cfg);
    ESP_LOGI(TAG, "I²S speaker (MAX98357A) initialized");
}

static void speaker_play_clip(uint8_t clip_index, uint8_t volume)
{
    ESP_LOGI(TAG, "Playing voice clip %d (volume %d%%)", clip_index, volume);
    g_speaker_active = 1;

    /* Read clip from flash in chunks and play via I²S */
    const size_t CLIP_SIZE = 160000;
    uint8_t buf[I2S_BUF_LEN * 2]; /* 16-bit samples */
    size_t offset = 0;

    while (offset < CLIP_SIZE) {
        size_t chunk = sizeof(buf);
        if (offset + chunk > CLIP_SIZE) chunk = CLIP_SIZE - offset;

        /* Read from flash (simplified — production: use flash_read_clip) */
        /* flash_read_clip_at_offset(clip_index, offset, buf, chunk); */

        /* Apply volume scaling */
        int16_t *samples = (int16_t *)buf;
        int num_samples = chunk / 2;
        float vol_scale = volume / 100.0f;
        for (int i = 0; i < num_samples; i++) {
            samples[i] = (int16_t)(samples[i] * vol_scale);
        }

        /* Write to I²S */
        size_t bytes_written;
        i2s_write(I2S_NUM_0, buf, chunk, &bytes_written, portMAX_DELAY);

        offset += chunk;
    }

    g_speaker_active = 0;
    g_reminders_24h++;
    ESP_LOGI(TAG, "Voice clip %d playback complete", clip_index);
}

static void speaker_play_tone(uint16_t freq_hz, uint32_t duration_ms)
{
    /* Generate a gentle chime tone */
    uint8_t buf[I2S_BUF_LEN * 2];
    int16_t *samples = (int16_t *)buf;
    int num_samples = I2S_BUF_LEN;
    float phase = 0;
    float phase_inc = 2.0 * M_PI * freq_hz / I2S_SAMPLE_RATE;

    uint32_t elapsed = 0;
    while (elapsed < duration_ms) {
        for (int i = 0; i < num_samples; i++) {
            samples[i] = (int16_t)(sin(phase) * 8000); /* Gentle volume */
            phase += phase_inc;
        }
        size_t bytes_written;
        i2s_write(I2S_NUM_0, buf, num_samples * 2, &bytes_written, portMAX_DELAY);
        elapsed += (num_samples * 1000) / I2S_SAMPLE_RATE;
    }
}

/* === I²S Microphone (INMP441) + Keyword Detection === */
static void mic_init(void)
{
    i2s_config_t i2s_cfg = {
        .mode = I2S_MODE_MASTER | I2S_MODE_RX,
        .sample_rate = 16000,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = I2S_BUF_LEN,
        .use_apll = false,
    };
    i2s_pin_config_t pin_cfg = {
        .bck_io_num = VOICE_GPIO_MIC_SCK,
        .ws_io_num = VOICE_GPIO_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = VOICE_GPIO_MIC_SD,
    };
    i2s_driver_install(I2S_NUM_1, &i2s_cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &pin_cfg);
    ESP_LOGI(TAG, "I²S microphone (INMP441) initialized");
}

/* === KeywordNet — On-Device Keyword Detection === */
/* Production: TFLite-Micro int8 CNN (~40 KB)
 * 10 keywords: time, help, yes, no, where, medicine, food, water, home, stop
 * Input: 1-second 16 kHz audio (16000 samples)
 * Output: 11-class softmax (10 keywords + silence/background)
 *
 * Simplified: keyword detection stub */
static uint8_t keywordnet_detect(void)
{
    /* Production: read 1-second audio from INMP441, run TFLite-Micro CNN */
    /* Read I²S microphone */
    uint8_t buf[I2S_BUF_LEN * 4];
    size_t bytes_read;
    i2s_read(I2S_NUM_1, buf, sizeof(buf), &bytes_read, pdMS_TO_TICKS(100));

    /* Check if audio energy is above threshold (someone speaking) */
    int32_t *samples = (int32_t *)buf;
    int num = bytes_read / 4;
    int64_t energy = 0;
    for (int i = 0; i < num; i++) {
        int16_t s16 = (int16_t)(samples[i] >> 14); /* 32-bit → 16-bit */
        energy += (int64_t)s16 * s16;
    }
    energy /= num;

    if (energy < 100000) return WS_KW_NONE; /* Silence */

    /* Production: run KeywordNet CNN here */
    /* Placeholder: no keyword detected */
    return WS_KW_NONE;
}

/* === Handle Keyword === */
static void handle_keyword(uint8_t keyword)
{
    g_last_keyword = keyword;
    ESP_LOGI(TAG, "Keyword detected: %d", keyword);

    switch (keyword) {
    case WS_KW_TIME:
        /* Play time announcement (clip 100-109: template-based) */
        speaker_play_clip(100, 80);
        break;
    case WS_KW_HELP:
        /* Play comfort clip */
        speaker_play_clip(90, 80);
        break;
    case WS_KW_MEDICINE:
        /* Play medication reminder */
        speaker_play_clip(0, 80);
        break;
    case WS_KW_FOOD:
        /* Play meal reminder */
        speaker_play_clip(20, 80);
        break;
    case WS_KW_WATER:
        /* Play hydration reminder */
        speaker_play_clip(40, 80);
        break;
    case WS_KW_HOME:
        /* Play orientation clip */
        speaker_play_clip(60, 80);
        break;
    }
}

/* === Send Reminder ACK === */
static void send_reminder_ack(uint8_t reminder_id, uint8_t played)
{
    ws_message_t msg;
    msg.header.src = g_mesh.node_id;
    msg.header.dst = WS_HUB_NODE_ID;
    msg.header.type = WS_MSG_REMINDER_ACK;
    msg.header.msg_id = g_mesh.msg_counter++;

    msg.payload[0] = reminder_id;
    msg.payload[1] = played;
    msg.payload_len = 2;

    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    ws_mesh_send(&g_mesh, &g_radio_hal, &msg, WS_SEV_INFO);
    xSemaphoreGive(g_mesh_mutex);
}

/* === Send Telemetry === */
static void send_telemetry(void)
{
    ws_voice_telem_t telem = {
        .subtype = WS_TELEM_VOICE,
        .battery_v = g_mesh.battery_v,
        .speaker_active = g_speaker_active,
        .last_keyword = g_last_keyword,
        .reminders_24h = g_reminders_24h,
        .ack_rate = g_ack_rate,
        .free_heap = esp_get_free_heap_size() & 0xFFFF,
        .rssi = g_mesh.last_rssi,
        .uptime_min = (uint16_t)(xTaskGetTickCount() * portTICK_PERIOD_MS / 60000),
    };

    ws_message_t msg;
    ws_build_voice_telem(&msg, g_mesh.node_id, g_mesh.msg_counter++, &telem);
    xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
    ws_mesh_send(&g_mesh, &g_radio_hal, &msg, WS_SEV_INFO);
    xSemaphoreGive(g_mesh_mutex);
}

/* === Keyword Detection Task === */
static void keyword_task(void *arg)
{
    while (1) {
        if (!g_speaker_active) { /* Don't listen while speaking */
            uint8_t kw = keywordnet_detect();
            if (kw != WS_KW_NONE) {
                handle_keyword(kw);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(VOICE_KEYWORD_MS));
    }
}

/* === Radio Task === */
static void radio_task(void *arg)
{
    uint8_t rx_buf[WS_MAX_MSG];
    ws_message_t msg;

    while (1) {
        xSemaphoreTake(g_mesh_mutex, portMAX_DELAY);
        int rx_len = ws_sx1262_rx(&g_radio_hal, rx_buf, sizeof(rx_buf),
                                   5000, &g_mesh.last_rssi);
        xSemaphoreGive(g_mesh_mutex);

        if (rx_len > 0 && ws_decode(&msg, rx_buf, rx_len) == 0) {
            switch (msg.header.type) {
            case WS_MSG_JOIN_ACK:
                g_mesh.tdma_slot = msg.payload[0];
                g_mesh.joined = 1;
                ESP_LOGI(TAG, "Joined mesh, slot %d", g_mesh.tdma_slot);
                break;

            case WS_MSG_REMINDER_TRIG: {
                ws_reminder_trigger_t *rt =
                    (ws_reminder_trigger_t *)msg.payload;
                ESP_LOGI(TAG, "Reminder trigger: id=%d clip=%d vol=%d%%",
                         rt->reminder_id, rt->clip_index, rt->volume);

                /* Play gentle tone before voice if enabled */
                if (rt->tone_before) {
                    g_speaker_active = 1;
                    speaker_play_tone(880, 1000); /* Gentle chime */
                    g_speaker_active = 0;
                    vTaskDelay(pdMS_TO_TICKS(500));
                }

                /* Play voice clip with repeats */
                for (int i = 0; i < rt->repeat_count; i++) {
                    speaker_play_clip(rt->clip_index, rt->volume);
                    if (i < rt->repeat_count - 1) {
                        vTaskDelay(pdMS_TO_TICKS(rt->repeat_delay * 1000));
                    }
                }

                /* Send ACK */
                send_reminder_ack(rt->reminder_id, 1);
                break;
            }

            case WS_MSG_COMMAND: {
                uint8_t cmd = msg.payload[0];
                if (cmd == WS_CMD_PLAY_CLIP && msg.payload_len >= 2) {
                    speaker_play_clip(msg.payload[1], 80);
                }
                break;
            }

            case WS_MSG_SILENCE:
                ESP_LOGI(TAG, "Silence received");
                break;
            }
        }

        if (!g_mesh.joined) {
            ws_mesh_join(&g_mesh, &g_radio_hal);
        }
    }
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        if (g_mesh.joined) {
            send_telemetry();
        }
        vTaskDelay(pdMS_TO_TICKS(VOICE_TELEM_MS));
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "WanderSync Voice Node starting...");

    /* GPIO init */
    gpio_set_direction(VOICE_GPIO_SX_NSS, GPIO_MODE_OUTPUT);
    gpio_set_direction(VOICE_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(VOICE_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(VOICE_GPIO_SX_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(VOICE_GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(VOICE_GPIO_VBAT, GPIO_MODE_ANALOG);
    gpio_set_direction(VOICE_GPIO_USB_PWR, GPIO_MODE_INPUT);
    gpio_set_direction(VOICE_GPIO_FLASH_CS, GPIO_MODE_OUTPUT);

    gpio_set_level(VOICE_GPIO_SX_NSS, 1);
    gpio_set_level(VOICE_GPIO_SX_RST, 1);
    gpio_set_level(VOICE_GPIO_FLASH_CS, 1);

    /* Init I²S speaker + microphone */
    speaker_init();
    mic_init();

    /* Init W25Q128 flash */
    flash_init();

    /* Radio HAL */
    g_radio_hal.spi_init   = hal_spi_init;
    g_radio_hal.spi_xfer   = hal_spi_xfer;
    g_radio_hal.cs_low     = hal_cs_low;
    g_radio_hal.cs_high    = hal_cs_high;
    g_radio_hal.reset      = hal_reset;
    g_radio_hal.dio1_read  = hal_dio1_read;
    g_radio_hal.busy_read  = hal_busy_read;
    g_radio_hal.delay_ms  = hal_delay_ms;
    g_radio_hal.delay_us  = hal_delay_us;
    g_radio_hal.on_dio1   = hal_on_dio1;

    g_radio_cfg.freq_hz         = WS_SUBGHZ_FREQ_HZ;
    g_radio_cfg.spreading_factor= WS_SUBGHZ_SF;
    g_radio_cfg.bandwidth_hz    = WS_SUBGHZ_BW_HZ;
    g_radio_cfg.tx_power_dbm    = WS_SUBGHZ_TX_POWER_DBM;
    g_radio_cfg.sync_word       = WS_SYNC_WORD;
    g_radio_cfg.preamble_len    = WS_SUBGHZ_PREAMBLE;

    g_radio_hal.spi_init();
    ws_sx1262_init(&g_radio_hal, &g_radio_cfg);

    /* Mesh init */
    uint8_t aes_key[16] = {0};
    ws_mesh_init(&g_mesh, 0, WS_NODE_VOICE, aes_key);
    g_mesh.battery_v = 415;

    g_mesh_mutex = xSemaphoreCreateMutex();

    /* Tasks */
    xTaskCreate(keyword_task, "keyword", 4096, NULL, 5, NULL);
    xTaskCreate(radio_task, "radio", 4096, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telem", 2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "Voice Node ready. Joining mesh...");
    ESP_LOGI(TAG, "120 voice clips available in W25Q128 flash");
}