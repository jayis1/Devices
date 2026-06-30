/**
 * AsthmaSync — Shared Protocol Definitions
 * ========================================
 * Common packet structure for all AsthmaSync nodes.
 * Used over Sub-GHz mesh (Hub ↔ Air Sentinel) and
 * BLE GATT (Hub ↔ Inhaler Tag / Wheeze Band).
 *
 * License: MIT
 */

#ifndef ASTHMASYNC_PROTOCOL_H
#define ASTHMASYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ── Versioning ─────────────────────────────────────────── */
#define PROTO_VERSION_MAJOR  1
#define PROTO_VERSION_MINOR  0
#define PROTO_VERSION        ((PROTO_VERSION_MAJOR << 4) | PROTO_VERSION_MINOR)

/* ── Node Types ─────────────────────────────────────────── */
typedef enum : uint8_t {
    NODE_TYPE_HUB           = 0x01,
    NODE_TYPE_AIR_SENTINEL  = 0x02,
    NODE_TYPE_INHALER_TAG   = 0x03,
    NODE_TYPE_WHEEZE_BAND   = 0x04,
} node_type_t;

/* ── Message Types ──────────────────────────────────────── */
typedef enum : uint8_t {
    MSG_TYPE_JOIN_REQ       = 0x01,   /* node → hub: join mesh */
    MSG_TYPE_JOIN_ACK       = 0x02,   /* hub → node: assigned slot */
    MSG_TYPE_TELEMETRY      = 0x10,   /* node → hub: sensor data */
    MSG_TYPE_EVENT          = 0x11,   /* node → hub: discrete event */
    MSG_TYPE_ALERT          = 0x12,   /* hub → node: alert notification */
    MSG_TYPE_CONFIG         = 0x20,   /* hub → node: config update */
    MSG_TYPE_ACK            = 0x30,   /* reliable delivery ack */
    MSG_TYPE_PING           = 0x40,
    MSG_TYPE_PONG           = 0x41,
    MSG_TYPE_TIME_SYNC      = 0x50,   /* hub → node: epoch time */
} msg_type_t;

/* ── Telemetry Sub-types (in payload[0]) ───────────────── */
typedef enum : uint8_t {
    TLV_AIR_QUALITY   = 0x01,   /* pm25, pm10, voc, co2, temp, rh */
    TLV_VITALS        = 0x02,   /* hr, hrv, spo2, skin_temp */
    TLV_AUDIO_FEATURE = 0x03,   /* mel-spectrogram slice or wheeze prob */
    TLV_ACTUATION     = 0x04,   /* inhaler actuation event */
    TLV_BATTERY       = 0x05,   /* battery voltage + level % */
} tlv_type_t;

/* ── Event Sub-types ────────────────────────────────────── */
typedef enum : uint8_t {
    EVT_WHEEZE_DETECTED   = 0x01,
    EVT_ACTUATION         = 0x02,
    EVT_SPO2_LOW          = 0x03,   /* SpO2 < 92% */
    EVT_HRV_DROP          = 0x04,   /* rmSSD < baseline - 20% */
    EVT_PM25_HIGH         = 0x05,   /* PM2.5 > 35 µg/m³ */
    EVT_VOC_HIGH          = 0x06,   /* VOC index > 400 */
    EVT_FALL              = 0x07,
    EVT_BUTTON_SOS        = 0x08,
} event_type_t;

/* ── Alert Zones (GINA-aligned) ─────────────────────────── */
typedef enum : uint8_t {
    ZONE_GREEN   = 0,   /* well-controlled */
    ZONE_YELLOW  = 1,   /* partly controlled — step up if sustained */
    ZONE_RED     = 2,   /* uncontrolled — seek medical help */
} alert_zone_t;

/* ── Packet Header (10 bytes) ───────────────────────────── */
#define PKT_MAGIC  0xA5

typedef struct __attribute__((packed)) {
    uint8_t  magic;        /* 0xA5 */
    uint8_t  version;      /* PROTO_VERSION */
    uint8_t  src_type;     /* node_type_t */
    uint16_t src_id;       /* node unique ID (last 2 bytes of MAC) */
    uint8_t  msg_type;     /* msg_type_t */
    uint8_t  seq;          /* sequence number (wraps) */
    uint16_t payload_len;  /* payload length in bytes */
    uint16_t crc;          /* CRC-16/CCITT over everything except this field */
} pkt_header_t;

