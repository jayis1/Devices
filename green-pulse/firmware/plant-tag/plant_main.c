/*
 * plant_main.c — GreenPulse Plant Tag Node firmware (nRF52832, nRF5 SDK)
 *
 * Samples capacitive soil moisture, ambient light (VEML7700), temperature
 * and humidity (SHT40) every 15 minutes, transmits telemetry over the
 * Sub-GHz mesh (SX1262, 868/915 MHz), and deep-sleeps (~14 µA) between
 * samples for 18+ months of coin-cell life.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "green_protocol.h"

/* ---- Hardware pins ---- */
#define PIN_SX1262_CS      12
#define PIN_SX1262_RST     13
#define PIN_SX1262_BUSY    14
#define PIN_SX1262_IRQ     15
#define PIN_SOIL_POWER     20   /* power-gate the soil sensor for low leak */
#define PIN_SOIL_ADC       0    /* ADC0 for capacitive soil moisture */
#define PIN_LED            22
#define PIN_BUTTON         24   /* pairing button */

/* ---- Sampling config ---- */
#define SAMPLE_INTERVAL_MS 900000  /* 15 minutes */
#define SOIL_SAMPLES       3       /* average 3 reads */

/* ---- nRF52832 RTC-based wake timer ---- */
static volatile uint8_t seq = 0;
static uint8_t tag_id = 1;      /* set during pairing */
static uint8_t profile_id = 0; /* set during pairing */

