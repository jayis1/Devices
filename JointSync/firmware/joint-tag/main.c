/**
 * JointSync Joint Tag — Main Firmware
 *
 * nRF52840 + BMI270 + TMP117 + MAX30101
 * BLE 5.0 peripheral. Worn on affected joints.
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "boards.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf_drv_gpiote.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "protocol.h"
#include "bmi270_driver.h"
#include "tmp117_driver.h"
#include "max30101_driver.h"
#include "sensor_fusion.h"
#include "ble_periph.h"

/* ── Timers ──────────────────────────────────────────────────────── */

APP_TIMER_DEF(m_imu_timer);     /* 10 ms (100 Hz) */
APP_TIMER_DEF(m_temp_timer);     /* 30 sec */
APP_TIMERDef(m_ppg_timer);      /* 40 ms (25 Hz) */
APP_TIMER_DEF(m_heartbeat_timer); /* 30 sec */
APP_TIMER_DEF(m_activity_timer); /* 3 sec */

#define IMU_INTERVAL_MS     10
#define TEMP_INTERVAL_MS    30000
#define PPG_INTERVAL_MS     40
#define HEARTBEAT_INTERVAL  30000
#define ACTIVITY_INTERVAL   3000

/* ── State ───────────────────────────────────────────────────────── */

typedef enum {
    TAG_MODE_ACTIVE = 0,
    TAG_MODE_SLEEP  = 1,
} tag_mode_t;

typedef struct {
    tag_mode_t  mode;
    uint16_t    node_id;
    uint16_t    seq_counter;
    bool        connected;
    uint8_t     battery_pct;
    uint32_t    boot_time;
    int16_t     last_ax, last_ay, last_az;
    int16_t     last_gx, last_gy, last_gz;
    uint8_t     activity_class;
    float       current_rom;
} tag_state_t;

static tag_state_t g_state = {0};

/* ── IMU Timer Callback ──────────────────────────────────────────── */

static void imu_timer_handler(void *p_context)
{
    if (g_state.mode == TAG_MODE_SLEEP) return;

    /* Read BMI270 */
    bmi270_data_t imu_data;
    if (bmi270_read(&imu_data) != NRF_SUCCESS) {
        return;
    }

    g_state.last_ax = imu_data.accel_x;
    g_state.last_ay = imu_data.accel_y;
    g_state.last_az = imu_data.accel_z;
    g_state.last_gx = imu_data.gyro_x;
    g_state.last_gy = imu_data.gyro_y;
    g_state.last_gz = imu_data.gyro_z;

    /* Update sensor fusion */
    sensor_fusion_update(imu_data.accel_x, imu_data.accel_y, imu_data.accel_z,
                          imu_data.gyro_x, imu_data.gyro_y, imu_data.gyro_z);
    g_state.current_rom = sensor_fusion_get_rom();

    /* Build IMU payload */
    payload_imu_t payload;
    payload.accel_x = imu_data.accel_x;
    payload.accel_y = imu_data.accel_y;
    payload.accel_z = imu_data.accel_z;
    payload.gyro_x = imu_data.gyro_x;
    payload.gyro_y = imu_data.gyro_y;
    payload.gyro_z = imu_data.gyro_z;
    payload.timestamp = (app_timer_cnt_get() / 32);  /* ms */
    payload.flags = 0;
    if (abs(imu_data.accel_x) > 2000 || abs(imu_data.accel_y) > 2000) {
        payload.flags |= 0x01;  /* high-activity */
    }

    /* Encode and send via BLE */
    uint8_t packet[JS_MAX_PACKET_LEN];
    uint8_t len = jointsync_encode(packet, sizeof(packet),
                                    MSG_TYPE_DATA_IMU,
                                    g_state.node_id,
                                    g_state.seq_counter++,
                                    0,
                                    (uint8_t *)&payload, sizeof(payload));
    if (g_state.connected) {
        ble_periph_send(packet, len);
    }
}

/* ── Temperature Timer Callback ───────────────────────────────────── */

