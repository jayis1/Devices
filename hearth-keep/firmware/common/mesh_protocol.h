/**
 * HearthKeep Mesh Protocol - Common Header
 * 
 * Sub-GHz LoRa mesh protocol for elder safety monitoring system.
 * Hub is coordinator, room monitors / bed mat / wearable tags are clients.
 * 
 * TDMA frame: 18 slots × 50ms = 900ms cycle
 * Slot 0: Hub sync/commands
 * Slots 1-8: Room monitors 1-8
 * Slot 9: Bed mat
 * Slots 10-15: Room monitors 9-14
 * Slot 16: Reserved/expansion
 * Slot 17: Control/ACK/alert override
 */

#ifndef HK_MESH_PROTOCOL_H
#define HK_MESH_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

/* --- Node Types --- */
typedef enum {
    HK_NODE_HUB          = 0x00,
    HK_NODE_ROOM_MONITOR = 0x01,
    HK_NODE_BED_MAT      = 0x02,
    HK_NODE_WEARABLE_TAG = 0x03,
} hk_node_type_t;

/* --- Packet Types --- */
typedef enum {
    HK_TYPE_RADAR_DATA   = 0x01,  /* Room monitor radar + presence data */
    HK_TYPE_ENV_DATA     = 0x02,  /* Environment sensor data */
    HK_TYPE_BED_VITALS   = 0x03,  /* Bed mat heart/breath/movement */
    HK_TYPE_COMMAND      = 0x04,  /* Hub -> node configuration */
    HK_TYPE_ACK          = 0x05,  /* Acknowledgment */
    HK_TYPE_OTA_BLOCK    = 0x06,  /* Firmware update chunk */
    HK_TYPE_FALL_ALERT   = 0x07,  /* CRITICAL - fall detected, bypasses TDMA */
    HK_TYPE_PANIC_ALERT  = 0x08,  /* Panic button pressed */
    HK_TYPE_HEARTBEAT    = 0x09,  /* Periodic alive signal */
    HK_TYPE_CALIBRATION  = 0x0A,  /* Calibration data */
    HK_TYPE_LOW_BATTERY  = 0x0B,  /* Battery warning */
} hk_packet_type_t;

/* --- Command Subtypes --- */
typedef enum {
    HK_CMD_SET_SENSITIVITY  = 0x01,  /* Set radar sensitivity */
    HK_CMD_SET_INTERVAL      = 0x02,  /* Set reporting interval */
    HK_CMD_START_CALIBRATE   = 0x03,  /* Start calibration sequence */
    HK_CMD_START_OTA          = 0x04,  /* Start OTA update */
    HK_CMD_RESET              = 0x05,  /* Reset node */
    HK_CMD_SET_SLOT           = 0x06,  /* Assign TDMA slot */
    HK_CMD_DISABLE_ALERTS     = 0x07,  /* Temporarily disable alerts */
    HK_CMD_ENABLE_ALERTS      = 0x08,  /* Re-enable alerts */
    HK_CMD_REQUEST_STATUS     = 0x09,  /* Request full status report */
} hk_cmd_subtype_t;

/* --- Alert Levels --- */
typedef enum {
    HK_ALERT_NONE       = 0x00,
    HK_ALERT_INFO        = 0x01,  /* Informational */
    HK_ALERT_WARNING     = 0x02,  /* Warning - routine change */
    HK_ALERT_URGENT      = 0x03,  /* Urgent - possible fall */
    HK_ALERT_EMERGENCY   = 0x04,  /* Emergency - confirmed fall */
} hk_alert_level_t;

/* --- Radar Position Classes --- */
typedef enum {
    HK_POS_UNKNOWN    = 0x00,
    HK_POS_STANDING   = 0x01,
    HK_POS_SITTING    = 0x02,
    HK_POS_LYING      = 0x03,
    HK_POS_FALLING    = 0x04,  /* In the process of falling */
    HK_POS_FALLEN     = 0x05,  /* On the ground after fall */
    HK_POS_ABSENT     = 0x06,  /* No one present */
} hk_position_class_t;

/* --- Sleep Phases --- */
typedef enum {
    HK_SLEEP_ABSENT    = 0x00,
    HK_SLEEP_AWAKE     = 0x01,
    HK_SLEEP_LIGHT     = 0x02,
    HK_SLEEP_DEEP      = 0x03,
    HK_SLEEP_REM       = 0x04,
} hk_sleep_phase_t;

