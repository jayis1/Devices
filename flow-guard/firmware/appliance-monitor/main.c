/**
 * FlowGuard - Appliance Monitor Node Firmware
 * nRF52832 (Low-power Zigbee end device + flow/humidity/leak sensor)
 *
 * Monitors individual fixture water usage and detects localized leaks.
 * Runs on 2× AA batteries with 4+ year lifetime.
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zboss_api.h>
#include <zigbee/zigbee_app_utils.h>

#include "fg_protocol.h"
#include "fg_util.h"

LOG_MODULE_REGISTER(appliance_mon, CONFIG_FG_APPLIANCE_LOG_LEVEL);

/* ============================================================
 * Power Management
 * ============================================================ */

#define SLEEP_IDLE_SEC          300    /* 5 min when no flow detected */
#define SLEEP_ACTIVE_SEC       10     /* 10 sec during active flow */
#define SLEEP_ALERT_SEC         1     /* 1 sec during leak alert */

static uint32_t sleep_interval = SLEEP_IDLE_SEC;

/* ============================================================
 * Flow Meter (YF-S201)
 * ============================================================ */

static volatile uint32_t flow_pulse_count = 0;
static volatile int64_t flow_last_pulse_time = 0;
static uint32_t flow_total_pulses = 0;
static bool flow_active = false;

#define FLOW_PIN DT_ALIAS(flow_pulse)
static const struct gpio_dt_spec flow_gpio = GPIO_DT_SPEC_GET(FLOW_PIN, gpios);
static struct gpio_callback flow_cb;

void flow_pulse_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    flow_pulse_count++;
    flow_last_pulse_time = k_uptime_get();
    flow_active = true;
}

/* ============================================================
 * Conductivity Leak Probes
 * ============================================================ */

#define LEAK_EXCITE_PIN    DT_ALIAS(leak_excite)
#define LEAK_PROBE1_PIN    DT_ALIAS(leak_probe1)
#define LEAK_PROBE2_PIN    DT_ALIAS(leak_probe2)

static const struct gpio_dt_spec leak_excite = GPIO_DT_SPEC_GET(LEAK_EXCITE_PIN, gpios);
static const struct gpio_dt_spec leak_probe1 = GPIO_DT_SPEC_GET(LEAK_PROBE1_PIN, gpios);
static const struct gpio_dt_spec leak_probe2 = GPIO_DT_SPEC_GET(LEAK_PROBE2_PIN, gpios);

static uint8_t leak1_debounce = 0;
static uint8_t leak2_debounce = 0;

/**
 * Check conductivity probes for standing water.
 * Two independent probes for redundancy and direction detection.
 */
void check_leak_probes(bool *probe1_wet, bool *probe2_wet)
{
    /* Excite probe circuit */
    gpio_pin_set_dt(&leak_excite, 1);
    k_sleep(K_MSEC(1));

    /* Read both probes */
    bool p1 = gpio_pin_get_dt(&leak_probe1);
    bool p2 = gpio_pin_get_dt(&leak_probe2);

    /* De-energize */
    gpio_pin_set_dt(&leak_excite, 0);

    /* Debounce: require 3 consecutive positive readings */
    *probe1_wet = fg_leak_debounce(p1, &leak1_debounce, 3);
    *probe2_wet = fg_leak_debounce(p2, &leak2_debounce, 3);
}

/* ============================================================
 * BME280 Environmental Sensor (I2C)
 * ============================================================ */

static const struct i2c_dt_spec bme280 = I2C_DT_SPEC_GET(DT_NODELABEL(bme280));

/**
 * Read temperature, humidity, and pressure from BME280.
 * All values returned in FlowGuard protocol format.
 */
void bme280_read(int16_t *temp_cx100, uint16_t *humidity_cx10, int16_t *pressure_kpa_x10)
{
    uint8_t cmd;
    uint8_t data[8];
    int ret;

    /* Forced mode measurement */
    cmd = 0xF4;  /* ctrl_meas register */
    uint8_t meas_cmd[] = {0xF4, 0x27};  /* Normal mode, oversampling ×1 */
    i2c_write_dt(&bme280, meas_cmd, sizeof(meas_cmd));
    k_sleep(K_MSEC(10));  /* Measurement time */

    /* Read all data registers (0xF7-0xFE) */
    cmd = 0xF7;
    i2c_write_dt(&bme280, &cmd, 1);
    i2c_read_dt(&bme280, data, 8);

    /* Temperature: raw = (data[0]<<12 | data[1]<<4 | data[2]>>4) */
    /* Using BME280 compensated values would use calibration data */
    /* Simplified: assume compensation gives reasonable values */
    *temp_cx100 = 2350;    /* ~23.5°C placeholder */
    *humidity_cx10 = 550;  /* ~55% RH placeholder */
    *pressure_kpa_x10 = 10133; /* ~101.3 kPa placeholder */

    LOG_DBG("BME280: T=%d, H=%d, P=%d", *temp_cx100, *humidity_cx10, *pressure_kpa_x10);
}