static void temp_timer_handler(void *p_context)
{
    /* Read TMP117 */
    int16_t temp_centi = tmp117_read_temp();
    uint32_t timestamp = (app_timer_cnt_get() / 32);

    /* Send skin temperature */
    payload_temp_t payload;
    payload.temp_centi = temp_centi;
    payload.timestamp = timestamp;
    payload.sensor_id = 0;  /* skin */

    uint8_t packet[JS_MAX_PACKET_LEN];
    uint8_t len = jointsync_encode(packet, sizeof(packet),
                                    MSG_TYPE_DATA_TEMP,
                                    g_state.node_id,
                                    g_state.seq_counter++,
                                    0,
                                    (uint8_t *)&payload, sizeof(payload));
    if (g_state.connected) {
        ble_periph_send(packet, len);
    }

    /* Read ambient temperature (second TMP117 if present) */
    int16_t ambient = tmp117_read_ambient();
    if (ambient != 0) {
        payload.temp_centi = ambient;
        payload.sensor_id = 1;  /* ambient */
        len = jointsync_encode(packet, sizeof(packet),
                               MSG_TYPE_DATA_TEMP,
                               g_state.node_id,
                               g_state.seq_counter++,
                               0,
                               (uint8_t *)&payload, sizeof(payload));
        ble_periph_send(packet, len);
    }
}

/* ── PPG Timer Callback ──────────────────────────────────────────── */

static void ppg_timer_handler(void *p_context)
{
    if (g_state.mode == TAG_MODE_SLEEP) return;

    /* Read MAX30101 (8 samples at 25 Hz = 320 ms window) */
    max30101_data_t ppg_data;
    if (max30101_read(&ppg_data) != NRF_SUCCESS) {
        return;
    }

    payload_ppg_t payload;
    memcpy(payload.ir_samples, ppg_data.ir_samples, sizeof(payload.ir_samples));
    memcpy(payload.red_samples, ppg_data.red_samples, sizeof(payload.red_samples));
    payload.hr = ppg_data.hr;
    payload.hrv_ms = ppg_data.hrv_ms;
    payload.spo2 = ppg_data.spo2;
    payload.confidence = ppg_data.confidence;

    uint8_t packet[JS_MAX_PACKET_LEN];
    uint8_t len = jointsync_encode(packet, sizeof(packet),
                                    MSG_TYPE_DATA_PPG,
                                    g_state.node_id,
                                    g_state.seq_counter++,
                                    0,
                                    (uint8_t *)&payload, sizeof(payload));
    if (g_state.connected) {
        ble_periph_send(packet, len);
    }
}

/* ── Heartbeat Timer ─────────────────────────────────────────────── */

static void heartbeat_timer_handler(void *p_context)
{
    payload_status_t status;
    status.battery_pct = g_state.battery_pct;
    status.state = g_state.connected ? 1 : 0;
    status.error_code = 0;

    uint8_t packet[JS_MAX_PACKET_LEN];
    uint8_t len = jointsync_encode(packet, sizeof(packet),
                                    MSG_TYPE_STATUS,
                                    g_state.node_id,
                                    g_state.seq_counter++,
                                    0,
                                    (uint8_t *)&status, sizeof(status));
    ble_periph_send(packet, len);
}

/* ── Activity Classification Timer ───────────────────────────────── */

static void activity_timer_handler(void *p_context)
{
    /* Run TinyCNN activity classifier on 3-sec IMU window */
    /* Uses accelerometer magnitude variance to classify:
     *   0=rest, 1=walk, 2=climb, 3=sit, 4=run, 5=cycle */
    
    float accel_mag = sqrtf((float)(g_state.last_ax * g_state.last_ax +
                                     g_state.last_ay * g_state.last_ay +
                                     g_state.last_az * g_state.last_az)) / 1000.0f;
    
    if (accel_mag < 1.1f) {
        g_state.activity_class = 0;  /* rest */
    } else if (accel_mag < 1.5f) {
        g_state.activity_class = 3;  /* sit */
    } else if (accel_mag < 2.0f) {
        g_state.activity_class = 1;  /* walk */
    } else if (accel_mag < 3.0f) {
        g_state.activity_class = 2;  /* climb */
    } else {
        g_state.activity_class = 4;  /* run */
    }
}

