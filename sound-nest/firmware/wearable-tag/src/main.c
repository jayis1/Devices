/**
 * @file main.c
 * @brief SoundNest Wearable Sound Tag — Main entry point.
 *
 * Personal sound dose tracker and haptic alert device.
 * nRF52832 with SPH0645 MEMS mic, LIS2DH12 accelerometer,
 * ERM haptic motor, and RGB LED.
 *
 * Monitors personal sound exposure, provides haptic alerts
 * for critical sound events, and tracks daily dose metrics.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <math.h>

LOG_MODULE_REGISTER(wearable_tag, LOG_LEVEL_INF);

#include "../../../common/protocol/mesh_packet.h"
#include "../../../common/dsp/spl.h"

/* ── Configuration ──────────────────────────────────────────────────── */

#define MIC_SAMPLE_RATE      16000
#define SPL_WINDOW_MS          1000   /* 1-second SPL averaging */
#define SPL_WINDOW_SAMPLES    (MIC_SAMPLE_RATE * SPL_WINDOW_MS / 1000)
#define DOSE_REPORT_INTERVAL_MS  30000  /* 30-second dose reports */
#define HEARTBEAT_INTERVAL_MS    60000  /* 1-minute heartbeats */

/* Haptic patterns (pulse durations in ms) */
#define HAPTIC_PATTERN_INFO     50
#define HAPTIC_PATTERN_LOW      100
#define HAPTIC_PATTERN_MEDIUM   100, 100, 100
#define HAPTIC_PATTERN_HIGH     200, 100, 200, 100, 200
#define HAPTIC_PATTERN_CRITICAL 500, 200, 500

/* LED color definitions (R:3 | G:3 | B:2 format) */
#define LED_RED     0x07  /* 111 in 3:3:2 */
#define LED_ORANGE  0x1C  /* 111000 in 3:3:2 = R:7, G:3, B:0 */
#define LED_YELLOW  0x1E  /* R:7, G:7, B:0 → 111110 */
#define LED_GREEN   0x04  /* R:0, G:4, B:0 */
#define LED_BLUE    0x03  /* R:0, G:0, B:3 */

/* ── State ──────────────────────────────────────────────────────────── */

typedef struct {
    /* Identity */
    uint16_t node_addr;
    uint16_t hub_addr;
    uint16_t seq_num;

    /* SPL measurement */
    spl_calculator_t spl_calc;
    float current_spl_dba;
    float spl_min_dba;
    float spl_max_dba;

    /* Sound dose tracking */
    float daily_dose_pct;
    float accumulated_dose;
    float twa_dba;
    float peak_dba;
    uint32_t exposure_sec;
    uint32_t start_time_sec;

    /* Activity detection */
    uint8_t activity;  /* 0=still, 1=walking, 2=running */
    float accel_magnitude;

    /* Alert state */
    alert_priority_t last_alert_priority;
    uint32_t last_alert_time_ms;

    /* Battery */
    uint16_t battery_mv;
    bool charging;

    /* BLE connection */
    bool ble_connected;

    /* Mesh */
    bool mesh_joined;
} tag_state_t;

static tag_state_t g_state;

/* ── Haptic Motor Control ─────────────────────────────────────────────── */

static const struct gpio_dt_spec haptic_en = GPIO_DT_SPEC_GET(DT_NODELABEL(haptic_en), gpios);
static const struct gpio_dt_spec haptic_pwm = GPIO_DT_SPEC_GET(DT_NODELABEL(haptic_pwm), gpios);

static void haptic_pulse(uint32_t duration_ms)
{
    gpio_pin_set_dt(&haptic_en, 1);
    gpio_pin_set_dt(&haptic_pwm, 1);
    k_msleep(duration_ms);
    gpio_pin_set_dt(&haptic_pwm, 0);
    gpio_pin_set_dt(&haptic_en, 0);
}

