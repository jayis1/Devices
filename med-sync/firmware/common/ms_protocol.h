/**
 * MedSync - Common Protocol Definitions
 * Shared BLE mesh model definitions, packet structures, and constants
 * Used by all nodes (hub, pill station, room beacon, wearable tag)
 *
 * Copyright (c) 2026 jayis1 - MIT License
 */

#ifndef MS_PROTOCOL_H
#define MS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * System Constants
 * ============================================================ */

#define MS_VERSION_MAJOR        1
#define MS_VERSION_MINOR        0
#define MS_VERSION_PATCH        0

/* BLE mesh network parameters */
#define MS_MESH_NETKEY         0x0100
#define MS_MESH_APPKEY         0x0100
#define MS_MESH_IV_INDEX       0
#define MS_MESH_MAX_NODES      16      /* Max nodes per hub (1 pill + 6 beacons + 1 wearable + spare) */

/* Node types */
typedef enum {
    MS_NODE_HUB            = 0x00,
    MS_NODE_PILL_STATION   = 0x01,
    MS_NODE_ROOM_BEACON    = 0x02,
    MS_NODE_WEARABLE_TAG   = 0x03,
} ms_node_type_t;

/* Node IDs (assigned during provisioning) */
#define MS_NODE_ID_HUB             0x0000
#define MS_NODE_ID_PILL_STATION    0x0001
#define MS_NODE_ID_BEACON_BASE     0x0010
#define MS_NODE_ID_WEARABLE_BASE   0x0020

/* ============================================================
 * Custom BLE Mesh Vendor Models
 * ============================================================ */

/* MedSync company ID (Nordic Semi vendor namespace) */
#define MS_COMPANY_ID         0x0059

/* Model IDs */
#define MS_MODEL_SCHEDULE     0xFC00   /* Medication schedule model */
#define MS_MODEL_VITALS       0xFC01   /* Health vitals model */
#define MS_MODEL_PILL_STATION 0xFC02   /* Pill station control model */
#define MS_MODEL_ALERT        0xFC03   /* Alert model */

/* ============================================================
 * MedSync Schedule Model (0xFC00)
 * ============================================================ */

/* Schedule model opcodes */
#define MS_OP_SCHEDULE_SET         0x00  /* payload: schedule_entry[variable] */
#define MS_OP_SCHEDULE_GET         0x01  /* payload: empty */
#define MS_OP_SCHEDULE_STATUS      0x02  /* payload: schedule_count[1], entries[variable] */
#define MS_OP_DOSE_TRIGGER         0x03  /* payload: bin_id[1], dose_count[1], urgency[1] */
#define MS_OP_DOSE_CONFIRM         0x04  /* payload: bin_id[1], method[1], timestamp[4] */
#define MS_OP_DOSE_MISSED         0x05  /* payload: bin_id[1], timestamp[4] */

/* Schedule entry structure (packed for BLE transmission) */
typedef struct __attribute__((packed)) {
    uint8_t  bin_id;           /* 0-7: which compartment */
    uint8_t  medication_id;    /* Medication database ID */
    uint8_t  dose_count;       /* Number of pills per dose */
    uint8_t  hour;             /* Hour (0-23) */
    uint8_t  minute;           /* Minute (0-59) */
    uint8_t  frequency;        /* Daily/Weekly/As-needed bitmask */
    uint8_t  food_instruction; /* 0=anytime, 1=before food, 2=with food, 3=after food */
    uint16_t pill_weight_mg;  /* Weight per pill in mg (for verification) */
    uint32_t schedule_id;     /* Unique schedule entry ID */
} ms_schedule_entry_t;

/* Dose confirmation methods */
typedef enum {
    MS_CONFIRM_WEIGHT     = 0x00,  /* Weight sensor detected removal */
    MS_CONFIRM_IR         = 0x01,  /* IR beam-break detected */
    MS_CONFIRM_NFC        = 0x02,  /* NFC tap confirmed */
    MS_CONFIRM_BUTTON     = 0x03,  /* Button press confirmed */
    MS_CONFIRM_APP        = 0x04,  /* Mobile app confirmed */
    MS_CONFIRM_COVER      = 0x05,  /* Cover opened/closed */
} ms_confirm_method_t;

