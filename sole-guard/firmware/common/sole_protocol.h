/*
 * sole_protocol.h — SoleGuard shared mesh protocol definitions
 *
 * Defines the message types, payload structs, and vendor mesh model IDs
 * shared across all SoleGuard body-worn nodes (insoles, ankle tag, hub).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SOLE_PROTOCOL_H
#define SOLE_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---- Node IDs ---- */
#define SOLE_NODE_ID_HUB        0x00
#define SOLE_NODE_ID_INSOLE_L   0x01
#define SOLE_NODE_ID_INSOLE_R   0x02
#define SOLE_NODE_ID_ANKLE      0x03
#define SOLE_NODE_ID_SCANNER    0x04

/* ---- Mesh vendor model IDs (SIG Mesh vendor model range) ---- */
#define SOLE_VENDOR_ID          0x05F0   /* placeholder OUI */
#define SOLE_MODEL_ID_PRESSURE  0x0001
#define SOLE_MODEL_ID_ALERT     0x0002
#define SOLE_MODEL_ID_RISK      0x0003

/* ---- Message types ---- */
typedef enum {
    SOLE_MSG_PRESSURE_TEMP = 0x10, /* insole -> hub: 24x FSR + 8x temp + flags */
    SOLE_MSG_GAIT          = 0x11, /* insole/ankle -> hub: gait features */
    SOLE_MSG_EDEMA         = 0x12, /* ankle -> hub: bioimpedance edema index */
    SOLE_MSG_ALERT         = 0x20, /* any -> hub: hotspot/fall/wound flag (flood) */
    SOLE_MSG_RISK_SCORE    = 0x30, /* hub -> mesh: broadcast current risk score */
    SOLE_MSG_SCAN_RESULT   = 0x40, /* scanner -> hub/cloud: wound flag + image ref */
    SOLE_MSG_HEARTBEAT     = 0xF0, /* keep-alive + battery level */
} sole_msg_type_t;

/* ---- Alert flags ---- */
#define SOLE_ALERT_HOTSPOT      0x01  /* pressure hotspot sustained */
#define SOLE_ALERT_TEMP_ASYM    0x02  /* temperature asymmetry > 2.2C */
#define SOLE_ALERT_FALL         0x04  /* fall detected (free-fall + impact) */
#define SOLE_ALERT_WOUND        0x08  /* wound detected by scanner */
#define SOLE_ALERT_LOW_BATT     0x10
#define SOLE_ALERT_EDEMA        0x20  /* ankle edema index elevated */

/* ---- Pressure / temperature payload (insole -> hub) ---- */
#define SOLE_PRESSURE_POINTS  24
#define SOLE_TEMP_POINTS       8
#define SOLE_GAIT_FEATURES     8

typedef struct __attribute__((packed)) {
    uint8_t  type;            /* SOLE_MSG_PRESSURE_TEMP */
    uint8_t  node_id;         /* SOLE_NODE_ID_INSOLE_L/R */
    uint8_t  seq;
    uint8_t  flags;           /* alert flags bitmask */
    uint8_t  pressure[SOLE_PRESSURE_POINTS];   /* 0-255 -> 0-500 kPa (linear) */
    int16_t  temp_centic[SOLE_TEMP_POINTS];    /* centi-degC: value/100 = degC */
    uint16_t pti_centic;      /* pressure-time integral, centi-kPa*s (worst zone) */
    uint16_t crc16;
} sole_pressure_payload_t;

/* ---- Gait payload (insole or ankle -> hub) ---- */
/* gait[0]=cadence(spm) gait[1]=stride_cm gait[2]=symmetry_idx(0-1000)
   gait[3]=double_support_pct(centi) gait[4]=shuffling_score(0-1000)
   gait[5]=foot_clearance_mm gait[6]=step_count gait[7]=activity_class */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* SOLE_MSG_GAIT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    int16_t  gait[SOLE_GAIT_FEATURES];
    uint16_t crc16;
} sole_gait_payload_t;

/* ---- Edema payload (ankle -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* SOLE_MSG_EDEMA */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    uint16_t impedance_ohm;   /* 1kHz magnitude */
    uint16_t edema_index;      /* 0-1000 (calibrated) */
    int16_t  skin_temp_centic; /* ankle skin temp, centi-degC */
    uint16_t crc16;
} sole_edema_payload_t;

/* ---- Alert payload (any -> hub, mesh-flood) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* SOLE_MSG_ALERT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;           /* which alert(s) */
    uint16_t value;           /* e.g. asymmetry centi-degC, or 0 */
    uint16_t crc16;
} sole_alert_payload_t;

/* ---- Risk score payload (hub -> mesh broadcast) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* SOLE_MSG_RISK_SCORE */
    uint8_t  node_id;         /* SOLE_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  risk_left;       /* 0-100 */
    uint8_t  risk_right;      /* 0-100 */
    uint16_t crc16;
} sole_risk_payload_t;

/* ---- Scan result payload (scanner -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* SOLE_MSG_SCAN_RESULT */
    uint8_t  node_id;         /* SOLE_NODE_ID_SCANNER */
    uint8_t  seq;
    uint8_t  flags;           /* SOLE_ALERT_WOUND if flagged */
    uint8_t  wound_class;     /* 0=normal 1=callus 2=blister 3=fissure 4=ulcer 5=fungal 6=maceration */
    uint8_t  confidence;      /* 0-100 */
    uint32_t image_ref;       /* cloud object-store ref id */
    int16_t  weight_dag;      /* body weight, decagrams (value/10=kg) */
    uint16_t crc16;
} sole_scan_payload_t;

/* ---- Heartbeat (any -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* SOLE_MSG_HEARTBEAT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  battery_pct;     /* 0-100 */
    uint16_t crc16;
} sole_heartbeat_payload_t;

/* ---- API ---- */
uint16_t sole_crc16(const uint8_t *data, size_t len);
uint16_t sole_pack_crc(void *payload, size_t struct_size_without_crc);
int      sole_verify_crc(const void *payload, size_t struct_size_without_crc, uint16_t crc);

/* Zone mapping: 24 pressure sensors -> 6 anatomical zones */
typedef enum {
    SOLE_ZONE_HEEL        = 0,  /* sensors 0-3   */
    SOLE_ZONE_MIDFOOT     = 1,  /* sensors 4-7   */
    SOLE_ZONE_META_1      = 2,  /* sensors 8-11  */
    SOLE_ZONE_META_25     = 3,  /* sensors 12-15 */
    SOLE_ZONE_HALLUX      = 4,  /* sensors 16-19 */
    SOLE_ZONE_LESSER_TOES = 5,  /* sensors 20-23 */
} sole_zone_t;

uint8_t sole_zone_of_sensor(uint8_t sensor_idx);
uint8_t sole_zone_peak_pressure(const uint8_t pressure[24], sole_zone_t zone);
uint32_t sole_zone_pti_sum(const uint8_t pressure[24], sole_zone_t zone, uint8_t samples);

#endif /* SOLE_PROTOCOL_H */