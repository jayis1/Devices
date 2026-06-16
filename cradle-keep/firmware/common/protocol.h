/**
 * CradleKeep — Common Mesh Protocol Definitions
 * Shared between all nodes (hub, crib pad, nursery monitor, feeding station)
 * 
 * Protocol: Custom TDMA over LoRa 868MHz
 * Frame: 500ms (5 slots × 100ms)
 * Packet: PREAMBLE(4) | SYNC(2) | LEN(1) | SRC(1) | DST(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2)
 */

#ifndef CRADLEKEEP_PROTOCOL_H
#define CRADLEKEEP_PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ── Node Addresses ──────────────────────────────────────────────── */
#define ADDR_BROADCAST       0xFF
#define ADDR_HUB             0x00
#define ADDR_CRIB_PAD        0x01
#define ADDR_NURSERY_MONITOR 0x02
#define ADDR_FEEDING_STATION 0x03

/* ── Packet Types ────────────────────────────────────────────────── */
#define PKT_CRIB_DATA        0x01
#define PKT_NURSERY_DATA     0x02
#define PKT_FEEDING_DATA     0x03
#define PKT_CRY_EVENT       0x04
#define PKT_BREATHING_ALERT  0x05
#define PKT_COMMAND          0x06
#define PKT_ACK              0x07
#define PKT_OTA_BLOCK        0x08
#define PKT_SLEEP_STAGE      0x09
#define PKT_ENV_EVENT        0x0A
#define PKT_HEARTBEAT        0x0B

/* ── Command IDs (for PKT_COMMAND payload[0]) ────────────────────── */
#define CMD_START_WARMING    0x01
#define CMD_STOP_WARMING     0x02
#define CMD_DISPENSE_FORMULA 0x03
#define CMD_PLAY_SOUND       0x04
#define CMD_STOP_SOUND       0x05
#define CMD_SET_TEMP_TARGET  0x06
#define CMD_CAPTURE_IMAGE    0x07
#define CMD_SET_ALERT_THRESH 0x08
#define CMD_OTA_START        0x09
#define CMD_RESET            0x0A
#define CMD_CALIBRATE_SCALE  0x0B

/* ── Alert Levels ────────────────────────────────────────────────── */
#define ALERT_INFO      0
#define ALERT_WARNING   1
#define ALERT_URGENT    2
#define ALERT_CRITICAL  3
#define ALERT_EMERGENCY 4

/* ── Cry Types ────────────────────────────────────────────────────── */
#define CRY_NONE        0
#define CRY_HUNGRY      1
#define CRY_TIRED       2
#define CRY_PAIN        3
#define CRY_COLIC       4
#define CRY_DISCOMFORT  5

/* ── Sleep Stages ─────────────────────────────────────────────────── */
#define SLEEP_AWAKE     0
#define SLEEP_LIGHT     1
#define SLEEP_DEEP      2
#define SLEEP_REM       3

/* ── Position Types ───────────────────────────────────────────────── */
#define POS_UNKNOWN     0
#define POS_SUPINE      1  /* On back (safest) */
#define POS_PRONE       2  /* On stomach (alert) */
#define POS_LEFT_SIDE   3
#define POS_RIGHT_SIDE  4
#define POS_SEMI_UPRIGHT 5

/* ── Feeding States ───────────────────────────────────────────────── */
#define FEED_IDLE       0
#define FEED_WARMING    1
#define FEED_READY      2
#define FEED_IN_PROGRESS 3
#define FEED_DONE       4

/* ── Sound Types (for CMD_PLAY_SOUND) ────────────────────────────── */
#define SOUND_WHITE_NOISE  0x01
#define SOUND_LULLABY_1    0x02
#define SOUND_LULLABY_2    0x03
#define SOUND_HEARTBEAT    0x04
#define SOUND_RAIN          0x05
#define SOUND_OCEAN         0x06
#define SOUND_SHUSH         0x07

/* ── TDMA Slot Assignments ──────────────────────────────────────── */
#define SLOT_CRIB_PAD        0  /* Always first — breathing safety priority */
#define SLOT_NURSERY_MONITOR 1
#define SLOT_FEEDING_STATION 2
#define SLOT_HUB_CMD         3
#define SLOT_CTRL_ACK        4

#define SLOT_DURATION_MS  100
#define FRAME_DURATION_MS 500

/* ── Packet Structure ────────────────────────────────────────────── */
#define PKT_PREAMBLE_LEN 4
#define PKT_SYNC_LEN     2
#define PKT_HEADER_LEN   6   /* LEN + SRC + DST + TYPE + ... */
#define PKT_CRC_LEN      2
#define PKT_MAX_PAYLOAD  50
#define PKT_MAX_SIZE     (PKT_PREAMBLE_LEN + PKT_SYNC_LEN + 1 + PKT_HEADER_LEN + PKT_MAX_PAYLOAD + PKT_CRC_LEN)

#define SYNC_WORD_0 0xCK
#define SYNC_WORD_1 0x4P

