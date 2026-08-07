/*
 * GuideSync — Smart Cane Firmware
 * nRF52840, Zephyr RTOS
 *
 * The Smart Cane is the ground-level sensing node. It uses an HC-SR04
 * ultrasonic sensor for obstacle detection (2 cm–4 m), a VL53L0X ToF
 * angled 45° downward for drop-off/stair/curb detection, an ICM-42688
 * IMU for swing tracking and tap detection, and a DRV2605L haptic
 * driver for direction-specific vibration feedback in the cane handle.
 *
 * Tap detection: user taps the cane on the ground 2× to trigger
 * "describe location" (sends beacon scan request to glasses/hub).
 *
 * Build: west build -b nrf52840dk_nrf52840
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <string.h>

#include "../common/protocol.h"
#include "../common/ble_mesh.h"
#include "../common/config.h"

LOG_MODULE_REGISTER(guidesync_cane, LOG_LEVEL_INF);

/* === Global state === */
static gs_ble_ctx_t g_ble;
static uint16_t g_swing_count = 0;
static uint16_t g_step_count = 0;
static uint8_t g_battery_v = 380; /* 3.80V */

/* === BLE Interface (Zephyr BLE) === */
static void ble_init(void) { /* bt_enable() in production */ }
static void ble_advert_start(void) { /* bt_le_adv_start() */ }
static void ble_advert_stop(void) { /* bt_le_adv_stop() */ }
static void ble_scan_start(void) { }
static void ble_scan_stop(void) { }
static int ble_connect(uint8_t *peer) { (void)peer; return 0; }
static int ble_send(const uint8_t *data, uint8_t len)
{
    LOG_INF("BLE TX %d bytes", len);
    return len;
}
static int ble_recv(uint8_t *buf, uint8_t max, uint32_t timeout_ms)
{
    k_msleep(MIN(timeout_ms, 100));
    return 0;
}
static void ble_delay_ms(uint32_t ms) { k_msleep(ms); }
static void ble_disconnect(void) { }

static const gs_ble_interface_t g_ble_iface = {
    .init = ble_init,
    .advert_start = ble_advert_start,
    .advert_stop = ble_advert_stop,
    .scan_start = ble_scan_start,
    .scan_stop = ble_scan_stop,
    .connect = ble_connect,
    .send = ble_send,
    .recv = ble_recv,
    .delay_ms = ble_delay_ms,
    .disconnect = ble_disconnect,
};

/* === HC-SR04 Ultrasonic === */
static const struct gpio_dt_spec us_trig =
    GPIO_DT_SPEC_GET(DT_ALIAS(us_trig), gpios);
static const struct gpio_dt_spec us_echo =
    GPIO_DT_SPEC_GET(DT_ALIAS(us_echo), gpios);

static void ultrasonic_init(void)
{
    gpio_pin_configure_dt(&us_trig, GPIO_OUTPUT_LOW);
    gpio_pin_configure_dt(&us_echo, GPIO_INPUT);
}

/* Read ultrasonic distance in decimeters (dm). 0 = invalid. */
static uint8_t ultrasonic_read_dm(void)
{
    /* Trigger: 10 us pulse */
    gpio_pin_set_dt(&us_trig, 1);
    k_busy_wait(10);
    gpio_pin_set_dt(&us_trig, 0);

    /* Wait for echo (timeout 30 ms = ~5 m) */
    uint32_t timeout_us = 30000;
    uint32_t start = k_cycle_get_32();

    /* Wait for echo high */
    while (gpio_pin_get_dt(&us_echo) == 0) {
        if (k_cyc_to_us_floor32(k_cycle_get_32() - start) > timeout_us)
            return 0; /* Timeout */
    }

    uint32_t echo_start = k_cycle_get_32();

    /* Wait for echo low */
    while (gpio_pin_get_dt(&us_echo) == 1) {
        if (k_cyc_to_us_floor32(k_cycle_get_32() - echo_start) > timeout_us)
            return 0;
    }

    uint32_t echo_us = k_cyc_to_us_floor32(k_cycle_get_32() - echo_start);

    /* Distance = (echo_us * 0.0343) / 2 cm = echo_us * 0.01715 cm
     * In dm: echo_us * 0.001715
     * Clamp to 40 dm (4 m max)
     */
    uint32_t dist_cm = echo_us / 58; /* HC-SR04 formula: cm = us / 58 */
    if (dist_cm > 400) return 0;     /* Out of range */
    if (dist_cm < 2) return 0;       /* Too close (noise) */

    return (uint8_t)(dist_cm / 10); /* Convert to decimeters */
}

