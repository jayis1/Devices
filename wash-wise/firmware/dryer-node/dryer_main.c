/*
 * dryer_main.c — WashWise Dryer Node (ESP32-S3)
 *
 * ⚠️ SAFETY-CRITICAL NODE — always running, battery backup
 *
 * Responsibilities:
 * - Lint trap clog detection (differential pressure)
 * - Exhaust temperature monitoring (K-type thermocouple)
 * - Dryness detection (exhaust humidity)
 * - Vibration monitoring (drum imbalance)
 * - Cycle detection (current sensor)
 * - Fire risk computation (local ML + cloud)
 * - Fire alert broadcast (SF10, overrides TDMA)
 * - Reports to hub every 10s (active) / 60s (idle)
 *
 * Sensors:
 * - MPXV7002DP: differential pressure (lint clog) — I2C/analog
 * - MAX6675 + K-type thermocouple: exhaust temp (SPI)
 * - SHT40: exhaust humidity + ambient temp (I2C)
 * - DS18B20: ambient room temp (1-Wire)
 * - ADXL313: vibration (I2C)
 * - ACS712: current clamp on dryer power (analog)
 * - MQ-2: smoke/gas (analog, optional)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/spi.h"
#include "driver/i2c.h"
#include "esp_timer.h"
#include "esp_sleep.h"

#include "mesh_protocol.h"

static const char *TAG = "DRYER";

/* ---- Pin Definitions (ESP32-S3) ---- */
#define PIN_SX1261_SCK    12
#define PIN_SX1261_MOSI   13
#define PIN_SX1261_MISO   14
#define PIN_SX1261_CS     15
#define PIN_SX1261_BUSY   16
#define PIN_SX1261_IRQ    17
#define PIN_SX1261_NRST   18

#define PIN_MAX6675_SCK   10
#define PIN_MAX6675_CS    11
#define PIN_MAX6675_MISO 9

/* ADC channels */
#define ADC_DIFF_PRESSURE  ADC1_CHANNEL_0  /* GPIO1 — MPXV7002DP */
#define ADC_CURRENT        ADC1_CHANNEL_1  /* GPIO2 — ACS712 */
#define ADC_SMOKE          ADC1_CHANNEL_2  /* GPIO3 — MQ-2 (optional) */
#define ADC_BATTERY        ADC1_CHANNEL_3  /* GPIO4 — battery voltage divider */

/* I2C */
#define I2C_PORT           I2C_NUM_0
#define PIN_I2C_SDA        8
#define PIN_I2C_SCL        9
#define SHT40_ADDR         0x44
#define ADXL313_ADDR       0x1D

/* Dryer state thresholds */
#define EXH_TEMP_WARNING_C    85.0f
#define EXH_TEMP_CRITICAL_C   95.0f
#define EXH_TEMP_EMERGENCY_C  105.0f
#define DIFF_PRESS_WARNING_PA 120.0f
#define DIFF_PRESS_CRITICAL_PA 200.0f
#define VIBRATION_IMBALANCE_MG 400.0f  /* 0.4g */

/* Reporting intervals */
#define REPORT_INTERVAL_ACTIVE_MS  10000  /* 10s when dryer running */
#define REPORT_INTERVAL_IDLE_MS    60000  /* 60s when idle */

/* ---- Sensor Reading Stubs (hardware-specific in production) ---- */

static float read_differential_pressure(void)
{
    /* MPXV7002DP: analog 0.5-4.5V → -1000 to +1000 Pa
     * Read ADC, convert to Pa */
    /* Stub: return simulated value */
    return 45.0f;  /* normal range ~20-80 Pa */
}

static float read_exhaust_temp(void)
{
    /* MAX6675: SPI read, 12-bit thermocouple ADC
     * Returns temp in °C (0-1024°C range) */
    /* Stub: return simulated normal temp */
    return 65.0f;  /* normal exhaust: 50-75°C */
}

