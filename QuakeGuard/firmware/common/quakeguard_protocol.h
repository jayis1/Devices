/*
 * quakeguard_protocol.h — Shared Sub-GHz protocol for QuakeGuard nodes
 *
 * All nodes (Hub, Floor, Shutoff, Structural) use this shared framing,
 * message types, and CRC for 868 MHz CC1101 communication.
 *
 * License: MIT
 */
#ifndef QUAKEGUARD_PROTOCOL_H
#define QUAKEGUARD_PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ── Network Constants ───────────────────────────────────────── */
#define QG_PREAMBLE_LEN      4       /* preamble bytes (0xAA)      */
#define QG_SYNC_WORD         0x2DD4  /* CC1101 sync word          */
#define QG_MAX_PAYLOAD       128     /* max payload bytes          */
#define QG_CRC_LEN           2       /* CRC16 at end of frame      */
#define QG_MAX_FRAME         (4 + 2 + 1 + 3 + QG_MAX_PAYLOAD + 2)

/* ── Node Addresses ──────────────────────────────────────────── */
#define QG_ADDR_HUB          0x00
#define QG_ADDR_FLOOR_BASE   0x10    /* Floor nodes 0x10–0x1F       */
#define QG_ADDR_SHUTOFF      0x20
#define QG_ADDR_STRUCT_BASE  0x30    /* Structural tags 0x30–0x3F  */
#define QG_ADDR_BROADCAST     0xFF

/* ── Message Types ───────────────────────────────────────────── */
typedef enum {
    MSG_HEARTBEAT        = 0x01,  /* Node→Hub: status every 60 s  */
    MSG_SEISMIC_CANDIDATE= 0x02,  /* Floor→Hub: 2 s waveform       */
    MSG_SEISMIC_CONFIRMED= 0x03,  /* Hub→All: event broadcast      */
    MSG_SHUTOFF_NOW      = 0x04,  /* Hub→Shutoff: close valves      */
    MSG_SHUTOFF_ACK      = 0x05,  /* Shutoff→Hub: valve states      */
    MSG_STRUCT_POLL      = 0x06,  /* Hub→StructTag: request report  */
    MSG_STRUCT_REPORT    = 0x07,  /* StructTag→Hub: strain + vib    */
    MSG_VALVE_TEST       = 0x08,  /* Hub→Shutoff: monthly test     */
    MSG_TEST_RESULT      = 0x09,  /* Shutoff→Hub: test results      */
    MSG_FAMILY_CHECKIN   = 0x0A,  /* Hub→Cloud: dispatch check-in   */
    MSG_FAMILY_RESPONSE  = 0x0B,  /* App→Cloud→Hub: safe/help       */
    MSG_CONFIG_UPDATE    = 0x0C,  /* Hub→Node: param update         */
    MSG_FIRMWARE_OTA     = 0x0D,  /* Hub→Node: firmware chunk       */
    MSG_CALIBRATION      = 0x0E,  /* Hub→Node: zero-offset + scale  */
} qg_msg_type_t;

/* ── Action Flags (for SHUTOFF_NOW) ──────────────────────────── */
#define QG_ACT_GAS_VALVE    (1 << 0)
#define QG_ACT_WATER_VALVE  (1 << 1)
#define QG_ACT_RELAY_1      (1 << 2)  /* elevator drop              */
#define QG_ACT_RELAY_2      (1 << 3)  /* awning retract             */
#define QG_ACT_RELAY_3      (1 << 4)  /* power cut                  */
#define QG_ACT_RELAY_4      (1 << 5)  /* gas appliance cutoff       */
#define QG_ACT_ALL          0x3F

/* ── Seismic Event Severity ──────────────────────────────────── */
typedef enum {
    SEV_NONE     = 0,
    SEV_MINOR     = 1,   /* P-wave only, no S-wave within 10 s  */
    SEV_MODERATE  = 2,   /* S-wave detected, low magnitude       */
    SEV_MAJOR      = 3,   /* S-wave detected, moderate magnitude  */
    SEV_SEVERE     = 4,   /* S-wave detected, high magnitude       */
} qg_severity_t;

/* ── Packet Header ────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;
    uint8_t  src_addr;
    uint8_t  dst_addr;
    uint8_t  seq_num;     /* rolling sequence for ACK/dedup      */
} qg_header_t;

/* ── Full Frame ───────────────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t     preamble[QG_PREAMBLE_LEN];
    uint16_t    sync;             /* 0x2DD4 (big-endian on wire)   */
    uint8_t     payload_len;      /* header + payload (not CRC)    */
    qg_header_t header;
    uint8_t     payload[QG_MAX_PAYLOAD];
    uint16_t    crc16;            /* CRC16-CCITT over payload_len  */
} qg_frame_t;

/* ── Payload Structures ───────────────────────────────────────── */

/* HEARTBEAT payload (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  battery_pct;     /* 0–100                        */
    int16_t  temperature_c;   /* °C × 10 (e.g., 235 = 23.5°C) */
    uint8_t  status_flags;    /* bit 0: online, bit 1: fault   */
    uint8_t  uptime_hours;    /* hours since boot             */
    uint16_t rssi_db;         /* Sub-GHz RSSI (0xFFFF=N/A)    */
} heartbeat_payload_t;

/* SEISMIC_CANDIDATE payload (4 bytes header + up to 124 bytes waveform)
 * Waveform: 3-axis ADXL355 at 1000 Hz, 2 s = 6000 samples × 2 bytes = 12 KB
 * Compressed: delta-encoded + RLE → typically 40–80 bytes
 * For multi-packet: chunk_id (1B) + total_chunks (1B) + data (122B)
 */
