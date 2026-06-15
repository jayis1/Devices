/*
 * ErgoFlow — Protocol Definitions
 * Message structures, serialization, and deserialization
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "mesh_config.h"

/* ── Pressure Map Message ─────────────────────────────────────────── */
/* 16 pressure values (8 seat + 8 backrest) + 1 IMU flag byte       */

typedef struct __attribute__((packed)) {
    uint8_t pressure[16];   /* Seat S1-S8, Back B1-B8, 0-255 */
    uint8_t imu_flags;      /* bit0=sitting, bit1=moving, bit2=tilt_alert */
} ergo_pressure_map_t;

#define ERGO_IMU_FLAG_SITTING   (1 << 0)
#define ERGO_IMU_FLAG_MOVING    (1 << 1)
#define ERGO_IMU_FLAG_TILT      (1 << 2)

/* ── IMU Orientation Message ──────────────────────────────────────── */
/* 4 quaternion floats + activity class + confidence                */

typedef struct __attribute__((packed)) {
    float quat[4];         /* Quaternion: w, x, y, z */
    uint8_t activity;      /* ERGO_ACTIVITY_* enum */
    uint8_t confidence;    /* 0-100% */
} ergo_imu_orientation_t;

/* ── Heart Rate Message ───────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t hr_bpm;        /* Heart rate in BPM */
    uint8_t spo2_pct;      /* SpO2 percentage */
} ergo_heart_rate_t;

/* ── Desk Command Message ──────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t cmd;            /* ERGO_DESK_CMD_* */
    uint16_t target_mm;     /* Target height in mm (for HEIGHT cmd) */
    uint8_t speed_pct;      /* Speed 0-100% */
} ergo_desk_command_t;

/* ── Desk Status Message ──────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t height_mm;     /* Current height in mm */
    uint8_t motor_state;    /* 0=idle, 1=moving_up, 2=moving_down, 3=error */
    uint16_t current_ma;    /* Motor current in mA */
} ergo_desk_status_t;

/* ── Ambient Reading Message ──────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint32_t lux;            /* Ambient light in lux */
    int16_t temp_celsius;   /* Temperature in 0.1°C units */
    uint16_t humidity_pct;   /* Relative humidity in 0.1% units */
} ergo_ambient_reading_t;

/* ── Posture Score Message ─────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t score;           /* 0-100 posture quality score */
    uint8_t risk_level;      /* 0=low, 1=medium, 2=high */
    uint16_t duration_s;     /* Seconds in current posture */
} ergo_posture_score_t;

/* ── Break Reminder Message ─────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t type;            /* ERGO_BREAK_* */
    uint16_t duration_s;     /* Suggested break duration */
} ergo_break_reminder_t;

/* ── Lighting Command Message ──────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t r, g, b, w;     /* RGBW color values 0-255 */
    uint8_t brightness_pct;  /* Brightness 0-100% */
    uint8_t mode;            /* 0=manual, 1=circadian, 2=focus, 3=relax */
} ergo_lighting_cmd_t;

/* ── Monitor Tilt Message ──────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    int8_t tilt_degrees;     /* -15 to +15 degrees */
    uint8_t speed_pct;      /* Speed 0-100% */
} ergo_monitor_tilt_t;

/* ── OTA Available Message ─────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint32_t firmware_size;
    uint8_t sha256[16];      /* First 16 bytes of SHA256 */
} ergo_ota_available_t;

/* ── OTA Data Message ──────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t seq_num;
    uint8_t chunk[16];
} ergo_ota_data_t;

/* ── Node Heartbeat Message ────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t battery_pct;     /* Battery 0-100% */
    uint8_t state;           /* ERGO_STATE_* */
    uint16_t uptime_min;     /* Uptime in minutes */
} ergo_node_heartbeat_t;

/* ── Calibration Message ──────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t target;          /* Which calibration (0=pressure, 1=imu, 2=desk) */
    uint32_t param1;
    uint32_t param2;
} ergo_calibration_t;

/* ── Serialization Functions ──────────────────────────────────────── */

/* Pack structures into byte buffers for BLE mesh transmission */
int ergo_pack_pressure_map(const ergo_pressure_map_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_imu_orientation(const ergo_imu_orientation_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_heart_rate(const ergo_heart_rate_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_desk_command(const ergo_desk_command_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_desk_status(const ergo_desk_status_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_ambient_reading(const ergo_ambient_reading_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_posture_score(const ergo_posture_score_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_break_reminder(const ergo_break_reminder_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_lighting_cmd(const ergo_lighting_cmd_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_monitor_tilt(const ergo_monitor_tilt_t *msg, uint8_t *buf, uint16_t *len);
int ergo_pack_node_heartbeat(const ergo_node_heartbeat_t *msg, uint8_t *buf, uint16_t *len);

/* Unpack byte buffers into structures */
int ergo_unpack_pressure_map(const uint8_t *buf, uint16_t len, ergo_pressure_map_t *msg);
int ergo_unpack_imu_orientation(const uint8_t *buf, uint16_t len, ergo_imu_orientation_t *msg);
int ergo_unpack_heart_rate(const uint8_t *buf, uint16_t len, ergo_heart_rate_t *msg);
int ergo_unpack_desk_command(const uint8_t *buf, uint16_t len, ergo_desk_command_t *msg);
int ergo_unpack_desk_status(const uint8_t *buf, uint16_t len, ergo_desk_status_t *msg);
int ergo_unpack_ambient_reading(const uint8_t *buf, uint16_t len, ergo_ambient_reading_t *msg);
int ergo_unpack_posture_score(const uint8_t *buf, uint16_t len, ergo_posture_score_t *msg);
int ergo_unpack_break_reminder(const uint8_t *buf, uint16_t len, ergo_break_reminder_t *msg);
int ergo_unpack_lighting_cmd(const uint8_t *buf, uint16_t len, ergo_lighting_cmd_t *msg);
int ergo_unpack_monitor_tilt(const uint8_t *buf, uint16_t len, ergo_monitor_tilt_t *msg);
int ergo_unpack_node_heartbeat(const uint8_t *buf, uint16_t len, ergo_node_heartbeat_t *msg);

#endif /* PROTOCOL_H */