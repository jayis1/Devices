/*
 * MosquitoSync — Weather Sentinel Firmware
 * nRF52840, bare-metal (no SoftDevice required for Sub-GHz)
 *
 * Reports temperature, humidity, pressure, wind speed/direction, and
 * rainfall — critical inputs for mosquito activity forecasting
 * (mosquitoes active 15–32°C, peak at 27°C; rain creates breeding
 * sites 7–14 days later).
 *
 * Build: nrf52 toolchain (arm-none-eabi-gcc) with nRF5 SDK
 * Solar-powered with LiFePO4 battery, 5-min sampling interval.
 */
#include <stdint.h>
#include <string.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_saadc.h"
#include "app_timer.h"
#include "nrf_log.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

/* === State === */
static ms_mesh_ctx_t g_mesh;
static uint16_t g_msg_seq = 0;
static volatile uint16_t g_wind_pulses = 0;
static volatile uint16_t g_rain_tips = 0;
static uint32_t g_last_wind_sample = 0;
static uint16_t g_wind_dir_avg = 0;

/* === SX1262 SPI Interface (nRF52840) === */
static const nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);

static void spi_init(void)
{
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_cfg.sck_pin = WX_GPIO_SX_SCK;
    spi_cfg.mosi_pin = WX_GPIO_SX_MOSI;
    spi_cfg.miso_pin = WX_GPIO_SX_MISO;
    spi_cfg.ss_pin = WX_GPIO_SX_NSS;
    spi_cfg.frequency = NRF_DRV_SPI_FREQ_8M;
    nrf_drv_spi_init(&g_spi, &spi_cfg, NULL, NULL);
}

