/*
 * cardiosync_protocol.h — Shared BLE GATT + protocol definitions for CardioSync nodes
 *
 * All nodes (Hub, ECG Patch, BP Cuff, Smart Ring) use these shared
 * GATT UUIDs, message types, and payload structures for BLE 5.0.
 *
 * License: MIT
 */
#ifndef CARDIOSYNC_PROTOCOL_H
#define CARDIOSYNC_PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ── BLE GATT Service UUID ────────────────────────────────────── */
/* CardioSync Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E */
#define CS_SERVICE_UUID_BASE   {0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, \
                                 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E}
#define CS_SERVICE_UUID        0x0001
#define CS_CHAR_ECG_DATA        0x0002  /* Patch→Hub: ECG samples         */
#define CS_CHAR_ECG_HR          0x0003  /* Patch→Hub: HR + R-R intervals  */
#define CS_CHAR_BP_RESULT        0x0004  /* Cuff→Hub: BP measurement       */
#define CS_CHAR_BP_COMMAND       0x0005  /* Hub→Cuff: BP trigger           */
#define CS_CHAR_PPG_HR           0x0006  /* Ring→Hub: HR + SpO₂ + temp     */
#define CS_CHAR_PPG_HRV          0x0007  /* Ring→Hub: HRV metrics          */
#define CS_CHAR_ACTIVITY         0x0008  /* Ring/Patch→Hub: activity       */
#define CS_CHAR_ALERT            0x0009  /* Hub→All: alert broadcast       */
#define CS_CHAR_HEARTBEAT        0x000A  /* Node→Hub: heartbeat / status   */

/* ── Node Addresses (for internal routing) ───────────────────── */
#define CS_ADDR_HUB             0x00
#define CS_ADDR_ECG_PATCH       0x01
#define CS_ADDR_BP_CUFF         0x02
#define CS_ADDR_SMART_RING      0x03
#define CS_ADDR_BROADCAST       0xFF

/* ── Message Types ────────────────────────────────────────────── */
typedef enum {
    MSG_HEARTBEAT        = 0x01,  /* Node→Hub: status every 30 s     */
    MSG_ECG_DATA         = 0x02,  /* Patch→Hub: 250 Hz ECG packets   */
    MSG_ECG_HR           = 0x03,  /* Patch→Hub: HR + R-R intervals   */
    MSG_BP_RESULT        = 0x04,  /* Cuff→Hub: BP measurement        */
    MSG_BP_COMMAND       = 0x05,  /* Hub→Cuff: BP trigger            */
    MSG_PPG_HR           = 0x06,  /* Ring→Hub: HR + SpO₂             */
    MSG_PPG_HRV          = 0x07,  /* Ring→Hub: HRV metrics           */
    MSG_ACTIVITY         = 0x08,  /* Ring/Patch→Hub: activity         */
    MSG_ALERT            = 0x09,  /* Hub→All: alert broadcast        */
    MSG_EMERGENCY        = 0x0A,  /* Hub→Cloud: emergency dispatch   */
    MSG_CONFIG_UPDATE    = 0x0B,  /* Hub→Node: param update          */
    MSG_FIRMWARE_OTA     = 0x0C,  /* Hub→Node: firmware chunk        */
    MSG_CALIBRATION      = 0x0D,  /* Hub→Node: calibration data      */
} cs_msg_type_t;

/* ── Alert Types ──────────────────────────────────────────────── */
typedef enum {
    ALERT_NONE          = 0x00,
    ALERT_AFIB          = 0x01,  /* Atrial fibrillation detected    */
    ALERT_PVC           = 0x02,  /* Premature ventricular contraction */
    ALERT_VT            = 0x03,  /* Ventricular tachycardia (emergency) */
    ALERT_BRADYCARDIA   = 0x04,  /* HR < 30 bpm for > 15 s          */
    ALERT_TACHYCARDIA   = 0x05,  /* HR > 180 bpm sustained          */
    ALERT_HYPERTENSION  = 0x06,  /* BP > 180/120 mmHg              */
    ALERT_HYPOTENSION   = 0x07,  /* BP < 90/60 mmHg                */
    ALERT_LOW_SPO2      = 0x08,  /* SpO₂ < 88% for > 30 s          */
    ALERT_LOW_BATTERY   = 0x09,  /* Node battery < 10%             */
    ALERT_DISCONNECT    = 0x0A,  /* ECG patch disconnected         */
} cs_alert_type_t;

/* ── Alert Severity ──────────────────────────────────────────── */
typedef enum {
    SEV_INFO     = 0,
    SEV_WARNING  = 1,  /* Yellow LED, haptic, push notification */
    SEV_URGENT   = 2,  /* Red LED, haptic, push + SMS            */
    SEV_EMERGENCY= 3,  /* Red LED, siren, push + SMS + call      */
} cs_severity_t;

