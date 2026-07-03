/**
 * DriveSync Seat Belt Tag — Main Firmware
 *
 * nRF52840 + MAX30101 PPG + LSM6DSO IMU
 * BLE 5.0 peripheral. Clips onto seatbelt near collarbone.
 * Samples PPG at 25 Hz, IMU at 50 Hz, computes HRV, sends at 1 Hz.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
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
#include "ppg_driver.h"
#include "body_imu.h"
#include "ble_periph.h"

/* ── Timers ──────────────────────────────────────────────────────── */

APP_TIMER_DEF(m_ppg_timer);        /* 40 ms (25 Hz) */
APP_TIMER_DEF(m_imu_timer);        /* 20 ms (50 Hz) */
APP_TIMER_DEF(m_send_timer);       /* 1 sec (HRV send) */
APP_TIMER_DEF(m_heartbeat_timer);  /* 30 sec */

#define PPG_INTERVAL_MS     40
#define IMU_INTERVAL_MS     20
#define SEND_INTERVAL_MS    1000
#define HEARTBEAT_INTERVAL  30000

/* ── HRV Computation State ────────────────────────────────────────── */

#define MAX_RR_INTERVALS  60  /* 60 beats at ~60 bpm = 60 sec window */
static uint32_t rr_intervals[MAX_RR_INTERVALS];
static uint8_t  rr_count = 0;
static uint8_t  rr_idx = 0;
static bool     rr_full = false;

static uint8_t  current_hr = 0;
static uint8_t  current_hrv_rmssd = 0;
static uint8_t  current_pnn50 = 0;
static uint8_t  current_spo2 = 0;
static uint8_t  current_confidence = 0;

/* ── Body IMU State ──────────────────────────────────────────────── */

static int16_t body_ax, body_ay, body_az;
static int16_t body_gx, body_gy, body_gz;
static uint16_t body_sway_amp = 0;

/* Sway computation: track acceleration magnitude variance */
#define SWAY_WINDOW 50  /* 50 samples at 50 Hz = 1 sec */
static int16_t accel_mag_history[SWAY_WINDOW];
static uint16_t sway_hist_idx = 0;

/* ── State ───────────────────────────────────────────────────────── */

typedef enum {
    BELT_MODE_ACTIVE = 0,
    BELT_MODE_SLEEP  = 1,
} belt_mode_t;

typedef struct {
    belt_mode_t  mode;
    uint16_t     node_id;
    uint16_t     seq_counter;
    bool         connected;
    uint8_t      battery_pct;
} belt_state_t;

static belt_state_t g_state = {0};

/* ── PPG Timer (25 Hz) ────────────────────────────────────────────── */

static void ppg_timer_handler(void *p_context)
{
    if (g_state.mode == BELT_MODE_SLEEP) return;

    ppg_data_t ppg_data;
    if (ppg_read(&ppg_data) != NRF_SUCCESS) {
        return;
    }

    /* Detect peaks (heartbeats) */
    static uint16_t prev_ir = 0;
    static bool above_threshold = false;
    static uint32_t last_peak_time = 0;
    static uint32_t current_time = 0;

    current_time += PPG_INTERVAL_MS;  /* ms */

    uint16_t ir = ppg_data.ir_samples[0];

    /* Simple threshold-based peak detection */
    if (ir > 50000 && !above_threshold) {
        above_threshold = true;

        /* Check if this is a valid peak (debounce) */
        if (current_time - last_peak_time > 400) {  /* > 400ms = < 150 bpm */
            uint32_t rr_interval = current_time - last_peak_time;
            if (last_peak_time > 0 && rr_interval > 400 && rr_interval < 2000) {
                /* Store RR interval */
                rr_intervals[rr_idx] = rr_interval;
                rr_idx = (rr_idx + 1) % MAX_RR_INTERVALS;
                if (rr_idx == 0) rr_full = true;

                /* Compute HR */
                uint8_t count = rr_full ? MAX_RR_INTERVALS : rr_idx;
                if (count > 0) {
                    uint32_t sum = 0;
                    for (uint8_t i = 0; i < count; i++) sum += rr_intervals[i];
                    current_hr = 60000 / (sum / count);
                }
            }
            last_peak_time = current_time;
        }
    } else if (ir < 40000) {
        above_threshold = false;
    }

    /* Update SpO2 and confidence */
    current_spo2 = ppg_data.spo2;
    current_confidence = ppg_data.confidence;
}

