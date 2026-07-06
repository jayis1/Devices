#ifndef SIGHTSYNC_PROTOCOL_H
#define SIGHTSYNC_PROTOCOL_H

/**
 * SightSync Shared Communication Protocol
 *
 * Used by all nodes (Vision Hub, Desk Sentinel, Wearable Eye Tag,
 * Smart Lamp Node) over BLE 5.0 and Sub-GHz 868 MHz links.
 *
 * License: MIT
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ── Protocol Constants ─────────────────────────────────────────── */

#define SS_SYNC_BYTE_0      0x53   /* 'S' */
#define SS_SYNC_BYTE_1      0x53   /* 'S' */
#define SS_PROTOCOL_VERSION 0x01
#define SS_MAX_PAYLOAD      245
#define SS_MAX_PACKET_LEN   256    /* 11-byte header + 245 payload */
#define SS_BROADCAST_ID     0x0000
#define SS_HUB_ID           0x0000
#define SS_DESK_ID_BASE     0x0100
#define SS_EYETAG_ID_BASE   0x0200
#define SS_LAMP_ID_BASE     0x0300

/* ── Message Types ──────────────────────────────────────────────── */

typedef enum {
    MSG_TYPE_DATA_BLINK     = 0x01,  /* Blink rate from Eye Tag */
    MSG_TYPE_DATA_DISTANCE  = 0x02,  /* Viewing distance from Desk Sentinel */
    MSG_TYPE_DATA_LIGHT     = 0x03,  /* Ambient + blue light from Desk */
    MSG_TYPE_DATA_POSTURE   = 0x04,  /* Head posture from Eye Tag */
    MSG_TYPE_DATA_TEMP      = 0x05,  /* Periocular skin temp from Eye Tag */
    MSG_TYPE_DATA_BLUE_DOSE = 0x06,  /* Cumulative blue-light dose */
    MSG_TYPE_ALERT_FATIGUE  = 0x20,  /* Visual fatigue alert */
    MSG_TYPE_ALERT_DISTANCE = 0x21,  /* Too-close viewing alert */
    MSG_TYPE_ALERT_DRY_EYE  = 0x22,  /* Dry-eye risk alert */
    MSG_TYPE_ALERT_BREAK    = 0x23,  /* 20-20-20 break reminder */
    MSG_TYPE_FORECAST       = 0x30,  /* Myopia forecast from cloud */
    MSG_TYPE_CMD_LAMP       = 0x10,  /* Lamp command (CCT + brightness) */
    MSG_TYPE_CMD_MODE       = 0x11,  /* Mode change (work/rest/child/sleep) */
    MSG_TYPE_CMD_PAIR       = 0x12,  /* Pairing request */
    MSG_TYPE_ACK            = 0x40,
    MSG_TYPE_NACK           = 0x41,
    MSG_TYPE_HEARTBEAT      = 0x50,
    MSG_TYPE_STATUS         = 0x51,
} sightsync_msg_type_t;

/* ── Flags ──────────────────────────────────────────────────────── */

#define SS_FLAG_ENCRYPTED  0x01
#define SS_FLAG_COMPRESSED 0x02
#define SS_FLAG_ACK_REQ    0x04

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
} __attribute__((packed)) sightsync_header_t;

#define SS_HEADER_LEN sizeof(sightsync_header_t)  /* 11 bytes */

/* ── Payload Structures ──────────────────────────────────────────── */

/**
 * Blink rate data payload (from Eye Tag).
 * Sent every 10 seconds.
 */
typedef struct {
    uint8_t  blinks_per_min;     /* computed blink rate (bpm) */
    uint8_t  blink_confidence;   /* 0-100 */
    uint8_t  blink_rate_quality; /* 0=poor,1=fair,2=good */
    uint8_t  blink_ir_amplitude; /* IR reflectance amplitude (0-255) */
    uint32_t timestamp;          /* Unix epoch seconds */
} __attribute__((packed)) payload_blink_t;   /* 8 bytes */

/**
 * Viewing distance payload (from Desk Sentinel).
 * Sent at 1 Hz.
 */
typedef struct {
    uint16_t distance_mm;       /* distance in mm (0 = out of range) */
    uint8_t  distance_quality;  /* 0=invalid,1=low,2=med,3=high */
    uint8_t  near_work_flag;    /* 1 if <300mm sustained >5min */
    uint32_t near_work_minutes; /* cumulative near-work minutes today */
    uint32_t timestamp;
} __attribute__((packed)) payload_distance_t;  /* 12 bytes */

/**
 * Ambient + blue light payload (from Desk Sentinel).
 * Sent every 30 seconds.
 */
typedef struct {
    uint16_t ambient_lux;       /* VEML7700 illuminance (0-65535 lux) */
    uint16_t blue_light_mw;     /* blue-light irradiance (mW/m² × 10) */
    uint16_t cct_estimate;      /* estimated CCT (K) from TCS34725 */
    uint8_t  blue_dose_today;   /* cumulative blue-light dose (0-255) */
    uint8_t  blue_dose_pct;     /* % of daily safe limit */
    uint8_t  ambient_quality;   /* 0=insufficient,1=adequate,2=good */
    uint32_t timestamp;
} __attribute__((packed)) payload_light_t;     /* 12 bytes */

/**
 * Head posture payload (from Eye Tag).
 * Sent at 1 Hz.
 */
