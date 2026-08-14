#ifndef POSTURESYNC_PROTOCOL_H
#define POSTURESYNC_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* Node IDs */
#define NODE_ID_HUB             0x0001
#define NODE_ID_SPINE_BAND      0x0002
#define NODE_ID_POSTURE_GARMENT 0x0003
#define NODE_ID_CHAIR_PAD       0x0004
#define NODE_ID_DESK_SENTINEL   0x0005
#define NODE_ID_BROADCAST       0xFFFF

/* Message types */
#define MSG_TYPE_BEACON         0x01
#define MSG_TYPE_SENSOR_DATA    0x02
#define MSG_TYPE_POSTURE_ALERT  0x03
#define MSG_TYPE_HAPTIC_CMD     0x04
#define MSG_TYPE_JOIN_REQ       0x05
#define MSG_TYPE_JOIN_ACK       0x06
#define MSG_TYPE_HEARTBEAT      0x07
#define MSG_TYPE_OTA_CHUNK      0x08
#define MSG_TYPE_CONFIG         0x09
#define MSG_TYPE_CAL_REQ        0x0A

/* Frame constants */
#define FRAME_PREAMBLE_LEN  4
#define FRAME_SYNC_LEN     2
#define FRAME_HEADER_LEN   12
#define FRAME_PAYLOAD_MAX   32
#define FRAME_CRC_LEN      2
#define FRAME_MAX_LEN      (FRAME_HEADER_LEN + FRAME_PAYLOAD_MAX + FRAME_CRC_LEN)

#define SYNC_WORD_0 0x2D
#define SYNC_WORD_1 0xD4

/* Haptic patterns */
#define HAPTIC_NONE         0x00
#define HAPTIC_SINGLE_TAP   0x01
#define HAPTIC_DOUBLE_PULSE 0x02
#define HAPTIC_TRIPLE_BURST 0x03
#define HAPTIC_LONG_BUZZ    0x04
#define HAPTIC_PATTERN_WAVE 0x05

/* Posture classes (PostureCNN) */
#define POSTURE_NEUTRAL           0
#define POSTURE_FORWARD_HEAD      1
#define POSTURE_SLOUCHING         2
#define POSTURE_HYPEREXTENSION    3
#define POSTURE_LATERAL_LEFT      4
#define POSTURE_LATERAL_RIGHT     5
#define POSTURE_KYPHOTIC          6
#define POSTURE_LORDOTIC          7
#define POSTURE_SCOLIOTIC         8
#define POSTURE_ANTERIOR_TILT     9
#define POSTURE_POSTERIOR_TILT    10
#define POSTURE_CROSSED_LEGS      11
#define POSTURE_COUNT             12

/* TDMA superframe */
#define TDMA_SUPERFRAME_MS    1000
#define TDMA_BEACON_MS        20
#define TDMA_SLOT_MS          50
#define TDMA_MAX_SLOTS        19  /* beacon + 18 slots */

#pragma pack(push, 1)

typedef struct {
    uint8_t  preamble[FRAME_PREAMBLE_LEN];
    uint8_t  sync[FRAME_SYNC_LEN];
    uint8_t  length;
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t  msg_type;
    uint16_t seq_num;
    uint8_t  payload[FRAME_PAYLOAD_MAX];
    uint16_t crc;
} postsync_frame_t;

/* Sensor data payload for Spine Band (32 bytes) */
typedef struct {
    float    pitch;          /* forward/backward tilt (deg) */
    float    roll;           /* lateral tilt (deg) */
    float    yaw;            /* rotation (deg) */
    float    accel_mag;      /* acceleration magnitude (g) */
    uint8_t  activity;       /* activity class */
    uint8_t  posture_class;  /* PostureCNN class */
    uint8_t  hr;             /* heart rate (bpm) */
    uint8_t  hrv;            /* HRV (ms) */
    uint8_t  spo2;           /* SpO2 (%) */
    uint8_t  battery;        /* battery (%) */
    uint8_t  flags;          /* bit0: calibrated, bit1: wearing */
    uint8_t  reserved[8];
} spine_band_data_t;

/* Sensor data payload for Posture Garment (32 bytes) */
typedef struct {
    uint16_t emg_rms[8];    /* 8-channel EMG RMS (mV) */
    float    cervical_angle; /* cervical curvature (deg) */
    float    thoracic_angle; /* thoracic curvature (deg) */
    float    lumbar_angle;   /* lumbar curvature (deg) */
    uint8_t  asymmetry_pct;  /* bilateral asymmetry (%) */
    uint8_t  fatigue_idx;    /* fatigue index (0-100) */
    uint8_t  battery;
    uint8_t  flags;
} garment_data_t;

/* Sensor data payload for Chair Pad (32 bytes) */
typedef struct {
    uint16_t weight_total;      /* total weight (g) */
    uint8_t  left_pct;          /* left side weight % */
    uint8_t  right_pct;         /* right side weight % */
    uint8_t  pelvic_tilt;       /* anterior/posterior tilt (deg) */
    uint8_t  posture_class;     /* detected sitting posture */
    uint8_t  pressure_map[16]; /* 16x16 downsampled to 4x4 pressure map */
    uint8_t  ischial_contact;   /* ischial vs sacral (0-100) */
    uint8_t  movement_var;     /* movement variance (active sitting) */
    uint8_t  battery;
    uint8_t  flags;
} chair_pad_data_t;

/* Sensor data payload for Desk Sentinel (32 bytes) */
typedef struct {
    uint16_t screen_distance_mm; /* user face to screen (mm) */
    uint16_t desk_height_mm;     /* floor to desk (mm) */
    uint16_t ambient_lux;        /* ambient light (lux) */
    uint8_t  sit_stand;          /* 0=sitting, 1=standing, 2=transition */
    uint8_t  time_in_position;   /* minutes in current position */
    uint8_t  battery;
    uint8_t  flags;
    uint8_t  reserved[16];
} desk_sentinel_data_t;

/* Posture alert payload */
typedef struct {
    uint8_t  posture_class;
    uint8_t  severity;       /* 0-100 */
    uint8_t  haptic_pattern;
    uint8_t  duration_sec;
    char     message[24];
} posture_alert_t;

/* Join request payload */
typedef struct {
    uint8_t  node_type;
    uint8_t  hw_version;
    uint8_t  fw_version;
    uint8_t  capabilities;
    uint64_t device_uid;
} join_req_t;

/* Beacon payload */
typedef struct {
    uint32_t timestamp;
    uint8_t  num_slots;
    uint8_t  slot_assignments[TDMA_MAX_SLOTS];
    uint8_t  channel;
    int8_t   rssi;
} beacon_t;

#pragma pack(pop)

/* API functions */
uint16_t postsync_crc16(const uint8_t *data, uint16_t len);
void postsync_build_frame(postsync_frame_t *frame, uint16_t src, uint16_t dst,
                          uint8_t msg_type, uint16_t seq, const uint8_t *payload, uint8_t plen);
bool postsync_parse_frame(const uint8_t *raw, uint16_t len, postsync_frame_t *out);
bool postsync_verify_crc(const postsync_frame_t *frame);

#endif /* POSTURESYNC_PROTOCOL_H */