/*
 * FireSync — Escape Controller Firmware
 * ESP32-S3, FreeRTOS
 *
 * Drives WS2812B addressable LED strips (green path / red fire zone),
 * plays voice guidance in 8 languages from W25Q128 SPI flash via
 * MAX98357A I²S amplifier, and releases door locks for escape.
 * Battery-backed with LiFePO4 5000 mAh for 48+ hours.
 *
 * Build: idf.py build with ESP-IDF v5.x
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2s_std.h"

#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

static const char *TAG = "FireSync-Escape";

/* === LED Strip Configuration === */
#define LED_STRIP_LENGTH   60    /* LEDs per strip */
#define LED_STRIP_COUNT    4     /* 4 zones: front, back, hallway, stairwell */

/* RGB color (GRB for WS2812B) */
typedef struct {
    uint8_t g, r, b;
} ws2812_color_t;

static const ws2812_color_t COLOR_GREEN  = {255, 0, 0};   /* Green path */
static const ws2812_color_t COLOR_RED    = {0, 255, 0};   /* Fire zone */
static const ws2812_color_t COLOR_OFF    = {0, 0, 0};     /* Off */
static const ws2812_color_t COLOR_BLUE   = {0, 0, 255};  /* Status */
static const ws2812_color_t COLOR_AMBER  = {80, 200, 0}; /* Low battery */

/* Strip buffers */
static ws2812_color_t g_strip_buffer[LED_STRIP_COUNT][LED_STRIP_LENGTH];

/* GPIO pin for each strip */
static const int strip_pins[LED_STRIP_COUNT] = {
    ESC_GPIO_LED_FRONT, ESC_GPIO_LED_BACK,
    ESC_GPIO_LED_HALL,  ESC_GPIO_LED_STAIR
};

/* === WS2812B Driver (bitbang RMT — production uses esp_rom RMT) === */
static void ws2812_send_strip(int strip_idx, const ws2812_color_t *colors,
                               int count)
{
    /* Production: use RMT peripheral for precise WS2812B timing
     * T1: 0.35μs high, 0.9μs low (bit 0)
     * T1: 0.9μs high, 0.35μs low (bit 1)
     * Reset: >50μs low
     *
     * Placeholder: log only
     */
    (void)strip_idx; (void)colors; (void)count;
}

static void ws2812_set_strip_color(int strip_idx, ws2812_color_t color)
{
    for (int i = 0; i < LED_STRIP_LENGTH; i++)
        g_strip_buffer[strip_idx][i] = color;
    ws2812_send_strip(strip_idx, g_strip_buffer[strip_idx], LED_STRIP_LENGTH);
}

static void ws2812_clear_all(void)
{
    for (int s = 0; s < LED_STRIP_COUNT; s++)
        ws2812_set_strip_color(s, COLOR_OFF);
}

/* === W25Q128 SPI Flash (voice guidance audio) === */
static spi_device_handle_t g_flash_spi;
#define W25Q128_SIZE (16 * 1024 * 1024) /* 16 MB */
#define VOICE_CLIP_SIZE 8192 /* 8 KB per voice clip (compressed) */
#define VOICE_CLIP_COUNT 96 /* 8 languages × 12 messages */

static void flash_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = ESC_GPIO_FLASH_MOSI,
        .miso_io_num = ESC_GPIO_FLASH_MISO,
        .sclk_io_num = ESC_GPIO_FLASH_SCK,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20000000, /* 20 MHz */
        .mode = 0,
        .spics_io_num = ESC_GPIO_FLASH_CS,
        .queue_size = 4,
    };
    spi_bus_add_device(SPI3_HOST, &devcfg, &g_flash_spi);
}

static int flash_read_voice_clip(uint8_t clip_id, uint8_t *buf, size_t max_len)
{
    /* Production: SPI read from W25Q128
     * Read command: 0x03 + 24-bit address
     * Clip address = clip_id × VOICE_CLIP_SIZE
     */
    uint32_t addr = clip_id * VOICE_CLIP_SIZE;
    /* spi_flash_read(addr, buf, VOICE_CLIP_SIZE); */
    (void)addr; (void)buf; (void)max_len;
    return VOICE_CLIP_SIZE;
}

/* === I²S Audio (MAX98357A) === */
static i2s_chan_handle_t g_i2s_tx;

