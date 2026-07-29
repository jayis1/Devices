/*
 * RehabSync — Protocol Header
 * Binary message encoding/decoding for BLE BAN + Sub-GHz mesh network.
 */
#ifndef REHABSYNC_PROTOCOL_H
#define REHABSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define RS_SYNC0           0x52  /* 'R' */
#define RS_SYNC1           0x53  /* 'S' */
#define RS_BROADCAST       0xFF
#define RS_MAX_PAYLOAD     240
#define RS_MAX_MSG         256
#define RS_MAX_NODES       16
#define RS_AES_KEY_LEN     16
#define RS_CRC_POLY        0x1021  /* CRC-16-CCITT */
#define RS_MAX_PROBES      6      /* Body sensors */

/* Message types */
enum rs_msg_type {
    RS_MSG_JOIN_REQ        = 0x01,
    RS_MSG_JOIN_ACK        = 0x02,
    RS_MSG_TELEMETRY       = 0x03,
    RS_MSG_COMMAND         = 0x04,
    RS_MSG_CMD_ACK         = 0x05,
    RS_MSG_ALERT           = 0x06,
    RS_MSG_OTA_BLOCK       = 0x07,
    RS_MSG_OTA_ACK         = 0x08,
    RS_MSG_HEARTBEAT       = 0x09,
    RS_MSG_MESH_RELAY      = 0x0A,
    RS_MSG_TIME_SYNC       = 0x0B,
    RS_MSG_CONFIG          = 0x0C,
    RS_MSG_CONFIG_ACK      = 0x0D,
    RS_MSG_SESSION_START   = 0x0E,
    RS_MSG_SESSION_END     = 0x0F,
    RS_MSG_IMU_STREAM      = 0x10,
    RS_MSG_FORCE_STREAM    = 0x11,
    RS_MSG_PRESSURE_FRAME  = 0x12,
    RS_MSG_FORM_UPDATE     = 0x13,
    RS_MSG_REP_COUNT       = 0x14,
    RS_MSG_EXERCISE_ID     = 0x15,
};

/* Telemetry sub-types */
enum rs_telem_subtype {
    RS_TELEM_BODY_SENSOR  = 0x01,
    RS_TELEM_SMART_BAND   = 0x02,
    RS_TELEM_PRESSURE_MAT = 0x03,
    RS_TELEM_HUB          = 0x04,
};

/* Alert types */
enum rs_alert_type {
    RS_ALERT_FORM_DEVIATION       = 0x01,
    RS_ALERT_POOR_FORM            = 0x02,
    RS_ALERT_OVEREXERTION         = 0x03,
    RS_ALERT_FATIGUE_DETECTED     = 0x04,
    RS_ALERT_REGRESSION           = 0x05,
    RS_ALERT_SENSOR_OFFLINE       = 0x06,
    RS_ALERT_SENSOR_LOW_BATT      = 0x07,
    RS_ALERT_FALL_DETECTED        = 0x08,
    RS_ALERT_ROM_REGRESSION       = 0x09,
    RS_ALERT_ADHERENCE_DROP       = 0x0A,
    RS_ALERT_PAIN_INDICATOR       = 0x0B,
    RS_ALERT_SESSION_TIMEOUT      = 0x0C,
};

/* Command sub-types */
enum rs_cmd_type {
    RS_CMD_START_SESSION       = 0x01,
    RS_CMD_STOP_SESSION        = 0x02,
    RS_CMD_START_EXERCISE      = 0x03,
    RS_CMD_STOP_EXERCISE       = 0x04,
    RS_CMD_SET_EXERCISE        = 0x05,
    RS_CMD_SET_TARGET_REPS     = 0x06,
    RS_CMD_SET_RESISTANCE      = 0x07,
    RS_CMD_CALIBRATE           = 0x08,
    RS_CMD_REBOOT              = 0x09,
    RS_CMD_SET_CONFIG          = 0x0A,
    RS_CMD_HAPTIC_FEEDBACK     = 0x0B,
    RS_CMD_AUDIO_FEEDBACK      = 0x0C,
    RS_CMD_EMERGENCY_STOP      = 0x0D,
    RS_CMD_PAUSE               = 0x0E,
    RS_CMD_RESUME              = 0x0F,
};

