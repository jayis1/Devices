/*
 * StormSync — Soil Saturation Probe Firmware
 * nRF52840, bare-metal scheduler
 *
 * Monitors soil moisture at 3 depths (15cm, 45cm, 90cm) + pore water
 * pressure + soil temperature. Transmits every 15 min via Sub-GHz mesh.
 * Solar-powered with LiFePO4 battery.
 *
 * Build: nRF5 SDK 17.x or nRF Connect SDK 2.x
 */
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_rtc.h"
#include "nrf_pwr_mgmt.h"
#include "app_timer.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

/* === Global state === */
static ss_mesh_ctx_t g_mesh;
static uint16_t g_msg_seq = 0;
static volatile bool g_wakeup_flag = false;

/* === SPI for SX1262 === */
static nrf_drv_spi_t g_spi = NRF_DRV_SPI_INSTANCE(0);

static void spi_init(void)
{
    nrf_drv_spi_config_t cfg = NRF_DRV_SPI_DEFAULT_CONFIG;
    cfg.sck_pin  = SOIL_GPIO_SX_SCK;
    cfg.mosi_pin = SOIL_GPIO_SX_MOSI;
    cfg.miso_pin = SOIL_GPIO_SX_MISO;
    cfg.ss_pin   = SOIL_GPIO_SX_NSS;
    cfg.frequency = NRF_DRV_SPI_FREQ_8M;
    cfg.mode = NRF_DRV_SPI_MODE_0;
    nrf_drv_spi_init(&g_spi, &cfg, NULL, NULL);
}

static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    nrf_drv_spi_transfer(&g_spi, &byte, 1, &rx, 1);
    return rx;
}
static void spi_reset(uint8_t assert) {
    nrf_gpio_pin_write(SOIL_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) { nrf_delay_ms(ms); }
static int spi_dio1_read(void) { return nrf_gpio_pin_read(SOIL_GPIO_SX_DIO1); }

static const ss_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = NULL, .cs_release = NULL,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = NULL,
};

/* === I²C for FDC2214 === */
static nrf_drv_twi_t g_twi = NRF_DRV_TWI_INSTANCE(0);

static void twi_init(void)
{
    nrf_drv_twi_config_t cfg = NRF_DRV_TWI_DEFAULT_CONFIG;
    cfg.scl = SOIL_GPIO_FDC_SCL;
    cfg.sda = SOIL_GPIO_FDC_SDA;
    cfg.frequency = NRF_DRV_TWI_FREQ_100K;
    nrf_drv_twi_init(&g_twi, &cfg, NULL, NULL);
    nrf_drv_twi_enable(&g_twi);
}

/* === Sensor Reading Functions === */

/* Read capacitive moisture via FDC2214 (I²C 0x2A)
 * 3 channels for 3 depths
 */
static uint16_t read_moisture_channel(uint8_t channel)
{
    /* FDC2214 has 4 channels. Each channel has MSB (0x00+ch*2) and LSB registers.
     * Read 2 bytes for each channel → raw capacitance value.
     * Convert to % VWC using calibration: air=SOIL_AIR_VALUE, water=SOIL_WATER_VALUE
     */
    uint8_t reg = 0x00 + channel * 2; /* MSB register for channel */
    uint8_t data[2];
    nrf_drv_twi_tx(&g_twi, 0x2A, &reg, 1, true);
    nrf_drv_twi_rx(&g_twi, 0x2A, data, 2);
    uint16_t raw = (data[0] << 8) | data[1];

    /* Linear interpolation: air → 0%, water → 100% */
    int32_t pct = ((int32_t)(SOIL_AIR_VALUE - raw) * 100) /
                  (SOIL_AIR_VALUE - SOIL_WATER_VALUE);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint16_t)(pct * 100); /* ×0.01% for protocol */
}

/* Read DS18B20 temperature at a given GPIO (1-Wire) */
static int8_t read_ds18b20_temp(uint8_t pin)
{
    /* Simplified 1-Wire read.
     * In production:
     * 1. Reset pulse (pull low 500µs, release, wait 70µs, check presence)
     * 2. Skip ROM (0xCC)
     * 3. Convert T (0x44) → wait 750ms
     * 4. Reset, Skip ROM, Read Scratchpad (0xBE) → read 2 bytes
     * 5. temp = (MSB << 8 | LSB) / 16 → °C
     */
    nrf_gpio_cfg_output(pin);
    nrf_gpio_pin_write(pin, 0);
    nrf_delay_us(500);
    nrf_gpio_pin_write(pin, 1);
    nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
    nrf_delay_us(70);
    /* Check presence */
    uint8_t presence = !nrf_gpio_pin_read(pin);
    nrf_delay_us(430);
    if (!presence) return -40; /* Sensor not connected */

    /* In production: send 0xCC, 0x44, wait 750ms, read scratchpad */
    return 18; /* Placeholder: 18°C */
}

/* Read pore water pressure via MPS20NR (ADC) */
static int16_t read_pore_pressure(void)
{
    nrf_saadc_value_t adc_val;
    nrf_drv_saadc_sample_convert(0, &adc_val);

    /* MPS20NR: 0–5V output, 0–100 kPa range
     * With voltage divider: V = adc * 3.6/1024
     * Pressure = V * 100 / 5 (kPa) → offset by 500 for signed protocol
     */
    float v = (float)adc_val * 3.6 / 1024.0;
    float kpa = v * 20.0; /* Simplified calibration */
    return (int16_t)((kpa + 500) * 10); /* ×0.1 kPa, offset 500 → signed */
}

