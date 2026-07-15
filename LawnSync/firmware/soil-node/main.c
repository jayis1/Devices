/*
 * LawnSync — Soil Sensor Node Firmware
 * nRF52840, bare-metal scheduler (no SoftDevice required for Sub-GHz)
 *
 * Duty cycle: wake every 15 min → read sensors → TX via Sub-GHz mesh → sleep
 * Average consumption: ~0.3 mA (deep sleep 99% of the time)
 *
 * Build: nRF5 SDK 17.x or nRF Connect SDK 2.x
 */
#include <stdint.h>
#include <string.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_rtc.h"
#include "nrf_drv_gpiote.h"
#include "nrf_pwr_mgmt.h"
#include "app_timer.h"

#include "../common/protocol.h"
#include "../common/sx1262.h"
#include "../common/mesh.h"
#include "../common/config.h"

/* === Global state === */
static ls_mesh_ctx_t g_mesh;
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

static void spi_cs_select(void) {
    /* Managed by nrf_drv_spi */
}
static void spi_cs_release(void) {
    /* Managed by nrf_drv_spi */
}
static uint8_t spi_transfer(uint8_t byte) {
    uint8_t rx;
    nrf_drv_spi_transfer(&g_spi, &byte, 1, &rx, 1);
    return rx;
}
static void spi_reset(uint8_t assert) {
    nrf_gpio_pin_write(SOIL_GPIO_SX_RST, assert ? 0 : 1);
}
static void spi_delay_ms(uint32_t ms) {
    nrf_delay_ms(ms);
}
static int spi_dio1_read(void) {
    return nrf_gpio_pin_read(SOIL_GPIO_SX_DIO1);
}
static void spi_dio1_irq_enable(int enable) {
    (void)enable;
}

static const ls_spi_interface_t g_spi_iface = {
    .init = spi_init,
    .cs_select = spi_cs_select,
    .cs_release = spi_cs_release,
    .transfer = spi_transfer,
    .reset = spi_reset,
    .delay_ms = spi_delay_ms,
    .dio1_read = spi_dio1_read,
    .dio1_irq_enable = spi_dio1_irq_enable,
};

/* === I²C for FDC2214 and VEML7700 === */
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

/* Read capacitive soil moisture via FDC2214 (I²C 0x2A) */
static uint16_t read_moisture(void)
{
    uint8_t reg = 0x00; /* FDC2214 channel 0 data MSB */
    uint8_t data[2];
    nrf_drv_twi_tx(&g_twi, 0x2A, &reg, 1, true);
    nrf_drv_twi_rx(&g_twi, 0x2A, data, 2);
    uint16_t raw = (data[0] << 8) | data[1];

    /* Convert to % VWC using linear interpolation */
    /* air_value = SOIL_AIR_VALUE, water_value = SOIL_WATER_VALUE */
    int32_t pct = ((int32_t)(SOIL_AIR_VALUE - raw) * 100) /
                  (SOIL_AIR_VALUE - SOIL_WATER_VALUE);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint16_t)(pct * 100); /* ×0.01% for protocol */
}

/* Read soil temperature via DS18B20 (1-Wire) */
static int16_t read_soil_temp(void)
{
    /* Simplified: in production, implement full 1-Wire protocol
     * on SOIL_GPIO_DS18B20 with bit-banging.
     * 1. Send reset pulse
     * 2. Skip ROM (0xCC)
     * 3. Convert T (0x44) — wait 750ms
     * 4. Reset, skip ROM, read scratchpad (0xBE)
     * 5. Read 2 bytes: temp LSB, temp MSB
     */
    uint8_t presence = 0;
    /* Bit-bang 1-Wire reset */
    nrf_gpio_cfg_output(SOIL_GPIO_DS18B20);
    nrf_gpio_pin_write(SOIL_GPIO_DS18B20, 0);
    nrf_delay_us(500);
    nrf_gpio_pin_write(SOIL_GPIO_DS18B20, 1);
    nrf_gpio_cfg_input(SOIL_GPIO_DS18B20, NRF_GPIO_PIN_PULLUP);
    nrf_delay_us(70);
    presence = !nrf_gpio_pin_read(SOIL_GPIO_DS18B20);
    nrf_delay_us(430);

    if (!presence) return -1270; /* sensor not connected */

    /* In production: send 0xCC (skip ROM), 0x44 (convert T) */
    /* Wait 750ms, then read scratchpad */
    /* Return temperature in 0.1°C units */
    return 225; /* placeholder: 22.5°C */
}