/* === VL53L0X Downward ToF === */
static const struct i2c_dt_spec tof_i2c =
    I2C_DT_SPEC_GET(DT_ALIAS(tof_down));

static void tof_down_init(void)
{
    /* Production: VL53L0X init via I²C — set 45° downward angle,
     * 30 cm–2 m range, continuous mode */
    LOG_INF("VL53L0X downward ToF initialized");
}

/* Read downward ToF in decimeters. 255 = no reading (ground visible = safe).
 * Short reading (<5 dm) = ground close = drop-off/stair edge detected. */
static uint8_t tof_down_read_dm(void)
{
    /* Production: read VL53L0X range via I²C */
    /* Stub: ground visible (normal walking) */
    return 3; /* 30 cm to ground (normal cane height) */
}

/* Detect drop-off: ground suddenly disappears (ToF reading > 10 dm or invalid) */
static uint8_t detect_dropoff(uint8_t tof_dm)
{
    /* Normal ground: 2–5 dm. Drop-off: >10 dm or invalid */
    if (tof_dm > 10 || tof_dm == 0) return 1;
    return 0;
}

/* Detect stairs: abrupt change in ToF reading (up = stair up, down = stair down) */
static uint8_t detect_stairs(uint8_t tof_dm, uint8_t prev_dm)
{
    if (prev_dm == 0 || tof_dm == 0) return 0;
    int16_t diff = (int16_t)tof_dm - (int16_t)prev_dm;
    if (diff > 3 || diff < -3) return 1; /* >30 cm change = stair edge */
    return 0;
}

/* === ICM-42688 IMU === */
static const struct i2c_dt_spec imu_i2c =
    I2C_DT_SPEC_GET(DT_ALIAS(imu_cane));

static void imu_init(void)
{
    /* Production: ICM-42688 init — accel ±4g, gyro ±2000 dps, ODR 100 Hz */
}

static void imu_read(int8_t *tilt_deg, int8_t *temp)
{
    /* Production: read accel + gyro, compute tilt angle */
    *tilt_deg = 15; /* Stub: 15° from vertical */
    *temp = 25;
}

/* Cane swing detection: count swings for gait analysis */
static void imu_swing_detect(void)
{
    /* Production: gyro peak detection — cane sweeps left/right */
    g_swing_count++;
}

/* Cane tap detection: 2 taps = "describe location" command */
static uint8_t tap_count = 0;
static uint32_t last_tap_time = 0;

static void imu_tap_detect(void)
{
    uint32_t now = k_uptime_get_32();
    if (now - last_tap_time < 1000) {
        tap_count++;
        if (tap_count >= 2) {
            LOG_INF("Double tap detected — describe location");
            /* Send beacon scan request to hub */
            gs_message_t cmd;
            gs_build_command(&cmd, g_ble.node_id, GS_HUB_NODE_ID,
                           g_ble.msg_seq++, GS_CMD_WHERE_AM_I, NULL, 0);
            if (g_ble.joined) gs_ble_send(&g_ble, &cmd);
            tap_count = 0;
        }
    } else {
        tap_count = 1;
    }
    last_tap_time = now;
}

/* === DRV2605L Haptic Driver === */
static const struct i2c_dt_spec haptic_i2c =
    I2C_DT_SPEC_GET(DT_ALIAS(haptic_cane));

static void haptic_init(void)
{
    /* Production: DRV2605L init — set mode INTERNAL_TRIG, library LRA */
    LOG_INF("DRV2605L haptic driver initialized");
}

static void haptic_play(uint8_t waveform_id)
{
    /* Production: write waveform sequence to DRV2605L registers,
     * then trigger GO bit */
    LOG_INF("Haptic: waveform %d", waveform_id);
}

/* Direction-specific haptic for obstacle alert */
static void haptic_obstacle_alert(uint8_t us_dist_dm, uint8_t direction)
{
    if (us_dist_dm <= 5) {
        /* Very close: long vibration */
        haptic_play(14); /* Long hum */
    } else if (us_dist_dm <= 10) {
        /* Close: double strong click */
        haptic_play(2);
        k_msleep(100);
        haptic_play(2);
    } else {
        /* Approaching: single sharp click */
        haptic_play(1);
    }
}

static void haptic_dropoff_alert(void)
{
    /* Drop-off: urgent triple pulse */
    for (int i = 0; i < 3; i++) {
        haptic_play(14);
        k_msleep(150);
    }
}

static void haptic_stair_alert(void)
{
    /* Stairs: ascending pulses */
    haptic_play(8);
    k_msleep(100);
    haptic_play(12);
}