/* Read battery voltage */
static uint8_t read_battery_mv(void)
{
    nrf_saadc_value_t adc_val;
    nrf_drv_saadc_sample_convert(1, &adc_val); /* AIN18 */
    uint32_t mv = (uint32_t)adc_val * 7200 / 1024;
    return (uint8_t)(mv / 10); /* ×0.01V */
}

/* Read solar voltage */
static uint8_t read_solar_voltage(void)
{
    nrf_saadc_value_t adc_val;
    nrf_drv_saadc_sample_convert(2, &adc_val); /* AIN19 */
    uint32_t mv = (uint32_t)adc_val * 7200 / 1024;
    return (uint8_t)(mv / 100); /* ×0.1V */
}

/* === RTC Wakeup Timer === */
static void rtc_handler(nrf_drv_rtc_int_type_t int_type)
{
    g_wakeup_flag = true;
}

static void rtc_init(void)
{
    nrf_drv_rtc_t rtc = NRF_DRV_RTC_INSTANCE(2);
    nrf_drv_rtc_config_t cfg = NRF_DRV_RTC_DEFAULT_CONFIG;
    cfg.prescaler = 327; /* ~10 Hz */
    nrf_drv_rtc_init(&rtc, &cfg, rtc_handler);
    nrf_drv_rtc_tick_enable(&rtc, true);
    nrf_drv_rtc_cc_set(&rtc, 0, 9000, true); /* 15 min at 10 Hz */
    nrf_drv_rtc_enable(&rtc);
}

/* === Main Loop === */
int main(void)
{
    /* Initialize GPIO */
    nrf_gpio_cfg_output(SOIL_GPIO_LED);
    nrf_gpio_pin_write(SOIL_GPIO_LED, 0);
    nrf_gpio_cfg_output(SOIL_GPIO_SENSOR_SW);
    nrf_gpio_pin_write(SOIL_GPIO_SENSOR_SW, 0);
    nrf_gpio_cfg_output(SOIL_GPIO_VDDH_EN);
    nrf_gpio_pin_write(SOIL_GPIO_VDDH_EN, 0);
    nrf_gpio_cfg_output(SOIL_GPIO_SX_RST);
    nrf_gpio_pin_write(SOIL_GPIO_SX_RST, 1);
    nrf_gpio_cfg_input(SOIL_GPIO_SX_DIO1, NRF_GPIO_PIN_PULLDOWN);

    /* Initialize SAADC */
    nrf_drv_saadc_init(NULL, NULL);
    nrf_saadc_channel_config_t adc_cfg = {
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
        .gain = NRF_SAADC_GAIN1_6,
        .reference = NRF_SAADC_REFERENCE_INTERNAL,
        .acq_time = NRF_SAADC_ACQTIME_40US,
        .mode = NRF_SAADC_MODE_SINGLE_ENDED,
        .burst = NRF_SAADC_BURST_DISABLED,
    };
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN7; /* Pore pressure */
    nrf_drv_saadc_channel_init(0, &adc_cfg);
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN18; /* Battery */
    nrf_drv_saadc_channel_init(1, &adc_cfg);
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN19; /* Solar */
    nrf_drv_saadc_channel_init(2, &adc_cfg);

    /* Initialize I²C */
    twi_init();

    /* Initialize SPI + Radio */
    spi_init();
    ss_radio_config_t radio_cfg = {
        .frequency = SS_NET_FREQ_HZ,
        .bandwidth = SS_NET_BW_HZ,
        .spreading_factor = SS_NET_SF,
        .coding_rate = SS_NET_CR,
        .preamble_len = SS_NET_PREAMBLE,
        .tx_power_dbm = SS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    ss_mesh_init(&g_mesh, SS_NODE_SOIL, &g_spi_iface, &radio_cfg);

    /* Join network */
    if (ss_mesh_join(&g_mesh) != 0) {
        /* Retry on next cycle */
    }

    rtc_init();

    while (1) {
        g_wakeup_flag = false;

        nrf_gpio_pin_write(SOIL_GPIO_LED, 1);
        nrf_gpio_pin_write(SOIL_GPIO_VDDH_EN, 1);
        nrf_delay_ms(100);

        /* Read all sensors */
        uint8_t  bat_mv = read_battery_mv();
        uint16_t m15 = read_moisture_channel(0);  /* 15cm depth */
        uint16_t m45 = read_moisture_channel(1);  /* 45cm depth */
        uint16_t m90 = read_moisture_channel(2);  /* 90cm depth */
        int16_t  pore = read_pore_pressure();
        int8_t   t15 = read_ds18b20_temp(SOIL_GPIO_DS18_1);
        int8_t   t45 = read_ds18b20_temp(SOIL_GPIO_DS18_2);
        int8_t   t90 = read_ds18b20_temp(SOIL_GPIO_DS18_3);
        uint8_t  solar_v = read_solar_voltage();

        nrf_gpio_pin_write(SOIL_GPIO_VDDH_EN, 0);

        /* Build and send telemetry */
        ss_message_t msg;
        ss_build_soil_telem(&msg, g_mesh.node_id, g_msg_seq++,
                            bat_mv, m15, m45, m90, pore,
                            t15, t45, t90, solar_v,
                            g_mesh.last_rssi);

        ss_mesh_wait_slot(&g_mesh);
        ss_mesh_send(&g_mesh, &msg);

        /* Check battery */
        if (bat_mv < BAT_CRIT_MV) {
            /* Enter low-power mode: extend interval */
        }

        nrf_gpio_pin_write(SOIL_GPIO_LED, 0);

        /* Deep sleep until next RTC wakeup */
        while (!g_wakeup_flag) {
            nrf_pwr_mgmt_run();
            __WFI();
        }
    }
}