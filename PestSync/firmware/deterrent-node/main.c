/*
 * Deterrent Node — Main
 * ESP32-C3, FreeRTOS, adaptive ultrasonic + strobe + diffuser
 * firmware/deterrent-node/main.c
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "nvs_flash.h"

#include "psp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

static const char *TAG = "DETER";

/* GPIO pins — ESP32-C3 */
#define PIN_SX1262_MOSI  2
#define PIN_SX1262_MISO  3
#define PIN_SX1262_SCK   4
#define PIN_SX1262_NSS   5
#define PIN_SX1262_RST   6
#define PIN_SX1262_DIO1  7
#define PIN_SX1262_BUSY  8

#define PIN_ULTRASONIC   9   /* PWM output to piezo transducer via MOSFET */
#define PIN_STROBE       10  /* Strobe LED via MOSFET */
#define PIN_DIFFUSER     1   /* Piezo atomizer disc via MOSFET */
#define PIN_OIL_ADC      0   /* ADC1_CH0 — oil level capacitive */
#define PIN_LED          18  /* WS2812B */
#define PIN_BUTTON       19

#define NODE_ID_SELF     NODE_ID_DETERRENT_BASE  /* 0x0030 */

static const uint8_t aes_key[AES_KEY_SIZE] = {
    0x50, 0x65, 0x73, 0x74, 0x53, 0x79, 0x6E, 0x63,
    0x4B, 0x65, 0x79, 0x21, 0x44, 0x54, 0x52, 0x00
};

static sx1262_t radio;
static uint8_t seq_num = 0;
static uint8_t my_slot = 5;

/* Forward declarations */
extern void ultrasonic_task(void *pvParameters);
extern void strobe_task(void *pvParameters);
extern void diffuser_task(void *pvParameters);
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
deterrent_data_t g_deterrent_data;

/* Deterrent configuration (set by Hub commands) */
volatile uint8_t  deter_mode = DETER_ADAPTIVE;
volatile uint8_t  deter_band = DETER_BAND_BOTH;
volatile uint16_t deter_duration_s = 300;  /* 5 min default activation */
volatile bool     ultrasonic_active = false;
volatile bool     strobe_active = false;
volatile bool     diffuser_active = false;
volatile uint32_t total_ultrasonic_s = 0;
volatile uint16_t diffuser_doses = 0;

void app_main(void)
{
    ESP_LOGI(TAG, "PestSync Deterrent Node starting (ESP32-C3)...");

    nvs_flash_init();

    /* GPIO init */
    gpio_config_t io_out = {
        .pin_bit_mask = (1ULL << PIN_SX1262_RST) | (1ULL << PIN_SX1262_NSS) |
                        (1ULL << PIN_ULTRASONIC) | (1ULL << PIN_STROBE) |
                        (1ULL << PIN_DIFFUSER) | (1ULL << PIN_LED),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_out);
    gpio_set_level(PIN_SX1262_NSS, 1);
    gpio_set_level(PIN_ULTRASONIC, 0);
    gpio_set_level(PIN_STROBE, 0);
    gpio_set_level(PIN_DIFFUSER, 0);

    gpio_config_t io_in = {
        .pin_bit_mask = (1ULL << PIN_SX1262_DIO1) | (1ULL << PIN_SX1262_BUSY) |
                        (1ULL << PIN_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
    };
    gpio_config(&io_in);

    /* ADC for oil level */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

    /* LEDC for ultrasonic PWM (frequency-agile) */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 25000, /* start at 25 kHz */
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t chan_cfg = {
        .channel = LEDC_CHANNEL_0,
        .duty = 128, /* 50% duty */
        .gpio_num = PIN_ULTRASONIC,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_0,
        .hpoint = 0,
    };
    ledc_channel_config(&chan_cfg);

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

    g_deterrent_data.node_id = NODE_ID_SELF;
    g_deterrent_data.oil_level = 100;

    ESP_LOGI(TAG, "Hardware initialized. Starting tasks...");

    xTaskCreate(ultrasonic_task, "us",    4096, NULL, 5, NULL);
    xTaskCreate(strobe_task,     "strobe", 2048, NULL, 4, NULL);
    xTaskCreate(diffuser_task,   "diff",   2048, NULL, 4, NULL);
    xTaskCreate(lora_node_task,  "lora",   4096, &radio, 4, NULL);
    xTaskCreate(power_task,      "power",  2048, NULL, 2, NULL);

    ESP_LOGI(TAG, "Deterrent Node ready. Mode: ADAPTIVE");
}