/* ── ECG Packet (20 bytes payload) ───────────────────────────── */
/* 10 samples × 2 bytes = 20 bytes ECG data per BLE notification */
#define ECG_SAMPLES_PER_PKT   10
#define ECG_SAMPLE_RATE_HZ    250

typedef struct __attribute__((packed)) {
    uint16_t seq_num;              /* rolling sequence for dedup   */
    int16_t  samples[ECG_SAMPLES_PER_PKT]; /* 10 × 16-bit ECG      */
} ecg_data_payload_t;

/* ── ECG HR Packet (6 bytes) ──────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t heart_rate_bpm;       /* HR in bpm                     */
    uint16_t rr_interval_ms;       /* last R-R interval in ms       */
    uint8_t  motion_artifact;      /* 0=clean, 1=motion detected   */
    uint8_t  lead_off;             /* 0=connected, 1=lead off      */
} ecg_hr_payload_t;

/* ── BP Result Packet (10 bytes) ──────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t systolic_mmhg;        /* systolic in mmHg              */
    uint16_t diastolic_mmhg;       /* diastolic in mmHg            */
    uint16_t map_mmhg;             /* mean arterial pressure        */
    uint16_t heart_rate_bpm;       /* HR during measurement         */
    uint8_t  position_ok;          /* 1=wrist at heart level        */
    uint8_t  quality;              /* 0-100 measurement quality      */
} bp_result_payload_t;

/* ── BP Command (2 bytes) ─────────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  command;              /* 0=cancel, 1=measure now       */
    uint8_t  schedule_id;          /* which schedule triggered this  */
} bp_command_payload_t;

/* ── PPG HR Packet (6 bytes) ──────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t heart_rate_bpm;       /* PPG HR in bpm                 */
    uint16_t spo2_pct;             /* SpO₂ in % (0-100)             */
    int16_t  skin_temp_c10;        /* skin temp °C × 10             */
} ppg_hr_payload_t;

/* ── PPG HRV Packet (4 bytes) ─────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint16_t rmssd_ms;             /* RMSSD in ms                    */
    uint16_t sdnn_ms;              /* SDNN in ms                     */
} ppg_hrv_payload_t;

/* ── Activity Packet (4 bytes) ────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  activity_class;       /* 0=rest,1=walk,2=run,3=cycle,4=sleep */
    uint8_t  intensity;            /* 0-100                           */
    uint16_t steps;                /* step count since last report    */
} activity_payload_t;

/* ── Alert Packet (2 bytes) ───────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  alert_type;           /* cs_alert_type_t                */
    uint8_t  severity;             /* cs_severity_t                 */
} alert_payload_t;

/* ── Heartbeat Packet (4 bytes) ────────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint8_t  battery_pct;          /* 0-100                         */
    uint8_t  status_flags;         /* bit 0: online, bit 1: fault   */
    int16_t  rssi_dbm;             /* BLE RSSI                      */
} heartbeat_payload_t;

/* ── Hypertension Classification (WHO/ISH Guidelines) ─────────── */
typedef enum {
    BP_CATEGORY_OPTIMAL      = 0,  /* <120 and <80                  */
    BP_CATEGORY_NORMAL       = 1,  /* 120-129 and/or 80-84          */
    BP_CATEGORY_HIGH_NORMAL  = 2,  /* 130-139 and/or 85-89          */
    BP_CATEGORY_HYPERT_S1    = 3,  /* 140-159 and/or 90-99          */
    BP_CATEGORY_HYPERT_S2    = 4,  /* 160-179 and/or 100-109        */
    BP_CATEGORY_HYPERT_S3    = 5,  /* ≥180 and/or ≥110              */
    BP_CATEGORY_ISOLATED_SYSTOLIC = 6, /* ≥140 and <90              */
} bp_category_t;

/* ── CRC16-CCITT ──────────────────────────────────────────────── */
static inline uint16_t cs_crc16(const uint8_t *data, size_t len)
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

/* ── BP Category Helper ──────────────────────────────────────── */
static inline bp_category_t cs_classify_bp(uint16_t sys, uint16_t dia)
{
    if (sys >= 180 || dia >= 110) return BP_CATEGORY_HYPERT_S3;
    if (sys >= 160 || dia >= 100) return BP_CATEGORY_HYPERT_S2;
    if (sys >= 140 || dia >= 90) {
        if (dia < 90) return BP_CATEGORY_ISOLATED_SYSTOLIC;
        return BP_CATEGORY_HYPERT_S1;
    }
    if (sys >= 130 || dia >= 85) return BP_CATEGORY_HIGH_NORMAL;
    if (sys >= 120 || dia >= 80) return BP_CATEGORY_NORMAL;
    return BP_CATEGORY_OPTIMAL;
}

#endif /* CARDIOSYNC_PROTOCOL_H */