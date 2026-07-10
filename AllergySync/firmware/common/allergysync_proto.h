/*
 * AllergySync — Shared Protocol Header
 * Common packet format, message types, and constants for all nodes.
 * Used by ESP32-S3 (ESP-IDF) and nRF52840 (Zephyr) firmware.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef ALLERGYSYNC_PROTO_H
#define ALLERGYSYNC_PROTO_H

#include <stdint.h>
#include <stddef.h>

/* ---- Version ---- */
#define AS_PROTO_VERSION  0x01

/* ---- Network constants ---- */
#define AS_MESH_CHANNEL       0    /* Sub-GHz channel index (868.1 MHz) */
#define AS_MESH_MAX_NODES    16
#define AS_MESH_MAX_HOPS     4
#define AS_MESH_BEACON_SLOT  0
#define AS_MESH_SLOTS        12
#define AS_MESH_SLOT_MS       500  /* 500 ms per TDMA slot */
#define AS_MESH_FRAME_MS     (AS_MESH_SLOTS * AS_MESH_SLOT_MS) /* 6000 ms */
#define AS_AES_KEY_LEN       16
#define AS_ECDH_PUBKEY_LEN   64    /* P-256 raw (X||Y) */

/* ---- Node types ---- */
typedef enum {
    AS_NODE_HUB        = 0x01,
    AS_NODE_SENTINEL   = 0x02,
    AS_NODE_WINDOW     = 0x03,
    AS_NODE_WEARABLE   = 0x04,
} as_node_type_t;

/* ---- Message types ---- */
typedef enum {
    AS_MSG_BEACON       = 0x01,  /* Hub → all: TDMA schedule + timestamp */
    AS_MSG_JOIN_REQ     = 0x02,  /* Node → hub: ECDH pubkey, request slot */
    AS_MSG_JOIN_RSP     = 0x03,  /* Hub → node: slot assignment, session key */
    AS_MSG_TELEMETRY    = 0x04,  /* Node → hub: sensor data */
    AS_MSG_COMMAND      = 0x05,  /* Hub → node: actuator command */
    AS_MSG_ACK          = 0x06,  /* Node → hub: command acknowledgment */
    AS_MSG_HEARTBEAT    = 0x07,  /* Node → hub: I'm alive */
    AS_MSG_OTAA_NOTIFY  = 0x08,  /* Hub → node: OTA available */
    AS_MSG_OTAA_CHUNK   = 0x09,  /* Hub → node: OTA binary chunk */
    AS_MSG_MESH_FWD     = 0x0A,  /* Any → any: mesh forwarding wrapper */
} as_msg_type_t;

/* ---- Telemetry sub-types ---- */
typedef enum {
    AS_TELEM_SENTINEL   = 0x01,  /* PM + pollen + CO2 + VOC + temp/hum */
    AS_TELEM_WINDOW     = 0x02,  /* Window state, light, battery, relay */
    AS_TELEM_WEARABLE   = 0x03,  /* Personal PM + activity + battery */
    AS_TELEM_HUB        = 0x04,  /* Hub status (WiFi, uptime, nodes) */
} as_telem_sub_t;

/* ---- Command sub-types ---- */
typedef enum {
    AS_CMD_CLOSE_WINDOW   = 0x01,
    AS_CMD_OPEN_WINDOW    = 0x02,
    AS_CMD_SET_POSITION   = 0x03,  /* Partial open 0-100% */
    AS_CMD_PURIFIER_ON    = 0x04,
    AS_CMD_PURIFIER_OFF   = 0x05,
    AS_CMD_RECALIBRATE    = 0x06,
    AS_CMD_OTA_BEGIN      = 0x07,
    AS_CMD_OTA_COMMIT     = 0x08,
    AS_CMD_REBOOT         = 0x09,
} as_cmd_sub_t;

/* ---- Pollen classes (PollenNet output) ---- */
typedef enum {
    AS_POLLEN_NONE      = 0,
    AS_POLLEN_BIRCH    = 1,
    AS_POLLEN_GRASS    = 2,
    AS_POLLEN_RAGWEED  = 3,
    AS_POLLEN_OAK      = 4,
    AS_POLLEN_PINE     = 5,
    AS_POLLEN_MOLD     = 6,
    AS_POLLEN_COUNT
} as_pollen_class_t;

/* ---- Activity classes ---- */
typedef enum {
    AS_ACT_STATIC    = 0,
    AS_ACT_WALKING   = 1,
    AS_ACT_RUNNING   = 2,
    AS_ACT_INDOOR    = 3,
    AS_ACT_OUTDOOR   = 4,
    AS_ACT_COMMUTING = 5,
    AS_ACT_COUNT
} as_activity_class_t;