/* ── Crib Pad Data Payload (24 bytes) ─────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  breath_rate;         /* Breaths per minute (0-60) */
    uint8_t  breath_regularity;   /* Regularity index 0-100 (100=perfectly regular) */
    uint8_t  movement_score;      /* Movement intensity 0-255 */
    uint8_t  position;           /* Position enum (POS_*) */
    int16_t  temp_c_x10;        /* Mattress temp ×10 (e.g., 345 = 34.5°C) */
    uint8_t  wetness_flag;       /* 0=dry, 1=wet detected */
    uint8_t  wetness_level;      /* 0-255 conductivity level */
    uint16_t breath_apnea_count; /* Number of apnea events (>5s pause) in last minute */
    uint16_t movement_epochs;   /* Movement count in last 2 seconds */
    uint8_t  alert_level;       /* Current alert level */
    uint8_t  battery_pct;       /* Battery percentage */
    uint8_t  signal_strength;   /* RSSI from hub */
    uint8_t  fsr1_raw_h;        /* FSR1 peak value high byte */
    uint8_t  fsr1_raw_l;        /* FSR1 peak value low byte */
    uint8_t  fsr2_raw_h;        /* FSR2 peak value high byte */
    uint8_t  fsr2_raw_l;        /* FSR2 peak value low byte */
    uint8_t  fsr3_raw_h;        /* FSR3 peak value high byte */
    uint8_t  fsr3_raw_l;        /* FSR3 peak value low byte */
    uint8_t  fsr4_raw_h;        /* FSR4 peak value high byte */
    uint8_t  fsr4_raw_l;        /* FSR4 peak value low byte */
    uint8_t  reserved[3];       /* Reserved for future use */
} crib_data_t;  /* 24 bytes */

/* ── Nursery Monitor Data Payload (30 bytes) ──────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  cry_type;            /* Current cry classification (CRY_*) */
    uint8_t  cry_confidence;     /* Confidence 0-255 */
    uint8_t  cry_intensity;      /* Sound intensity 0-255 */
    int16_t  room_temp_c_x10;   /* Room temp ×10 °C */
    uint16_t room_humidity_x10; /* Humidity ×10 % */
    uint16_t co2_ppm;           /* CO2 ppm (400-5000) */
    uint16_t voc_index;          /* VOC index 0-500 */
    uint16_t light_lux;          /* Ambient light in lux */
    uint8_t  noise_level_db;     /* Background noise level (dB) */
    uint8_t  ir_active;          /* IR LEDs on (0/1) */
    uint8_t  camera_ready;        /* Camera module ready */
    uint8_t  baby_present;        /* Baby detected in crib */
    uint8_t  alert_level;        /* Current alert level */
    uint8_t  battery_pct;        /* Battery percentage */
    uint8_t  signal_strength;    /* RSSI from hub */
    uint8_t  sound_type_playing; /* Currently playing sound type (0=none) */
    uint16_t sound_duration_s;   /* Duration of current sound in seconds */
    uint8_t  reserved[10];      /* Reserved for future use */
} nursery_data_t;  /* 30 bytes */

/* ── Feeding Station Data Payload (20 bytes) ───────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  feeding_state;      /* FEED_IDLE, FEED_WARMING, etc. */
    int16_t  bottle_temp_c_x10; /* Bottle temp ×10 °C */
    int16_t  target_temp_c_x10; /* Target temp ×10 °C */
    uint16_t weight_mg;          /* Current weight in mg */
    uint16_t start_weight_mg;    /* Weight at feeding start */
    uint16_t volume_consumed_ml; /* Calculated ml consumed */
    uint16_t feeding_duration_s; /* Seconds since feeding start */
    uint8_t  heater_pct;         /* Heater PWM duty cycle % */
    uint8_t  uv_turbidity;       /* Milk freshness indicator 0-255 */
    uint8_t  battery_pct;        /* Battery percentage */
    uint8_t  signal_strength;    /* RSSI from hub */
    uint8_t  scale_calibrated;   /* 0=uncalibrated, 1=calibrated */
    uint8_t  reserved[2];        /* Reserved for future use */
} feeding_data_t;  /* 20 bytes */

/* ── Cry Event Payload (12 bytes) ──────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  cry_type;           /* CRY_* classification */
    uint8_t  cry_confidence;     /* 0-255 */
    uint8_t  cry_intensity;      /* 0-255 */
    uint8_t  duration_s;         /* Duration of cry event */
    uint8_t  preceding_sleep;    /* Sleep stage before cry */
    uint16_t time_since_feed_m;  /* Minutes since last feeding */
    uint16_t time_since_sleep_m; /* Minutes since last sleep */
    uint32_t timestamp_ms;      /* Hub-relative timestamp */
} cry_event_t;  /* 12 bytes */