/* --- Packet Structure --- */
#define HK_PREAMBLE_LEN    4
#define HK_SYNC_WORD       0x484B  /* "HK" in ASCII */
#define HK_MAX_PAYLOAD     48
#define HK_HEADER_LEN      8
#define HK_CRC_LEN         2
#define HK_MAX_PACKET_LEN  (HK_HEADER_LEN + HK_MAX_PAYLOAD + HK_CRC_LEN)

typedef struct __attribute__((packed)) {
    uint8_t  preamble[HK_PREAMBLE_LEN];  /* 0x48, 0x4B, 0x48, 0x4B */
    uint16_t sync_word;                   /* 0x4B48 */
    uint8_t  length;                       /* Total packet length */
    uint8_t  src_id;                       /* Source node ID */
    uint8_t  dst_id;                       /* Destination node ID (0xFF = broadcast) */
    uint8_t  type;                         /* hk_packet_type_t */
    uint16_t seq_num;                       /* Sequence number */
    uint8_t  payload[HK_MAX_PAYLOAD];     /* Type-specific payload */
    uint16_t crc16;                        /* CRC-16/CCITT */
} hk_packet_t;

/* --- Radar Data Payload (HK_TYPE_RADAR_DATA) --- */
typedef struct __attribute__((packed)) {
    uint8_t  presence_count;    /* Number of people detected (0-4) */
    uint8_t  position_class;    /* hk_position_class_t - primary person */
    float    fall_probability;   /* 0.0-1.0, >0.85 = fall alert */
    float    movement_index;    /* 0.0-1.0, overall movement level */
    float    distance_m;        /* Distance to primary person (meters) */
    float    velocity_ms;       /* Velocity of primary person (m/s) */
    uint16_t radar_timestamp;   /* Milliseconds since last frame */
    uint8_t  confidence;        /* 0-100% detection confidence */
} hk_radar_data_t;

/* --- Environment Data Payload (HK_TYPE_ENV_DATA) --- */
typedef struct __attribute__((packed)) {
    float    temperature_c;     /* Temperature in Celsius */
    float    humidity_pct;      /* Relative humidity % */
    float    pressure_hpa;       /* Atmospheric pressure hPa */
    float    iaq_index;          /* Indoor Air Quality index 0-500 */
    float    light_lux;          /* Ambient light in lux */
    uint8_t  room_id;            /* Room identifier */
    uint8_t  occupancy;          /* Number of people detected (0-4) */
} hk_env_data_t;

/* --- Bed Vitals Payload (HK_TYPE_BED_VITALS) --- */
typedef struct __attribute__((packed)) {
    float    heart_rate_bpm;     /* Heart rate in BPM */
    float    breathing_rate;     /* Breaths per minute */
    float    movement_index;     /* 0.0-1.0, overall movement */
    uint8_t  in_bed;             /* 0 = empty, 1 = occupied */
    uint8_t  sleep_phase;        /* hk_sleep_phase_t */
    float    hr_confidence;      /* Heart rate confidence 0-1 */
    float    br_confidence;      /* Breathing rate confidence 0-1 */
    float    mattress_temp_c;    /* Mattress temperature */
    uint16_t sample_count;      /* Number of samples in this report */
} hk_bed_vitals_t;

/* --- Command Payload (HK_TYPE_COMMAND) --- */
typedef struct __attribute__((packed)) {
    uint8_t  cmd;               /* hk_cmd_subtype_t */
    uint8_t  param_len;          /* Length of parameters */
    uint8_t  params[16];        /* Command parameters */
} hk_command_t;

/* --- Fall Alert Payload (HK_TYPE_FALL_ALERT) --- */
typedef struct __attribute__((packed)) {
    uint8_t  room_id;            /* Room where fall detected */
    uint8_t  position_class;    /* hk_position_class_t at time of fall */
    float    fall_probability;   /* Fall confidence score */
    float    impact_velocity;   /* Estimated impact velocity m/s */
    uint32_t timestamp;         /* UTC timestamp of fall detection */
    uint8_t  verification_attempts; /* Times system has tried to verify */
    uint8_t  verified;          /* 0 = unverified, 1 = confirmed by radar */
} hk_fall_alert_t;

