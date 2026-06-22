/*
 * uv_patch_main.c — SkinSync UV Patch Node firmware (nRF52832, nRF5 SDK)
 *
 * Samples UVA + UVB (VEML6075), skin temperature (TMP117), and ambient
 * light/UV index (LTR390) every 1 minute when active (outdoor/light
 * detected) or every 5 minutes when sleeping (indoor/dark). Accumulates
 * UVA/UVB dose, computes MED fraction from personal MED received from hub,
 * triggers haptic alerts at 50/70/90% MED thresholds, transmits telemetry
 * over Sub-GHz mesh (SX1262, 868/915 MHz), and deep-sleeps (~14 µA)
 * between samples for 14-day coin-cell life.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <string.h>
#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "skin_protocol.h"

/* ---- Hardware pins ---- */
#define PIN_SX1262_CS      8
#define PIN_SX1262_RST     12
#define PIN_SX1262_BUSY    13
#define PIN_SX1262_IRQ     14
#define PIN_SENSOR_POWER   20   /* power-gate I2C sensors for low leak */
#define PIN_LED            1
#define PIN_BUTTON         2
#define PIN_HAPTIC_GPIO    5    /* DRV2605L GPIO trigger (alt to I2C) */

/* ---- Sampling config ---- */
#define SAMPLE_INTERVAL_ACTIVE_MS  60000   /* 1 minute when outdoor */
#define SAMPLE_INTERVAL_SLEEP_MS   300000  /* 5 minutes when indoor/dark */
#define INDOOR_LUX_THRESHOLD       100     /* lux below this = indoor */

/* ---- State ---- */
static volatile uint8_t seq = 0;
static uint8_t patch_id = 1;       /* set during pairing */
static uint8_t fitz_type = SS_FITZ_III;
static uint16_t personal_med = 350; /* J/m², received from hub */
static uint16_t med_threshold_50 = 175;
static uint16_t med_threshold_70 = 245;
static uint16_t med_threshold_90 = 315;

/* Cumulative dose accumulators (J/m² * 10) */
static uint16_t uva_total = 0;
static uint16_t uvb_total = 0;
static uint16_t last_uva_total = 0;
static uint16_t last_uvb_total = 0;
static uint8_t  med_fraction = 0;
static int16_t  skin_temp_baseline = 3200; /* 32.00°C */
static uint8_t  med_alert_sent = 0;        /* bitmask: 50/70/90 */