typedef struct __attribute__((packed)) {
    uint8_t  chunk_id;
    uint8_t  total_chunks;
    uint8_t  axis_flags;    /* bit 0: X, bit 1: Y, bit 2: Z   */
    uint8_t  sample_rate_khz; /* 1 = 1000 Hz                    */
    uint8_t  data[120];     /* compressed waveform chunk       */
} seismic_payload_t;

/* SEISMIC_CONFIRMED payload (16 bytes) */
typedef struct __attribute__((packed)) {
    uint32_t timestamp_utc;   /* Unix epoch                     */
    uint8_t  severity;        /* qg_severity_t                  */
    uint8_t  magnitude_x10;   /* Mw × 10 (e.g., 52 = M5.2)     */
    uint16_t epicenter_dist_km; /* estimated distance             */
    uint8_t  actions_taken;   /* QG_ACT_* bitmask                */
    uint8_t  node_count;      /* how many floor nodes detected  */
    uint8_t  reserved[5];
} seismic_confirmed_payload_t;

/* SHUTOFF_NOW payload (4 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  action_flags;    /* QG_ACT_* bitmask                */
    uint8_t  urgency;         /* 0=test, 1=normal, 2=immediate  */
    uint16_t event_id;        /* matches SEISMIC_CONFIRMED      */
} shutoff_now_payload_t;

/* SHUTOFF_ACK payload (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  gas_valve_closed;   /* 0=open, 1=closed            */
    uint8_t  water_valve_closed; /* 0=open, 1=closed            */
    uint16_t h2_ppm;             /* MQ-8 H2 reading             */
    uint16_t ch4_ppm;            /* MQ-4 CH4 reading            */
    int16_t  temperature_c;      /* °C × 10                    */
    uint8_t  relay_states;      /* bit per relay               */
} shutoff_ack_payload_t;

/* STRUCT_POLL payload (2 bytes) */
typedef struct __attribute__((packed)) {
    uint16_t event_id;     /* event to assess               */
} struct_poll_payload_t;

/* STRUCT_REPORT payload (20 bytes) */
typedef struct __attribute__((packed)) {
    int32_t  strain_max_micro;   /* μStrain (max since last report) */
    int32_t  strain_mean_micro;  /* μStrain (mean over 5 min)      */
    int16_t  resonance_shift_hz;  /* Hz shift from baseline         */
    int16_t  peak_accel_mg;      /* mg = milli-g                  */
    int16_t  temperature_c10;    /* °C × 10                       */
    uint8_t  battery_pct;
    uint8_t  anomaly_score;      /* 0–255 (autoencoder output)    */
    uint8_t  fault_flags;        /* bit 0: sensor, bit 1: bridge  */
    uint8_t  reserved[3];
} struct_report_payload_t;

/* CALIBRATION payload (12 bytes) */
typedef struct __attribute__((packed)) {
    int32_t accel_offset_x;
    int32_t accel_offset_y;
    int32_t accel_offset_z;
} calibration_payload_t;

/* ── CRC16-CCITT ──────────────────────────────────────────────── */
static inline uint16_t qg_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x8408;  /* reversed poly */
            else
                crc >>= 1;
        }
    }
    return crc;
}

/* ── Frame Builder ───────────────────────────────────────────── */
static inline size_t qg_build_frame(qg_frame_t *frame,
                                     uint8_t msg_type,
                                     uint8_t src_addr,
                                     uint8_t dst_addr,
                                     uint8_t seq_num,
                                     const uint8_t *payload,
                                     uint8_t payload_len)
{
    memset(frame, 0xAA, QG_PREAMBLE_LEN);
    frame->sync = QG_SYNC_WORD;
    frame->payload_len = sizeof(qg_header_t) + payload_len;

    frame->header.msg_type = msg_type;
    frame->header.src_addr = src_addr;
    frame->header.dst_addr = dst_addr;
    frame->header.seq_num  = seq_num;

    if (payload_len > 0 && payload)
        memcpy(frame->payload, payload, payload_len);

    frame->crc16 = qg_crc16((uint8_t *)&frame->payload_len,
                            1 + sizeof(qg_header_t) + payload_len);

    return QG_PREAMBLE_LEN + 2 + 1 + 1 + sizeof(qg_header_t)
           + payload_len + QG_CRC_LEN;
}

/* ── Frame Parser (returns 0 on success, -1 on error) ────────── */
static inline int qg_parse_frame(const uint8_t *raw, size_t raw_len,
                                  qg_frame_t *out)
{
    if (raw_len < QG_PREAMBLE_LEN + 2 + 1 + sizeof(qg_header_t) + QG_CRC_LEN)
        return -1;

    /* Skip preamble (find sync) */
    size_t off = 0;
    while (off < QG_PREAMBLE_LEN && raw[off] == 0xAA) off++;
    if (off == QG_PREAMBLE_LEN) off = 0; /* CC1101 may strip preamble */

    memcpy(out, raw, raw_len < sizeof(qg_frame_t) ? raw_len : sizeof(qg_frame_t));

    /* Verify CRC */
    uint16_t expected = qg_crc16((uint8_t *)&out->payload_len,
                                 1 + sizeof(qg_header_t) + out->payload_len
                                 - sizeof(qg_header_t));
    /* Note: payload_len includes header size */
    uint16_t actual = out->crc16;
    if (expected != actual)
        return -2;

    return 0;
}

#endif /* QUAKEGUARD_PROTOCOL_H */