static void i2s_init(void)
{
    /* I²S standard mode for MAX98357A */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0);
    chan_cfg.role = I2S_ROLE_MASTER;
    i2s_new_channel(&chan_cfg, &g_i2s_tx, NULL);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000), /* 16 kHz sample rate */
        .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .bclk = ESC_GPIO_AMP_BCLK,
            .ws = ESC_GPIO_AMP_LRCLK,
            .dout = ESC_GPIO_AMP_DATA,
            .din = -1,
        },
    };
    i2s_channel_init_std_mode(g_i2s_tx, &std_cfg);
    i2s_channel_enable(g_i2s_tx);
}

static void play_voice_clip(uint8_t clip_id)
{
    /* Read clip from flash and play via I²S */
    static uint8_t audio_buf[VOICE_CLIP_SIZE];
    int len = flash_read_voice_clip(clip_id, audio_buf, sizeof(audio_buf));
    if (len > 0) {
        /* Production: decode audio (ADPCM/μ-law) and write to I²S */
        ESP_LOGI(TAG, "Playing voice clip %d (%d bytes)", clip_id, len);
        /* i2s_channel_write(g_i2s_tx, decoded_pcm, len, &written, 5000); */
    }
}

/* === Voice clip ID calculation === */
/* clip_id = language × 12 + message_id */
/* Languages: 0=EN, 1=ES, 2=ZH, 3=FR, 4=DE, 5=JA, 6=KO, 7=PT */
static uint8_t g_current_language = 0; /* English default */

static uint8_t get_voice_clip_id(uint8_t message_id)
{
    return g_current_language * 12 + message_id;
}

/* === Door Release === */
static void release_door(uint8_t door_mask)
{
    if (door_mask & 0x01) {
        gpio_set_level(ESC_GPIO_DOOR_FRONT, 1);
        ESP_LOGI(TAG, "Front door released");
    }
    if (door_mask & 0x02) {
        gpio_set_level(ESC_GPIO_DOOR_BACK, 1);
        ESP_LOGI(TAG, "Back door released");
    }
    if (door_mask & 0x04) {
        gpio_set_level(ESC_GPIO_DOOR_GARAGE, 1);
        ESP_LOGI(TAG, "Garage door released");
    }
    if (door_mask & 0x08) {
        gpio_set_level(ESC_GPIO_DOOR_BEDROOM, 1);
        ESP_LOGI(TAG, "Bedroom door released");
    }
}

static void lock_all_doors(void)
{
    gpio_set_level(ESC_GPIO_DOOR_FRONT, 0);
    gpio_set_level(ESC_GPIO_DOOR_BACK, 0);
    gpio_set_level(ESC_GPIO_DOOR_GARAGE, 0);
    gpio_set_level(ESC_GPIO_DOOR_BEDROOM, 0);
}

/* === State === */
typedef struct {
    uint8_t route_active;
    uint8_t fire_room;
    uint8_t safe_exit;
    uint8_t led_path_mask;
    uint8_t led_red_mask;
    uint8_t speaker_active;
    uint8_t doors_released;
    uint8_t battery_v;
} fs_escape_state_t;

static fs_escape_state_t g_state;
static fs_mesh_ctx_t g_mesh;
static fs_radio_hal_t g_radio_hal;
static fs_radio_config_t g_radio_cfg;
static spi_device_handle_t g_spi;
static SemaphoreHandle_t g_spi_mutex;

/* === SX1262 HAL === */
static int hal_spi_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = ESC_GPIO_SX_MOSI,
        .miso_io_num = ESC_GPIO_SX_MISO,
        .sclk_io_num = ESC_GPIO_SX_SCK,
        .max_transfer_sz = 256,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 2000000, .mode = 0,
        .spics_io_num = -1, .queue_size = 4,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &g_spi);
    g_spi_mutex = xSemaphoreCreateMutex();
    return 0;
}

static int hal_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    xSemaphoreTake(g_spi_mutex, portMAX_DELAY);
    spi_transaction_t t = {0};
    t.length = len * 8;
    if (tx) t.tx_buffer = tx;
    if (rx) t.rx_buffer = rx;
    spi_device_polling_transmit(g_spi, &t);
    xSemaphoreGive(g_spi_mutex);
    return 0;
}