/* ---- SX1262 Sub-GHz radio (simplified SPI interface) ---- */
static void sx1262_init(void)
{
    nrf_gpio_cfg_output(PIN_SX1262_CS);
    nrf_gpio_cfg_output(PIN_SX1262_RST);
    nrf_gpio_cfg_input(PIN_SX1262_BUSY, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(PIN_SX1262_IRQ, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_pin_set(PIN_SX1262_CS);
    /* Full SPI + register init: set frequency 868/915 MHz, modulation,
     * TX power +10 dBm, sync word 0xSS, CRC. See SX1262 datasheet. */
}

static int sx1262_tx(const uint8_t *data, size_t len)
{
    /* In production: load TX buffer, set TX mode, wait for IRQ (TxDone). */
    (void)data; (void)len;
    return (int)len;
}

/* ---- VEML6075 UVA + UVB sensor (I2C 0x10) ---- */
typedef struct { uint16_t uva_raw; uint16_t uvb_raw; } veml6075_data_t;

static veml6075_data_t veml6075_read(void)
{
    /* In production: read UVA (reg 0x07) + UVB (reg 0x09) + UVCOMP1 (0x0A)
     * + UVCOMP2 (0x0B). Convert raw to irradiance:
     *   UVA_wm2 = uva_raw * 0.93 * UVA_sensitivity / integration_time
     *   UVB_wm2 = uvb_raw * 2.08 * UVB_sensitivity / integration_time
     * Apply compensation from UVCOMP1/UVCOMP2 (visible + IR leakage).
     * Returns raw counts as proxy. */
    veml6075_data_t d = { 0, 0 };
    return d;
}

/* ---- TMP117 skin temperature (I2C 0x48) ---- */
static int16_t tmp117_read_centic(void)
{
    /* In production: read TMP117 register 0x00, convert:
     * T_centi = raw * 7.8125 / 100 (16-bit signed, 0.0078°C LSB) */
    return 3200; /* 32.00°C stub */
}

/* ---- LTR390 UV index + ambient light (I2C 0x53) ---- */
typedef struct { uint8_t uv_index_10; uint16_t lux; } ltr390_data_t;

static ltr390_data_t ltr390_read(void)
{
    /* In production: read UVS + ALS registers, convert to UV index + lux.
     * UV index = uvs_raw / sensitivity * gain_correction
     * lux = als_raw / sensitivity */
    ltr390_data_t d = { 0, 500 }; /* UV index 0.0, 500 lux stub (indoor) */
    return d;
}

/* ---- DRV2605L haptic driver (I2C 0x5A) ---- */
static void haptic_pulse(uint8_t pattern)
{
    /* In production: write DRV2605L register 0x01 (mode) = 0 (internal trig),
     * register 0x0C (waveform seq) = pattern, register 0x0D = 0 (end).
     * Patterns:
     *   1 pulse (50% MED): waveform 17 (strong click 60%)
     *   2 pulses (70% MED): waveform 48 (double click 60%)
     *   3 pulses (90% MED): waveform 65 (alert 100%) */
    (void)pattern;
    nrf_gpio_pin_set(PIN_HAPTIC_GPIO);
    nrf_delay_ms(50);
    nrf_gpio_pin_clear(PIN_HAPTIC_GPIO);
}

/* ---- Compute UV dose from sensor readings ---- */
static void update_dose(veml6075_data_t *veml, ltr390_data_t *ltr, uint32_t dt_seconds)
{
    /* Convert raw UV readings to irradiance (W/m²) and integrate over time.
     *
     * VEML6075 UVA/UVB raw → irradiance:
     *   uva_wm2 = veml->uva_raw * 0.93 / integration_time_ms
     *   uvb_wm2 = veml->uvb_raw * 2.08 / integration_time_ms
     *
     * Dose (J/m²) = irradiance (W/m²) * time (s)
     * We store dose * 10 for sub-J/m² resolution in uint16_t.
     *
     * For this stub: use UV index as proxy.
     * UV index 10 ≈ 250 W/m² total UV ≈ 25 W/m² UVB (10:1 UVA:UVB ratio)
     * erythemally effective ≈ UVB * 1.0 + UVA * 0.05 */
    float uv_index = (float)ltr->uv_index_10 / 10.0f;
    float total_uv_wm2 = uv_index * 25.0f;       /* rough: UV idx → W/m² */
    float uvb_wm2 = total_uv_wm2 * 0.1f;          /* ~10% UVB */
    float uva_wm2 = total_uv_wm2 * 0.9f;          /* ~90% UVA */

    /* Dose in J/m² * 10 */
    uint16_t uva_dose = (uint16_t)(uva_wm2 * dt_seconds * 10.0f);
    uint16_t uvb_dose = (uint16_t)(uvb_wm2 * dt_seconds * 10.0f);

    uva_total += uva_dose;
    uvb_total += uvb_dose;

    /* Compute MED fraction using erythema effectiveness */
    float eff_dose = ss_erythema_weighted_dose(uva_wm2, uvb_wm2,
                                               (float)dt_seconds);
    /* eff_dose is in J/m² for this interval.
     * Accumulate effective dose and compare to personal MED. */
    static float eff_dose_accum = 0;
    eff_dose_accum += eff_dose;
    med_fraction = (uint8_t)((eff_dose_accum / (float)personal_med) * 100.0f);
    if (med_fraction > 100) med_fraction = 100;
}

/* ---- Check MED thresholds and trigger haptic ---- */
static void check_med_alerts(void)
{
    if ((med_alert_sent & 0x01) == 0 && med_fraction >= 50) {
        haptic_pulse(17);  /* 1 pulse — 50% MED */
        med_alert_sent |= 0x01;
    }
    if ((med_alert_sent & 0x02) == 0 && med_fraction >= 70) {
        haptic_pulse(48);  /* 2 pulses — 70% MED */
        med_alert_sent |= 0x02;
    }
    if ((med_alert_sent & 0x04) == 0 && med_fraction >= 90) {
        haptic_pulse(65);  /* 3 pulses — 90% MED */
        med_alert_sent |= 0x04;
    }
}

/* ---- Read battery voltage via ADC divider ---- */
static uint8_t read_battery_pct(void)
{
    /* CR2477: 3.0V full → 2.0V empty.
     * Read battery via SAADC with 1/2 divider, map 2.0-3.0V → 0-100%. */
    return 85; /* stub */
}

/* ---- Assemble and send telemetry ---- */
static void send_telemetry(void)
{
    uint16_t uva_delta = uva_total - last_uva_total;
    uint16_t uvb_delta = uvb_total - last_uvb_total;
    last_uva_total = uva_total;
    last_uvb_total = uvb_total;

    int16_t skin_temp = tmp117_read_centic();
    ltr390_data_t ltr = ltr390_read();
    uint8_t batt = read_battery_pct();

    uint8_t flags = 0;
    if (batt < 15) flags |= SS_ALERT_LOW_BATT;
    if (med_fraction >= 50) flags |= SS_ALERT_MED_50;
    if (med_fraction >= 70) flags |= SS_ALERT_MED_70;
    if (med_fraction >= 90) flags |= SS_ALERT_MED_90;

    /* Skin temp flush detection: >2°C rise from baseline */
    if (skin_temp - skin_temp_baseline > 200) {  /* 2.00°C in centi */
        flags |= SS_ALERT_FLUSH;
    }

    ss_telemetry_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = SS_MSG_TELEMETRY;
    p.node_id = patch_id;
    p.seq = seq++;
    p.flags = flags;
    p.uva_dose_delta = uva_delta;
    p.uvb_dose_delta = uvb_delta;
    p.uva_total = uva_total;
    p.uvb_total = uvb_total;
    p.skin_temp_centic = skin_temp;
    p.uv_index = ltr.uv_index_10;
    p.med_fraction = med_fraction;
    p.battery_pct = batt;
    ss_pack_crc(&p, sizeof(p) - 2);

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
    /* In production: enable softdevice BLE advertising with patch_id +
     * fitz_type in manufacturer data. The mobile app scans, pairs,
     * and writes patch_id + fitz_type + personal_med via GATT. */
    nrf_gpio_pin_set(PIN_LED);
    nrf_delay_ms(500);
    nrf_gpio_pin_clear(PIN_LED);
    nrf_delay_ms(60000);  /* 60s pairing window */
}

/* ---- Deep sleep until next sample ---- */
static void deep_sleep(uint32_t ms)
{
    /* In production: configure RTC to wake after ms, enter System OFF
     * (2 µA) or WFE with RTC (14 µA). The RTC interrupt wakes the MCU. */
    nrf_delay_ms(ms);  /* stub: busy-wait */
}

/* ---- Main loop ---- */
int main(void)
{
    /* Initialize GPIO + peripherals */
    nrf_gpio_cfg_output(PIN_LED);
    nrf_gpio_cfg_output(PIN_SENSOR_POWER);
    nrf_gpio_cfg_output(PIN_HAPTIC_GPIO);
    nrf_gpio_cfg_input(PIN_BUTTON, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_pin_clear(PIN_SENSOR_POWER);

    sx1262_init();
    ss_mesh_set_tx(mesh_tx);

    /* Check if pairing button is held at boot */
    if (nrf_gpio_pin_read(PIN_BUTTON) == 0) {
        enter_pairing_mode();
    }

    /* Set MED thresholds from personal MED */
    med_threshold_50 = (uint16_t)(personal_med * 0.5f);
    med_threshold_70 = (uint16_t)(personal_med * 0.7f);
    med_threshold_90 = (uint16_t)(personal_med * 0.9f);

    /* Establish skin temp baseline (3 readings averaged) */
    nrf_gpio_pin_set(PIN_SENSOR_POWER);
    nrf_delay_ms(100);
    int32_t temp_sum = 0;
    for (int i = 0; i < 3; i++) {
        temp_sum += tmp117_read_centic();
        nrf_delay_ms(100);
    }
    skin_temp_baseline = (int16_t)(temp_sum / 3);
    nrf_gpio_pin_clear(PIN_SENSOR_POWER);

    uint32_t last_sample_ms = 0;

    /* Main sampling loop */
    while (1) {
        /* Power-gate sensors */
        nrf_gpio_pin_set(PIN_SENSOR_POWER);
        nrf_delay_ms(10);  /* let sensors settle */

        /* Read sensors */
        veml6075_data_t veml = veml6075_read();
        ltr390_data_t ltr = ltr390_read();
        int16_t skin_temp = tmp117_read_centic();

        /* Power off sensors */
        nrf_gpio_pin_clear(PIN_SENSOR_POWER);

        /* Determine sampling interval: indoor (low lux) = sleep mode */
        uint32_t dt = (ltr.lux < INDOOR_LUX_THRESHOLD)
                      ? SAMPLE_INTERVAL_SLEEP_MS / 1000
                      : SAMPLE_INTERVAL_ACTIVE_MS / 1000;

        /* Update UV dose accumulators */
        update_dose(&veml, &ltr, dt);

        /* Check MED thresholds and trigger haptic */
        check_med_alerts();

        /* Send telemetry every 5 min (or when MED threshold crossed) */
        uint32_t now = 0; /* in production: app_timer */
        if (now - last_sample_ms >= 300000 || med_alert_sent) {
            send_telemetry();
            last_sample_ms = now;
        }

        /* Deep sleep until next sample */
        deep_sleep(dt * 1000);
    }

    return 0;
}