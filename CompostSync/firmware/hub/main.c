/*
 * CompostSync Hub — Main entry point
 * ESP32-WROOM-32E, FreeRTOS
 * firmware/hub/main.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "sdkconfig.h"

#include "csp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

static const char *TAG = "HUB";

/* GPIO pins */
#define PIN_SX1262_MOSI  14
#define PIN_SX1262_MISO  12
#define PIN_SX1262_SCK   13
#define PIN_SX1262 NSS   15
#define PIN_SX1262_RST   2
#define PIN_SX1262_DIO1  4
#define PIN_SX1262_BUSY  5

/* Default AES key (replace with provisioning in production) */
static const uint8_t aes_key[AES_KEY_SIZE] = {
    0x43, 0x6F, 0x6D, 0x70, 0x6F, 0x73, 0x74, 0x53,
    0x79, 0x6E, 0x63, 0x4B, 0x65, 0x79, 0x21, 0x00
};

/* Global radio handle */
static sx1262_t radio;

/* Message queue for received sensor data */
static QueueHandle_t telemetry_queue;

/* TDMA state */
typedef enum {
    TDMA_STATE_BEACON,
    TDMA_STATE_RX_BIN,
    TDMA_STATE_RX_BIN2,
    TDMA_STATE_RX_WEATHER,
    TDMA_STATE_CMD
} tdma_state_t;

/* Forward declarations */
extern void lora_mesh_task(void *pvParameters);
extern void ble_service_task(void *pvParameters);
extern void wifi_mqtt_task(void *pvParameters);
extern void edge_ml_task(void *pvParameters);
extern void display_task(void *pvParameters);
extern void sd_logger_task(void *pvParameters);

/* HAL implementation for SX1262 */
static spi_device_handle_t spi_dev;

static void hal_spi_transfer(uint8_t *tx, uint8_t *rx, size_t len)
{
    if (len == 0) return;
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_polling_transmit(spi_dev, &t);
}

static void hal_cs_select(void)  { gpio_set_level(PIN_SX1262_NSS, 0); }
static void hal_cs_deselect(void){ gpio_set_level(PIN_SX1262_NSS, 1); }
static void hal_reset(bool state) { gpio_set_level(PIN_SX1262_RST, state ? 0 : 1); }
static void hal_wait_busy(void)  {
    while (gpio_get_level(PIN_SX1262_BUSY) == 1) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
static bool hal_get_dio1(void)    { return gpio_get_level(PIN_SX1262_DIO1) == 1; }
static uint32_t hal_millis(void)  { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

static void init_spi(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SX1262_MOSI,
        .miso_io_num = PIN_SX1262_MISO,
        .sclk_io_num = PIN_SX1262_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 8 * 1000 * 1000, /* 8 MHz */
        .mode = 0,
        .spics_io_num = PIN_SX1262_NSS,
        .queue_size = 7,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi_dev);
}

static void init_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_SX1262_RST) | (1ULL << PIN_SX1262_NSS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << PIN_SX1262_DIO1) | (1ULL << PIN_SX1262_BUSY);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
}

void app_main(void)
{
    ESP_LOGI(TAG, "CompostSync Hub starting...");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* GPIO + SPI */
    init_gpio();
    init_spi();

    /* Radio HAL */
    radio.hal = (sx1262_hal_t){
        .spi_transfer = hal_spi_transfer,
        .cs_select    = hal_cs_select,
        .cs_deselect  = hal_cs_deselect,
        .reset        = hal_reset,
        .wait_busy    = hal_wait_busy,
        .get_dio1     = hal_get_dio1,
        .millis       = hal_millis,
    };

    /* Init LoRa radio */
    if (sx1262_init(&radio) != 0) {
        ESP_LOGE(TAG, "SX1262 init failed!");
    }
    sx1262_set_key(&radio, aes_key);
    ESP_LOGI(TAG, "LoRa 868 MHz initialized, SF11, 125kHz BW");

    /* Create telemetry queue */
    telemetry_queue = xQueueCreate(20, sizeof(bin_node_data_t));

    /* Start tasks */
    xTaskCreate(lora_mesh_task,    "lora",    8192, &radio,     5, NULL);
    xTaskCreate(ble_service_task,   "ble",    4096, NULL,       3, NULL);
    xTaskCreate(wifi_mqtt_task,     "mqtt",   8192, NULL,       4, NULL);
    xTaskCreate(edge_ml_task,       "ml",     8192, telemetry_queue, 4, NULL);
    xTaskCreate(display_task,       "disp",   4096, NULL,       2, NULL);
    xTaskCreate(sd_logger_task,     "sdlog",  4096, NULL,       2, NULL);

    ESP_LOGI(TAG, "All tasks started. Hub ready.");
}