/*
 * ErgoFlow — Chair Pad Node Main
 * nRF52832 based pressure mapping + IMU sensor node
 *
 * Responsibilities:
 *   - Scan 16× FSR pressure grid via mux + ADC
 *   - Read IMU for tilt/motion detection
 *   - Local threshold-based slouch detection
 *   - Transmit pressure map + IMU data via BLE mesh
 *   - Haptic alerts for prolonged static posture
 *   - Ultra-low-power operation between scans
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include "common/ble_mesh/mesh_config.h"
#include "common/ble_mesh/mesh_handler.h"
#include "common/ble_mesh/protocol.h"
#include "common/sensors/i2c_bus.h"
#include "common/sensors/ads1115.h"
#include "common/sensors/lsm6dsox.h"

LOG_MODULE_REGISTER(chair_main, CONFIG_ERGO_LOG_LEVEL);

/* ── Configuration ──────────────────────────────────────────────────── */
#define PRESSURE_SCAN_MS     200    /* 5 Hz scanning */
#define IMU_SAMPLE_MS        100    /* 10 Hz IMU */
#define MUX_SETTLE_US        50     /* Mux settling time */
#define HAPTIC_ALERT_MIN     1800   /* 30 min = 1800 seconds */

/* ── Mux GPIO pins (CD74HC4067) ────────────────────────────────────── */
#define MUX1_EN_PIN   4   /* P0.04 — backrest mux enable */
#define MUX2_EN_PIN   5   /* P0.05 — seat mux enable */
#define MUX_S0_PIN    6   /* P0.06 */
#define MUX_S1_PIN    7   /* P0.07 */
#define MUX_S2_PIN    8   /* P0.08 */
#define MUX_S3_PIN    9   /* P0.09 */

/* ── Haptic motor ───────────────────────────────────────────────────── */
#define HAPTIC_EN_PIN  13  /* P0.13 — DRV2605L enable */
#define HAPTIC_PWM_PIN 11  /* P0.11 — ERM motor drive */

/* ── State ──────────────────────────────────────────────────────────── */
static ergo_pressure_map_t pressure_map;
static lsm6dsox_accel_t accel_data;
static lsm6dsox_gyro_t gyro_data;
static uint32_t static_sit_seconds = 0;
static uint8_t battery_pct = 100;
static uint8_t node_state = ERGO_STATE_INIT;

/* ── Pressure scanning ──────────────────────────────────────────────── */
static void set_mux_channel(uint8_t channel)
{
    /* Set 4-bit address on MUX_S0-S3 */
    gpio_pin_set(gpio_dev, MUX_S0_PIN, (channel >> 0) & 1);
    gpio_pin_set(gpio_dev, MUX_S1_PIN, (channel >> 1) & 1);
    gpio_pin_set(gpio_dev, MUX_S2_PIN, (channel >> 2) & 1);
    gpio_pin_set(gpio_dev, MUX_S3_PIN, (channel >> 3) & 1);
    k_usleep(MUX_SETTLE_US);
}

static void scan_pressure_grid(void)
{
    /* Enable backrest mux, scan channels 0-7 */
    gpio_pin_set(gpio_dev, MUX1_EN_PIN, 1);  /* Enable backrest mux */
    gpio_pin_set(gpio_dev, MUX2_EN_PIN, 0);  /* Disable seat mux */

    int16_t raw;
    for (int i = 0; i < 8; i++) {
        set_mux_channel(i);
        if (ads1115_read_channel(0, &raw) == 0) {
            /* Map ADC value (0-32767) to 0-255 pressure */
            int pressure = (int)raw * 255 / 32767;
            if (pressure < 0) pressure = 0;
            if (pressure > 255) pressure = 255;
            pressure_map.pressure[i] = (uint8_t)pressure;
        }
    }

    /* Enable seat mux, scan channels 0-7 */
    gpio_pin_set(gpio_dev, MUX1_EN_PIN, 0);  /* Disable backrest mux */
    gpio_pin_set(gpio_dev, MUX2_EN_PIN, 1);  /* Enable seat mux */

    for (int i = 0; i < 8; i++) {
        set_mux_channel(i);
        if (ads1115_read_channel(0, &raw) == 0) {
            int pressure = (int)raw * 255 / 32767;
            if (pressure < 0) pressure = 0;
            if (pressure > 255) pressure = 255;
            pressure_map.pressure[8 + i] = (uint8_t)pressure;
        }
    }

    /* Disable both muxes to save power */
    gpio_pin_set(gpio_dev, MUX1_EN_PIN, 0);
    gpio_pin_set(gpio_dev, MUX2_EN_PIN, 0);
}

/* ── IMU reading ────────────────────────────────────────────────────── */
static void read_imu(void)
{
    if (lsm6dsox_accel_data_ready()) {
        lsm6dsox_read_accel(&accel_data);
    }
    if (lsm6dsox_gyro_data_ready()) {
        lsm6dsox_read_gyro(&gyro_data);
    }

    /* Update IMU flags */
    float accel_mag = sqrtf(accel_data.x * accel_data.x +
                             accel_data.y * accel_data.y +
                             accel_data.z * accel_data.z);
    pressure_map.imu_flags = 0;

    /* Detect sitting: Z-axis should be ~1g when sitting upright */
    if (fabsf(accel_data.z) > 0.7f && fabsf(accel_data.z) < 1.3f) {
        pressure_map.imu_flags |= ERGO_IMU_FLAG_SITTING;
    }

    /* Detect motion: if gyroscope magnitude > threshold */
    float gyro_mag = sqrtf(gyro_data.x * gyro_data.x +
                            gyro_data.y * gyro_data.y +
                            gyro_data.z * gyro_data.z);
    if (gyro_mag > 5.0f) {
        pressure_map.imu_flags |= ERGO_IMU_FLAG_MOVING;
    }

    /* Detect tilt: if X or Y acceleration deviates from vertical */
    if (fabsf(accel_data.x) > 0.2f || fabsf(accel_data.y) > 0.2f) {
        pressure_map.imu_flags |= ERGO_IMU_FLAG_TILT;
    }
}