static float read_ambient_temp(void)
{
    /* DS18B20: 1-Wire, ±0.1°C accuracy */
    return 22.0f;
}

static float read_exhaust_humidity(void)
{
    /* SHT40: I2C, ±1.5% RH accuracy */
    return 35.0f;  /* drops when clothes dry */
}

static float read_vibration_rms(void)
{
    /* ADXL313: I2C, read x/y/z, compute RMS */
    return 120.0f;  /* milli-g */
}

static uint16_t read_current_ma(void)
{
    /* ACS712: 100mV/A, offset at VCC/2
     * Dryer: 0 (off) / 2-4A (tumble) / 10-25A (heating) */
    return 0;  /* idle */
}

static uint8_t read_smoke_level(void)
{
    /* MQ-2: analog, threshold ~300 ppm */
    return 0;  /* clean */
}

static uint8_t read_battery_pct(void)
{
    /* ADC battery voltage divider: 3.0V=0%, 4.2V=100% */
    return 85;
}

/* ---- SX1261 Radio (stub) ---- */

static void sx1261_init(void)
{
    ESP_LOGI(TAG, "SX1261 initialized (868MHz LoRa)");
}

static void sx1261_send(const uint8_t *data, uint16_t len)
{
    /* In production: SPI write FIFO, set TX mode */
}

/* ---- Cycle Detection ---- */

typedef enum {
    DRYER_OFF = 0,
    DRYER_HEATING,
    DRYER_TUMBLING,
    DRYER_COOLING,
    DRYER_DONE
} dryer_state_t;

static dryer_state_t detect_cycle_state(uint16_t current_ma, float exh_temp)
{
    if (current_ma < 100) {
        return DRYER_OFF;
    } else if (current_ma > 8000) {
        /* High current = heating element on */
        return DRYER_HEATING;
    } else if (current_ma > 1000) {
        /* Motor only = tumbling without heat */
        return DRYER_TUMBLING;
    } else if (exh_temp > 40.0f && current_ma < 1000) {
        return DRYER_COOLING;
    }
    return DRYER_OFF;
}

/* ---- Dryness Detection ---- */

static uint8_t compute_dryness(float exh_humidity, dryer_state_t state)
{
    /* When clothes are wet: exhaust humidity is high (60-90%)
     * When clothes dry: humidity drops to ambient (20-35%)
     * 0=wet, 1=damp, 2=dry, 3=over-dry */
    if (state == DRYER_OFF)
        return 0;

    if (exh_humidity > 55.0f) return 0; /* wet */
    if (exh_humidity > 40.0f) return 1; /* damp */
    if (exh_humidity > 25.0f) return 2; /* dry */
    return 3; /* over-dry — stop! */
}

/* ---- Lint Clog Level ---- */

static uint8_t compute_lint_clog(float diff_pressure_pa)
{
    /* Normal: 10-80 Pa
     * Mild clog: 80-120 Pa
     * Moderate: 120-200 Pa
     * Severe: >200 Pa (fire risk!) */
    if (diff_pressure_pa > 200.0f) return 3; /* severe */
    if (diff_pressure_pa > 120.0f) return 2; /* moderate */
    if (diff_pressure_pa > 80.0f)  return 1; /* mild */
    return 0; /* clean */
}

/* ---- Fire Risk Score (local ML stub) ---- */