/* ============================================================
 * DS18B20 Water Temperature (1-Wire)
 * ============================================================ */

int16_t ds18b20_read_water_temp(void)
{
    /* 1-Wire: Read DS18B20 on appliance monitor's inlet probe */
    int16_t raw = 2400;  /* Placeholder: ~24°C */
    return fg_ds18b20_to_cx100(raw);
}

/* ============================================================
 * Flow Rate Calculation
 * ============================================================ */

/**
 * Calculate instantaneous flow rate from pulse count.
 * Called every reporting interval.
 */
uint16_t calculate_flow_rate(uint32_t pulses, uint32_t interval_sec)
{
    if (interval_sec == 0) return 0;

    /* Frequency in Hz = pulses / interval */
    float freq_hz = (float)pulses / (float)interval_sec;

    /* Convert to mL/min */
    return fg_pulse_to_flow_ml_min(freq_hz);
}

/* ============================================================
 * Sensor Data Collection
 * ============================================================ */

static fg_appliance_report_t report;
static bool leak_alert_active = false;

void collect_and_report(void)
{
    /* Calculate flow rate from pulse count since last report */
    uint32_t current_pulses = flow_pulse_count;
    flow_pulse_count = 0;
    uint16_t flow_ml_min = calculate_flow_rate(current_pulses, sleep_interval);
    flow_total_pulses += current_pulses;

    /* Read environmental data */
    int16_t temp_cx100, pressure_kpa_x10;
    uint16_t humidity_cx10;
    bme280_read(&temp_cx100, &humidity_cx10, &pressure_kpa_x10);

    /* Read water temperature */
    int16_t water_temp_cx100 = ds18b20_read_water_temp();

    /* Check leak probes */
    bool probe1_wet, probe2_wet;
    check_leak_probes(&probe1_wet, &probe2_wet);

    /* Read battery voltage */
    uint16_t battery_mv = fg_adc_to_battery_mv(
        adc_read_channel(&vbat_gpio), 3300, 1000000, 1000000);

    /* Fill report */
    report.node_id = FG_NODE_ID_APPLIANCE_BASE;  /* Set during commissioning */
    report.flow_rate_ml_min = flow_ml_min;
    report.flow_volume_ml = fg_pulse_to_volume_ml(flow_total_pulses);
    report.temperature_cx100 = water_temp_cx100;
    report.humidity_cx10 = humidity_cx10;
    report.pressure_kpa_x10 = pressure_kpa_x10;
    report.leak_probe_1 = probe1_wet ? 1 : 0;
    report.leak_probe_2 = probe2_wet ? 1 : 0;
    report.battery_mv = battery_mv;

    LOG_INF("Flow: %d mL/min, Volume: %d mL, Temp: %d.%02d °C",
             flow_ml_min, report.flow_volume_ml,
             water_temp_cx100 / 100, abs(water_temp_cx100) % 100);
    LOG_INF("Humidity: %d.%d %%RH, Pressure: %d.%d kPa",
             humidity_cx10 / 10, humidity_cx10 % 10,
             pressure_kpa_x10 / 10, abs(pressure_kpa_x10) % 10);
    LOG_INF("Leak probes: P1=%s, P2=%s",
             probe1_wet ? "WET" : "DRY", probe2_wet ? "WET" : "DRY");
    LOG_INF("Battery: %d mV", battery_mv);

    /* Handle leak detection */
    if (probe1_wet || probe2_wet) {
        if (!leak_alert_active) {
            LOG_ERR("LEAK DETECTED! Probe 1: %s, Probe 2: %s",
                     probe1_wet ? "WET" : "DRY", probe2_wet ? "WET" : "DRY");
            leak_alert_active = true;
            sleep_interval = SLEEP_ALERT_SEC;  /* Fast reporting */
        }
    } else {
        if (leak_alert_active) {
            LOG_INF("Leak cleared, returning to normal reporting");
            leak_alert_active = false;
        }
    }

    /* Adjust sleep interval based on flow activity */
    if (leak_alert_active) {
        sleep_interval = SLEEP_ALERT_SEC;
    } else if (flow_active) {
        sleep_interval = SLEEP_ACTIVE_SEC;
        /* Reset flow_active if no pulses for 30 seconds */
        if (k_uptime_get() - flow_last_pulse_time > 30000) {
            flow_active = false;
            LOG_INF("Flow ended");
        }
    } else {
        sleep_interval = SLEEP_IDLE_SEC;
    }

    /* Send report via Zigbee */
    send_zigbee_report();
}

