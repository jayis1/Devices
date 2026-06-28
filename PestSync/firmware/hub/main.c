/*
 * PestSync Hub — Main
 * ESP32-WROOM-32E, FreeRTOS
 * firmware/hub/main.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "nvs_flash.h"

#include "psp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

static const char *TAG = "HUB";

/* GPIO pins */
#define PIN_SX1262_MOSI  14
#define PIN_SX1262_MISO  12
#define PIN_SX1262_SCK   13
#define PIN_SX1262_NSS   15
#define PIN_SX1262_RST   2
#define PIN_SX1262_DIO1  4
#define PIN_SX1262_BUSY  5

#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22

#define PIN_SD_MISO      19
#define PIN_SD_MOSI      23
#define PIN_SD_SCK       18
#define PIN_SD_CS        25

#define PIN_LED          26  /* WS2812B */
#define PIN_BUTTON       27

/* Default AES key */
static const uint8_t aes_key[AES_KEY_SIZE] = {
    0x50, 0x65, 0x73, 0x74, 0x53, 0x79, 0x6E, 0x63,
    0x4B, 0x65, 0x79, 0x21, 0x48, 0x55, 0x42, 0x00
};

static sx1262_t radio;

/* Forward declarations */
extern void lora_mesh_task(void *pvParameters);
extern void ble_service_task(void *pvParameters);
extern void wifi_mqtt_task(void *pvParameters);
extern void edge_ml_task(void *pvParameters);
extern void display_task(void *pvParameters);
extern void sd_logger_task(void *pvParameters);

/* HAL for SX1262 */
static spi_device_handle_t spi_dev;

static void hal_spi_transfer(uint8_t *tx, uint8_t *rx, size_t len)
{
    if (len == 0) return;
    spi_transaction_t t = { .length = len * 8, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(spi_dev, &t);
}
static void hal_cs_select(void)   { gpio_set_level(PIN_SX1262_NSS, 0); }
static void hal_cs_deselect(void) { gpio_set_level(PIN_SX1262_NSS, 1); }
static void hal_reset(bool s)     { gpio_set_level(PIN_SX1262_RST, s ? 0 : 1); }
static void hal_wait_busy(void)   {
    while (gpio_get_level(PIN_SX1262_BUSY) == 1) vTaskDelay(pdMS_TO_TICKS(1));
}
static bool hal_get_dio1(void)    { return gpio_get_level(PIN_SX1262_DIO1) == 1; }
static uint32_t hal_millis(void)  { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

/* Latest data from each node type (shared with tasks) */
sentinel_data_t  g_sentinel_data;
trap_data_t      g_trap_data;
deterrent_data_t g_deterrent_data;

/* Activity heatmap: per-hour detection count (24 hours) */
static uint16_t activity_heatmap[24] = {0};

void app_main(void)
{
    ESP_LOGI(TAG, "PestSync Hub starting...");

    nvs_flash_init();

    /* GPIO init */
    gpio_config_t io_out = {
        .pin_bit_mask = (1ULL << PIN_SX1262_RST) | (1ULL << PIN_SX1262_NSS) |
                        (1ULL << PIN_SD_CS),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_out);
    gpio_set_level(PIN_SX1262_NSS, 1);
    gpio_set_level(PIN_SD_CS, 1);

    gpio_config_t io_in = {
        .pin_bit_mask = (1ULL << PIN_SX1262_DIO1) | (1ULL << PIN_SX1262_BUSY) |
                        (1ULL << PIN_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&io_in);

    /* SPI for Sub-GHz radio */
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SX1262_MOSI,
        .miso_io_num = PIN_SX1262_MISO,
        .sclk_io_num = PIN_SX1262_SCK,
        .max_transfer_sz = 512,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8000000,
        .mode = 0,
        .spics_io_num = PIN_SX1262_NSS,
        .queue_size = 7,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi_dev);

    /* Radio init */
    radio.hal = (sx1262_hal_t){
        .spi_transfer = hal_spi_transfer,
        .cs_select    = hal_cs_select,
        .cs_deselect  = hal_cs_deselect,
        .reset        = hal_reset,
        .wait_busy    = hal_wait_busy,
        .get_dio1     = hal_get_dio1,
        .millis       = hal_millis,
    };
    radio.freq_hz = LORA_FREQ;
    radio.bw_hz = LORA_BW;
    radio.sf = LORA_SF;
    radio.cr = LORA_CR;
    radio.tx_power_dbm = LORA_TX_POWER;
    radio.sync_word = LORA_SYNC_WORD;
    sx1262_init(&radio);
    sx1262_set_key(&radio, aes_key);

    ESP_LOGI(TAG, "Hardware initialized. Starting tasks...");

    xTaskCreate(lora_mesh_task,   "lora",   8192, &radio, 5, NULL);
    xTaskCreate(ble_service_task,  "ble",    4096, NULL, 3, NULL);
    xTaskCreate(wifi_mqtt_task,    "wifi",   8192, NULL, 4, NULL);
    xTaskCreate(edge_ml_task,      "ml",     8192, NULL, 3, NULL);
    xTaskCreate(display_task,      "disp",   2048, NULL, 2, NULL);
    xTaskCreate(sd_logger_task,    "sdlog",  4096, NULL, 2, NULL);

    ESP_LOGI(TAG, "PestSync Hub ready.");
}