static uint8_t compute_fire_risk(float exh_temp, float diff_pres,
                                  float humidity, uint8_t heating_on,
                                  uint8_t smoke_level)
{
    /* In production: TFLite Micro 1D-CNN+LSTM
     * Stub: rule-based heuristic (mirrors hub logic) */
    float score = 0.0f;

    /* Primary: exhaust temperature */
    if (exh_temp > 105.0f)      score += 0.9f;
    else if (exh_temp > 95.0f)  score += 0.7f;
    else if (exh_temp > 85.0f)  score += 0.4f;
    else if (exh_temp > 75.0f)  score += 0.15f;

    /* Amplifier: high backpressure (lint clog) */
    if (diff_pres > 200.0f)    score += 0.3f;
    else if (diff_pres > 120.0f) score += 0.15f;

    /* Dry lint + heat = more flammable */
    if (heating_on && humidity < 20.0f) score += 0.15f;

    /* Smoke detected = immediate high risk */
    if (smoke_level > 200)     score = 0.95f;

    if (score > 1.0f) score = 1.0f;
    return (uint8_t)(score * 255.0f);
}

/* ---- Send telemetry to hub ---- */

static void send_dryer_data(const dryer_data_payload_t *data)
{
    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(
        NODE_ID_DRYER, NODE_ID_HUB, PKT_DRYER_DATA,
        (const uint8_t *)data, sizeof(*data), &pkt);
    sx1261_send((uint8_t *)&pkt, len);
}

static void send_fire_alert(const fire_alert_payload_t *alert)
{
    /* Fire alerts use SF10 (long range) + broadcast address */
    mesh_packet_t pkt;
    uint16_t len = mesh_build_packet(
        NODE_ID_DRYER, NODE_ID_BROADCAST, PKT_FIRE_ALERT,
        (const uint8_t *)alert, sizeof(*alert), &pkt);
    sx1261_send((uint8_t *)&pkt, len);
    ESP_LOGW(TAG, "FIRE ALERT broadcast! Risk=%d/255", alert->fire_risk_score);
}

/* ---- Main Task ---- */

