/*
 * CompostSync Weather Station — Main
 * nRF52840 (Adafruit Feather nRF52840 Express)
 * nRF SDK 17 + FreeRTOS
 * firmware/weather-station/main.c
 */
#include <stdint.h>
#include <string.h>
#include "nrf.h"
#include "nrf_drv_rtc.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_adc.h"
#include "app_util_platform.h"
#include "app_error.h"
#include "boards.h"
#include "nrf_delay.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_timer.h"
#include "nrf_drv_clock.h"
#include "nrfx_gpiote.h"

#include "csp_protocol.h"
#include "sx1262_driver.h"
#include "sensor_types.h"

/* Pin definitions */
#define PIN_I2C_SDA     26   /* P0.26 */
#define PIN_I2C_SCL     27   /* P0.27 */
#define PIN_ANEMOMETER  1    /* P1.01 (reed switch, GPIOTE) */
#define PIN_WIND_VANE   4    /* P0.04 (ADC) */
#define PIN_RAIN_GAUGE   2    /* P1.02 (reed switch, GPIOTE) */

#define PIN_SX1262_MOSI 15   /* P1.15 */
#define PIN_SX1262_MISO 13   /* P1.13 */
#define PIN_SX1262_SCK  14   /* P1.14 */
#define PIN_SX1262_NSS  12   /* P1.12 */
#define PIN_SX1262_RST  11   /* P1.11 */
#define PIN_SX1262_DIO1 10   /* P1.10 */
#define PIN_SX1262_BUSY 8    /* P0.08 */

/* BME280 I2C address */
#define BME280_ADDR 0x76

/* Default AES key */
static const uint8_t aes_key[AES_KEY_SIZE] = {
    0x43, 0x6F, 0x6D, 0x70, 0x6F, 0x73, 0x74, 0x53,
    0x79, 0x6E, 0x63, 0x4B, 0x65, 0x79, 0x21, 0x00
};

static sx1262_t radio;
static weather_data_t latest_data;

/* Counters for wind speed and rain */
static volatile uint32_t wind_pulse_count = 0;
static volatile uint32_t rain_tip_count = 0;
static volatile uint32_t last_wind_time = 0;

/* SAADC callback */
static volatile bool saadc_done = false;

/* TWI (I2C) instance */
static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);

/* SPI instance for SX1262 */
static const nrf_drv_spi_t m_spi = NRF_DRV_SPI_INSTANCE(1);

/* Forward declarations */
static void init_hardware(void);
static void read_bme280(bme280_reading_t *b);
static void read_anemometer(anemometer_reading_t *a);
static void read_rain_gauge(rain_gauge_reading_t *r);
static void lora_send_data(void);

/* GPIOTE handler: wind speed pulses + rain gauge tips */
static void gpiote_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    uint32_t now = app_timer_cnt_get();
    if (pin == PIN_ANEMOMETER) {
        wind_pulse_count++;
        last_wind_time = now;
    } else if (pin == PIN_RAIN_GAUGE) {
        rain_tip_count++;
    }
}

/* SPI HAL for SX1262 */
static void spi_transfer(uint8_t *tx, uint8_t *rx, size_t len)
{
    if (len == 0) return;
    nrf_drv_spi_transfer(&m_spi, tx, len, rx, len);
}

static void cs_select(void)    { nrf_gpio_pin_clear(PIN_SX1262_NSS); }
static void cs_deselect(void)  { nrf_gpio_pin_set(PIN_SX1262_NSS); }
static void radio_reset(bool s){ nrf_gpio_pin_write(PIN_SX1262_RST, s ? 0 : 1); }
static void wait_busy(void)    {
    while (nrf_gpio_pin_read(PIN_SX1262_BUSY)) nrf_delay_us(100);
}
static bool get_dio1(void)    { return nrf_gpio_pin_read(PIN_SX1262_DIO1) != 0; }
static uint32_t get_millis(void) { return app_timer_cnt_get() * 1000 / 32768; }