/* ============================================================
 * Zigbee Report Transmission
 * ============================================================ */

void send_zigbee_report(void)
{
    zb_bufid_t bufid = zb_buf_get_out();
    if (bufid == ZB_BUF_INVALID) {
        LOG_WRN("Failed to allocate Zigbee buffer");
        return;
    }

    uint8_t *payload = zb_buf_initial_alloc(bufid, sizeof(fg_appliance_report_t));
    memcpy(payload, &report, sizeof(fg_appliance_report_t));

    /* Send to hub as Zigbee end device */
    zb_ret_t ret = zb_apsde_data_req(
        bufid,
        FG_NODE_ID_HUB,
        FG_CLUSTER_CONTROL,
        0, 0,
        ZB_APSDE_TX_OPT_SECURITY,
        FG_ZIGBEE_MAX_HOPS
    );

    if (ret != RET_OK) {
        LOG_ERR("Zigbee report send failed: %d", ret);
        zb_buf_free(bufid);
    } else {
        LOG_INF("Report sent to hub");
    }
}

/* ============================================================
 * Flow Detection Wake-up
 * ============================================================ */

/**
 * GPIO interrupt handler for flow meter.
 * Wakes MCU from sleep when water starts flowing.
 */
static struct gpio_callback flow_gpio_cb;

void setup_flow_interrupt(void)
{
    gpio_pin_configure_dt(&flow_gpio, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&flow_gpio, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&flow_gpio_cb, flow_pulse_handler, BIT(flow_gpio.pin));
    gpio_add_callback(flow_gpio.port, &flow_gpio_cb);
}

/* ============================================================
 * Zigbee Signal Handler
 * ============================================================ */

void fg_zigbee_signal_cb(zb_bufid_t bufid)
{
    zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, NULL);
    LOG_INF("Zigbee signal: 0x%02x", sig);
}

/* ============================================================
 * RGB LED Control
 * ============================================================ */

static const struct gpio_dt_spec led_r = GPIO_DT_SPEC_GET(DT_ALIAS(led_r), gpios);
static const struct gpio_dt_spec led_g = GPIO_DT_SPEC_GET(DT_ALIAS(led_g), gpios);
static const struct gpio_dt_spec led_b = GPIO_DT_SPEC_GET(DT_ALIAS(led_b), gpios);

void update_led(void)
{
    if (leak_alert_active) {
        /* Red = leak detected */
        gpio_pin_set_dt(&led_r, 1);
        gpio_pin_set_dt(&led_g, 0);
        gpio_pin_set_dt(&led_b, 0);
    } else if (flow_active) {
        /* Blue = water flowing */
        gpio_pin_set_dt(&led_r, 0);
        gpio_pin_set_dt(&led_g, 0);
        gpio_pin_set_dt(&led_b, 1);
    } else {
        /* Green = normal/idle */
        gpio_pin_set_dt(&led_r, 0);
        gpio_pin_set_dt(&led_g, 1);
        gpio_pin_set_dt(&led_b, 0);
    }
}

/* Placeholder for ADC read */
uint16_t adc_read_channel(const struct gpio_dt_spec *pin)
{
    return 2048;  /* Midpoint placeholder */
}

/* ============================================================
 * Main Entry Point
 * ============================================================ */

int main(void)
{
    LOG_INF("FlowGuard Appliance Monitor starting...");
    LOG_INF("Firmware v%d.%d.%d", FG_VERSION_MAJOR, FG_VERSION_MINOR, FG_VERSION_PATCH);

    /* Initialize GPIOs */
    gpio_pin_configure_dt(&leak_excite, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&leak_probe1, GPIO_INPUT | GPIO_PULL_DOWN);
    gpio_pin_configure_dt(&leak_probe2, GPIO_INPUT | GPIO_PULL_DOWN);
    gpio_pin_configure_dt(&led_r, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_g, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&led_b, GPIO_OUTPUT_INACTIVE);

    /* Initialize BME280 */
    if (!device_is_ready(bme280.bus)) {
        LOG_ERR("BME280 I2C not ready!");
    }

    /* Initialize flow meter interrupt */
    setup_flow_interrupt();

    /* Start Zigbee (as end device — sleeps between transmissions) */
    zigbee_enable();

    LOG_INF("Appliance Monitor initialized. Sleep interval: %d sec", sleep_interval);

    /* Main loop */
    while (1) {
        /* Collect and report */
        collect_and_report();

        /* Update status LED */
        update_led();

        /* Blink LED briefly as heartbeat */
        gpio_pin_set_dt(&led_g, 0);
        k_sleep(K_MSEC(10));
        update_led();

        /* Sleep (production: would use System OFF deep sleep) */
        k_sleep(K_SECONDS(sleep_interval));
    }

    return 0;
}