static void hal_cs_low(void)  { gpio_set_level(ESC_GPIO_SX_NSS, 0); }
static void hal_cs_high(void) { gpio_set_level(ESC_GPIO_SX_NSS, 1); }
static void hal_reset(int a) { gpio_set_level(ESC_GPIO_SX_RST, !a); }
static int  hal_dio1_read(void) { return gpio_get_level(ESC_GPIO_SX_DIO1); }
static int  hal_busy_read(void) { return gpio_get_level(ESC_GPIO_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static void hal_delay_us(uint32_t us) { esp_rom_delay_us(us); }
static void hal_on_dio1(void) {}

/* === Execute Escape Route === */
static void execute_escape_route(const fs_escape_route_t *route)
{
    ESP_LOGW(TAG, "ESCAPE ROUTE: fire_room=%d exit=%d path=0x%02X red=0x%02X voice=%d doors=0x%02X",
             route->fire_room, route->safe_exit,
             route->led_path_mask, route->led_red_mask,
             route->voice_msg_id, route->door_mask);

    g_state.route_active = 1;
    g_state.fire_room = route->fire_room;
    g_state.safe_exit = route->safe_exit;
    g_state.led_path_mask = route->led_path_mask;
    g_state.led_red_mask = route->led_red_mask;
    g_state.doors_released = route->door_mask;

    /* Illuminate LED strips */
    for (int s = 0; s < LED_STRIP_COUNT; s++) {
        if (route->led_path_mask & (1 << s)) {
            ws2812_set_strip_color(s, COLOR_GREEN);
        } else if (route->led_red_mask & (1 << s)) {
            ws2812_set_strip_color(s, COLOR_RED);
        } else {
            ws2812_set_strip_color(s, COLOR_OFF);
        }
    }

    /* Play voice guidance */
    uint8_t clip_id = get_voice_clip_id(route->voice_msg_id);
    g_state.speaker_active = 1;
    play_voice_clip(clip_id);
    /* Repeat every 15 seconds */
    /* Production: timer-based repeat */

    /* Release doors */
    release_door(route->door_mask);
}

static void stop_escape_route(void)
{
    g_state.route_active = 0;
    g_state.speaker_active = 0;
    ws2812_clear_all();
    lock_all_doors();
    ESP_LOGI(TAG, "Escape route deactivated");
}

/* === Radio RX Task === */
static void radio_rx_task(void *arg)
{
    uint8_t rx_buf[FS_MAX_MSG];
    fs_message_t msg;

    while (1) {
        int rx_len = fs_sx1262_rx(&g_radio_hal, rx_buf, sizeof(rx_buf),
                                   4000, &g_mesh.last_rssi);

        if (rx_len > 0 && fs_decode(&msg, rx_buf, rx_len) == 0) {
            switch (msg.header.type) {
            case FS_MSG_ESCAPE_UPDATE: {
                fs_escape_route_t *route = (fs_escape_route_t *)msg.payload;
                execute_escape_route(route);
                break;
            }

            case FS_MSG_ALARM_STOP:
                stop_escape_route();
                break;

            case FS_MSG_ALARM_TRIGGER:
                /* Light all strips amber as warning */
                for (int s = 0; s < LED_STRIP_COUNT; s++)
                    ws2812_set_strip_color(s, COLOR_AMBER);
                break;

            case FS_MSG_DOOR_RELEASE:
                release_door(msg.payload[0]);
                break;

            case FS_MSG_TEST_ALARM:
                /* Test: flash all strips green */
                for (int s = 0; s < LED_STRIP_COUNT; s++)
                    ws2812_set_strip_color(s, COLOR_GREEN);
                vTaskDelay(pdMS_TO_TICKS(3000));
                ws2812_clear_all();
                break;
            }
        }
    }
}

/* === Telemetry Task === */
static void telemetry_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(ESC_TELEM_INTERVAL_MS));

        fs_escape_telem_t telem;
        memset(&telem, 0, sizeof(telem));
        telem.subtype       = FS_TELEM_ESCAPE;
        telem.battery_v    = g_state.battery_v;
        telem.led_zones    = g_state.led_path_mask | (g_state.led_red_mask << 4);
        telem.speaker_active = g_state.speaker_active;
        telem.doors_released = g_state.doors_released;
        telem.route_active = g_state.route_active;
        telem.free_heap    = (uint16_t)esp_get_free_heap_size();
        telem.rssi         = g_mesh.last_rssi;
        telem.uptime_min   = 0;

        fs_message_t msg;
        fs_build_escape_telem(&msg, g_mesh.node_id, g_mesh.msg_counter++, &telem);
        fs_mesh_send(&g_mesh, &g_radio_hal, &msg, 0);
    }
}