/* ── HRV Computation (1 Hz) ───────────────────────────────────────── */

static void compute_hrv(void)
{
    uint8_t count = rr_full ? MAX_RR_INTERVALS : rr_idx;
    if (count < 5) {
        current_hrv_rmssd = 0;
        current_pnn50 = 0;
        return;
    }

    /* RMSSD: root mean square of successive differences */
    uint32_t sum_sq_diff = 0;
    uint16_t nn50_count = 0;
    for (uint8_t i = 1; i < count; i++) {
        int32_t diff = (int32_t)rr_intervals[i] - (int32_t)rr_intervals[i-1];
        sum_sq_diff += (uint32_t)(diff * diff);
        if (abs(diff) > 50) nn50_count++;
    }

    current_hrv_rmssd = (uint8_t)sqrtf((float)sum_sq_diff / (count - 1));
    current_pnn50 = (uint8_t)((float)nn50_count / (count - 1) * 100);
}

/* ── IMU Timer (50 Hz) ────────────────────────────────────────────── */

static void imu_timer_handler(void *p_context)
{
    if (g_state.mode == BELT_MODE_SLEEP) return;

    body_imu_read(&body_ax, &body_ay, &body_az,
                  &body_gx, &body_gy, &body_gz);

    /* Compute acceleration magnitude for sway detection */
    int32_t mag = (int32_t)sqrtf((float)(body_ax*body_ax + body_ay*body_ay + body_az*body_az));
    accel_mag_history[sway_hist_idx] = (int16_t)mag;
    sway_hist_idx = (sway_hist_idx + 1) % SWAY_WINDOW;

    /* Compute sway amplitude (variance of acceleration magnitude, 0.3-1.5 Hz band) */
    /* Simplified: std deviation of acceleration magnitude */
    float mean = 0;
    for (uint16_t i = 0; i < SWAY_WINDOW; i++) mean += accel_mag_history[i];
    mean /= SWAY_WINDOW;

    float variance = 0;
    for (uint16_t i = 0; i < SWAY_WINDOW; i++) {
        float d = accel_mag_history[i] - mean;
        variance += d * d;
    }
    variance /= SWAY_WINDOW;
    body_sway_amp = (uint16_t)sqrtf(variance);
}

/* ── Send Timer (1 Hz) ────────────────────────────────────────────── */

