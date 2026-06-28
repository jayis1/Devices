/*
 * Smart Trap — Main
 * ESP32-C3, FreeRTOS, event-driven, ultra-low-power
 * firmware/smart-trap/main.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "nvs_flash.h"

#include "psp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

static const char *TAG = "TRAP";

/* GPIO pins — ESP32-C3 */
#define PIN_SX1262_MOSI  2
#define PIN_SX1262_MISO  3
#define PIN_SX1262_SCK   4
#define PIN_SX1262_NSS   5
#define PIN_SX1262_RST   6
#define PIN_SX1262_DIO1  7
#define PIN_SX1262_BUSY  8

#define PIN_REED         0   /* Reed switch input (GPIO interrupt) */
#define PIN_HX711_DOUT   1
#define PIN_HX711_SCK    10
#define PIN_BAIT_ADC     9   /* ADC1_CH0 */
#define PIN_ADXL_CS      21
#define PIN_ADXL_INT     20
#define PIN_LED          18  /* Bicolor LED: red=triggered, green=armed */

#define NODE_ID_SELF     NODE_ID_TRAP_BASE  /* 0x0020 */

static const uint8_t aes_key[AES_KEY_SIZE] = {
    0x50, 0x65, 0x73, 0x74, 0x53, 0x79, 0x6E, 0x63,
    0x4B, 0x65, 0x79, 0x21, 0x54, 0x52, 0x50, 0x00
};

static sx1262_t radio;
static uint8_t seq_num = 0;
static uint8_t my_slot = 3;

/* Forward declarations */
extern void trap_sensors_task(void *pvParameters);
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

/* Shared state */
trap_data_t g_trap_data;
volatile bool trap_triggered = false;
volatile bool trap_tampered = false;

/* Reed switch ISR */
static void IRAM_ATTR reed_isr_handler(void *arg)
{
    trap_triggered = true;
}

/* ADXL362 tamper ISR */
static void IRAM_ATTR adxl_isr_handler(void *arg)
{
    trap_tampered = true;
}

void app_main(void)
{
    ESP_LOGI(TAG, "PestSync Smart Trap starting (ESP32-C3)...");

    nvs_flash_init();

    /* GPIO init */
    gpio_config_t io_out = {
        .pin_bit_mask = (1ULL << PIN_SX1262_RST) | (1ULL << PIN_SX1262_NSS) |
                        (1ULL << PIN_HX711_SCK) | (1ULL << PIN_ADXL_CS) |
                        (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_out);
    gpio_set_level(PIN_SX1262_NSS, 1);
    gpio_set_level(PIN_ADXL_CS, 1);
    gpio_set_level(PIN_LED, 0); /* green=armed (will be set in task) */

    gpio_config_t io_in = {
        .pin_bit_mask = (1ULL << PIN_SX1262_DIO1) | (1ULL << PIN_SX1262_BUSY) |
                        (1ULL << PIN_REED) | (1ULL << PIN_HX711_DOUT) |
                        (1ULL << PIN_ADXL_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&io_in);

    /* Reed switch interrupt */
    gpio_set_intr_type(PIN_REED, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_REED, reed_isr_handler, NULL);

    /* ADXL362 interrupt */
    gpio_set_intr_type(PIN_ADXL_INT, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(PIN_ADXL_INT, adxl_isr_handler, NULL);

    /* ADC for bait level */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

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

    /* Init trap data */
    g_trap_data.node_id = NODE_ID_SELF;
    g_trap_data.trap_status = TRAP_ARMED;
    g_trap_data.catch_weight_g = 0;
    g_trap_data.bait_level = 100;
    g_trap_data.catch_class = CATCH_UNKNOWN;

    ESP_LOGI(TAG, "Hardware initialized. Starting tasks...");

    xTaskCreate(trap_sensors_task, "sensors", 4096, NULL, 5, NULL);
    xTaskCreate(lora_node_task,    "lora",    4096, &radio, 4, NULL);
    xTaskCreate(power_task,        "power",   2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Smart Trap ready. Status: ARMED");
}