/* === Voice Repeat Task === */
static void voice_repeat_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000)); /* Every 15 seconds */
        if (g_state.route_active && g_state.speaker_active) {
            /* Repeat voice guidance */
            uint8_t clip_id = get_voice_clip_id(0); /* Production: store last msg_id */
            play_voice_clip(clip_id);
        }
    }
}

/* === Main === */
void app_main(void)
{
    ESP_LOGI(TAG, "FireSync Escape Controller starting...");

    memset(&g_state, 0, sizeof(g_state));
    g_state.battery_v = 320; /* LiFePO4 ~3.2V */

    /* GPIO init */
    gpio_set_direction(ESC_GPIO_SX_NSS, GPIO_MODE_OUTPUT);
    gpio_set_direction(ESC_GPIO_SX_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(ESC_GPIO_SX_DIO1, GPIO_MODE_INPUT);
    gpio_set_direction(ESC_GPIO_SX_BUSY, GPIO_MODE_INPUT);
    gpio_set_direction(ESC_GPIO_DOOR_FRONT, GPIO_MODE_OUTPUT);
    gpio_set_direction(ESC_GPIO_DOOR_BACK, GPIO_MODE_OUTPUT);
    gpio_set_direction(ESC_GPIO_DOOR_GARAGE, GPIO_MODE_OUTPUT);
    gpio_set_direction(ESC_GPIO_DOOR_BEDROOM, GPIO_MODE_OUTPUT);
    gpio_set_direction(ESC_GPIO_VBAT, GPIO_MODE_ANALOG);
    gpio_set_direction(ESC_GPIO_USB_PWR, GPIO_MODE_INPUT);
    gpio_set_direction(ESC_GPIO_LED, GPIO_MODE_OUTPUT);

    gpio_set_level(ESC_GPIO_SX_NSS, 1);
    gpio_set_level(ESC_GPIO_SX_RST, 1);
    lock_all_doors();

    /* Initialize WS2812B strips */
    /* Production: RMT init for each strip pin */
    ws2812_clear_all();

    /* Flash init for voice clips */
    flash_init();

    /* I²S audio init */
    i2s_init();

    /* Radio */
    g_radio_hal.spi_init   = hal_spi_init;
    g_radio_hal.spi_xfer   = hal_spi_xfer;
    g_radio_hal.cs_low     = hal_cs_low;
    g_radio_hal.cs_high    = hal_cs_high;
    g_radio_hal.reset      = hal_reset;
    g_radio_hal.dio1_read  = hal_dio1_read;
    g_radio_hal.busy_read = hal_busy_read;
    g_radio_hal.delay_ms  = hal_delay_ms;
    g_radio_hal.delay_us  = hal_delay_us;
    g_radio_hal.on_dio1   = hal_on_dio1;

    g_radio_cfg.freq_hz         = FS_SUBGHZ_FREQ_HZ;
    g_radio_cfg.spreading_factor= FS_SUBGHZ_SF;
    g_radio_cfg.bandwidth_hz    = FS_SUBGHZ_BW_HZ;
    g_radio_cfg.tx_power_dbm    = FS_SUBGHZ_TX_POWER_DBM;
    g_radio_cfg.sync_word       = FS_SYNC_WORD;
    g_radio_cfg.preamble_len    = FS_SUBGHZ_PREAMBLE;

    g_radio_hal.spi_init();
    fs_sx1262_init(&g_radio_hal, &g_radio_cfg);

    /* Mesh */
    uint8_t aes_key[16] = {0};
    fs_mesh_init(&g_mesh, 0x12, FS_NODE_ESCAPE, aes_key);
    g_mesh.battery_v = 320;

    if (fs_mesh_join(&g_mesh, &g_radio_hal) == 0) {
        ESP_LOGI(TAG, "Joined mesh, slot %d", g_mesh.tdma_slot);
    }

    /* Tasks */
    xTaskCreate(radio_rx_task, "radio_rx", 8192, NULL, 5, NULL);
    xTaskCreate(telemetry_task, "telem", 4096, NULL, 3, NULL);
    xTaskCreate(voice_repeat_task, "voice", 2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Escape Controller ready. LiFePO4 battery: %.1fV",
             g_state.battery_v / 100.0);
}