static void haptic_pattern(const uint32_t *pattern, int count)
{
    for (int i = 0; i < count; i++) {
        if (i % 2 == 0) {
            haptic_pulse(pattern[i]);
        } else {
            k_msleep(pattern[i]);
        }
    }
}

static void haptic_alert(alert_priority_t priority)
{
    switch (priority) {
    case ALERT_PRIORITY_INFO:
        haptic_pulse(HAPTIC_PATTERN_INFO);
        break;
    case ALERT_PRIORITY_LOW:
        haptic_pulse(HAPTIC_PATTERN_LOW);
        break;
    case ALERT_PRIORITY_MEDIUM: {
        uint32_t pattern[] = {HAPTIC_PATTERN_MEDIUM};
        haptic_pattern(pattern, 3);
        break;
    }
    case ALERT_PRIORITY_HIGH: {
        uint32_t pattern[] = {HAPTIC_PATTERN_HIGH};
        haptic_pattern(pattern, 5);
        break;
    }
    case ALERT_PRIORITY_CRITICAL: {
        uint32_t pattern[] = {HAPTIC_PATTERN_CRITICAL};
        haptic_pattern(pattern, 3);
        break;
    }
    }
}

/* ── LED Control ──────────────────────────────────────────────────────── */

static const struct gpio_dt_spec led_data = GPIO_DT_SPEC_GET(DT_NODELABEL(led0), gpios);

static void led_set_color(uint8_t color)
{
    /* APA106-2020 RGB LED: 24-bit color data sent via single GPIO */
    /* In production, use proper WS2812B/APA106 bitbang driver */
    gpio_pin_set_dt(&led_data, (color & 0x80) ? 1 : 0);
}

static void led_alert(alert_priority_t priority)
{
    switch (priority) {
    case ALERT_PRIORITY_INFO:
        led_set_color(LED_GREEN);
        break;
    case ALERT_PRIORITY_LOW:
        led_set_color(LED_GREEN);
        break;
    case ALERT_PRIORITY_MEDIUM:
        led_set_color(LED_ORANGE);
        break;
    case ALERT_PRIORITY_HIGH:
        led_set_color(LED_YELLOW);
        break;
    case ALERT_PRIORITY_CRITICAL:
        led_set_color(LED_RED);
        break;
    }
}

/* ── SPL Monitoring Task ───────────────────────────────────────────────── */

static void spl_monitor_task(void *p1, void *p2, void *p3)
{
    LOG_INF("SPL monitor task started");

    int16_t *audio_buf = k_malloc(SPL_WINDOW_SAMPLES * sizeof(int16_t));
    if (!audio_buf) {
        LOG_ERR("Failed to allocate audio buffer");
        return;
    }

    spl_init(&g_state.spl_calc);
    g_state.spl_min_dba = 200.0f;
    g_state.spl_max_dba = -120.0f;

    while (1) {
        /* In production, read from SPH0645 via I2S */
        /* Placeholder: simulate SPL reading */
        /* Real implementation captures 1 second of audio at 16kHz */

        /* For now, just compute from mic data when available */
        k_msleep(SPL_WINDOW_MS);

        /* Simulate SPL values (in production, read from I2S) */
        /* The actual SPL would come from spl_process() on real audio data */

        /* Update min/max */
        if (g_state.current_spl_dba < g_state.spl_min_dba) {
            g_state.spl_min_dba = g_state.current_spl_dba;
        }
        if (g_state.current_spl_dba > g_state.spl_max_dba) {
            g_state.spl_max_dba = g_state.current_spl_dba;
        }

        /* Update dose */
        g_state.exposure_sec += 1;
        float dose_increment = spl_calculate_dose(g_state.current_spl_dba,
                                                   1.0f / 3600.0f);
        g_state.accumulated_dose += dose_increment;
        g_state.daily_dose_pct = g_state.accumulated_dose;

        /* Update TWA (time-weighted average) */
        if (g_state.exposure_sec > 0) {
            g_state.twa_dba = 10.0f * log10f(
                g_state.accumulated_dose / (float)g_state.exposure_sec);
        }

        /* Update peak */
        if (g_state.current_spl_dba > g_state.peak_dba) {
            g_state.peak_dba = g_state.current_spl_dba;
        }

        /* Dose alerts */
        if (g_state.daily_dose_pct > 100.0f && g_state.daily_dose_pct < 200.0f) {
            /* Yellow alert: 100-200% dose */
            haptic_alert(ALERT_PRIORITY_MEDIUM);
            led_alert(ALERT_PRIORITY_MEDIUM);
        } else if (g_state.daily_dose_pct >= 200.0f) {
            /* Red alert: >200% dose */
            haptic_alert(ALERT_PRIORITY_HIGH);
            led_alert(ALERT_PRIORITY_HIGH);
        }
    }
}

