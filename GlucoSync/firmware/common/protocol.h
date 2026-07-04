#ifndef GLUCOSYNC_PROTOCOL_H
#define GLUCOSYNC_PROTOCOL_H

/**
 * GlucoSync Shared Communication Protocol
 *
 * Used by all nodes (Metabolic Hub, Meal Scanner, Activity Band,
 * Insulin Pen Tag) over BLE 5.0 links.
 *
 * License: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Protocol Constants ─────────────────────────────────────────── */

#define GS_SYNC_BYTE_0      0x47   /* 'G' */
#define GS_SYNC_BYTE_1      0x53   /* 'S' */
#define GS_PROTOCOL_VERSION 0x01
#define GS_MAX_PAYLOAD      245
#define GS_MAX_PACKET_LEN   256    /* 11-byte header + 245 payload */
#define GS_BROADCAST_ID     0x0000
#define GS_HUB_ID           0x0000
#define GS_SCANNER_ID_BASE  0x0100
#define GS_BAND_ID_BASE     0x0200
#define GS_PEN_ID_BASE      0x0300

/* ── Message Types ──────────────────────────────────────────────── */

typedef enum {
    MSG_TYPE_DATA_CGM       = 0x01,  /* Glucose reading from CGM */
    MSG_TYPE_DATA_MEAL      = 0x02,  /* Meal scan results */
    MSG_TYPE_DATA_ACTIVITY  = 0x03,  /* HR + activity classification */
    MSG_TYPE_DATA_INSULIN   = 0x04,  /* Insulin injection event */
    MSG_TYPE_DATA_HUB_IMU   = 0x05,  /* Hub inertial (tap/orientation) */
    MSG_TYPE_ALERT_HYPO     = 0x20,  /* Hypoglycemia warning */
    MSG_TYPE_ALERT_HYPER    = 0x21,  /* Hyperglycemia warning */
    MSG_TYPE_ALERT_CRITICAL  = 0x22,  /* Critical glucose alert */
    MSG_TYPE_FORECAST       = 0x30,  /* Glucose forecast from hub */
    MSG_TYPE_CMD_MODE       = 0x10,  /* Mode change (active/sleep/fasting) */
    MSG_TYPE_CMD_PAIR       = 0x11,  /* Pairing request */
    MSG_TYPE_ACK            = 0x40,
    MSG_TYPE_NACK           = 0x41,
    MSG_TYPE_HEARTBEAT      = 0x50,
    MSG_TYPE_STATUS         = 0x51,
} glucosync_msg_type_t;

/* ── Flags ──────────────────────────────────────────────────────── */

#define GS_FLAG_ENCRYPTED  0x01
#define GS_FLAG_COMPRESSED 0x02
#define GS_FLAG_ACK_REQ    0x04

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
} __attribute__((packed)) glucosync_header_t;

#define GS_HEADER_LEN sizeof(glucosync_header_t)  /* 11 bytes */

/* ── Payload Structures ──────────────────────────────────────────── */

/**
 * CGM glucose reading payload.
 * Sent at 1/min from CGM (or manual entry from app).
 */
typedef struct {
    uint16_t glucose_mgdl;    /* mg/dL, 0 = invalid */
    int16_t  trend_mgdl_min;  /* rate of change, centi (150 = 1.50 mg/dL/min) */
    uint8_t  sensor_state;    /* 0=ok, 1=warmup, 2=calibrating, 3=error */
    uint8_t  confidence;       /* 0-100 */
    uint32_t timestamp;       /* Unix epoch seconds */
} __attribute__((packed)) payload_cgm_t;    /* 12 bytes */

/**
 * Meal scan results payload (from Meal Scanner).
 * Sent on-demand after each scan.
 */
typedef struct {
    uint16_t food_class_id;   /* food class ID (0-199) */
    uint8_t  food_confidence; /* 0-100 */
    uint16_t carb_grams;      /* estimated carbs (g) */
    uint16_t portion_grams;   /* estimated portion (g) */
    uint8_t  glycemic_index;  /* estimated GI (0-100) */
    uint8_t  spectral_bands;  /* bitmask of captured bands */
    uint32_t timestamp;
} __attribute__((packed)) payload_meal_t;    /* 12 bytes */