/* === Battery Monitor === */
static uint8_t read_battery_v(void)
{
    /* Production: ADC read on CANE_GPIO_VBAT, convert to 0.01V units */
    return 380; /* Stub: 3.80V */
}

/* === Sensing Task === */
static void sense_task(void *arg1, void *arg2, void *arg3)
{
    LOG_INF("Cane sensing task started");

    uint8_t prev_tof_dm = 0;
    uint8_t haptic_last = 0;

    while (1) {
        /* Read ultrasonic */
        uint8_t us_dist = ultrasonic_read_dm();

        /* Read downward ToF */
        uint8_t tof_dm = tof_down_read_dm();

        /* Detect hazards */
        uint8_t dropoff = detect_dropoff(tof_dm);
        uint8_t stairs = detect_stairs(tof_dm, prev_tof_dm);
        prev_tof_dm = tof_dm;

        /* IMU */
        int8_t tilt, imu_temp;
        imu_read(&tilt, &imu_temp);
        imu_swing_detect();
        imu_tap_detect();

        /* Haptic feedback */
        if (dropoff) {
            haptic_dropoff_alert();
            haptic_last = GS_ALERT_DROP_OFF;
        } else if (stairs) {
            haptic_stair_alert();
            haptic_last = GS_ALERT_STAIRS;
        } else if (us_dist > 0 && us_dist <= SCENE_OBSTACLE_DIST_DM) {
            haptic_obstacle_alert(us_dist, 0);
            haptic_last = (us_dist <= TOF_CRITICAL_DIST_DM) ?
                GS_ALERT_OBSTACLE_CRIT : GS_ALERT_OBSTACLE_WARN;
        }

        /* Send telemetry to hub every 5 seconds */
        static uint32_t telem_counter = 0;
        if (++telem_counter >= 50) { /* 50 × 100ms = 5s */
            g_battery_v = read_battery_v();
            gs_message_t telem;
            gs_build_cane_telem(&telem, g_ble.node_id, g_ble.msg_seq++,
                g_battery_v, us_dist, us_dist > 0,
                tof_dm, dropoff, stairs,
                g_swing_count, imu_temp, haptic_last, 0,
                tilt, g_step_count, -60);
            if (g_ble.joined) gs_ble_send(&g_ble, &telem);
            telem_counter = 0;
        }

        /* Send critical alerts */
        if (dropoff) {
            gs_message_t alert;
            uint8_t data[1] = {tof_dm};
            gs_build_alert(&alert, g_ble.node_id, g_ble.msg_seq++,
                          GS_ALERT_DROP_OFF, GS_SEV_CRITICAL, data, 1);
            if (g_ble.joined) gs_ble_send(&g_ble, &alert);
        }

        k_msleep(CANE_SAMPLE_INTERVAL_MS);
    }
}

/* === BLE Task === */
static void ble_task(void *arg1, void *arg2, void *arg3)
{
    gs_ble_init(&g_ble, GS_NODE_CANE, &g_ble_iface);
    LOG_INF("Cane BLE task started, joining network...");

    /* Production: gs_ble_join(&g_ble); */
    g_ble.node_id = 2; /* Stub */
    g_ble.joined = 1;

    gs_message_t msg;
    while (1) {
        if (gs_ble_recv(&g_ble, &msg, 5000) == 0) {
            if (msg.header.type == GS_MSG_COMMAND) {
                uint8_t cmd = msg.payload[0];
                LOG_INF("Command received: %d", cmd);
                switch (cmd) {
                    case GS_CMD_NAV_STOP:
                        haptic_play(14); /* Stop vibration */
                        break;
                    case GS_CMD_CALIBRATE:
                        LOG_INF("Calibration requested");
                        break;
                    case GS_CMD_REBOOT:
                        LOG_INF("Reboot requested");
                        /* sys_reboot() */
                        break;
                }
            }
        }
    }
}

/* === Main === */
void main(void)
{
    LOG_INF("GuideSync Smart Cane starting...");

    /* Init sensors */
    ultrasonic_init();
    tof_down_init();
    imu_init();
    haptic_init();

    /* Start BLE beacon scanning */
    bt_scan_start();

    /* Threads */
    k_thread_create(&sense_thread, sense_stack, K_THREAD_STACK_SIZEOF(sense_stack),
                    sense_task, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
    k_thread_create(&ble_thread, ble_stack, K_THREAD_STACK_SIZEOF(ble_stack),
                    ble_task, NULL, NULL, NULL, 4, 0, K_NO_WAIT);

    LOG_INF("GuideSync Smart Cane ready.");
}