/* ---- SX1262 Sub-GHz radio (simplified SPI interface) ---- */
static void sx1262_init(void)
{
    nrf_gpio_cfg_output(PIN_SX1262_CS);
    nrf_gpio_cfg_output(PIN_SX1262_RST);
    nrf_gpio_cfg_input(PIN_SX1262_BUSY, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(PIN_SX1262_IRQ, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_pin_set(PIN_SX1262_CS);
    /* Full SPI + register init would go here — set frequency, modulation,
     * TX power (+10 dBm), sync word, CRC. See SX1262 datasheet. */
}

static int sx1262_tx(const uint8_t *data, size_t len)
{
    /* In production: load TX buffer, set TX mode, wait for IRQ (TxDone).
     * Returns 0 on success. */
    (void)data; (void)len;
    /* Simulate successful TX */
    return (int)len;
}

/* ---- Capacitive soil moisture sensor ---- */
static uint8_t read_soil_moisture(void)
{
    /* Power-gate the sensor to eliminate DC leak between reads */
    nrf_gpio_pin_set(PIN_SOIL_POWER);
    nrf_delay_ms(5);  /* let sensor settle */

    uint32_t sum = 0;
    for (int i = 0; i < SOIL_SAMPLES; i++) {
        /* In production: configure SAADC, sample AIN0, convert to VWC %.
         * Capacitive sensor: dry ≈ 3.0V (0%), wet ≈ 1.5V (100%).
         * vwc = (3.0 - v) / 1.5 * 100, clamped 0-100. */
        uint16_t raw = 0; /* NRF_SAADC->RESULT.PTR read would go here */
        float v = (float)raw / 4095.0f * 3.6f;  /* 12-bit ADC, 3.6V ref */
        float vwc = (3.0f - v) / 1.5f * 100.0f;
        if (vwc < 0) vwc = 0;
        if (vwc > 100) vwc = 100;
        sum += (uint32_t)vwc;
        nrf_delay_ms(10);
    }

    nrf_gpio_pin_clear(PIN_SOIL_POWER);  /* power off to save battery */
    return (uint8_t)(sum / SOIL_SAMPLES);
}

/* ---- VEML7700 ambient light sensor (I2C) ---- */
static uint16_t read_ambient_lux(void)
{
    /* In production: read VEML7700 ALS register over TWI (I2C).
     * Convert raw count to lux based on gain/integration time settings.
     * Returns lux * 10. */
    return 0; /* stub */
}

/* ---- SHT40 temperature + humidity (I2C) ---- */
static int16_t read_temp_centic(void)
{
    /* In production: SHT40 single-shot measurement, read raw T,
     * convert: T = -45 + 175 * raw/65535 (centi-degC). */
    return 2200; /* 22.00 C stub */
}

static uint16_t read_humidity_centi(void)
{
    /* In production: SHT40 RH conversion: RH = 100 * raw/65535 (centi-%). */
    return 4500; /* 45.00% stub */
}

/* ---- Battery voltage via ADC divider ---- */
static uint8_t read_battery_pct(void)
{
    /* CR2477: 3.0V full → 2.0V empty.
     * Read battery via SAADC with 1/2 divider, map 2.0-3.0V → 0-100%.
     * CR2477 discharge curve is nearly flat then drops sharply near end. */
    return 85; /* stub */
}

/* ---- Assemble and send telemetry ---- */
static void send_telemetry(void)
{
    uint8_t soil = read_soil_moisture();
    uint16_t lux = read_ambient_lux();
    int16_t temp = read_temp_centic();
    uint16_t humidity = read_humidity_centi();
    uint8_t batt = read_battery_pct();

    uint8_t flags = 0;
    if (batt < 15) flags |= GP_ALERT_LOW_BATT;

    /* Check against profile thresholds */
    const gp_plant_profile_t *prof = gp_get_profile(profile_id);
    if (prof) {
        if (soil < prof->min_moisture) flags |= GP_ALERT_LOW_MOISTURE;
        if (temp > prof->temp_max_centic) flags |= GP_ALERT_HIGH_TEMP;
        if (temp < prof->temp_min_centic) flags |= GP_ALERT_LOW_TEMP;
    }

    gp_telemetry_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = GP_MSG_TELEMETRY;
    p.node_id = tag_id;
    p.seq = seq++;
    p.flags = flags;
    p.soil_moisture = soil;
    p.ambient_lux = lux;
    p.temp_centic = temp;
    p.humidity_centi = humidity;
    p.battery_pct = batt;
    p.plant_profile_id = profile_id;
    gp_pack_crc(&p, sizeof(p) - 2);

    sx1262_tx((const uint8_t *)&p, sizeof(p));
}

/* ---- Mesh TX function for protocol layer ---- */
static int mesh_tx(const uint8_t *data, size_t len)
{
    return sx1262_tx(data, len);
}

/* ---- Pairing mode (BLE advertising for app) ---- */
static void enter_pairing_mode(void)
{
    /* In production: enable softdevice BLE advertising with tag_id +
     * profile_id in manufacturer data. The mobile app scans, pairs,
     * and writes tag_id + profile_id via a GATT characteristic. */
    nrf_gpio_pin_set(PIN_LED);
    nrf_delay_ms(500);
    nrf_gpio_pin_clear(PIN_LED);
    /* Stay in pairing for 60s then return to normal */
    nrf_delay_ms(60000);
}

/* ---- Deep sleep until next sample ---- */
static void deep_sleep(uint32_t ms)
{
    /* In production: configure RTC to wake after ms, enter System OFF
     * (2 µA) or WFE with RTC (14 µA). The RTC interrupt wakes the MCU. */
    /* Stub: busy-wait (production uses RTC + System OFF) */
    nrf_delay_ms(ms);
}

/* ---- Main loop ---- */
int main(void)
{
    /* Initialize GPIO + peripherals */
    nrf_gpio_cfg_output(PIN_LED);
    nrf_gpio_cfg_output(PIN_SOIL_POWER);
    nrf_gpio_cfg_input(PIN_BUTTON, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_pin_clear(PIN_SOIL_POWER);

    sx1262_init();
    gp_mesh_set_tx(mesh_tx);

    /* Check if pairing button is held at boot */
    if (nrf_gpio_pin_read(PIN_BUTTON) == 0) {
        enter_pairing_mode();
    }

    /* Main sampling loop */
    while (1) {
        send_telemetry();
        deep_sleep(SAMPLE_INTERVAL_MS);
    }

    return 0;
}