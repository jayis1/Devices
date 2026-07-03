/**
 * DriveSync Steering Wheel Node — Main Firmware
 *
 * nRF52840 + LSM6DSO IMU + FDC2214 capacitive grip sensor
 * BLE 5.0 peripheral. Straps onto steering wheel.
 * Samples IMU at 1 kHz (FIFO), grip at 10 Hz, sends aggregated data at 10 Hz.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "boards.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_twi.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "protocol.h"
#include "steering_imu.h"
#include "grip_sensor.h"
#include "ble_periph.h"

/* ── Timers ──────────────────────────────────────────────────────── */

APP_TIMER_DEF(m_sample_timer);      /* 100 ms (10 Hz send) */
APP_TIMER_DEF(m_grip_timer);        /* 100 ms (10 Hz) */
APP_TIMER_DEF(m_heartbeat_timer);   /* 30 sec */
#define SAMPLE_INTERVAL_MS   100
#define GRIP_INTERVAL_MS     100
#define HEARTBEAT_INTERVAL   30000

/* ── State ───────────────────────────────────────────────────────── */

typedef enum {
    WHEEL_MODE_ACTIVE = 0,
    WHEEL_MODE_SLEEP  = 1,
    WHEEL_MODE_PARK   = 2,
} wheel_mode_t;

typedef struct {
    wheel_mode_t  mode;
    uint16_t      node_id;
    uint16_t      seq_counter;
    bool          connected;
    uint8_t       battery_pct;

    /* IMU data (accumulated between sends) */
    int16_t       last_gyro_z;       /* milli-degrees/sec */
    int16_t       last_ax, last_ay, last_az;
    uint16_t      jerk_count;        /* reversal count in 100ms window */

    /* Grip data */
    uint16_t      grip_raw[4];
    uint8_t       hands_on;
    uint8_t       grip_force;

    /* Alert state */
    bool          haptic_active;
} wheel_state_t;

static wheel_state_t g_state = {0};

/* ── IMU FIFO callback ───────────────────────────────────────────── */

/**
 * Called from steering_imu when FIFO is read (1 kHz internally).
 * We count direction reversals (jerk events) for drowsiness detection.
 */
static void imu_data_handler(int16_t gyro_z, int16_t ax, int16_t ay, int16_t az)
{
    static int16_t prev_gyro_z = 0;
    static bool prev_sign = false;

    g_state.last_gyro_z = gyro_z;
    g_state.last_ax = ax;
    g_state.last_ay = ay;
    g_state.last_az = az;

    /* Detect steering reversal (sign change of gyro_z above threshold) */
    if (abs(gyro_z) > 200) {  /* > 200 milli-deg/s = meaningful steering */
        bool curr_sign = (gyro_z > 0);
        if (curr_sign != prev_sign && prev_sign != false) {
            g_state.jerk_count++;
        }
        prev_sign = curr_sign;
    }
}

/* ── Sample Timer (10 Hz send) ────────────────────────────────────── */

static void sample_timer_handler(void *p_context)
{
    if (g_state.mode == WHEEL_MODE_SLEEP) return;

    /* Build steering payload */
    payload_steering_t payload = {0};
    payload.gyro_z = g_state.last_gyro_z;
    payload.accel_x = g_state.last_ax;
    payload.accel_y = g_state.last_ay;
    payload.accel_z = g_state.last_az;
    payload.jerk_count = g_state.jerk_count;
    payload.grip_raw[0] = g_state.grip_raw[0];
    payload.grip_raw[1] = g_state.grip_raw[1];
    payload.grip_raw[2] = g_state.grip_raw[2];
    payload.grip_raw[3] = g_state.grip_raw[3];
    payload.hands_on = g_state.hands_on;
    payload.grip_force = g_state.grip_force;
    payload.timestamp = (app_timer_cnt_get() / 32);

    /* Reset jerk count for next window */
    g_state.jerk_count = 0;

    /* Encode and send via BLE */
    uint8_t packet[DS_MAX_PACKET_LEN];
    uint8_t len = drivesync_encode(packet, sizeof(packet),
                                    MSG_TYPE_DATA_STEERING,
                                    g_state.node_id,
                                    g_state.seq_counter++,
                                    0,
                                    (uint8_t *)&payload, sizeof(payload));
    if (g_state.connected) {
        ble_periph_send(packet, len);
    }
}

/* ── Grip Timer (10 Hz) ───────────────────────────────────────────── */

static void grip_timer_handler(void *p_context)
{
    if (g_state.mode == WHEEL_MODE_SLEEP) return;

    /* Read FDC2214 capacitive sensor (4 channels) */
    grip_sensor_read(g_state.grip_raw);

    /* Derive hands-on/off and grip force */
    uint32_t grip_sum = g_state.grip_raw[0] + g_state.grip_raw[1] +
                        g_state.grip_raw[2] + g_state.grip_raw[3];
    uint32_t grip_baseline = 0;  /* Calibrated at startup */

    /* If capacitance > baseline + threshold → hands on */
    if (grip_sum > grip_baseline + 100) {
        g_state.hands_on = 1;
        /* Grip force proxy: 0-100 based on capacitance magnitude */
        uint32_t force = (grip_sum - grip_baseline) / 10;
        g_state.grip_force = (force > 100) ? 100 : (uint8_t)force;
    } else {
        g_state.hands_on = 0;
        g_state.grip_force = 0;
    }
}