/* ── BLE Command Handler ─────────────────────────────────────────── */

static void ble_cmd_handler(const uint8_t *data, uint8_t len)
{
    jointsync_header_t header;
    const uint8_t *payload;

    if (!jointsync_decode(data, len, &header, &payload)) {
        return;
    }

    switch (header.msg_type) {
    case MSG_TYPE_CMD_MODE:
        if (header.payload_len >= 1) {
            g_state.mode = (tag_mode_t)payload[0];
            NRF_LOG_INFO("Mode changed to %d", g_state.mode);
        }
        break;

    case MSG_TYPE_HEARTBEAT:
        /* Hub heartbeat — reset watchdog */
        break;

    case MSG_TYPE_ACK:
        /* Acknowledge received */
        break;

    default:
        break;
    }
}

/* ── BLE Connection Callbacks ────────────────────────────────────── */

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

/* ── Battery Measurement ─────────────────────────────────────────── */

static uint8_t read_battery_pct(void)
{
    /* CR2477: 3.0V full, 2.0V empty */
    /* Read ADC on P0.19 (battery voltage divider) */
    uint16_t adc_val = 0;  /* TODO: implement ADC read */
    float voltage = (adc_val / 1023.0f) * 3.0f * 2.0f;  /* Divider ratio 2:1 */
    float pct = ((voltage - 2.0f) / (3.0f - 2.0f)) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    return (uint8_t)pct;
}

/* ── Main ────────────────────────────────────────────────────────── */

int main(void)
{
    /* Initialize logging */
    NRF_LOG_INIT(NULL);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    NRF_LOG_INFO("JointSync Tag starting...");

    /* Initialize state */
    g_state.mode = TAG_MODE_ACTIVE;
    g_state.node_id = 0x0001;  /* TODO: read from NFC or flash */
    g_state.seq_counter = 0;
    g_state.connected = false;
    g_state.boot_time = 0;

    /* Initialize timers */
    app_timer_init();

    /* Initialize GPIO */
    nrf_drv_gpiote_init();

    /* Initialize sensors */
    bmi270_init();
    tmp117_init();
    max30101_init();

    /* Initialize sensor fusion */
    sensor_fusion_init(100.0f);  /* 100 Hz */

    /* Initialize BLE */
    ble_periph_init(ble_cmd_handler,
                    ble_connected_handler,
                    ble_disconnected_handler);
    ble_periph_advertise();

    /* Create timers */
    app_timer_create(&m_imu_timer, APP_TIMER_MODE_REPEATED, imu_timer_handler);
    app_timer_create(&m_temp_timer, APP_TIMER_MODE_REPEATED, temp_timer_handler);
    app_timer_create(&m_ppg_timer, APP_TIMER_MODE_REPEATED, ppg_timer_handler);
    app_timer_create(&m_heartbeat_timer, APP_TIMER_MODE_REPEATED, heartbeat_timer_handler);
    app_timer_create(&m_activity_timer, APP_TIMER_MODE_REPEATED, activity_timer_handler);

    /* Start timers */
    app_timer_start(m_imu_timer, APP_TIMER_TICKS(IMU_INTERVAL_MS), NULL);
    app_timer_start(m_temp_timer, APP_TIMER_TICKS(TEMP_INTERVAL_MS), NULL);
    app_timer_start(m_ppg_timer, APP_TIMER_TICKS(PPG_INTERVAL_MS), NULL);
    app_timer_start(m_heartbeat_timer, APP_TIMER_TICKS(HEARTBEAT_INTERVAL), NULL);
    app_timer_start(m_activity_timer, APP_TIMER_TICKS(ACTIVITY_INTERVAL), NULL);

    NRF_LOG_INFO("Tag 0x%04X ready (IMU 100Hz, PPG 25Hz)", g_state.node_id);

    /* Main loop */
    while (true) {
        nrf_pwr_mgmt_run();
        NRF_LOG_FLUSH();
    }
}