int main(void)
{
    /* Init board */
    bsp_board_init();
    nrf_drv_clock_init();
    app_timer_init();

    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("CompostSync Weather Station starting...");

    init_hardware();

    /* Init radio */
    radio.hal = (sx1262_hal_t){
        .spi_transfer = spi_transfer,
        .cs_select    = cs_select,
        .cs_deselect  = cs_deselect,
        .reset        = radio_reset,
        .wait_busy    = wait_busy,
        .get_dio1     = get_dio1,
        .millis       = get_millis,
    };
    sx1262_init(&radio);
    sx1262_set_key(&radio, aes_key);

    latest_data.node_id = NODE_ID_WEATHER;
    latest_data.uptime_s = 0;

    NRF_LOG_INFO("Weather Station ready. Sending data every 5 minutes.");

    while (1) {
        /* Read BME280 */
        read_bme280(&latest_data.bme280);

        /* Read anemometer */
        read_anemometer(&latest_data.anemometer);

        /* Read rain gauge */
        read_rain_gauge(&latest_data.rain);

        /* Update battery */
        latest_data.battery_pct = 85; /* simplified */

        /* Send via LoRa */
        lora_send_data();

        NRF_LOG_INFO("T=%d.%d H=%d%% P=%d Wind=%d.%d Rain=%.1fmm",
                     latest_data.temp_c/10, abs(latest_data.temp_c%10),
                     latest_data.humidity_pct, latest_data.pressure_hpa,
                     latest_data.wind_speed_ms/10, latest_data.wind_speed_ms%10,
                     (float)latest_data.rain_mm/10.0f);

        /* Wait 5 minutes (TDMA frame alignment handled by LoRa layer) */
        nrf_delay_ms(300000);
    }
}

static void init_hardware(void)
{
    /* Init GPIO for reed switches (anemometer + rain gauge) */
    nrfx_gpiote_init();
    nrfx_gpiote_in_config_t in_cfg = NRFX_GPIOTE_IN_CONFIG(NRF_GPIOTE_POLARITY_HITOLO);
    nrfx_gpiote_in_init(PIN_ANEMOMETER, &in_cfg, gpiote_handler);
    nrfx_gpiote_in_init(PIN_RAIN_GAUGE, &in_cfg, gpiote_handler);
    nrfx_gpiote_in_event_enable(PIN_ANEMOMETER, true);
    nrfx_gpiote_in_event_enable(PIN_RAIN_GAUGE, true);

    /* Init I2C for BME280 */
    nrf_drv_twi_config_t twi_cfg = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_cfg.sda = PIN_I2C_SDA;
    twi_cfg.scl = PIN_I2C_SCL;
    twi_cfg.frequency = NRF_DRV_TWI_FREQ_400K;
    nrf_drv_twi_init(&m_twi, &twi_cfg, NULL, NULL);
    nrf_drv_twi_enable(&m_twi);

    /* Init SPI for SX1262 */
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_cfg.mosi_pin = PIN_SX1262_MOSI;
    spi_cfg.miso_pin = PIN_SX1262_MISO;
    spi_cfg.sck_pin = PIN_SX1262_SCK;
    spi_cfg.frequency = NRF_DRV_SPI_FREQ_8M;
    nrf_drv_spi_init(&m_spi, &spi_cfg, NULL, NULL);

    /* Init GPIO outputs for radio control */
    nrf_gpio_cfg_output(PIN_SX1262_NSS);
    nrf_gpio_cfg_output(PIN_SX1262_RST);
    nrf_gpio_cfg_input(PIN_SX1262_DIO1, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(PIN_SX1262_BUSY, NRF_GPIO_PIN_PULLUP);

    /* Init ADC for wind vane */
    nrf_drv_saadc_init(NULL, NULL);
    nrf_saadc_channel_config_t adc_ch = {
        .resistor_p = NRF_SAADC_RESISTOR_PULLDOWN,
        .resistor_n = NRF_SAADC_RESISTOR_PULLUP,
        .gain = NRF_SAADC_GAIN1_4,
        .reference = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time = NRF_SAADC_ACQTIME_40US,
        .mode = NRF_SAADC_MODE_SINGLE_ENDED,
        .pin_p = PIN_WIND_VANE,
        .pin_n = NRF_SAADC_INPUT_DISABLED,
    };
    nrf_drv_saadc_channel_init(0, &adc_ch);

    NRF_LOG_INFO("Hardware initialized");
}

/* Read BME280 (simplified: read raw registers and convert) */
static void read_bme280(bme280_reading_t *b)
{
    uint8_t reg = 0xFA; /* temperature MSB */
    uint8_t data[3];

    nrf_drv_twi_tx(&m_twi, BME280_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, BME280_ADDR, data, 3);

    int32_t raw_temp = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);

    /* Simplified conversion (real BME280 uses calibration coefficients) */
    b->temp_c = raw_temp / 100.0f - 40.0f;

    /* Humidity */
    reg = 0xF2;
    nrf_drv_twi_tx(&m_twi, BME280_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, BME280_ADDR, data, 2);
    int32_t raw_hum = (data[0] << 8) | data[1];
    b->humidity_pct = (uint16_t)(raw_hum / 1024.0f);

    /* Pressure */
    reg = 0xF7;
    nrf_drv_twi_tx(&m_twi, BME280_ADDR, &reg, 1, true);
    nrf_drv_twi_rx(&m_twi, BME280_ADDR, data, 3);
    int32_t raw_press = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    b->pressure_hpa = (uint16_t)(raw_press / 256.0f);

    b->valid = true;
}

