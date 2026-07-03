#ifndef DRIVESYNC_PROTOCOL_H
#define DRIVESYNC_PROTOCOL_H

/**
 * DriveSync Shared Communication Protocol
 *
 * Used by all nodes (Dash Hub, Steering Wheel Node, Seat Belt Tag, OBD-II Dongle)
 * over BLE 5.0 links.
 *
 * License: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Protocol Constants ─────────────────────────────────────────── */

#define DS_SYNC_BYTE_0      0x44   /* 'D' */
#define DS_SYNC_BYTE_1      0x53   /* 'S' */
#define DS_PROTOCOL_VERSION 0x01
#define DS_MAX_PAYLOAD      245
#define DS_MAX_PACKET_LEN   256    /* 11-byte header + 245 payload */
#define DS_BROADCAST_ID     0x0000
#define DS_HUB_ID           0x0000
#define DS_WHEEL_ID_BASE    0x0100
#define DS_BELT_ID_BASE     0x0200
#define DS_OBD_ID_BASE      0x0300

/* ── Message Types ──────────────────────────────────────────────── */

typedef enum {
    MSG_TYPE_DATA_CAMERA    = 0x01,  /* PERCLOS/blink/head-pose features */
    MSG_TYPE_DATA_STEERING  = 0x02,  /* Steering IMU + grip */
    MSG_TYPE_DATA_PPG       = 0x03,  /* PPG features + HRV */
    MSG_TYPE_DATA_OBD       = 0x04,  /* Vehicle telemetry */
    MSG_TYPE_DATA_BODY_IMU  = 0x05,  /* Body sway from belt IMU */
    MSG_TYPE_DATA_HUB_IMU   = 0x06,  /* Hub inertial (vehicle motion) */
    MSG_TYPE_CMD_MODE       = 0x10,  /* Mode change (active/sleep/park) */
    MSG_TYPE_CMD_PAIR       = 0x11,  /* Pairing request */
    MSG_TYPE_ALERT_DROWSY  = 0x20,  /* Drowsiness alert */
    MSG_TYPE_ALERT_DISTRACT = 0x21, /* Distraction alert */
    MSG_TYPE_ALERT_CRITICAL = 0x22,  /* Critical drowsiness alert */
    MSG_TYPE_ACK            = 0x30,
    MSG_TYPE_NACK           = 0x31,
    MSG_TYPE_HEARTBEAT      = 0x50,
    MSG_TYPE_STATUS         = 0x51,
} drivesync_msg_type_t;

/* ── Flags ──────────────────────────────────────────────────────── */

#define DS_FLAG_ENCRYPTED  0x01
#define DS_FLAG_COMPRESSED 0x02
#define DS_FLAG_ACK_REQ    0x04

/* ── Packet Header ───────────────────────────────────────────────── */

typedef struct {
    uint8_t  sync[2];
    uint8_t  version;
    uint8_t  msg_type;
    uint16_t sender_id;
    uint16_t seq_num;
    uint8_t  flags;
    uint8_t  payload_len;
    uint8_t  checksum;
} __attribute__((packed)) drivesync_header_t;

#define DS_HEADER_LEN sizeof(drivesync_header_t)  /* 11 bytes */

/* ── Payload Structures ──────────────────────────────────────────── */

/**
 * Camera features payload (from Hub edge inference).
 * Sent to cloud; never sent over BLE (hub processes locally).
 */
typedef struct {
    float    perclos;         /* 0.0-1.0, fraction eyes >80% closed (1-min) */
    uint16_t blink_rate;      /* blinks per minute */
    uint16_t avg_blink_dur;   /* avg blink duration (ms) */
    int16_t  head_pitch;      /* degrees, centi (150 = 1.50°) */
    int16_t  head_yaw;
    int16_t  head_roll;
    uint8_t  head_bob_count;  /* head-bob events in last minute */
    uint8_t  confidence;      /* 0-100 */
    uint32_t timestamp;       /* ms since boot */
} __attribute__((packed)) payload_camera_t;    /* 20 bytes */

/**
 * Steering IMU + grip payload (from Wheel Node).
 * Sent at 10 Hz to Hub.
 */
typedef struct {
    int16_t  gyro_z;          /* milli-degrees/sec (steering angular velocity) */
    int16_t  accel_x;         /* milli-g */
    int16_t  accel_y;
    int16_t  accel_z;
    uint16_t jerk_count;      /* reversal count in last 100ms window */
    uint16_t grip_raw[4];     /* FDC2214 capacitance readings (4 channels) */
    uint8_t  hands_on;        /* 0=off, 1=on (derived from grip) */
    uint8_t  grip_force;      /* 0-100 (proxy) */
    uint32_t timestamp;
} __attribute__((packed)) payload_steering_t;  /* 24 bytes */