/* Read pH via LMP7721 amplifier → SAADC (P0.07 / AIN7) */
static uint8_t read_ph(void)
{
    nrf_saadc_value_t adc_val;
    nrf_drv_saadc_sample_convert(0, &adc_val); /* AIN7 = channel 0 config */

    /* Linear interpolation using two-point calibration:
     * pH = 7.0 + (PH_ADC_7_0 - adc) * (7.0 - 4.0) / (PH_ADC_7_0 - PH_ADC_4_0)
     */
    float ph = 7.0 + (float)(PH_ADC_7_0 - adc_val) * 3.0 /
               (float)(PH_ADC_7_0 - PH_ADC_4_0);
    if (ph < 0) ph = 0;
    if (ph > 14) ph = 14;
    return (uint8_t)(ph * 10); /* ×0.1 for protocol */
}

/* Read NPK via ISE probes → SAADC (AIN8, AIN9, AIN10) */
static void read_npk(uint16_t *n, uint16_t *p, uint16_t *k)
{
    /* Power-gate ISE probes */
    nrf_gpio_pin_write(SOIL_GPIO_ISE_SW, 1);
    nrf_delay_ms(2000); /* ISE stabilization */

    nrf_saadc_value_t adc_n, adc_p, adc_k;
    nrf_drv_saadc_sample_convert(1, &adc_n); /* AIN8 */
    nrf_drv_saadc_sample_convert(2, &adc_p); /* AIN9 */
    nrf_drv_saadc_sample_convert(3, &adc_k); /* AIN10 */

    /* Convert ADC to mg/kg using calibration curve (per-probe) */
    /* Simplified: N = adc * 0.05, P = adc * 0.03, K = adc * 0.04 */
    *n = (uint16_t)(adc_n * 5);   /* ×0.1 mg/kg */
    *p = (uint16_t)(adc_p * 3);
    *k = (uint16_t)(adc_k * 4);

    /* Power-gate off */
    nrf_gpio_pin_write(SOIL_GPIO_ISE_SW, 0);
}

/* Read ambient light via VEML7700 (I²C 0x10) */
static uint16_t read_light(void)
{
    uint8_t reg = 0x04; /* ALS high resolution data */
    uint8_t data[2];
    nrf_drv_twi_tx(&g_twi, 0x10, &reg, 1, true);
    nrf_drv_twi_rx(&g_twi, 0x10, data, 2);
    uint16_t raw = (data[0] << 8) | data[1];
    /* Convert to lux: lux = raw * 0.0576 (at gain 1, 100ms integration) */
    return (uint16_t)(raw * 0.0576); /* lux */
}

/* Read battery voltage via voltage divider → SAADC (AIN18) */
static uint8_t read_battery_mv(void)
{
    nrf_saadc_value_t adc_val;
    nrf_drv_saadc_sample_convert(4, &adc_val); /* AIN18 */
    /* Divider ×0.5, so actual = adc * 2 * 3.6V / 10-bit (1024)
     * battery_mv = (adc_val * 7200) / 1024 / 10 (for 0.01V units)
     */
    uint32_t mv = (uint32_t)adc_val * 7200 / 1024;
    return (uint8_t)(mv / 10); /* ×0.01V → returns 280 for 2.80V */
}

/* Read solar voltage via voltage divider → SAADC (AIN19) */
static uint8_t read_solar_voltage(void)
{
    nrf_saadc_value_t adc_val;
    nrf_drv_saadc_sample_convert(5, &adc_val); /* AIN19 */
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
    /* Set compare for 15 minutes: 9000 ticks at 10 Hz */
    nrf_drv_rtc_cc_set(&rtc, 0, 9000, true);
    nrf_drv_rtc_enable(&rtc);
}