/* ---- Packet header (10 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  version;       /* AS_PROTO_VERSION */
    uint8_t  msg_type;      /* as_msg_type_t */
    uint8_t  src_id;        /* Node ID (0 = hub) */
    uint8_t  dst_id;        /* Node ID (0xFF = broadcast) */
    uint8_t  hop_count;     /* incremented per hop */
    uint8_t  flags;         /* bit0: encrypted, bit1: mesh-fwd */
    uint16_t seq;           /* sequence number */
    uint16_t payload_len;   /* payload length (bytes) */
} as_header_t;

#define AS_HEADER_LEN  10
#define AS_MAX_PAYLOAD 128
#define AS_MAX_PACKET  (AS_HEADER_LEN + AS_MAX_PAYLOAD + 4) /* +4 MAC */
#define AS_MIC_LEN     4   /* AES-CCM-32 truncated MAC */

/* ---- Beacon payload (16 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint32_t unix_time;     /* Hub timestamp for sync */
    uint16_t slot_bitmap;  /* Assigned slots bitmap */
    uint8_t  active_nodes; /* Count of active nodes */
    uint8_t  flags;        /* bit0: OTA pending, bit1: cloud online */
    uint8_t  reserved[8];
} as_beacon_t;

/* ---- Join request payload (72 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  node_type;    /* as_node_type_t */
    uint8_t  hw_version;
    uint8_t  fw_version;
    uint8_t  pubkey[AS_ECDH_PUBKEY_LEN]; /* P-256 public key */
} as_join_req_t;

/* ---- Join response payload (20 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  assigned_id;  /* Node ID assigned by hub */
    uint8_t  slot;         /* TDMA slot index */
    uint8_t  session_key[AS_AES_KEY_LEN]; /* AES-128 key */
} as_join_rsp_t;

/* ---- Sentinel telemetry payload (40 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint16_t pm1_0;        /* µg/m³ × 10 */
    uint16_t pm2_5;        /* µg/m³ × 10 */
    uint16_t pm10;         /* µg/m³ × 10 */
    uint16_t co2_ppm;      /* ppm */
    uint16_t voc_index;    /* BME688 index 0-500 */
    int16_t  temp_c;       /* °C × 100 */
    uint16_t humidity_pct; /* % × 10 */
    uint16_t pressure_hpa; /* hPa */
    uint8_t  pollen_class; /* as_pollen_class_t */
    uint8_t  pollen_conf;  /* confidence 0-100 */
    uint16_t pollen_count; /* particles/L × 1 */
    uint16_t fan_rpm;      /* current fan speed */
    uint8_t  battery_pct;  /* 0-100 (if battery) */
    uint8_t  flags;        /* bit0: sensor_error */
    uint8_t  reserved[16];
} as_telem_sentinel_t;

/* ---- Window telemetry payload (16 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  window_state;  /* 0=closed, 1=open, 2=partial */
    uint8_t  position_pct;  /* open percentage 0-100 */
    uint16_t light_lux;    /* VEML7700 */
    uint16_t battery_mv;   /* battery voltage mV */
    uint8_t  battery_pct;  /* 0-100 */
    uint8_t  relay_state;  /* 0=off, 1=on */
    uint8_t  motor_fault;   /* 0=ok, 1=stall, 2=overcurrent */
    uint8_t  reserved[6];
} as_telem_window_t;

/* ---- Wearable telemetry payload (24 bytes) ---- */
typedef struct __attribute__((packed)) {
    uint16_t pm2_5;        /* personal PM2.5 µg/m³ × 10 */
    uint16_t pm10;         /* personal PM10 µg/m³ × 10 */
    uint8_t  pollen_class; /* as_pollen_class_t */
    uint8_t  activity;     /* as_activity_class_t */
    uint16_t battery_mv;   /* mV */
    uint8_t  battery_pct;  /* 0-100 */
    uint16_t exposure_idx; /* cumulative exposure index */
    uint8_t  reserved[12];
} as_telem_wearable_t;

/* ---- Command payload (8 bytes max) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  cmd_type;     /* as_cmd_sub_t */
    uint8_t  param;        /* command parameter (e.g., position %) */
    uint8_t  reserved[6];
} as_command_t;

/* ---- Functions (implemented per platform) ---- */
uint16_t as_crc16(const uint8_t *data, size_t len);
void as_build_packet(uint8_t *buf, size_t *len, uint8_t msg_type,
                     uint8_t src_id, uint8_t dst_id,
                     const uint8_t *payload, uint16_t payload_len);
int as_parse_packet(const uint8_t *buf, size_t len, as_header_t *hdr,
                    uint8_t *payload, size_t *payload_len);

#endif /* ALLERGYSYNC_PROTO_H */