/* Dose urgency levels */
typedef enum {
    MS_URGENCY_NORMAL     = 0x00,  /* Scheduled dose, normal reminder */
    MS_URGENCY_ELEVATED   = 0x01,  /* 15 min overdue */
    MS_URGENCY_URGENT     = 0x02,  /* 30+ min overdue */
    MS_URGENCY_EMERGENCY  = 0x03,  /* Critical medication, 2+ hours overdue */
} ms_urgency_t;

/* ============================================================
 * MedSync Vitals Model (0xFC01)
 * ============================================================ */

/* Vitals model attributes */
typedef enum {
    MS_ATTR_HEART_RATE    = 0x0000,  /* uint8: BPM */
    MS_ATTR_SPO2          = 0x0001,  /* uint8: % oxygen saturation */
    MS_ATTR_ACTIVITY      = 0x0002,  /* enum8: activity level */
    MS_ATTR_FALL_DETECTED = 0x0003,  /* boolean: fall state */
    MS_ATTR_STEPS         = 0x0004,  /* uint16: step count today */
    MS_ATTR_SKIN_TEMP     = 0x0005,  /* int16: °C × 100 */
} ms_vitals_attr_t;

/* Activity levels */
typedef enum {
    MS_ACTIVITY_STILL    = 0x00,
    MS_ACTIVITY_WALKING  = 0x01,
    MS_ACTIVITY_RUNNING  = 0x02,
    MS_ACTIVITY_SLEEPING = 0x03,
    MS_ACTIVITY_UNKNOWN  = 0xFF,
} ms_activity_t;

/* Vitals report structure */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint8_t  heart_rate_bpm;      /* Heart rate in BPM */
    uint8_t  spo2_percent;        /* SpO2 in % (0-100) */
    uint8_t  activity_level;      /* ms_activity_t */
    uint8_t  fall_detected;       /* 0=false, 1=true */
    uint16_t steps_count;         /* Total steps today */
    int16_t  skin_temp_cx100;    /* Skin temp in °C × 100 */
    uint16_t battery_mv;          /* Battery voltage in mV */
    uint32_t timestamp;           /* Unix timestamp */
} ms_vitals_report_t;

/* ============================================================
 * MedSync Pill Station Model (0xFC02)
 * ============================================================ */

/* Pill station attributes */
typedef enum {
    MS_ATTR_CAROUSEL_POS  = 0x0000,  /* uint8: current bin 0-7 */
    MS_ATTR_BIN_WEIGHT    = 0x0001,  /* int32: grams × 100 per bin */
    MS_ATTR_BIN_STATUS    = 0x0002,  /* enum8: bin status */
    MS_ATTR_COVER_STATE   = 0x0003,  /* boolean: cover open/closed */
    MS_ATTR_MOTOR_FAULT   = 0x0004,  /* bitmap8: per-motor fault flags */
} ms_pill_attr_t;

/* Bin status enum */
typedef enum {
    MS_BIN_EMPTY          = 0x00,
    MS_BIN_READY           = 0x01,  /* Loaded with medication, waiting for scheduled time */
    MS_BIN_DISPENSING      = 0x02,  /* Currently dispensing */
    MS_BIN_WAITING_PICKUP  = 0x03,  /* Dispensed, waiting for pickup confirmation */
    MS_BIN_CONFIRMED        = 0x04,  /* Dose confirmed taken */
    MS_BIN_OVERDUE          = 0x05,  /* Dose overdue */
    MS_BIN_REFILL_NEEDED   = 0x06,  /* Running low on pills */
    MS_BIN_ERROR            = 0x07,  /* Motor or sensor error */
} ms_bin_status_t;