/**
 * Activity data payload (from Activity Band).
 * Sent at 1 Hz to Hub.
 */
typedef struct {
    uint8_t  hr;              /* bpm, 0=not computed */
    uint8_t  hrv_rmssd;       /* RMSSD in ms, 0=not computed */
    uint8_t  activity_class;  /* 0=sedentary,1=walk,2=run,3=bike,4=strength */
    uint8_t  intensity;       /* 0-100 (Karvonen) */
    uint8_t  confidence;      /* 0-100 */
    uint32_t timestamp;
} __attribute__((packed)) payload_activity_t;   /* 8 bytes */

/**
 * Insulin injection event payload (from Pen Tag).
 * Sent on-demand after injection detection.
 */
typedef struct {
    uint8_t  pen_type;       /* 0=basal, 1=bolus */
    uint8_t  pen_id;         /* pen identifier (1-4) */
    uint8_t  estimated_units;/* estimated from pen config */
    uint8_t  confidence;     /* 0-100 (detection confidence) */
    uint16_t injection_dur_ms; /* injection duration */
    uint32_t timestamp;
} __attribute__((packed)) payload_insulin_t;    /* 10 bytes */

/**
 * Hub inertial payload (tap/orientation from hub IMU).
 * Internal — not sent over BLE.
 */
typedef struct {
    int16_t  accel_x;
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;
    int16_t  gyro_y;
    int16_t  gyro_z;
    uint8_t  tap_detected;   /* 0=none, 1=single, 2=double */
    uint32_t timestamp;
} __attribute__((packed)) payload_hub_imu_t;   /* 16 bytes */

/**
 * Glucose forecast payload (from Hub edge ML).
 * Sent to cloud; displayed on e-ink.
 */
typedef struct {
    uint16_t glucose_30min;  /* predicted glucose at t+30 min (mg/dL) */
    uint16_t glucose_60min;  /* predicted glucose at t+60 min (mg/dL) */
    uint8_t  hypo_risk_30;   /* 0-100, prob glucose <70 in 30 min */
    uint8_t  hyper_risk_60;  /* 0-100, prob glucose >180 in 60 min */
    uint8_t  risk_score;     /* 0-100 (fused metabolic risk) */
    uint8_t  recommendation; /* 0=none,1=monitor,2=snack,3=insulin,4=check,5=help */
    uint32_t timestamp;
} __attribute__((packed)) payload_forecast_t;  /* 14 bytes */

/**
 * Hypoglycemia alert payload (from Hub to nodes).
 */
typedef struct {
    uint8_t  risk_score;     /* 0-100 */
    uint8_t  alert_level;     /* 0=none,1=low,2=moderate,3=high,4=critical */
    uint8_t  predicted_glucose; /* predicted nadir (mg/dL) */
    uint8_t  minutes_to_event;  /* minutes until predicted event */
    uint8_t  duration_sec;   /* haptic/audio duration */
} __attribute__((packed)) payload_alert_t;    /* 5 bytes */

/**
 * Mode command payload.
 */
typedef struct {
    uint8_t  mode;           /* 0=active, 1=sleep, 2=fasting, 3=exercise */
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
    uint8_t  node_type;      /* 1=scanner, 2=band, 3=pen */
    uint8_t  firmware_ver;
    uint8_t  hardware_rev;
} __attribute__((packed)) payload_pair_t;   /* 3 bytes */

/* ── API ─────────────────────────────────────────────────────────── */

uint8_t glucosync_checksum(const uint8_t *data, uint8_t len);

uint8_t glucosync_encode(uint8_t *buf, uint8_t buf_len,
                         glucosync_msg_type_t msg_type,
                         uint16_t sender_id, uint16_t seq_num,
                         uint8_t flags,
                         const uint8_t *payload, uint8_t payload_len);

bool glucosync_decode(const uint8_t *buf, uint8_t buf_len,
                      glucosync_header_t *header,
                      const uint8_t **payload);

bool glucosync_validate(const uint8_t *buf, uint8_t buf_len);

#endif /* GLUCOSYNC_PROTOCOL_H */