static void spi_cs_select(void) {
    nrf_gpio_pin_clear(WX_GPIO_SX_NSS);
}
static void spi_cs_release(void) {
    nrf_gpio_pin_set(WX_GPIO_SX_NSS);
}
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    nrf_drv_spi_transfer(&g_spi, &byte, 1, &rx, 1);
    return rx;
}
static void spi_reset(uint8_t assert) {
    nrf_gpio_pin_write(WX_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { nrf_delay_ms(ms); }
static int spi_dio1_read(void) { return nrf_gpio_pin_read(WX_GPIO_SX_DIO1); }
static void spi_dio1_irq_enable(int enable) { (void)enable; }

static const ms_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I2C for BME280 === */
static const nrf_drv_twi_t g_twi = NRF_DRV_TWI_INSTANCE(0);

static void twi_init(void)
{
    nrf_drv_twi_config_t twi_cfg = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_cfg.scl = WX_GPIO_BME_SCL;
    twi_cfg.sda = WX_GPIO_BME_SDA;
    twi_cfg.frequency = NRF_TWI_FREQ_100K;
    nrf_drv_twi_init(&g_twi, &twi_cfg, NULL, NULL);
    nrf_drv_twi_enable(&g_twi);
}

static void read_bme280(int16_t *temp_deci, uint16_t *humidity_deci,
                         uint16_t *pressure_deci)
{
    uint8_t reg = 0xF7;
    uint8_t buf[8];

    nrf_drv_twi_tx(&g_twi, 0x76, &reg, 1, true);
    nrf_drv_twi_rx(&g_twi, 0x76, buf, 8);

    *pressure_deci = (uint16_t)((buf[0] << 12 | buf[1] << 4 | buf[2] >> 4) / 256.0 * 10);
    *temp_deci = (int16_t)((buf[3] << 12 | buf[4] << 4 | buf[5] >> 4) / 100.0 * 10);
    *humidity_deci = (uint16_t)((buf[6] << 8 | buf[7]) / 1024.0 * 1000.0);
}

/* === ADC for Battery + Solar + Wind Direction === */
static void saadc_init(void)
{
    nrf_saadc_channel_config_t cfg = {
        .resolution = NRF_SAADC_RESOLUTION_12BIT,
        .gain = NRF_SAADC_GAIN1_4,
        .reference = NRF_SAADC_REFERENCE_VDD4,
        .acq_time = NRF_SAADC_ACQTIME_10US,
        .mode = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst = NRF_SAADC_BURST_DISABLED,
    };
    cfg.pin_p = (nrf_saadc_input_t)(WX_GPIO_VBAT + 1);
    nrf_drv_saadc_channel_init(0, &cfg);
    cfg.pin_p = (nrf_saadc_input_t)(WX_GPIO_VSOL + 1);
    nrf_drv_saadc_channel_init(1, &cfg);
    cfg.pin_p = (nrf_saadc_input_t)(WX_GPIO_WIND_DIR + 1);
    nrf_drv_saadc_channel_init(2, &cfg);
}

static uint16_t adc_read(uint8_t channel)
{
    nrf_saadc_value_t val;
    nrf_drv_saadc_sample_convert(channel, &val);
    return (uint16_t)val;
}

static uint8_t read_battery_mv(void)
{
    uint16_t raw = adc_read(0);
    /* LiFePO4: 3.2V nominal, voltage divider 2:1
     * Vbat = raw * 3.3 * 2 / 4095 (VDD/4 ref)
     */
    float voltage = raw * 3.3 * 2.0 * 4.0 / 4095.0;
    return (uint8_t)(voltage * 100); /* x0.01V */
}

static uint16_t read_wind_direction(void)
{
    uint16_t raw = adc_read(2);
    /* Davis 6410 vane: 0-3.3V → 0-360° */
    return (uint16_t)(raw * 360 / 4095);
}

/* === GPIO Interrupts for Wind + Rain === */
static void wind_isr(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    g_wind_pulses++;
}

static void rain_isr(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    g_rain_tips++;
}

static void gpio_interrupts_init(void)
{
    nrf_drv_gpiote_init();

    nrf_drv_gpiote_in_config_t wind_cfg = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    nrf_drv_gpiote_in_init(WX_GPIO_WIND_SPD, &wind_cfg, wind_isr);
    nrf_drv_gpiote_in_event_enable(WX_GPIO_WIND_SPD, true);

    nrf_drv_gpiote_in_config_t rain_cfg = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    nrf_drv_gpiote_in_init(WX_GPIO_RAIN_TIP, &rain_cfg, rain_isr);
    nrf_drv_gpiote_in_event_enable(WX_GPIO_RAIN_TIP, true);
}

/* === Wind Speed Calculation ===
 * Davis 6410: 1 pulse per revolution, 2.25 km/h per Hz
 * = 0.625 m/s per pulse/s
 */
static uint16_t compute_wind_speed(uint32_t elapsed_ms)
{
    /* pulses per second → m/s */
    float pps = (float)g_wind_pulses / (elapsed_ms / 1000.0f);
    float mps = pps * 0.625f;
    return (uint16_t)(mps * 10); /* 0.1 m/s resolution */
}

/* === Send Telemetry to Hub === */
static void send_telemetry(void)
{
    int16_t temp_deci;
    uint16_t hum_deci, pres_deci;
    read_bme280(&temp_deci, &hum_deci, &pres_deci);

    /* Wind speed (2-second sample window) */
    uint32_t now = app_timer_cnt_get() / 32768 * 1000; /* ms */
    uint32_t elapsed = now - g_last_wind_sample;
    g_last_wind_sample = now;
    uint16_t wind_speed = compute_wind_speed(elapsed);
    g_wind_pulses = 0;

    uint16_t wind_dir = read_wind_direction();

    uint16_t rain = g_rain_tips;
    g_rain_tips = 0;

    uint8_t battery_v = read_battery_mv();

    ms_message_t msg;
    ms_build_weather_telem(&msg, g_mesh.node_id, g_msg_seq++,
                           battery_v, temp_deci, hum_deci, pres_deci,
                           wind_speed, wind_dir, rain, g_mesh.last_rssi);
    ms_mesh_send(&g_mesh, &msg);

    NRF_LOG_INFO("Weather telemetry: temp=%.1f°C hum=%.1f%% pres=%.1f "
                 "wind=%.1fm/s dir=%d° rain=%.1fmm",
                 temp_deci/10.0, hum_deci/10.0, (pres_deci+8000)/10.0,
                 wind_speed/10.0, wind_dir, rain*0.2);
}

/* === Main === */
int main(void)
{
    NRF_LOG_INIT(NULL);
    NRF_LOG_INFO("MosquitoSync Weather Sentinel starting...");

    /* Initialize GPIOs */
    nrf_gpio_pin_dir_set(WX_GPIO_SX_RST, NRF_GPIO_PIN_DIR_OUTPUT);
    nrf_gpio_pin_set(WX_GPIO_SX_RST);
    nrf_gpio_pin_dir_set(WX_GPIO_SX_DIO1, NRF_GPIO_PIN_DIR_INPUT);
    nrf_gpio_pin_dir_set(WX_GPIO_LED, NRF_GPIO_PIN_DIR_OUTPUT);
    nrf_gpio_pin_set(WX_GPIO_LED);

    /* Initialize peripherals */
    twi_init();
    saadc_init();
    gpio_interrupts_init();
    spi_init();
    app_timer_init();

    /* Initialize mesh */
    ms_radio_config_t radio_cfg = {
        .frequency = MS_NET_FREQ_HZ,
        .bandwidth = MS_NET_BW_HZ,
        .spreading_factor = MS_NET_SF,
        .coding_rate = MS_NET_CR,
        .preamble_len = MS_NET_PREAMBLE,
        .tx_power_dbm = MS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    if (ms_mesh_init(&g_mesh, MS_NODE_WEATHER, &g_spi_iface, &radio_cfg) != 0) {
        NRF_LOG_ERROR("Mesh init failed");
        while (1) { nrf_delay_ms(1000); }
    }

    /* Join network */
    int join_retries = 0;
    while (join_retries < 10) {
        if (ms_mesh_join(&g_mesh) == 0) {
            NRF_LOG_INFO("Joined mesh: node_id=%d slot=%d",
                         g_mesh.node_id, g_mesh.tdma_slot);
            break;
        }
        NRF_LOG_WARNING("Join failed, retry %d", join_retries);
        nrf_delay_ms(2000);
        join_retries++;
    }

    g_last_wind_sample = app_timer_cnt_get() / 32768 * 1000;

    /* Main loop: sample every 5 minutes, sleep between */
    while (1) {
        send_telemetry();

        /* Sleep until next sample (5 min)
         * In production: use system-off + RTC wakeup
         */
        for (int i = 0; i < WEATHER_SAMPLE_INTERVAL; i++) {
            nrf_delay_ms(1000);
        }
    }
}