void dryer_task(void *arg)
{
    ESP_LOGI(TAG, "WashWise Dryer Node starting (SAFETY-CRITICAL)");

    dryer_data_payload_t dryer_data;
    memset(&dryer_data, 0, sizeof(dryer_data));

    bool dryer_was_running = false;
    uint32_t cycle_start_time = 0;
    uint16_t cycle_duration_s = 0;

    while (1) {
        /* Read all sensors */
        float diff_pres = read_differential_pressure();
        float exh_temp = read_exhaust_temp();
        float amb_temp = read_ambient_temp();
        float exh_hum = read_exhaust_humidity();
        float vib_rms = read_vibration_rms();
        uint16_t current = read_current_ma();
        uint8_t smoke = read_smoke_level();
        uint8_t batt = read_battery_pct();

        dryer_state_t state = detect_cycle_state(current, exh_temp);
        uint8_t dryness = compute_dryness(exh_hum, state);
        uint8_t lint_clog = compute_lint_clog(diff_pres);
        uint8_t heating_on = (state == DRYER_HEATING) ? 1 : 0;

        /* Fire risk computation */
        uint8_t fire_risk = compute_fire_risk(exh_temp, diff_pres,
                                              exh_hum, heating_on, smoke);

        /* Track cycle duration */
        bool dryer_running = (state != DRYER_OFF);
        if (dryer_running && !dryer_was_running) {
            cycle_start_time = esp_timer_get_time() / 1000000;
            ESP_LOGI(TAG, "Dryer cycle started");
        }
        if (!dryer_running && dryer_was_running) {
            cycle_duration_s = (esp_timer_get_time() / 1000000) - cycle_start_time;
            ESP_LOGI(TAG, "Dryer cycle complete: %d min %d s",
                     cycle_duration_s / 60, cycle_duration_s % 60);
        }
        dryer_was_running = dryer_running;

        /* Build telemetry payload */
        dryer_data.exhaust_temp_c_x10 = (int16_t)(exh_temp * 10);
        dryer_data.ambient_temp_c_x10 = (int16_t)(amb_temp * 10);
        dryer_data.diff_pressure_pa  = (uint16_t)diff_pres;
        dryer_data.exhaust_hum_x10   = (uint16_t)(exh_hum * 10);
        dryer_data.vibration_rms_x10 = (uint16_t)(vib_rms * 10);
        dryer_data.current_ma        = current;
        dryer_data.dryer_state       = state;
        dryer_data.heating_on        = heating_on;
        dryer_data.fire_risk_score   = fire_risk;
        dryer_data.lint_clog_level   = lint_clog;
        dryer_data.dryness_level     = dryness;
        dryer_data.alert_level       = ALERT_OK;
        dryer_data.battery_pct       = batt;
        dryer_data.signal_rssi       = 0;
        dryer_data.reserved          = 0;

        /* Set alert level based on risk */
        float risk_f = fire_risk / 255.0f;
        if (risk_f > 0.95f || exh_temp > EXH_TEMP_EMERGENCY_C) {
            dryer_data.alert_level = ALERT_EMERGENCY;
        } else if (risk_f > 0.8f) {
            dryer_data.alert_level = ALERT_CRITICAL;
        } else if (risk_f > 0.6f) {
            dryer_data.alert_level = ALERT_WARNING;
        }

        /* Send telemetry to hub */
        send_dryer_data(&dryer_data);

        /* EMERGENCY: Fire alert broadcast */
        if (dryer_data.alert_level >= ALERT_CRITICAL) {
            fire_alert_payload_t alert;
            memset(&alert, 0, sizeof(alert));
            alert.alert_level = dryer_data.alert_level;
            alert.fire_risk_score = fire_risk;
            alert.exhaust_temp_c_x10 = dryer_data.exhaust_temp_c_x10;
            alert.diff_pressure_pa = (uint16_t)diff_pres;
            alert.lint_clog_level = lint_clog;
            alert.heating_on = heating_on;
            alert.source_node = NODE_ID_DRYER;
            alert.timestamp_ms = (uint16_t)(esp_timer_get_time() / 1000);
            send_fire_alert(&alert);
        }

        /* Log to console */
        ESP_LOGI(TAG, "state=%d exh=%.1fC pres=%.0fPa hum=%.0f%% vib=%.0fmA risk=%d lint=%d dry=%d",
                 state, exh_temp, diff_pres, exh_hum, vib_rms, fire_risk, lint_clog, dryness);

        /* Determine sleep interval: shorter when dryer active */
        uint32_t interval = dryer_running ? REPORT_INTERVAL_ACTIVE_MS : REPORT_INTERVAL_IDLE_MS;

        /* When idle and battery < 20%, go to deep sleep between reports to save power
         * (but NEVER when dryer was recently running — thermal mass fire risk) */
        if (!dryer_running && batt < 20 &&
            (cycle_duration_s == 0 || /* never ran */
             (esp_timer_get_time() / 1000000) - cycle_start_time - cycle_duration_s > 300)) {
            /* Deep sleep for 55 seconds, wake via timer */
            esp_sleep_enable_timer_wakeup(55 * 1000000);
            esp_deep_sleep_start();
            /* Returns here after wake */
        } else {
            vTaskDelay(pdMS_TO_TICKS(interval));
        }
    }
}

void app_main(void)
{
    /* Initialize I2C for SHT40 + ADXL313 */
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = true,
        .scl_pullup_en = true,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_PORT, &i2c_cfg);
    i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);

    /* Initialize ADC */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_DIFF_PRESSURE, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_CURRENT, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_SMOKE, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_BATTERY, ADC_ATTEN_DB_11);

    /* Initialize SPI for MAX6675 + SX1261 */
    spi_bus_config_t spi_cfg = {
        .mosi_io_num = PIN_MAX6675_SCK, /* simplified */
        .miso_io_num = PIN_MAX6675_MISO,
        .sclk_io_num = PIN_MAX6675_SCK,
        .max_transfer_sz = 4096,
    };
    /* In production: spi_bus_initialize(SPI2_HOST, &spi_cfg, 0) */

    sx1261_init();

    xTaskCreate(dryer_task, "dryer_task", 8192, NULL, 5, NULL);
}