/* Exercise types (30 exercises) */
enum rs_exercise_type {
    RS_EX_SQUAT                = 0x01,
    RS_EX_LUNGE                = 0x02,
    RS_EX_LEG_RAISE            = 0x03,
    RS_EX_KNEE_EXTENSION       = 0x04,
    RS_EX_HIP_ABDUCTION        = 0x05,
    RS_EX_SHOULDER_FLEXION     = 0x06,
    RS_EX_SHOULDER_ABDUCTION   = 0x07,
    RS_EX_BICEP_CURL           = 0x08,
    RS_EX_TRICEP_EXTENSION     = 0x09,
    RS_EX_EXTERNAL_ROTATION    = 0x0A,
    RS_EX_WALL_PUSHUP          = 0x0B,
    RS_EX_SIT_TO_STAND         = 0x0C,
    RS_EX_SINGLE_LEG_STANCE    = 0x0D,
    RS_EX_HEEL_RAISE           = 0x0E,
    RS_EX_STEP_UP              = 0x0F,
    RS_EX_BRIDGE               = 0x10,
    RS_EX_CLAMS_SHELL          = 0x11,
    RS_EX_SIDE_PLANK           = 0x12,
    RS_EX_BIRD_DOG             = 0x13,
    RS_EX_DEAD_BUG             = 0x14,
    RS_EX_HAMSTRING_CURL       = 0x15,
    RS_EX_CALF_RAISE           = 0x16,
    RS_EX_TERMINAL_KNEE_EXT    = 0x17,
    RS_EX_LATERAL_WALK         = 0x18,
    RS_EX_MONSTER_WALK         = 0x19,
    RS_EX_GLUTE_BRIDGE         = 0x1A,
    RS_EX_PLANK                = 0x1B,
    RS_EX_WALL_SIT             = 0x1C,
    RS_EX_CHOP                 = 0x1D,
    RS_EX_BAND_PULL_APART      = 0x1E,
};

/* Form deviation types */
enum rs_form_deviation {
    RS_DEV_NONE             = 0x00,
    RS_DEV_KNEE_VALGUS      = 0x01,
    RS_DEV_HIP_HIKE         = 0x02,
    RS_DEV_TRUNK_LEAN       = 0x03,
    RS_DEV_ROM_SHORTFALL    = 0x04,
    RS_DEV_EXCESSIVE_SPEED  = 0x05,
    RS_DEV_ASYMMETRY        = 0x06,
};

/* IMU data packet (12 bytes: accel[3] + gyro[3], int16 each) */
typedef struct __attribute__((packed)) {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
} rs_imu_sample_t;

/* Quaternion packet (8 bytes: q[4] int16, scaled) */
typedef struct __attribute__((packed)) {
    int16_t q0, q1, q2, q3;
} rs_quat_t;

/* Force data packet (4 bytes) */
typedef struct __attribute__((packed)) {
    int32_t force_mg;   /* force in milligrams-force */
} rs_force_sample_t;

/* Pressure frame header */
typedef struct __attribute__((packed)) {
    uint16_t frame_seq;
    uint16_t cop_x;     /* center of pressure x (0-65535 → 0-15.99) */
    uint16_t cop_y;     /* center of pressure y */
    uint16_t total_weight_g;  /* total detected weight in grams */
    uint16_t asymmetry; /* left-right asymmetry 0-1000 (0=perfect, 1000=one-legged) */
} rs_pressure_header_t;

/* Message header (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  sync0;
    uint8_t  sync1;
    uint8_t  src_id;
    uint8_t  dst_id;
    uint8_t  msg_type;
    uint8_t  subtype;
    uint8_t  seq;
    uint8_t  payload_len;
} rs_msg_header_t;

/* CRC-16-CCITT */
uint16_t rs_crc16(const uint8_t *data, size_t len);

/* Message encode (returns total message length including header + payload + crc) */
size_t rs_encode(uint8_t *out, size_t out_cap,
                 uint8_t src_id, uint8_t dst_id,
                 uint8_t msg_type, uint8_t subtype,
                 uint8_t seq, const uint8_t *payload, size_t payload_len);

/* Message decode (returns payload length, -1 on error) */
int rs_decode(const uint8_t *in, size_t in_len,
              rs_msg_header_t *hdr, uint8_t *payload, size_t payload_cap);

/* AES-128-CTR encrypt/decrypt (in-place) */
void rs_aes_encrypt(uint8_t *data, size_t len, const uint8_t *key, const uint8_t *nonce);

#endif /* REHABSYNC_PROTOCOL_H */