#define PKT_HEADER_SIZE  sizeof(pkt_header_t)   /* 10 bytes */
#define PKT_MAX_PAYLOAD  128
#define PKT_MAX_SIZE     (PKT_HEADER_SIZE + PKT_MAX_PAYLOAD)

/* ── Payload Structures ─────────────────────────────────── */

/* Air quality telemetry (TLV_AIR_QUALITY) — 16 bytes */
typedef struct __attribute__((packed)) {
    uint16_t pm1_0;      /* µg/m³ × 10 */
    uint16_t pm2_5;      /* µg/m³ × 10 */
    uint16_t pm10;       /* µg/m³ × 10 */
    uint16_t voc_index;  /* 0-500 (BME688 IAQ) */
    uint16_t hcho_ppb;   /* formaldehyde ppb (SGP40 VOC-equiv) */
    uint16_t co2_ppm;    /* SCD41 CO₂ */
    int16_t  temp_c_x10; /* °C × 10 */
    uint16_t rh_x10;     /* %RH × 10 */
} air_quality_t;

/* Vitals telemetry (TLV_VITALS) — 12 bytes */
typedef struct __attribute__((packed)) {
    uint8_t  hr;            /* heart rate bpm */
    uint8_t  spo2;          /* % SpO₂ */
    uint16_t hrv_rmssd_x10; /* rmSSD × 10 (ms) */
    int16_t  skin_temp_x10; /* °C × 10 (TMP117) */
    uint8_t  activity;      /* 0=rest,1=light,2=moderate,3=high */
    uint8_t  reserved[5];   /* future: respiration rate, etc. */
} vitals_t;

/* Audio feature telemetry (TLV_AUDIO_FEATURE) — 48 bytes */
/* 16 × uint16_t mel-spectrogram bins (compressed to 8-bit each = 32 bytes)
   plus 8 bytes metadata: wheeze_prob, snr, timestamp_offset */
typedef struct __attribute__((packed)) {
    uint8_t  mel_bins[32];    /* 32 log-mel bins (0-255) */
    uint8_t  wheeze_prob;     /* on-device pre-classifier probability 0-100 */
    uint8_t  snr_db;          /* signal-to-noise ratio */
    uint16_t window_ms;       /* analysis window length */
    uint16_t reserved;
} audio_feature_t;

/* Actuation event payload (TLV_ACTUATION) — 8 bytes */
typedef struct __attribute__((packed)) {
    uint8_t  actuation_type;  /* 0=MDI, 1=DPI, 2=unknown */
    uint8_t  confidence;      /* classifier confidence 0-100 */
    int16_t  peak_accel_x1000;/* peak acceleration in g × 1000 */
    uint16_t duration_ms;     /* event duration */
    uint8_t  battery_pct;
    uint8_t  reserved;
} actuation_t;

/* Event payload (MSG_TYPE_EVENT) — 8 bytes */
typedef struct __attribute__((packed)) {
    uint8_t  event_type;      /* event_type_t */
    uint8_t  severity;        /* 0=info,1=warning,2=critical */
    uint8_t  zone;            /* alert_zone_t (if applicable) */
    uint8_t  reserved;
    uint32_t timestamp;       /* epoch seconds (from time sync) */
} event_payload_t;

/* ── Function Prototypes ────────────────────────────────── */

/** Pack a packet: fills header, computes CRC, returns total size. */
size_t proto_pack(pkt_header_t *hdr, const uint8_t *payload,
                  uint16_t payload_len, uint8_t *out_buf, size_t buf_size);

/** Unpack a packet: validates magic + CRC, returns 0 on success. */
int proto_unpack(const uint8_t *buf, size_t buf_len,
                 pkt_header_t *out_hdr, uint8_t *out_payload,
                 uint16_t *out_payload_len);

/** Compute CRC-16/CCITT. */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

#endif /* ASTHMASYNC_PROTOCOL_H */