typedef struct {
    int16_t  pitch_centi;       /* pitch angle in centi-degrees (1800 = 18.00°) */
    int16_t  roll_centi;        /* roll angle in centi-degrees */
    int16_t  yaw_centi;         /* yaw angle in centi-degrees */
    uint8_t  forward_head_flag; /* 1 if forward head >15° sustained */
    uint8_t  posture_risk;      /* 0-100 (from edge CNN) */
    uint32_t timestamp;
} __attribute__((packed)) payload_posture_t;   /* 14 bytes */

/**
 * Periocular skin temperature payload (from Eye Tag).
 * Sent every 10 seconds.
 */
typedef struct {
    int16_t  temp_centi;        /* temperature in centi-Celsius (3170 = 31.70°C) */
    int16_t  temp_delta_centi;  /* delta from baseline (centi-Celsius) */
    uint32_t timestamp;
} __attribute__((packed)) payload_temp_t;     /* 8 bytes */

/**
 * Cumulative blue-light dose payload.
 * Sent hourly from Eye Tag and Desk Sentinel.
 */
typedef struct {
    uint16_t dose_mj_cm2;       /* cumulative blue-light dose (mJ/cm²) */
    uint8_t  source;            /* 0=eye_tag, 1=desk_sentinel */
    uint8_t  pct_daily_limit;   /* % of daily safe exposure limit */
    uint32_t timestamp;
} __attribute__((packed)) payload_blue_dose_t;  /* 8 bytes */

/**
 * Lamp command payload (Hub → Lamp Node).
 */
typedef struct {
    uint16_t target_cct;        /* target color temperature in K (1800-6500) */
    uint8_t  brightness_pct;    /* target brightness (0-100) */
    uint8_t  mode;               /* 0=auto, 1=manual, 2=circadian, 3=reading */
    uint8_t  transition_sec;     /* transition duration in seconds */
    uint8_t  reserved;
} __attribute__((packed)) payload_lamp_cmd_t;  /* 6 bytes */

/**
 * Visual fatigue alert payload (Hub → nodes/app).
 */
typedef struct {
    uint8_t  fatigue_score;     /* 0-100 */
    uint8_t  alert_level;       /* 0=none,1=low,2=moderate,3=high,4=critical */
    uint8_t  blink_rate;        /* current blink rate (bpm) */
    uint8_t  minutes_since_break; /* minutes since last 20-20-20 break */
    uint16_t viewing_distance_mm;
    uint16_t ambient_lux;
} __attribute__((packed)) payload_fatigue_t;   /* 8 bytes */

/**
 * Distance alert payload.
 */
typedef struct {
    uint16_t distance_mm;
    uint8_t  sustained_minutes; /* minutes sustained at this distance */
    uint8_t  risk_level;        /* 0=ok, 1=watch, 2=warn, 3=critical */
} __attribute__((packed)) payload_dist_alert_t;  /* 4 bytes */

/**
 * Dry-eye risk alert payload.
 */
typedef struct {
    uint8_t  risk_score;       /* 0-100 */
    uint8_t  blink_rate;       /* current bpm */
    int16_t  temp_delta;       /* periocular temp delta (centi-C) */
    uint8_t  recommendation;   /* 0=none, 1=blink exercise, 2=eye drops, 3=break, 4=doctor */
} __attribute__((packed)) payload_dry_eye_t;  /* 6 bytes */

/**
 * Break reminder payload (20-20-20).
 */
typedef struct {
    uint8_t  break_type;      /* 0=20-20-20, 1=long break, 2=look away */
    uint8_t  duration_sec;    /* recommended duration */
    uint8_t  overdue_min;     /* minutes overdue */
} __attribute__((packed)) payload_break_t;   /* 3 bytes */

/**
 * Myopia forecast payload (Cloud → App).
 */
typedef struct {
    uint8_t  risk_30day;      /* 0-100 */
    uint8_t  risk_90day;      /* 0-100 */
    int16_t  refractive_delta;/* projected refractive change (centi-diopter) */
    uint16_t near_work_today; /* near-work minutes today */
    uint16_t outdoor_today;   /* outdoor-light minutes today */
    uint16_t avg_distance_mm; /* average viewing distance today */
    uint8_t  recommendation;  /* 0=none,1=more_outdoor,2=reduce_near,3=checkup */
    uint8_t  reserved;
    uint32_t timestamp;
} __attribute__((packed)) payload_forecast_t;  /* 16 bytes */

/**
 * Mode command payload.
 */
typedef struct {
    uint8_t  mode;           /* 0=work, 1=rest, 2=child, 3=sleep, 4=reading */
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
    uint8_t  node_type;      /* 1=desk, 2=eye_tag, 3=lamp */
    uint8_t  firmware_ver;
    uint8_t  hardware_rev;
} __attribute__((packed)) payload_pair_t;   /* 3 bytes */

/* ── API ─────────────────────────────────────────────────────────── */

uint8_t sightsync_checksum(const uint8_t *data, uint8_t len);

uint8_t sightsync_encode(uint8_t *buf, uint8_t buf_len,
                         sightsync_msg_type_t msg_type,
                         uint16_t sender_id, uint16_t seq_num,
                         uint8_t flags,
                         const uint8_t *payload, uint8_t payload_len);

bool sightsync_decode(const uint8_t *buf, uint8_t buf_len,
                      sightsync_header_t *header,
                      const uint8_t **payload);

bool sightsync_validate(const uint8_t *buf, uint8_t buf_len);

#endif /* SIGHTSYNC_PROTOCOL_H */