/* Pill station opcodes */
#define MS_OP_DISPENSE_DOSE     0x00  /* payload: bin_id[1], pill_count[1] */
#define MS_OP_REFILL_BIN        0x01  /* payload: bin_id[1], pill_name[16], pill_weight_mg[2], total_count[1] */
#define MS_OP_CALIBRATE_SCALE   0x02  /* payload: bin_id[1], known_weight_mg[4] */
#define MS_OP_HOME_CAROUSEL     0x03  /* no payload */
#define MS_OP_EMERGENCY_STOP    0x04  /* no payload */

/* Pill station status report */
typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint8_t  carousel_position;    /* Current bin position 0-7 */
    int32_t  bin_weights_mg[8];   /* Per-bin weight in mg */
    uint8_t  bin_status[8];       /* Per-bin status (ms_bin_status_t) */
    uint8_t  cover_open;          /* 0=closed, 1=open */
    uint8_t  motor_fault_flags;   /* Bit per motor (0-7), 1=fault */
    uint16_t battery_mv;          /* Battery voltage in mV */
    uint8_t  supply_5v_ok;        /* 5V supply present */
    uint32_t timestamp;           /* Unix timestamp */
} ms_pill_status_t;

/* ============================================================
 * MedSync Alert Model (0xFC03)
 * ============================================================ */

/* Alert levels */
typedef enum {
    MS_ALERT_INFO        = 0x00,  /* Scheduled dose available */
    MS_ALERT_REMINDER    = 0x01,  /* Dose is due now */
    MS_ALERT_WARNING      = 0x02,  /* Dose overdue, abnormal vitals */
    MS_ALERT_URGENT       = 0x03,  /* Dose 30+ min overdue, SpO2 <92% */
    MS_ALERT_EMERGENCY    = 0x04,  /* Fall detected, SpO2 <88% */
} ms_alert_level_t;

/* Alert types */
typedef enum {
    MS_ALERT_TYPE_DOSE_DUE     = 0x00,
    MS_ALERT_TYPE_DOSE_OVERDUE  = 0x01,
    MS_ALERT_TYPE_DOSE_MISSED   = 0x02,
    MS_ALERT_TYPE_FALL          = 0x03,
    MS_ALERT_TYPE_ADVERSE_EFFECT = 0x04,
    MS_ALERT_TYPE_BATTERY       = 0x05,
    MS_ALERT_TYPE_MOTOR_FAULT   = 0x06,
    MS_ALERT_TYPE_REFILL        = 0x07,
    MS_ALERT_TYPE_SYSTEM        = 0x08,
} ms_alert_type_t;

typedef struct __attribute__((packed)) {
    ms_alert_level_t level;
    ms_alert_type_t  type;
    uint16_t         source_node_id;
    uint8_t          bin_id;            /* Relevant bin (0xFF = N/A) */
    uint8_t          medication_id;     /* Relevant medication (0xFF = N/A) */
    char             message[64];       /* Human-readable alert message */
} ms_alert_t;

/* ============================================================
 * Room Beacon Report
 * ============================================================ */

typedef struct __attribute__((packed)) {
    uint16_t node_id;
    uint8_t  occupancy;          /* 0=unoccupied, 1=occupied (PIR) */
    int16_t  temperature_cx100; /* Room temperature °C × 100 */
    uint16_t humidity_cx10;      /* %RH × 10 */
    uint16_t light_lux;          /* Ambient light in lux */
    uint8_t  sound_level_db;     /* Sound level in dB (A-weighted) */
    uint8_t  reminder_active;    /* 0=no reminder, 1=reminder pending */
    uint16_t battery_mv;          /* Battery voltage in mV */
    uint32_t uptime_sec;          /* Node uptime in seconds */
} ms_beacon_report_t;

/* ============================================================
 * Authentication
 * ============================================================ */

/* 4-byte auth token for pill station commands (prevents accidental BLE spoofing) */
#define MS_AUTH_TOKEN_LEN  4

/* Default auth token (changed during provisioning) */
#define MS_AUTH_TOKEN_DEFAULT  { 0x4D, 0x53, 0x00, 0x01 }

