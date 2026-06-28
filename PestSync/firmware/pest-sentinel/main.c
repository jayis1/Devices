/*
 * Pest Sentinel — Main
 * ESP32-S3-N8R2, FreeRTOS, camera + ML + thermal + Sub-GHz
 * firmware/pest-sentinel/main.c
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

#include "psp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

static const char *TAG = "SENTINEL";

/* GPIO pins — ESP32-S3 */
#define PIN_CAM_D0      4
#define PIN_CAM_VSYNC    5
#define PIN_CAM_HREF     6
#define PIN_CAM_PCLK     7
#define PIN_CAM_XCLK     8
#define PIN_CAM_SDA      9
#define PIN_CAM_SCL      10
#define PIN_CAM_D2       11
#define PIN_CAM_D3       12
#define PIN_CAM_D4       13
#define PIN_CAM_D5       14
#define PIN_CAM_D6       15
#define PIN_CAM_D7       16

#define PIN_PIR          17
#define PIN_MLX_SDA      18  /* I2C #2 for MLX90640 */
#define PIN_MLX_SCL      8   /* shared via mux in real impl */

#define PIN_SX1262_MOSI  38
#define PIN_SX1262_MISO  37
#define PIN_SX1262_SCK   36
#define PIN_SX1262_NSS   35
#define PIN_SX1262_RST   1
#define PIN_SX1262_DIO1  2
#define PIN_SX1262_BUSY  3

#define PIN_IR_LED       41
#define PIN_LED          42  /* WS2812B */
#define PIN_BUTTON       21

#define NODE_ID_SELF     NODE_ID_SENTINEL_BASE  /* 0x0010 */

/* Default AES key */
static const uint8_t aes_key[AES_KEY_SIZE] = {
    0x50, 0x65, 0x73, 0x74, 0x53, 0x79, 0x6E, 0x63,
    0x4B, 0x65, 0x79, 0x21, 0x53, 0x4E, 0x54, 0x00
};

static sx1262_t radio;
static uint8_t seq_num = 0;

/* Forward declarations */
extern void camera_task(void *pvParameters);
extern void pest_cnn_task(void *pvParameters);
extern void thermal_task(void *pvParameters);
extern void pir_task(void *pvParameters);
extern void lora_node_task(void *pvParameters);
extern void power_task(void *pvParameters);

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

/* Latest detection result (shared) */
sentinel_data_t g_sentinel_data;
uint8_t g_latest_pest_class = PEST_NONE;
uint8_t g_latest_confidence = 0;
uint16_t g_detection_count = 0;
float g_thermal_max_c = 0.0f;
bool g_ir_illumination_on = false;

void app_main(void)
{
    ESP_LOGI(TAG, "PestSync Sentinel starting (ESP32-S3)...");

    nvs_flash_init();

    /* GPIO init */
    gpio_config_t io_out = {
        .pin_bit_mask = (1ULL << PIN_SX1262_RST) | (1ULL << PIN_SX1262_NSS) |
                        (1ULL << PIN_IR_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_out);
    gpio_set_level(PIN_SX1262_NSS, 1);
    gpio_set_level(PIN_IR_LED, 0); /* IR off initially */

    gpio_config_t io_in = {
        .pin_bit_mask = (1ULL << PIN_SX1262_DIO1) | (1ULL << PIN_SX1262_BUSY) |
                        (1ULL << PIN_PIR) | (1ULL << PIN_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&io_in);

    /* ADC for battery monitoring */
    adc1_config_width(ADC_WIDTH_BIT_12);
    /* Battery on ADC1_CH0 (GPIO1) via divider — adjust per HW */

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
    spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI3_HOST, &devcfg, &spi_dev);

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

    /* Init sentinel data */
    g_sentinel_data.node_id = NODE_ID_SELF;
    g_sentinel_data.pest_class = PEST_NONE;
    g_sentinel_data.confidence = 0;
    g_sentinel_data.count_since_last = 0;

    ESP_LOGI(TAG, "Hardware initialized. Starting tasks...");

    xTaskCreate(pir_task,        "pir",     2048, NULL, 6, NULL);
    xTaskCreate(camera_task,     "camera",  16384, NULL, 5, NULL);
    xTaskCreate(pest_cnn_task,   "cnn",     16384, NULL, 5, NULL);
    xTaskCreate(thermal_task,    "thermal", 4096, NULL, 4, NULL);
    xTaskCreate(lora_node_task,  "lora",    8192, &radio, 4, NULL);
    xTaskCreate(power_task,      "power",   2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Pest Sentinel ready.");
}