static void send_timer_handler(void *p_context)
{
    if (g_state.mode == BELT_MODE_SLEEP) return;

    compute_hrv();

    /* Send PPG payload */
    payload_ppg_t ppg_payload = {0};
    ppg_payload.hr = current_hr;
    ppg_payload.hrv_rmssd = current_hrv_rmssd;
    ppg_payload.pnn50 = current_pnn50;
    ppg_payload.spo2 = current_spo2;
    ppg_payload.confidence = current_confidence;
    ppg_payload.timestamp = (app_timer_cnt_get() / 32);

    uint8_t packet[DS_MAX_PACKET_LEN];
    uint8_t len = drivesync_encode(packet, sizeof(packet),
                                    MSG_TYPE_DATA_PPG,
                                    g_state.node_id,
                                    g_state.seq_counter++,
                                    0,
                                    (uint8_t *)&ppg_payload, sizeof(ppg_payload));
    if (g_state.connected) {
        ble_periph_send(packet, len);
    }

    /* Send body IMU payload */
    payload_body_imu_t imu_payload = {0};
    imu_payload.accel_x = body_ax;
    imu_payload.accel_y = body_ay;
    imu_payload.accel_z = body_az;
    imu_payload.gyro_x = body_gx;
    imu_payload.gyro_y = body_gy;
    imu_payload.gyro_z = body_gz;
    imu_payload.sway_amp = body_sway_amp;
    imu_payload.timestamp = (app_timer_cnt_get() / 32);

    len = drivesync_encode(packet, sizeof(packet),
                           MSG_TYPE_DATA_BODY_IMU,
                           g_state.node_id,
                           g_state.seq_counter++,
                           0,
                           (uint8_t *)&imu_payload, sizeof(imu_payload));
    if (g_state.connected) {
        ble_periph_send(packet, len);
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

    if (!drivesync_decode(data, len, &header, &payload)) return;

    switch (header.msg_type) {
    case MSG_TYPE_CMD_MODE:
        if (header.payload_len >= 1) {
            g_state.mode = (belt_mode_t)payload[0];
            NRF_LOG_INFO("Mode changed to %d", g_state.mode);
        }
        break;

    case MSG_TYPE_ALERT_DROWSY:
    case MSG_TYPE_ALERT_CRITICAL: {
        /* Trigger haptic vibration on chest */
        if (header.payload_len >= sizeof(payload_alert_t)) {
            payload_alert_t *alert = (payload_alert_t *)payload;
            NRF_LOG_INFO("Drowsiness alert: level %d", alert->alert_level);
            /* Drive ERM motor on P0.06 */
            nrf_gpio_cfg_output(6);
            nrf_gpio_pin_write(6, 1);
            nrf_delay_ms(alert->duration_sec * 1000);
            nrf_gpio_pin_write(6, 0);
        }
        break;
    }

    case MSG_TYPE_HEARTBEAT:
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

/* ── Main ─────────────────────────────────────────────────────────── */

int main(void)
{
    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("DriveSync Belt Tag starting...");

    g_state.mode = BELT_MODE_ACTIVE;
    g_state.node_id = DS_BELT_ID_BASE;  /* 0x0200 */
    g_state.seq_counter = 0;
    g_state.connected = false;

    app_timer_init();
    nrf_drv_gpiote_init();

    /* Initialize PPG sensor (MAX30101) */
    ppg_init();

    /* Initialize body IMU (LSM6DSO) */
    body_imu_init();

    /* Initialize BLE peripheral */
    ble_periph_init(ble_cmd_handler,
                    ble_connected_handler,
                    ble_disconnected_handler);
    ble_periph_advertise();

    /* Create timers */
    app_timer_create(&m_ppg_timer, APP_TIMER_MODE_REPEATED, ppg_timer_handler);
    app_timer_create(&m_imu_timer, APP_TIMER_MODE_REPEATED, imu_timer_handler);
    app_timer_create(&m_send_timer, APP_TIMER_MODE_REPEATED, send_timer_handler);
    app_timer_create(&m_heartbeat_timer, APP_TIMER_MODE_REPEATED, heartbeat_timer_handler);

    app_timer_start(m_ppg_timer, APP_TIMER_TICKS(PPG_INTERVAL_MS), NULL);
    app_timer_start(m_imu_timer, APP_TIMER_TICKS(IMU_INTERVAL_MS), NULL);
    app_timer_start(m_send_timer, APP_TIMER_TICKS(SEND_INTERVAL_MS), NULL);
    app_timer_start(m_heartbeat_timer, APP_TIMER_TICKS(HEARTBEAT_INTERVAL), NULL);

    NRF_LOG_INFO("Belt Tag 0x%04X ready (PPG 25Hz, IMU 50Hz)", g_state.node_id);

    while (true) {
        nrf_pwr_mgmt_run();
        NRF_LOG_FLUSH();
    }
}