/* ============================================================
 * Timing Constants
 * ============================================================ */

#define MS_REPORT_INTERVAL_SEC      300     /* Normal report interval: 5 minutes */
#define MS_REPORT_INTERVAL_FAST_SEC 10      /* Fast report during active dose: 10 seconds */
#define MS_DOSE_REMINDER_FIRST_SEC  0       /* First reminder at scheduled time */
#define MS_DOSE_REMINDER_5MIN_SEC   300     /* Second reminder at +5 minutes */
#define MS_DOSE_REMINDER_15MIN_SEC  900     /* Escalate at +15 minutes */
#define MS_DOSE_REMINDER_30MIN_SEC  1800    /* Urgent at +30 minutes */
#define MS_DOSE_MISSED_SEC          7200    /* Mark as missed at +2 hours */
#define MS_FALL_VERIFY_SEC          60      /* Verify fall within 60 seconds */
#define MS_PULSE_OX_INTERVAL_SEC    300     /* Pulse oximetry every 5 minutes */
#define MS_VITALS_REPORT_SEC        300     /* Vitals report every 5 minutes */
#define MS_BATTERY_LOW_MV          2400     /* Low battery threshold: 2.4V (coin cell) */
#define MS_BATTERY_CRITICAL_MV     2000     /* Critical battery: 2.0V (coin cell) */

/* ============================================================
 * ML Model Constants (TFLite Micro)
 * ============================================================ */

/* Fall detection model (on wearable) */
#define MS_FALL_MODEL_INPUT_SIZE    1800    /* 3 sec × 6 axes × 100 Hz = 1800 samples */
#define MS_FALL_MODEL_SIZE          45000   /* INT8 quantized model size */
#define MS_FALL_NUM_CLASSES         2       /* fall, not-fall */
#define MS_FALL_THRESHOLD            0.80f   /* Confidence threshold for fall detection */
#define MS_FALL_IMPACT_THRESHOLD_G  2.0f    /* g-force threshold for initial impact */

/* Adherence prediction (on hub, cloud-refined) */
#define MS_ADHERENCE_FEATURES       24      /* 24 features for adherence prediction */
#define MS_ADHERENCE_THRESHOLD_LOW  0.70f   /* Below this = escalate reminders */

/* Pulse oximetry thresholds */
#define MS_SPO2_NORMAL_MIN         95       /* Normal SpO2 ≥ 95% */
#define MS_SPO2_WARNING_MIN        92       /* Warning SpO2 < 92% */
#define MS_SPO2_EMERGENCY_MIN      88       /* Emergency SpO2 < 88% */
#define MS_HR_NORMAL_MIN           50       /* Normal resting HR ≥ 50 BPM */
#define MS_HR_NORMAL_MAX           120      /* Normal resting HR ≤ 120 BPM */
#define MS_HR_BRADY_THRESHOLD     45       /* Bradycardia threshold */
#define MS_HR_TACHY_THRESHOLD     130      /* Tachycardia threshold */

/* ============================================================
 * Pill Station Constants
 * ============================================================ */

#define MS_NUM_BINS             8       /* Number of medication bins */
#define MS_CAROUSEL_STEPS_PER_BIN  512  /* Stepper steps per bin position */
#define MS_MOTOR_MAX_CURRENT_MA   350   /* Max motor current before fault (mA) */
#define MS_WEIGHT_RESOLUTION_MG   100   /* Weight resolution: 0.1mg */
#define MS_IR_DEBOUNCE_MS         50    /* IR beam-break debounce time */
#define MS_DISPENSE_TIMEOUT_SEC   300   /* Maximum time for dose to be taken (5 min) */
#define MS_REFILL_THRESHOLD_PCT   20    /* Alert when bin has <20% pills remaining */

/* ============================================================
 * CRC and Checksum
 * ============================================================ */

uint16_t ms_crc16(const uint8_t *data, uint16_t len);

/* BLE mesh uses AES-128-CCM (handled by nRF52 hardware) */

#endif /* MS_PROTOCOL_H */