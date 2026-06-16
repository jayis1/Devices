/**
 * FreshKeep — Common Mesh Protocol Definitions
 * Shared between all nodes (hub, fridge, pantry, stove guard)
 * 
 * Protocol: Custom TDMA over LoRa 868MHz
 * Frame: 500ms (5 slots × 100ms)
 * Packet: PREAMBLE(4) | SYNC(2) | LEN(1) | SRC(1) | DST(1) | TYPE(1) | PAYLOAD(0-50) | CRC16(2)
 */

#ifndef FRESHKEEP_PROTOCOL_H
#define FRESHKEEP_PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ── Node Addresses ──────────────────────────────────────────────── */
#define ADDR_BROADCAST   0xFF
#define ADDR_HUB         0x00
#define ADDR_FRIDGE      0x01
#define ADDR_PANTRY       0x02
#define ADDR_STOVE_GUARD 0x03

/* ── Packet Types ────────────────────────────────────────────────── */
#define PKT_FRIDGE_DATA      0x01
#define PKT_PANTRY_DATA      0x02
#define PKT_STOVE_DATA       0x03
#define PKT_FIRE_ALARM       0x04
#define PKT_COMMAND          0x05
#define PKT_ACK              0x06
#define PKT_OTA_BLOCK        0x07
#define PKT_INVENTORY_UPDATE 0x08
#define PKT_HEARTBEAT        0x09
#define PKT_SHOPPING_LIST    0x0A

/* ── Command IDs (for PKT_COMMAND payload[0]) ────────────────────── */
#define CMD_GAS_SHUTOFF      0x01
#define CMD_SUPPRESSION_ON   0x02
#define CMD_SUPPRESSION_OFF  0x03
#define CMD_PHOTO_TRIGGER    0x04
#define CMD_BARCODE_SCAN     0x05
#define CMD_CALIBRATE_WEIGHT 0x06
#define CMD_SET_ALERT_THRESH 0x07
#define CMD_OTA_START        0x08
#define CMD_RESET            0x09

/* ── Alert Levels ────────────────────────────────────────────────── */
#define ALERT_INFO      0
#define ALERT_WARNING   1
#define ALERT_URGENT    2
#define ALERT_CRITICAL  3
#define ALERT_EMERGENCY 4

/* ── TDMA Slot Assignments ──────────────────────────────────────── */
#define SLOT_STOVE_GUARD  0  /* Always first — fire safety priority */
#define SLOT_FRIDGE       1
#define SLOT_PANTRY       2
#define SLOT_HUB_CMD      3
#define SLOT_CTRL_ACK     4

#define SLOT_DURATION_MS  100
#define FRAME_DURATION_MS 500

/* ── Packet Structure ────────────────────────────────────────────── */
#define PKT_PREAMBLE_LEN 4
#define PKT_SYNC_LEN     2
#define PKT_HEADER_LEN   6   /* LEN + SRC + DST + TYPE + ... */
#define PKT_CRC_LEN      2
#define PKT_MAX_PAYLOAD  50
#define PKT_MAX_SIZE     (PKT_PREAMBLE_LEN + PKT_SYNC_LEN + 1 + PKT_HEADER_LEN + PKT_MAX_PAYLOAD + PKT_CRC_LEN)

#define SYNC_WORD_0 0xFK
#define SYNC_WORD_1 0x4P

/* ── Fridge Data Payload (28 bytes) ──────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t voc_index;        /* SGP40 VOC index (0-500) */
    uint16_t co2_ppm;         /* SCD30 CO2 ppm */
    uint16_t ethylene_raw;    /* MQ-3 ADC raw value */
    int16_t  temp_c_x10;      /* Temperature ×10 (e.g. 38 = 3.8°C) */
    uint16_t humidity_x10;    /* Humidity ×10% */
    uint32_t weight_mg[4];    /* Weight per shelf in milligrams */
    uint8_t  door_state;      /* 0=closed, 1=open */
    uint8_t  light_lux_x10;   /* Ambient light ×10 */
    uint8_t  spoilage_score;   /* 0-100 (0=fresh, 100=spoiled) */
    uint8_t  image_ready;      /* 0=no, 1=new image available */
    uint8_t  battery_pct;      /* Battery percentage */
} fridge_data_t;

/* ── Pantry Data Payload (30 bytes) ──────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t temp_c_x10;      /* Temperature ×10 */
    uint16_t humidity_x10;    /* Humidity ×10% */
    uint32_t weight_mg[6];    /* Weight per shelf in milligrams */
    uint8_t  door_state;      /* 0=closed, 1=open */
    uint8_t  barcode_ready;   /* 0=no, 1=barcode scanned */
    uint8_t  image_ready;     /* 0=no, 1=new image available */
    uint8_t  items_count;     /* Current item count (estimated) */
    uint8_t  battery_pct;     /* Battery percentage */
} pantry_data_t;

