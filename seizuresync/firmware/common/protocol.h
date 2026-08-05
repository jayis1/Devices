/*
 * SeizureSync — Shared Protocol Definitions
 * Defines the TDMA mesh protocol, packet types, and wire format
 * used by all nodes (Hub, Band, AuraPatch, CaregiverBeacon).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SEIZURESYNC_PROTOCOL_H
#define SEIZURESYNC_PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ---- Radio / mesh constants ---- */
#define SZ_RADIO_FREQ_HZ       868000000UL
#define SZ_RADIO_BW_HZ         125000UL
#define SZ_RADIO_SF             7
#define SZ_RADIO_CR             5   /* 4/5 */
#define SZ_RADIO_TX_DBM         14
#define SZ_RADIO_MAX_PKT        255

/* ---- TDMA ---- */
#define SZ_TDMA_SLOTS          8       /* 8 slots per superframe */
#define SZ_TDMA_SLOT_MS        125     /* 125 ms per slot */
#define SZ_TDMA_SUPERFRAME_MS  (SZ_TDMA_SLOTS * SZ_TDMA_SLOT_MS) /* 1000 ms */

/* Fixed slot assignments */
#define SZ_SLOT_HUB            0
#define SZ_SLOT_BAND            1
#define SZ_SLOT_PATCH           2
#define SZ_SLOT_BEACON          3
/* Slots 4-7 dynamic for additional nodes */

/* ---- AES-128 ---- */
#define SZ_AES_KEY_LEN         16
#define SZ_AES_IV_LEN          16

/* ---- Network ---- */
#define SZ_NET_ID_LEN          6

/* ---- Packet types ---- */
typedef enum : uint8_t {
    SZ_PKT_BEACON          = 0x01,  /* TDMA sync beacon from hub */
    SZ_PKT_JOIN            = 0x02,  /* Node joins mesh */
    SZ_PKT_JOIN_ACK        = 0x03,  /* Hub assigns slot */
    SZ_PKT_HEARTBEAT       = 0x04,  /* Periodic health */
    SZ_PKT_SEIZURE_ALERT   = 0x10,  /* Seizure detected — primary alert */
    SZ_PKT_AURA_ALERT      = 0x11,  /* Pre-ictal prodrome warning */
    SZ_PKT_SUDEP_ALERT     = 0x12,  /* SUDEP nocturnal critical alert */
    SZ_PKT_ACK             = 0x20,  /* Caregiver acknowledge */
    SZ_PKT_DISPATCH        = 0x21,  /* Dispatch 911 */
    SZ_PKT_SIGNAL_CHUNK    = 0x30,  /* Raw physiological signal chunk */
    SZ_PKT_CONFIG          = 0x40,  /* Configuration update */
    SZ_PKT_OTA_MODEL       = 0x50,  /* ML model OTA chunk */
    SZ_PKT_TEST            = 0x60,  /* Self-test command/response */
} sz_pkt_type_t;

/* ---- Seizure classification (ILAE 2017) ---- */
typedef enum : uint8_t {
    SZ_SEMI_UNKNOWN        = 0,
    SZ_SEMI_FOCAL_AWARE    = 1,   /* focal aware / simple partial */
    SZ_SEMI_FOCAL_IMPAIRED = 2,   /* focal impaired-awareness / complex partial */
    SZ_SEMI_FBTCS         = 3,   /* focal-to-bilateral tonic-clonic */
    SZ_SEMI_GENERALIZED   = 4,   /* generalized tonic-clonic */
    SZ_SEMI_MYOLONIC      = 5,
    SZ_SEMI_ATONIC        = 6,
    SZ_SEMI_ABSENCE       = 7,
} sz_semiology_t;

/* ---- Alert severity ---- */
typedef enum : uint8_t {
    SZ_SEV_INFO    = 0,   /* green */
    SZ_SEV_AURA    = 1,   /* yellow — pre-ictal prodrome */
    SZ_SEV_SEIZURE = 2,   /* red — active seizure */
    SZ_SEV_SUDEP   = 3,   /* flashing red — SUDEP critical */
    SZ_SEV_RECOVERY= 4,   /* blue — post-ictal recovery */
} sz_severity_t;

/* ---- Packet header (all packets) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  net_id[SZ_NET_ID_LEN];     /* 6-byte network ID */
    uint8_t  src_node;                   /* source node ID */
    uint8_t  dst_node;                   /* 0xFF = broadcast */
    uint8_t  type;                        /* sz_pkt_type_t */
    uint8_t  seq;                         /* sequence number */
    uint16_t crc;                         /* CRC-16 over payload */
} sz_header_t;

#define SZ_HEADER_LEN  sizeof(sz_header_t)   /* 12 */

/* ---- Payload: Seizure alert ---- */
typedef struct __attribute__((packed)) {
    uint32_t onset_unix;                 /* seizure onset time (UTC) */
    uint8_t  semiology;                  /* sz_semiology_t */
    uint8_t  severity;                   /* sz_severity_t */
    uint16_t duration_s;                 /* duration in seconds (0=ongoing) */
    uint8_t  confidence;                 /* 0-100 % */
    uint8_t  recovery_state;             /* 0=active 1=postictal 2=recovered */
} sz_seizure_payload_t;

/* ---- Payload: Aura (pre-ictal) alert ---- */
typedef struct __attribute__((packed)) {
    uint32_t predicted_unix;             /* predicted seizure time */
    uint16_t lead_time_s;                /* estimated lead time */
    uint8_t  probability;                /* 0-100 % */
} sz_aura_payload_t;

/* ---- Payload: SUDEP alert ---- */
typedef struct __attribute__((packed)) {
    uint8_t  apnea_state;                /* 0=normal 1=mild 2=mod 3=sev 4=crit */
    uint16_t apnea_duration_s;
    uint8_t  prone_flag;                 /* 1=prone (face-down) */
    uint8_t  spo2_pct;                   /* SpO2 in % */
    uint8_t  hr_bpm;                      /* heart rate */
} sz_sudep_payload_t;

/* ---- Payload: Heartbeat ---- */
typedef struct __attribute__((packed)) {
    uint8_t  battery_pct;
    int8_t   rssi_dbm;
    uint8_t  status_flags;               /* bit0: worn, bit1: charging, bit2: error */
    uint16_t free_heap_kb;
} sz_heartbeat_payload_t;

/* ---- Payload: Ack / Dispatch ---- */
typedef struct __attribute__((packed)) {
    uint32_t event_unix;
    uint8_t  action;                     /* 0=ack 1=dispatch911 */
} sz_ack_payload_t;

/* ---- Helper: CRC-16-CCITT ---- */
static inline uint16_t sz_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else              crc <<= 1;
        }
    }
    return crc;
}

/* ---- Helper: pack header + payload into buffer ---- */
static inline size_t sz_pack(uint8_t *buf, const sz_header_t *h,
                             const uint8_t *payload, size_t plen)
{
    memcpy(buf, h, SZ_HEADER_LEN);
    if (plen && payload) memcpy(buf + SZ_HEADER_LEN, payload, plen);
    /* CRC over payload */
    uint16_t crc = sz_crc16(payload ? payload : (const uint8_t *)"", plen);
    buf[SZ_HEADER_LEN + plen]     = crc & 0xFF;
    buf[SZ_HEADER_LEN + plen + 1] = (crc >> 8) & 0xFF;
    return SZ_HEADER_LEN + plen + 2;
}

#endif /* SEIZURESYNC_PROTOCOL_H */