/**
 * PPG + HRV payload (from Belt Tag).
 * Sent at 1 Hz to Hub (HRV computed over 60s windows).
 */
typedef struct {
    uint8_t  hr;          /* bpm, 0=not computed */
    uint8_t  hrv_rmssd;   /* RMSSD in ms, 0=not computed */
    uint8_t  pnn50;       /* pNN50 percentage, 0-100 */
    uint8_t  spo2;        /* %, 0=not computed */
    uint8_t  confidence;  /* 0-100 */
    uint32_t timestamp;
} __attribute__((packed)) payload_ppg_t;    /* 8 bytes */

/**
 * Body IMU payload (from Belt Tag).
 * Sent at 10 Hz to Hub.
 */
typedef struct {
    int16_t  accel_x;     /* milli-g */
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;      /* milli-degrees/sec */
    int16_t  gyro_y;
    int16_t  gyro_z;
    uint16_t sway_amp;    /* torso sway amplitude (milli-g, 0.3-1.5 Hz band) */
    uint32_t timestamp;
} __attribute__((packed)) payload_body_imu_t;   /* 16 bytes */

/**
 * OBD-II vehicle telemetry payload (from OBD Dongle).
 * Sent at 10 Hz to Hub.
 */
typedef struct {
    uint16_t speed_kmh;       /* km/h */
    uint16_t rpm;              /* engine RPM */
    uint8_t  throttle_pct;     /* 0-100 */
    uint8_t  engine_load;     /* 0-100 */
    int16_t  coolant_temp_c;   /* centi-degrees C */
    uint8_t  fuel_level;       /* 0-100 (if available) */
    uint8_t  obd_pid_flags;   /* bitmask of supported PIDs */
    uint32_t timestamp;
} __attribute__((packed)) payload_obd_t;   /* 13 bytes */

/**
 * Hub inertial payload (vehicle motion from hub IMU).
 * Internal — not sent over BLE.
 */
typedef struct {
    int16_t  accel_x;
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;
    int16_t  gyro_y;
    int16_t  gyro_z;
    uint32_t timestamp;
} __attribute__((packed)) payload_hub_imu_t;   /* 14 bytes */

/**
 * Drowsiness alert payload (from Hub to Wheel/Belt nodes).
 */
typedef struct {
    uint8_t  risk_score;     /* 0-100 */
    uint8_t  alert_level;     /* 0=none, 1=low, 2=moderate, 3=high, 4=critical */
    uint8_t  source;         /* 0=perclos, 1=steering, 2=hrv, 3=fusion */
    uint8_t  duration_sec;   /* haptic duration */
} __attribute__((packed)) payload_alert_t;    /* 4 bytes */

/**
 * Mode command payload.
 */
typedef struct {
    uint8_t  mode;           /* 0=active, 1=sleep, 2=park */
} __attribute__((packed)) payload_mode_t;    /* 1 byte */

/**
 * Status payload (heartbeats).
 */
typedef struct {
    uint8_t  battery_pct;    /* 0-100 */
    uint8_t  state;          /* 0=idle, 1=active, 2=charging, 3=error */
    uint8_t  error_code;     /* 0=none */
} __attribute__((packed)) payload_status_t;  /* 3 bytes */

/**
 * Pairing payload.
 */
typedef struct {
    uint8_t  node_type;      /* 1=wheel, 2=belt, 3=obd */
    uint8_t  firmware_ver;
    uint8_t  hardware_rev;
} __attribute__((packed)) payload_pair_t;   /* 3 bytes */

/* ── API ─────────────────────────────────────────────────────────── */

/**
 * Compute XOR checksum over header bytes (excluding checksum field).
 */
uint8_t drivesync_checksum(const uint8_t *data, uint8_t len);

/**
 * Encode a packet (header + payload) into a byte buffer.
 * Returns total packet length, or 0 on error.
 */
uint8_t drivesync_encode(uint8_t *buf, uint8_t buf_len,
                         drivesync_msg_type_t msg_type,
                         uint16_t sender_id, uint16_t seq_num,
                         uint8_t flags,
                         const uint8_t *payload, uint8_t payload_len);

/**
 * Decode a byte buffer into header + payload pointer.
 * Returns true on success, false on checksum error / bad sync.
 */
bool drivesync_decode(const uint8_t *buf, uint8_t buf_len,
                      drivesync_header_t *header,
                      const uint8_t **payload);

/**
 * Validate sync bytes + version + checksum.
 */
bool drivesync_validate(const uint8_t *buf, uint8_t buf_len);

#endif /* DRIVESYNC_PROTOCOL_H */