/* === Main Loop === */
int main(void)
{
    /* Initialize GPIO */
    nrf_gpio_cfg_output(SOIL_GPIO_LED);
    nrf_gpio_pin_write(SOIL_GPIO_LED, 0);
    nrf_gpio_cfg_output(SOIL_GPIO_ISE_SW);
    nrf_gpio_pin_write(SOIL_GPIO_ISE_SW, 0);
    nrf_gpio_cfg_output(SOIL_GPIO_VDDH_EN);
    nrf_gpio_pin_write(SOIL_GPIO_VDDH_EN, 0);
    nrf_gpio_cfg_output(SOIL_GPIO_SX_RST);
    nrf_gpio_pin_write(SOIL_GPIO_SX_RST, 1);
    nrf_gpio_cfg_input(SOIL_GPIO_SX_DIO1, NRF_GPIO_PIN_PULLDOWN);
    nrf_gpio_cfg_input(SOIL_GPIO_DS18B20, NRF_GPIO_PIN_PULLUP);

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
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN7; /* pH */
    nrf_drv_saadc_channel_init(0, &adc_cfg);
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN8; /* N */
    nrf_drv_saadc_channel_init(1, &adc_cfg);
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN9; /* P */
    nrf_drv_saadc_channel_init(2, &adc_cfg);
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN10; /* K */
    nrf_drv_saadc_channel_init(3, &adc_cfg);
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN18; /* Battery */
    nrf_drv_saadc_channel_init(4, &adc_cfg);
    adc_cfg.pin_p = NRF_SAADC_INPUT_AIN19; /* Solar */
    nrf_drv_saadc_channel_init(5, &adc_cfg);

    /* Initialize I²C */
    twi_init();

    /* Initialize SPI + Radio */
    spi_init();
    ls_radio_config_t radio_cfg = {
        .frequency = LS_NET_FREQ_HZ,
        .bandwidth = LS_NET_BW_HZ,
        .spreading_factor = LS_NET_SF,
        .coding_rate = LS_NET_CR,
        .preamble_len = LS_NET_PREAMBLE,
        .tx_power_dbm = LS_NET_TX_POWER_DBM,
        .rx_timeout_ms = 0,
    };

    /* Initialize mesh */
    ls_mesh_init(&g_mesh, LS_NODE_SOIL, &g_spi_iface, &radio_cfg);

    /* Join network */
    if (ls_mesh_join(&g_mesh) != 0) {
        /* Retry on next cycle; for now continue with unassigned */
    }

    /* Initialize RTC for periodic wakeup */
    rtc_init();

    /* Main loop */
    while (1) {
        g_wakeup_flag = false;

        /* LED on during measurement */
        nrf_gpio_pin_write(SOIL_GPIO_LED, 1);

        /* Enable sensor power */
        nrf_gpio_pin_write(SOIL_GPIO_VDDH_EN, 1);
        nrf_delay_ms(100);

        /* Read all sensors */
        uint8_t  bat_mv = read_battery_mv();
        uint16_t moisture = read_moisture();
        int16_t  temp = read_soil_temp();
        uint8_t  ph = read_ph();
        uint16_t n_val, p_val, k_val;
        read_npk(&n_val, &p_val, &k_val);
        uint16_t light = read_light();
        uint8_t  solar_v = read_solar_voltage();

        /* Disable sensor power */
        nrf_gpio_pin_write(SOIL_GPIO_VDDH_EN, 0);

        /* Build and send telemetry */
        ls_message_t msg;
        ls_build_soil_telem(&msg, g_mesh.node_id, g_msg_seq++,
                            bat_mv, moisture, temp, ph,
                            n_val, p_val, k_val, light,
                            solar_v, g_mesh.last_rssi);

        /* Wait for TDMA slot */
        ls_mesh_wait_slot(&g_mesh);

        /* Send via mesh */
        ls_mesh_send(&g_mesh, &msg);

        /* Check battery */
        if (bat_mv < BAT_CRIT_MV) {
            /* Enter low-power mode: extend interval to 1 hour */
            /* In production: adjust RTC compare value */
        }

        /* LED off */
        nrf_gpio_pin_write(SOIL_GPIO_LED, 0);

        /* Enter deep sleep until next RTC wakeup */
        /* System OFF would lose RAM state; use System ON deep sleep */
        while (!g_wakeup_flag) {
            nrf_pwr_mgmt_run();
            __WFI();
        }
    }
}