/* ── Heartbeat Timer ──────────────────────────────────────────────── */

static void heartbeat_timer_handler(void *p_context)
{
    payload_status_t status;
    status.battery_pct = g_state.battery_pct;
    status.state = g_state.connected ? 1 : 0;
    status.error_code = 0;

    uint8_t packet[DS_MAX_PACKET_LEN];
    uint8_t len = drivesync_encode(packet, sizeof(packet),
                                    MSG_TYPE_STATUS,
                                    g_state.node_id,
                                    g_state.seq_counter++,
                                    0,
                                    (uint8_t *)&status, sizeof(status));
    ble_periph_send(packet, len);
}

/* ── BLE Command Handler ──────────────────────────────────────────── */

static void ble_cmd_handler(const uint8_t *data, uint8_t len)
{
    drivesync_header_t header;
    const uint8_t *payload;

    if (!drivesync_decode(data, len, &header, &payload)) {
        return;
    }

    switch (header.msg_type) {
    case MSG_TYPE_CMD_MODE:
        if (header.payload_len >= 1) {
            g_state.mode = (wheel_mode_t)payload[0];
            NRF_LOG_INFO("Mode changed to %d", g_state.mode);
        }
        break;

    case MSG_TYPE_ALERT_DROWSY: {
        /* Trigger haptic vibration on steering wheel */
        if (header.payload_len >= sizeof(payload_alert_t)) {
            payload_alert_t *alert = (payload_alert_t *)payload;
            NRF_LOG_INFO("Drowsiness alert: level %d, duration %ds",
                         alert->alert_level, alert->duration_sec);
            /* Drive ERM haptic motor */
            nrf_drv_gpiote_out_task_enable(NRF_GPIO_PIN_MAP(0, 6));
            nrf_delay_ms(alert->duration_sec * 1000);
            nrf_drv_gpiote_out_task_disable(NRF_GPIO_PIN_MAP(0, 6));
        }
        break;
    }

    case MSG_TYPE_HEARTBEAT:
        break;

    case MSG_TYPE_ACK:
        break;

    default:
        break;
    }
}

/* ── BLE Connection Callbacks ─────────────────────────────────────── */

static void ble_connected_handler(void)
{
    g_state.connected = true;
    NRF_LOG_INFO("BLE connected to Hub");
}

static void ble_disconnected_handler(void)
{
    g_state.connected = false;
    NRF_LOG_INFO("BLE disconnected, advertising...");
    ble_periph_advertise();
}

/* ── Battery Measurement ──────────────────────────────────────────── */

static uint8_t read_battery_pct(void)
{
    /* CR2477: 3.0V full, 2.0V empty */
    /* Read ADC on P0.19 (battery voltage divider) */
    uint16_t adc_val = 0;  /* TODO: implement ADC read */
    float voltage = (adc_val / 1023.0f) * 3.0f * 2.0f;
    float pct = ((voltage - 2.0f) / (3.0f - 2.0f)) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    return (uint8_t)pct;
}

/* ── Main ─────────────────────────────────────────────────────────── */

int main(void)
{
    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("DriveSync Wheel Node starting...");

    g_state.mode = WHEEL_MODE_ACTIVE;
    g_state.node_id = DS_WHEEL_ID_BASE;  /* 0x0100 */
    g_state.seq_counter = 0;
    g_state.connected = false;
    g_state.jerk_count = 0;

    app_timer_init();
    nrf_drv_gpiote_init();

    /* Initialize steering IMU (LSM6DSO) at 1 kHz with FIFO */
    steering_imu_init(imu_data_handler);

    /* Initialize grip sensor (FDC2214) */
    grip_sensor_init();

    /* Initialize BLE peripheral */
    ble_periph_init(ble_cmd_handler,
                    ble_connected_handler,
                    ble_disconnected_handler);
    ble_periph_advertise();

    /* Create timers */
    app_timer_create(&m_sample_timer, APP_TIMER_MODE_REPEATED, sample_timer_handler);
    app_timer_create(&m_grip_timer, APP_TIMER_MODE_REPEATED, grip_timer_handler);
    app_timer_create(&m_heartbeat_timer, APP_TIMER_MODE_REPEATED, heartbeat_timer_handler);

    app_timer_start(m_sample_timer, APP_TIMER_TICKS(SAMPLE_INTERVAL_MS), NULL);
    app_timer_start(m_grip_timer, APP_TIMER_TICKS(GRIP_INTERVAL_MS), NULL);
    app_timer_start(m_heartbeat_timer, APP_TIMER_TICKS(HEARTBEAT_INTERVAL), NULL);

    NRF_LOG_INFO("Wheel Node 0x%04X ready (IMU 1kHz, grip 10Hz)", g_state.node_id);

    while (true) {
        nrf_pwr_mgmt_run();
        NRF_LOG_FLUSH();
    }
}