/* ── Breathing Alert Payload (10 bytes) ────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  alert_level;        /* ALERT_* level */
    uint8_t  breath_rate;        /* Current breath rate */
    uint16_t apnea_duration_ms;  /* Duration of apnea event */
    uint16_t time_since_breath;  /* ms since last detected breath */
    uint8_t  position;           /* Current position */
    uint8_t  movement_score;     /* Movement intensity */
    uint8_t  source_node;        /* Which node detected */
    uint16_t timestamp_ms;      /* Hub-relative timestamp */
} breathing_alert_t;  /* 10 bytes */

/* ── Command Packet Payload (10 bytes) ─────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;             /* CMD_* command */
    uint8_t  target_node;        /* Which node to execute */
    int16_t  param1;             /* Command parameter 1 */
    int16_t  param2;             /* Command parameter 2 */
    uint16_t param3;             /* Command parameter 3 */
    uint8_t  reserved[2];       /* Reserved */
} command_payload_t;  /* 10 bytes */

/* ── Sleep Stage Update Payload (8 bytes) ─────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  sleep_stage;        /* SLEEP_* stage */
    uint8_t  confidence;          /* Classification confidence 0-255 */
    uint8_t  breath_rate;        /* Current breath rate */
    uint8_t  movement_score;     /* Movement intensity */
    uint16_t duration_s;         /* Duration in this stage */
    uint16_t timestamp_ms;      /* Hub-relative timestamp */
} sleep_stage_t;  /* 8 bytes */

/* ── Environment Event Payload (8 bytes) ──────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  event_type;         /* 0=temp_high, 1=temp_low, 2=humidity_high, 3=co2_high, 4=noise_high, 5=light_high, 6=wetness */
    uint8_t  severity;           /* 0-4 (info to emergency) */
    int16_t  value;              /* Event value (temp, ppm, dB, etc.) */
    int16_t  threshold;          /* Threshold that was crossed */
    uint16_t timestamp_ms;      /* Hub-relative timestamp */
} env_event_t;  /* 8 bytes */

/* ── CRC16-CCITT ──────────────────────────────────────────────────── */
static inline uint16_t crc16_ccitt(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* ── Packet Encode/Decode Helpers ─────────────────────────────────── */
typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t type;
    uint8_t payload_len;
    uint8_t payload[PKT_MAX_PAYLOAD];
} packet_t;

static inline int packet_encode(const packet_t *pkt, uint8_t *buf, uint16_t *len) {
    uint16_t idx = 0;
    
    /* Preamble */
    buf[idx++] = 0xAA; buf[idx++] = 0xAA; buf[idx++] = 0xAA; buf[idx++] = 0xAA;
    
    /* Sync word */
    buf[idx++] = 0x0C; buf[idx++] = 0x4B;  /* "CK" */
    
    /* Length */
    uint8_t payload_and_header_len = 3 + pkt->payload_len; /* SRC + DST + TYPE + PAYLOAD */
    buf[idx++] = payload_and_header_len;
    
    /* Header */
    buf[idx++] = pkt->src;
    buf[idx++] = pkt->dst;
    buf[idx++] = pkt->type;
    
    /* Payload */
    memcpy(&buf[idx], pkt->payload, pkt->payload_len);
    idx += pkt->payload_len;
    
    /* CRC16 over LEN + header + payload */
    uint16_t crc = crc16_ccitt(&buf[PKT_PREAMBLE_LEN + PKT_SYNC_LEN], idx - PKT_PREAMBLE_LEN - PKT_SYNC_LEN);
    buf[idx++] = crc & 0xFF;
    buf[idx++] = (crc >> 8) & 0xFF;
    
    *len = idx;
    return 0;
}

static inline int packet_decode(const uint8_t *buf, uint16_t buf_len, packet_t *pkt) {
    if (buf_len < PKT_PREAMBLE_LEN + PKT_SYNC_LEN + 1 + 3 + PKT_CRC_LEN) {
        return -1;  /* Too short */
    }
    
    /* Check preamble */
    if (buf[0] != 0xAA || buf[1] != 0xAA || buf[2] != 0xAA || buf[3] != 0xAA) {
        return -2;  /* Bad preamble */
    }
    
    /* Check sync word */
    if (buf[4] != 0x0C || buf[5] != 0x4B) {
        return -3;  /* Bad sync */
    }
    
    /* Length */
    uint8_t len_field = buf[6];
    if (len_field < 3) {
        return -4;  /* Too short payload */
    }
    
    /* Decode */
    pkt->src = buf[7];
    pkt->dst = buf[8];
    pkt->type = buf[9];
    pkt->payload_len = len_field - 3;
    
    /* Verify CRC */
    uint16_t computed_crc = crc16_ccitt(&buf[6], 3 + pkt->payload_len);
    uint16_t received_crc = buf[6 + 3 + pkt->payload_len] | (buf[6 + 3 + pkt->payload_len + 1] << 8);
    if (computed_crc != received_crc) {
        return -5;  /* CRC mismatch */
    }
    
    /* Copy payload */
    memcpy(pkt->payload, &buf[10], pkt->payload_len);
    
    return 0;
}

#endif /* CRADLEKEEP_PROTOCOL_H */