/* ── Haptic alert ───────────────────────────────────────────────────── */
static void haptic_alert(uint8_t pattern)
{
    switch (pattern) {
        case 0: /* Gentle pulse */
            gpio_pin_set(gpio_dev, HAPTIC_EN_PIN, 1);
            k_msleep(200);
            gpio_pin_set(gpio_dev, HAPTIC_EN_PIN, 0);
            break;
        case 1: /* Double pulse */
            for (int i = 0; i < 2; i++) {
                gpio_pin_set(gpio_dev, HAPTIC_EN_PIN, 1);
                k_msleep(150);
                gpio_pin_set(gpio_dev, HAPTIC_EN_PIN, 0);
                k_msleep(100);
            }
            break;
        case 2: /* Urgent triple pulse */
            for (int i = 0; i < 3; i++) {
                gpio_pin_set(gpio_dev, HAPTIC_EN_PIN, 1);
                k_msleep(300);
                gpio_pin_set(gpio_dev, HAPTIC_EN_PIN, 0);
                k_msleep(150);
            }
            break;
    }
}

/* ── BLE mesh transmit ──────────────────────────────────────────────── */
static void transmit_pressure_map(void)
{
    uint8_t buf[20];
    uint16_t len;
    ergo_pack_pressure_map(&pressure_map, buf, &len);
    mesh_handler_send(ERGO_OP_PRESSURE_MAP, buf, len, ERGO_ADDR_HUB);
}

static void transmit_heartbeat(void)
{
    ergo_node_heartbeat_t hb = {
        .battery_pct = battery_pct,
        .state = node_state,
        .uptime_min = (uint16_t)(k_uptime_get() / 60000),
    };
    uint8_t buf[8];
    uint16_t len;
    ergo_pack_node_heartbeat(&hb, buf, &len);
    mesh_handler_send(ERGO_OP_NODE_HEARTBEAT, buf, len, ERGO_ADDR_HUB);
}

/* ── Mesh message callback ──────────────────────────────────────────── */
static void mesh_callback(uint16_t opcode, const uint8_t *data,
                           uint16_t len, uint16_t src_addr, void *user_data)
{
    if (opcode == ERGO_OP_BREAK_REMINDER) {
        ergo_break_reminder_t reminder;
        if (ergo_unpack_break_reminder(data, len, &reminder) == 0) {
            /* Vibrate to alert user of break reminder */
            haptic_alert(reminder.type == ERGO_BREAK_WALK ? 2 : 0);
            LOG_INF("Break reminder: type=%d, duration=%ds",
                    reminder.type, reminder.duration_s);
        }
    } else if (opcode == ERGO_OP_CALIBRATION) {
        /* Re-calibrate pressure baseline */
        LOG_INF("Calibration command received");
        memset(pressure_map.pressure, 0, sizeof(pressure_map.pressure));
    } else if (opcode == ERGO_OP_FACTORY_RESET) {
        LOG_INF("Factory reset requested");
        /* In production: clear all stored settings */
    }
}

/* ── Main ───────────────────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("ErgoFlow Chair Pad Node starting...");

    /* Initialize I2C */
    i2c_bus_init();

    /* Initialize sensors */
    ads1115_init(ADS1115_ADDR_GND);
    lsm6dsox_init(LSM6DSOX_I2C_ADDR);

    /* Initialize mesh handler */
    mesh_handler_init();
    mesh_handler_register_callback(0xFFFF, mesh_callback, NULL);

    /* Configure GPIO for mux control */
    const struct device *gpio_dev = device_get_binding("GPIO_0");
    gpio_pin_configure(gpio_dev, MUX1_EN_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, MUX2_EN_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, MUX_S0_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, MUX_S1_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, MUX_S2_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, MUX_S3_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_dev, HAPTIC_EN_PIN, GPIO_OUTPUT_LOW);

    node_state = ERGO_STATE_RUNNING;
    LOG_INF("Chair Pad Node running");

    /* Main loop */
    uint32_t heartbeat_counter = 0;

    while (1) {
        /* Scan pressure grid */
        scan_pressure_grid();

        /* Read IMU */
        read_imu();

        /* Local slouch detection (fast threshold, before cloud inference) */
        uint16_t back_total = 0, seat_total = 0;
        for (int i = 0; i < 8; i++) back_total += pressure_map.pressure[i];
        for (int i = 8; i < 16; i++) seat_total += pressure_map.pressure[i];

        /* Track static sitting duration */
        if (seat_total > 200) {
            static_sit_seconds += (PRESSURE_SCAN_MS / 1000);

            /* Haptic alert if sitting too long without moving */
            if (static_sit_seconds > HAPTIC_ALERT_MIN && (static_sit_seconds % 300 == 0)) {
                haptic_alert(1);  /* Double pulse every 5 min after threshold */
            }
        } else {
            static_sit_seconds = 0;
        }

        /* Transmit data to hub */
        transmit_pressure_map();

        /* Periodic heartbeat */
        heartbeat_counter++;
        if (heartbeat_counter >= (60000 / PRESSURE_SCAN_MS)) {
            transmit_heartbeat();
            heartbeat_counter = 0;
        }

        /* Sleep until next scan */
        k_msleep(PRESSURE_SCAN_MS);
    }

    return 0;
}