/* ── Stove Guard Data Payload (24 bytes) ──────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t max_temp_c;       /* Max pixel temp from thermal array */
    uint16_t avg_temp_c;       /* Average temp of hot zone */
    uint16_t lpg_ppm;          /* MQ-2 LPG reading */
    uint16_t co_ppm;           /* MQ-135 CO reading */
    uint16_t nh3_ppm;          /* MQ-137 ammonia reading */
    uint8_t  smoke_level;      /* 0-255 smoke density */
    uint8_t  flame_detected;  /* 0=no, 1=flame IR detected */
    uint8_t  burner_state;    /* 0=off, 1=low, 2=medium, 3=high heat */
    uint8_t  motion_detected; /* 0=no person, 1=person nearby */
    uint8_t  gas_valve_state; /* 0=closed, 1=open */
    uint8_t  fire_confidence; /* 0-255 ML fire confidence */
    uint8_t  alert_level;     /* ALERT_INFO..ALERT_EMERGENCY */
    uint16_t thermal_checksum;/* CRC of thermal frame for verification */
} stove_data_t;

/* ── Fire Alarm Payload (10 bytes) ───────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t max_temp_c;
    uint16_t lpg_ppm;
    uint8_t  smoke_level;
    uint8_t  flame_detected;
    uint8_t  fire_confidence;
    uint8_t  source_node;
    uint16_t timestamp_ms;
} fire_alarm_t;

/* ── Command Payload (variable) ───────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_id;
    uint8_t  param_len;
    uint8_t  params[8];       /* Command-specific parameters */
} command_t;

/* ── Inventory Update Payload (variable) ─────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  action;          /* 0=added, 1=removed, 2=expired, 3=consumed */
    uint8_t  location;        /* 0=fridge, 1=pantry */
    uint32_t barcode;         /* UPC barcode (0 if no barcode) */
    uint8_t  name_len;
    char     name[16];        /* Product name (truncated) */
    uint16_t weight_mg;       /* Current weight */
    uint8_t  expiry_days;     /* Days until expiry */
    uint8_t  category;        /* 0=produce, 1=dairy, 2=meat, 3=grain, 4=condiment, 5=other */
} inventory_update_t;

/* ── Heartbeat Payload (4 bytes) ──────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  node_id;
    uint8_t  battery_pct;
    uint16_t uptime_min;
} heartbeat_t;

/* ── CRC16 (CCITT) ───────────────────────────────────────────────── */
static inline uint16_t crc16_ccitt(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ── Packet Builder ──────────────────────────────────────────────── */
typedef struct {
    uint8_t  data[PKT_MAX_SIZE];
    uint8_t  len;
} packet_t;

static inline void pkt_init(packet_t *pkt, uint8_t src, uint8_t dst, uint8_t type) {
    memset(pkt, 0, sizeof(packet_t));
    uint8_t *d = pkt->data;
    /* Preamble */
    d[0] = 0xAA; d[1] = 0xAA; d[2] = 0xAA; d[3] = 0xAA;
    /* Sync word */
    d[4] = 0xFK; d[5] = 0x4P;
    /* Header */
    d[6] = 0;      /* LEN — filled later */
    d[7] = src;
    d[8] = dst;
    d[9] = type;
    pkt->len = 10;  /* Preamble(4) + Sync(2) + LEN(1) + SRC(1) + DST(1) + TYPE(1) */
}

static inline void pkt_add_payload(packet_t *pkt, const uint8_t *payload, uint8_t payload_len) {
    if (payload_len > PKT_MAX_PAYLOAD) payload_len = PKT_MAX_PAYLOAD;
    memcpy(&pkt->data[pkt->len], payload, payload_len);
    pkt->len += payload_len;
}

static inline uint16_t pkt_finalize(packet_t *pkt) {
    /* Set length field */
    pkt->data[6] = pkt->len - 6; /* Length from LEN field to end of payload */
    /* Append CRC16 */
    uint16_t crc = crc16_ccitt(&pkt->data[6], pkt->len - 6);
    pkt->data[pkt->len] = crc & 0xFF;
    pkt->data[pkt->len + 1] = (crc >> 8) & 0xFF;
    pkt->len += 2;
    return pkt->len;
}

/* ── Packet Parser ────────────────────────────────────────────────── */
typedef struct {
    uint8_t src;
    uint8_t dst;
    uint8_t type;
    uint8_t payload_len;
    const uint8_t *payload;
    uint16_t crc_received;
    uint16_t crc_computed;
    int valid;
} pkt_parse_t;

static inline int pkt_parse(const uint8_t *buf, uint16_t buf_len, pkt_parse_t *out) {
    memset(out, 0, sizeof(pkt_parse_t));
    
    /* Find preamble */
    uint16_t idx = 0;
    while (idx < buf_len - 1) {
        if (buf[idx] == 0xAA && buf[idx+1] == 0xAA) break;
        idx++;
    }
    if (idx >= buf_len - 10) return -1; /* Too short */
    
    /* Check sync word */
    if (buf[idx+4] != 0xF0 || buf[idx+5] != 0x4F) return -2; /* Use actual sync bytes */
    
    uint8_t len = buf[idx+6];
    out->src = buf[idx+7];
    out->dst = buf[idx+8];
    out->type = buf[idx+9];
    out->payload_len = len - 4; /* LEN includes SRC+DST+TYPE+PAYLOAD */
    out->payload = &buf[idx+10];
    
    /* CRC check */
    out->crc_received = buf[idx+10+out->payload_len] | (buf[idx+10+out->payload_len+1] << 8);
    out->crc_computed = crc16_ccitt(&buf[idx+6], len);
    out->valid = (out->crc_received == out->crc_computed);
    
    return out->valid ? 0 : -3;
}

#endif /* FRESHKEEP_PROTOCOL_H */