/* --- Panic Alert Payload (HK_TYPE_PANIC_ALERT) --- */
typedef struct __attribute__((packed)) {
    uint8_t  tag_id;             /* Wearable tag identifier */
    uint8_t  battery_pct;        /* Battery percentage */
    uint32_t timestamp;         /* UTC timestamp */
    uint8_t  button_hold_time;  /* Seconds button was held (0 = quick press) */
} hk_panic_alert_t;

/* --- Heartbeat Payload (HK_TYPE_HEARTBEAT) --- */
typedef struct __attribute__((packed)) {
    uint8_t  node_type;          /* hk_node_type_t */
    uint8_t  battery_pct;       /* Battery percentage */
    uint8_t  signal_rssi;       /* Signal strength (negative, -128 to 0) */
    uint16_t uptime_min;        /* Uptime in minutes */
    uint8_t  fault_flags;       /* Bit field of any faults */
    uint8_t  firmware_version;  /* Firmware version byte */
} hk_heartbeat_t;

/* --- Calibration Payload (HK_TYPE_CALIBRATION) --- */
typedef struct __attribute__((packed)) {
    uint8_t  cal_type;           /* 0x01 = radar, 0x02 = pressure, 0x03 = env */
    float    params[8];          /* Calibration parameters (node-type-specific) */
} hk_calibration_t;

/* --- Low Battery Payload (HK_TYPE_LOW_BATTERY) --- */
typedef struct __attribute__((packed)) {
    uint8_t  node_id;            /* Node reporting low battery */
    uint8_t  battery_pct;       /* Current battery percentage */
    uint16_t estimated_hours;   /* Estimated hours remaining */
} hk_low_battery_t;

/* --- Node IDs --- */
#define HK_NODE_ID_BROADCAST    0xFF
#define HK_NODE_ID_HUB          0x00
#define HK_MAX_ROOM_MONITORS    14
#define HK_MAX_NODES            16

/* --- TDMA Timing --- */
#define HK_SLOT_DURATION_MS     50
#define HK_SLOTS_PER_FRAME      18
#define HK_FRAME_DURATION_MS    900
#define HK_SLOT_HUB             0
#define HK_SLOT_BED_MAT         9
#define HK_SLOT_CONTROL         17

/* --- Radio Configuration --- */
#define HK_RADIO_FREQUENCY_EU   868.0f
#define HK_RADIO_FREQUENCY_US   915.0f
#define HK_RADIO_BW             125.0f
#define HK_RADIO_SF_NORMAL      7
#define HK_RADIO_SF_LONGRANGE   9
#define HK_RADIO_TX_POWER_EU    14
#define HK_RADIO_TX_POWER_US    20

/* --- Alert Thresholds --- */
#define HK_FALL_PROBABILITY_THRESHOLD    0.85f
#define HK_FALL_CONFIRM_FRAMES           3
#define HK_FALL_CONFIRM_TIME_MS          300
#define HK_HEART_RATE_LOW                40.0f
#define HK_HEART_RATE_HIGH              150.0f
#define HK_BREATHING_RATE_LOW             8.0f
#define HK_BREATHING_RATE_HIGH           30.0f
#define HK_BATTERY_LOW_PCT               15
#define HK_BATTERY_CRITICAL_PCT           5

/* --- CRC-16/CCITT --- */
uint16_t hk_crc16(const uint8_t *data, uint16_t len);

/* --- Packet Construction --- */
void hk_packet_init(hk_packet_t *pkt, uint8_t src_id, uint8_t dst_id, hk_packet_type_t type);
uint16_t hk_packet_serialize(const hk_packet_t *pkt, uint8_t *buf, uint16_t buf_len);
bool hk_packet_deserialize(const uint8_t *buf, uint16_t len, hk_packet_t *pkt);
bool hk_packet_validate(const hk_packet_t *pkt);

/* --- Slot Assignment --- */
uint8_t hk_slot_for_node(uint8_t node_id);
uint8_t hk_node_for_slot(uint8_t slot);

/* --- Debug --- */
const char* hk_type_str(hk_packet_type_t type);
const char* hk_position_str(hk_position_class_t pos);
const char* hk_alert_str(hk_alert_level_t level);

#endif /* HK_MESH_PROTOCOL_H */