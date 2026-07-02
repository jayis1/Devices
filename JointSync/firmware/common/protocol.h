#ifndef JOINTSYNC_PROTOCOL_H
#define JOINTSYNC_PROTOCOL_H

/**
 * JointSync Shared Communication Protocol
 *
 * Used by all nodes (Hub, Joint Tag, Compression Sleeve, Joint Scanner)
 * over BLE 5.0 and Sub-GHz 868 MHz links.
 *
 * License: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Protocol Constants ─────────────────────────────────────────── */

#define JS_SYNC_BYTE_0      0x4A   /* 'J' */
#define JS_SYNC_BYTE_1      0x53   /* 'S' */
#define JS_PROTOCOL_VERSION 0x01
#define JS_MAX_PAYLOAD      245
#define JS_MAX_PACKET_LEN   256    /* 11-byte header + 245 payload */
#define JS_BROADCAST_ID     0x0000
#define JS_HUB_ID           0x0000
#define JS_SCANNER_ID_BASE  0x0200
#define JS_SLEEVE_ID_BASE   0x0100

/* ── Message Types ──────────────────────────────────────────────── */

typedef enum {
    MSG_TYPE_DATA_IMU       = 0x01,
    MSG_TYPE_DATA_TEMP     = 0x02,
    MSG_TYPE_DATA_PPG      = 0x03,
    MSG_TYPE_DATA_THERMAL  = 0x04,
    MSG_TYPE_DATA_IMAGE    = 0x05,
    MSG_TYPE_DATA_PRESSURE = 0x06,
    MSG_TYPE_CMD_THERAPY   = 0x10,
    MSG_TYPE_CMD_SCAN      = 0x11,
    MSG_TYPE_CMD_MODE      = 0x12,
    MSG_TYPE_ALERT_FLARE   = 0x20,
    MSG_TYPE_ALERT_INFLAME = 0x21,
    MSG_TYPE_ALERT_THERAPY = 0x22,
    MSG_TYPE_ACK           = 0x30,
    MSG_TYPE_NACK          = 0x31,
    MSG_TYPE_PAIR_REQ      = 0x40,
    MSG_TYPE_PAIR_ACK     = 0x41,
    MSG_TYPE_HEARTBEAT    = 0x50,
    MSG_TYPE_STATUS       = 0x51,
} jointsync_msg_type_t;

/* ── Flags ──────────────────────────────────────────────────────── */

#define JS_FLAG_ENCRYPTED  0x01
#define JS_FLAG_COMPRESSED 0x02
#define JS_FLAG_ACK_REQ    0x04

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
} __attribute__((packed)) jointsync_header_t;

/* 11 bytes total (packed) */
#define JS_HEADER_LEN sizeof(jointsync_header_t)

/* ── Payload Structures ──────────────────────────────────────────── */

typedef struct {
    int16_t  accel_x;    /* milli-g */
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;     /* milli-degrees/sec */
    int16_t  gyro_y;
    int16_t  gyro_z;
    uint32_t timestamp;  /* ms since boot */
    uint8_t  flags;      /* bit0: high-activity, bit1: fall */
} __attribute__((packed)) payload_imu_t;       /* 15 bytes */

typedef struct {
    int16_t  temp_centi;  /* centi-degrees C (3250 = 32.50) */
    uint32_t timestamp;
    uint8_t  sensor_id;   /* 0=skin, 1=ambient */
} __attribute__((packed)) payload_temp_t;      /* 6 bytes */

typedef struct {
    uint16_t ir_samples[8];
    uint16_t red_samples[8];
    uint8_t  hr;          /* bpm, 0=not computed */
    uint8_t  hrv_ms;      /* RMSSD ms, 0=not computed */
    uint8_t  spo2;        /* %, 0=not computed */
    uint8_t  confidence;  /* 0-100 */
} __attribute__((packed)) payload_ppg_t;       /* 38 bytes */

typedef struct {
    uint8_t  mode;        /* 0=rest, 1=active, 2=pulsed, 3=adaptive */
    uint8_t  target_mmhg; /* 20-40 */
    uint16_t duration_sec;
    uint8_t  joint_id;    /* 0=knee, 1=elbow, 2=wrist, 3=ankle */
} __attribute__((packed)) payload_therapy_t;  /* 5 bytes */

typedef struct {
    int16_t  pressure_centi;  /* centi-mmHg (2000 = 20.00 mmHg) */
    int16_t  load_raw;        /* load cell raw ADC */
    uint32_t timestamp;
    uint8_t  pump_state;     /* 0=off, 1=inflate, 2=hold, 3=deflate */
} __attribute__((packed)) payload_pressure_t; /* 8 bytes */

/* Thermal scan chunk (MLX90640 32×24 = 768 pixels, sent in chunks) */
#define JS_THERMAL_PIXELS_PER_CHUNK  64   /* 64 int16 = 128 bytes */
#define JS_THERMAL_CHUNKS            12   /* ceil(768/64) = 12 chunks */

typedef struct {
    uint8_t  chunk_idx;       /* 0-11 */
    uint8_t  total_chunks;   /* 12 */
    int16_t  pixels[JS_THERMAL_PIXELS_PER_CHUNK]; /* centi-degrees C */
} __attribute__((packed)) payload_thermal_chunk_t; /* 1+1+128 = 130 bytes */

typedef struct {
    uint8_t  battery_pct;   /* 0-100 */
    uint8_t  state;         /* 0=idle, 1=active, 2=charging, 3=error */
    uint8_t  error_code;    /* 0=none */
} __attribute__((packed)) payload_status_t;  /* 3 bytes */

typedef struct {
    uint8_t  joint_type;    /* 0=knee, 1=elbow, 2=wrist, 3=ankle */
    uint8_t  side;          /* 0=left, 1=right */
    uint8_t  firmware_ver;
    uint8_t  hardware_rev;
} __attribute__((packed)) payload_pair_t;   /* 4 bytes */

/* ── API ─────────────────────────────────────────────────────────── */

/**
 * Compute XOR checksum over header bytes (excluding checksum field).
 */
uint8_t jointsync_checksum(const uint8_t *data, uint8_t len);

/**
 * Encode a packet (header + payload) into a byte buffer.
 * Returns total packet length, or 0 on error.
 */
uint8_t jointsync_encode(uint8_t *buf, uint8_t buf_len,
                         jointsync_msg_type_t msg_type,
                         uint16_t sender_id, uint16_t seq_num,
                         uint8_t flags,
                         const uint8_t *payload, uint8_t payload_len);

/**
 * Decode a byte buffer into header + payload pointer.
 * Returns true on success, false on checksum error / bad sync.
 */
bool jointsync_decode(const uint8_t *buf, uint8_t buf_len,
                      jointsync_header_t *header,
                      const uint8_t **payload);

/**
 * Validate sync bytes + version + checksum.
 */
bool jointsync_validate(const uint8_t *buf, uint8_t buf_len);

#endif /* JOINTSYNC_PROTOCOL_H */