/* ── BLE Communication Task ─────────────────────────────────────────────── */

static void ble_task(void *p1, void *p2, void *p3)
{
    LOG_INF("BLE task started");

    /* In production, use Zephyr BLE GATT server */
    /* Nordic UART Service for config, custom SoundNest service for data */

    while (1) {
        /* Process BLE connection management */
        /* Send SPL and dose updates via notifications */

        k_msleep(1000);
    }
}

/* ── Mesh Communication Task ─────────────────────────────────────────────── */

static void mesh_task(void *p1, void *p2, void *p3)
{
    LOG_INF("Mesh task started");

    /* In production, use SX1262 Sub-GHz radio via SPI */

    /* Join mesh network */
    while (!g_state.mesh_joined) {
        /* Send JOIN_REQ */
        LOG_INF("Sending join request...");
        k_msleep(5000);
    }

    /* Main loop: send periodic dose reports and heartbeats */
    int64_t last_dose_report = 0;
    int64_t last_heartbeat = 0;

    while (1) {
        int64_t now = k_uptime_get();

        /* Send dose report every 30 seconds */
        if (now - last_dose_report > DOSE_REPORT_INTERVAL_MS) {
            last_dose_report = now;

            dose_report_payload_t dose = {
                .daily_dose_pct = (uint8_t)fminf(g_state.daily_dose_pct, 255.0f),
                .current_spl_dba = (uint8_t)g_state.current_spl_dba,
                .twa_dba_x10 = (uint16_t)(g_state.twa_dba * 10),
                .peak_dba = (uint8_t)fminf(g_state.peak_dba, 255.0f),
                .exposure_min = (uint16_t)(g_state.exposure_sec / 60),
                .activity = g_state.activity,
                .battery_mv = g_state.battery_mv,
                .timestamp = (uint32_t)(now / 1000),
            };

            /* Send via BLE to hub (wearable uses BLE, not Sub-GHz) */
            LOG_INF("Dose: %.1f%%, TWA: %.1f dBA, Peak: %.1f dBA",
                    g_state.daily_dose_pct, g_state.twa_dba, g_state.peak_dba);
        }

        /* Send heartbeat every 60 seconds */
        if (now - last_heartbeat > HEARTBEAT_INTERVAL_MS) {
            last_heartbeat = now;

            heartbeat_payload_t hb = {
                .node_type = NODE_TYPE_WEARABLE_TAG,
                .battery_mv = g_state.battery_mv,
                .rssi = 0,
                .status = 0x03,  /* Mic + BLE OK */
                .uptime_sec = (uint32_t)(now / 1000),
                .events_today = 0,
                .packets_sent = g_state.seq_num,
                .packets_missed = 0,
            };

            LOG_INF("Heartbeat: bat=%dmV, uptime=%ds",
                    g_state.battery_mv, hb.uptime_sec);
        }

        /* Receive alert commands from hub */
        /* In production, parse incoming BLE notifications */
        /* For each alert, activate haptic and LED */

        k_msleep(100);
    }
}

/* ── Accelerometer Task ──────────────────────────────────────────────── */

