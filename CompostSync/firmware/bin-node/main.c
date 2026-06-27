/*
 * CompostSync Bin Node — Main
 * ESP32-WROOM-32E, FreeRTOS, solar-powered
 * firmware/bin-node/main.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "nvs_flash.h"

#include "csp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

static const char *TAG = "BIN_NODE";

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

#define PIN_DS18B20_1    32
#define PIN_DS18B20_2    33
#define PIN_DS18B20_3    34

#define PIN_MOISTURE_1   35  /* ADC1_CH7 */
#define PIN_MOISTURE_2   36  /* ADC1_CH0 */
#define PIN_MOISTURE_3   39  /* ADC1_CH3 */

#define PIN_MQ4          25  /* ADC2_CH8 */
#define PIN_HX711_DOUT   26
#define PIN_HX711_SCK    27

#define PIN_SERVO        18  /* LEDC PWM */
#define PIN_LED          19  /* WS2812B */

#define SAMPLE_INTERVAL_S  900  /* 15 min default */

/* Default AES key */
static const uint8_t aes_key[AES_KEY_SIZE] = {
    0x43, 0x6F, 0x6D, 0x70, 0x6F, 0x73, 0x74, 0x53,
    0x79, 0x6E, 0x63, 0x4B, 0x65, 0x79, 0x21, 0x00
};

static sx1262_t radio;
static uint32_t sample_interval = SAMPLE_INTERVAL_S;
static uint8_t  vent_position = 0;
static bool     tared = false;
static int32_t  tare_offset = 0;

/* Forward declarations */
extern void sensors_task(void *pvParameters);
extern void servo_task(void *pvParameters);
extern void lora_node_task(void *pvParameters);
extern void ble_bridge_task(void *pvParameters);
extern void power_task(void *pvParameters);

/* HAL for SX1262 (same pattern as Hub) */
static spi_device_handle_t spi_dev;

static void hal_spi_transfer(uint8_t *tx, uint8_t *rx, size_t len) {
    if (len == 0) return;
    spi_transaction_t t = { .length = len * 8, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(spi_dev, &t);
}
static void hal_cs_select(void)  { gpio_set_level(PIN_SX1262_NSS, 0); }
static void hal_cs_deselect(void){ gpio_set_level(PIN_SX1262_NSS, 1); }
static void hal_reset(bool s)    { gpio_set_level(PIN_SX1262_RST, s ? 0 : 1); }
static void hal_wait_busy(void)  {
    while (gpio_get_level(PIN_SX1262_BUSY) == 1) vTaskDelay(pdMS_TO_TICKS(1));
}
static bool hal_get_dio1(void)    { return gpio_get_level(PIN_SX1262_DIO1) == 1; }
static uint32_t hal_millis(void)  { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

/* Latest sensor reading (shared between tasks) */
static bin_node_data_t latest_data;

void app_main(void)
{
    ESP_LOGI(TAG, "CompostSync Bin Node starting...");

    nvs_flash_init();

    /* GPIO init */
    gpio_config_t io_out = {
        .pin_bit_mask = (1ULL << PIN_SX1262_RST) | (1ULL << PIN_SX1262_NSS) |
                        (1ULL << PIN_HX711_SCK),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_out);

    gpio_config_t io_in = {
        .pin_bit_mask = (1ULL << PIN_SX1262_DIO1) | (1ULL << PIN_SX1262_BUSY) |
                        (1ULL << PIN_DS18B20_1) | (1ULL << PIN_DS18B20_2) |
                        (1ULL << PIN_DS18B20_3) | (1ULL << PIN_HX711_DOUT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&io_in);

    /* ADC init for moisture sensors */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11); /* GPIO35 */
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11); /* GPIO36 */
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_11); /* GPIO39 */

    /* SPI for LoRa */
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_SX1262_MOSI,
        .miso_io_num = PIN_SX1262_MISO,
        .sclk_io_num = PIN_SX1262_SCK,
        .max_transfer_sz = 256,
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
    sx1262_init(&radio);
    sx1262_set_key(&radio, aes_key);

    /* Init latest_data */
    latest_data.node_id = NODE_ID_BIN;
    latest_data.uptime_s = 0;
    latest_data.vent_position = 0;
    latest_data.phase = PHASE_UNKNOWN;

    ESP_LOGI(TAG, "All hardware initialized. Starting tasks...");

    xTaskCreate(sensors_task,    "sensors", 8192, NULL, 5, NULL);
    xTaskCreate(lora_node_task,   "lora",    8192, &radio, 4, NULL);
    xTaskCreate(ble_bridge_task,  "ble",     4096, NULL, 2, NULL);
    xTaskCreate(power_task,       "power",   2048, NULL, 2, NULL);
    xTaskCreate(servo_task,       "servo",   2048, NULL, 3, NULL);

    ESP_LOGI(TAG, "Bin Node ready.");
}