/* Read anemometer (wind speed from pulse count, direction from ADC) */
static void read_anemometer(anemometer_reading_t *a)
{
    /* Wind speed: pulses per second × conversion factor
     * Davis 6410: 1 pulse/sec = 2.25 mph = 1.0058 m/s */
    uint32_t pulses = wind_pulse_count;
    wind_pulse_count = 0;

    /* Estimate wind speed from pulse count over sampling period (5 min = 300s) */
    float speed_ms = (pulses / 300.0f) * 1.0058f;
    a->wind_speed_ms = (uint16_t)(speed_ms * 10);

    /* Wind direction: ADC voltage → compass heading
     * Davis 6410 uses resistor network: 0V=N, ... */
    nrf_saadc_value_t adc_val;
    nrf_drv_saadc_sample_convert(0, &adc_val);

    /* Map ADC (0-1023 on nRF52 10-bit) to 0-360 degrees */
    float voltage = adc_val * 3.3f / 1023.0f;
    a->wind_dir_deg = (uint16_t)(voltage / 3.3f * 360.0f);

    a->valid = true;
}

/* Read rain gauge */
static void read_rain_gauge(rain_gauge_reading_t *r)
{
    /* Tipping bucket: 0.2794 mm per tip (0.011") */
    uint32_t tips = rain_tip_count;
    rain_tip_count = 0;

    r->rain_mm = (uint16_t)(tips * 0.2794f * 10); /* x10 for protocol */
    r->rain_total_mm += tips * 0.2794f;
    r->valid = true;
}

/* Send weather data via LoRa */
static void lora_send_data(void)
{
    /* Pack data into weather_data_t */
    latest_data.uptime_s++;
    latest_data.temp_c = (int16_t)(latest_data.bme280.temp_c * 10);
    latest_data.humidity_pct = latest_data.bme280.humidity_pct;
    latest_data.pressure_hpa = latest_data.bme280.pressure_hpa;
    latest_data.wind_speed_ms = latest_data.anemometer.wind_speed_ms;
    latest_data.wind_dir_deg = latest_data.anemometer.wind_dir_deg;
    latest_data.rain_mm = latest_data.rain.rain_mm;
    latest_data.rssi_dbm = -70; /* will be updated */

    /* Wait for hub beacon then send in our TDMA slot (slot 3) */
    csp_header_t hdr;
    uint8_t payload[CSP_MAX_PAYLOAD];
    uint8_t len;

    /* Listen for beacon */
    int result = sx1262_rx(&radio, 3000, &hdr, payload, &len);
    if (result == 0 && hdr.msg_type == CSP_MSG_SYNC) {
        /* Wait for slot 3 (3000 ms after beacon) */
        nrf_delay_ms(3000 - 200);

        /* TX */
        sx1262_tx(&radio, NODE_ID_WEATHER, NODE_ID_HUB,
                  CSP_MSG_DATA, (uint8_t *)&latest_data,
                  sizeof(latest_data));
    }
}