static void accel_task(void *p1, void *p2, void *p3)
{
    LOG_INF("Accelerometer task started");

    const struct device *lis2dh12 = DEVICE_DT_GET(DT_NODELABEL(lis2dh12));

    while (1) {
        if (device_is_ready(lis2dh12)) {
            struct sensor_value accel[3];
            sensor_sample_fetch(lis2dh12);
            sensor_channel_get(lis2dh12, SENSOR_CHAN_ACCEL_XYZ, accel);

            float x = sensor_value_to_double(&accel[0]);
            float y = sensor_value_to_double(&accel[1]);
            float z = sensor_value_to_double(&accel[2]);

            g_state.accel_magnitude = sqrtf(x*x + y*y + z*z);

            /* Classify activity */
            if (g_state.accel_magnitude < 1.2f) {
                g_state.activity = 0;  /* Still */
            } else if (g_state.accel_magnitude < 2.0f) {
                g_state.activity = 1;  /* Walking */
            } else {
                g_state.activity = 2;  /* Running */
            }
        }

        k_msleep(500);  /* Read every 500ms */
    }
}

/* ── Battery Monitoring Task ──────────────────────────────────────────── */

static void battery_task(void *p1, void *p2, void *p3)
{
    LOG_INF("Battery monitoring task started");

    while (1) {
        /* Read battery voltage via nRF ADC */
        /* Placeholder: assume 3.7V (mid-charge) */
        g_state.battery_mv = 3700;

        /* Check USB charging status */
        const struct gpio_dt_spec usb_detect = GPIO_DT_SPEC_GET(DT_NODELABEL(usb_detect), gpios);
        g_state.charging = gpio_pin_get_dt(&usb_detect);

        /* Low battery warning */
        if (g_state.battery_mv < 3300 && !g_state.charging) {
            /* Battery below 3.3V: critical */
            haptic_pulse(500);  /* Long buzz */
            led_set_color(LED_RED);
            LOG_WRN("Low battery: %dmV", g_state.battery_mv);
        } else if (g_state.battery_mv < 3500 && !g_state.charging) {
            /* Battery below 3.5V: warning */
            haptic_pulse(200);
            led_set_color(LED_ORANGE);
        }

        k_msleep(60000);  /* Check every minute */
    }
}

/* ── Main Entry Point ─────────────────────────────────────────────────── */

int main(void)
{
    LOG_INF("╔══════════════════════════════════════╗");
    LOG_INF("║   SoundNest Wearable Tag v1.0        ║");
    LOG_INF("║   Personal Sound Guardian             ║");
    LOG_INF("╚══════════════════════════════════════╝");

    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.spl_min_dba = 200.0f;
    g_state.spl_max_dba = -120.0f;
    g_state.peak_dba = -120.0f;
    g_state.hub_addr = 0x0001;
    g_state.current_spl_dba = 40.0f;  /* Default quiet environment */

    /* Initialize GPIOs */
    gpio_pin_configure_dt(&haptic_en, GPIO_OUTPUT);
    gpio_pin_configure_dt(&haptic_pwm, GPIO_OUTPUT);
    gpio_pin_configure_dt(&led_data, GPIO_OUTPUT);

    /* Start tasks */
    K_THREAD_DEFINE(spl_tid, 4096, spl_monitor_task, NULL, NULL, NULL, 5, 0, 0);
    K_THREAD_DEFINE(ble_tid, 4096, ble_task, NULL, NULL, NULL, 3, 0, 0);
    K_THREAD_DEFINE(mesh_tid, 2048, mesh_task, NULL, NULL, NULL, 4, 0, 0);
    K_THREAD_DEFINE(accel_tid, 1024, accel_task, NULL, NULL, NULL, 2, 0, 0);
    K_THREAD_DEFINE(bat_tid, 1024, battery_task, NULL, NULL, NULL, 1, 0, 0);

    LOG_INF("All tasks started. Wearable tag is running.");

    /* Initial haptic pulse to confirm power-on */
    haptic_pulse(100);
    k_msleep(200);
    haptic_pulse(100);
    led_set_color(LED_BLUE);

    return 0;
}