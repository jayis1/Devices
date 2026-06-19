/*
 * paw_protocol.h — PawSync shared mesh protocol definitions
 *
 * Defines the message types, payload structs, and vendor mesh model IDs
 * shared across all PawSync nodes (collar tag, behavior camera, hub, feeder).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef PAW_PROTOCOL_H
#define PAW_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---- Node IDs ---- */
#define PAW_NODE_ID_HUB       0x00
#define PAW_NODE_ID_COLLAR    0x01
#define PAW_NODE_ID_CAMERA    0x02
#define PAW_NODE_ID_FEEDER    0x03

/* ---- Mesh vendor model IDs (SIG Mesh vendor model range) ---- */
#define PAW_VENDOR_ID         0x05F1   /* placeholder OUI */
#define PAW_MODEL_ID_VITALS   0x0001
#define PAW_MODEL_ID_ACTIVITY  0x0002
#define PAW_MODEL_ID_FEEDING   0x0003
#define PAW_MODEL_ID_ALERT     0x0004

/* ---- Message types ---- */
typedef enum {
    PAW_MSG_VITALS      = 0x10, /* collar -> hub: HR, HRV, temp, battery */
    PAW_MSG_ACTIVITY    = 0x11, /* collar -> hub: activity class + gait features */
    PAW_MSG_BEHAVIOR     = 0x12, /* camera -> hub/cloud: behavior class + vocalization */
    PAW_MSG_FEEDING      = 0x13, /* feeder -> hub: feeding event + intake */
    PAW_MSG_ALERT        = 0x20, /* any -> hub: illness/anxiety/low-food (flood) */
    PAW_MSG_WELLNESS     = 0x30, /* hub -> mesh: broadcast current wellness score */
    PAW_MSG_ENRICHMENT   = 0x40, /* hub -> feeder/camera: trigger treat/audio */
    PAW_MSG_HEARTBEAT    = 0xF0, /* keep-alive + battery level */
} paw_msg_type_t;

/* ---- Alert flags ---- */
#define PAW_ALERT_HRV_DECLINE   0x01  /* HRV >20% below baseline */
#define PAW_ALERT_HR_ELEVATED   0x02  /* resting HR >15% above baseline */
#define PAW_ALERT_FEVER          0x04  /* skin temp >0.5C above baseline */
#define PAW_ALERT_LAMENESS       0x08  /* gait asymmetry > threshold */
#define PAW_ALERT_SCRATCHING     0x10  /* scratching >3x baseline */
#define PAW_ALERT_APPETITE_LOSS  0x20  /* >25% food uneaten */
#define PAW_ALERT_ANXIETY        0x40  /* separation anxiety episode */
#define PAW_ALERT_LOW_BATT       0x80

/* ---- Vitals payload (collar -> hub) ---- */
#define PAW_GAIT_FEATURES  6

typedef struct __attribute__((packed)) {
    uint8_t  type;            /* PAW_MSG_VITALS */
    uint8_t  node_id;         /* PAW_NODE_ID_COLLAR */
    uint8_t  seq;
    uint8_t  flags;           /* alert flags bitmask */
    uint8_t  hr_bpm;          /* heart rate, bpm (0-255) */
    uint16_t hrv_rmssd;       /* RMSSD, centi-ms (value/100 = ms) */
    int16_t  skin_temp_centic;/* skin temp, centi-degC */
    int16_t  gait[PAW_GAIT_FEATURES]; /* gait features */
    uint8_t  battery_pct;     /* 0-100 */
    uint16_t crc16;
} paw_vitals_payload_t;

/* ---- Activity payload (collar -> hub) ---- */
/* gait[0]=stride_cm  gait[1]=stance_ms  gait[2]=symmetry_idx(0-1000)
   gait[3]=weight_asym(0-1000)  gait[4]=activity_count  gait[5]=scratch_count */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* PAW_MSG_ACTIVITY */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  activity_class; /* 0=rest 1=walk 2=run 3=sleep 4=scratch 5=headshake 6=lick 7=eat 8=play */
    uint8_t  confidence;     /* 0-100 */
    uint16_t duration_s;     /* seconds in this activity since last report */
    int16_t  gait[PAW_GAIT_FEATURES];
    uint16_t crc16;
} paw_activity_payload_t;

/* ---- Behavior payload (camera -> hub/cloud) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* PAW_MSG_BEHAVIOR */
    uint8_t  node_id;         /* PAW_NODE_ID_CAMERA */
    uint8_t  seq;
    uint8_t  flags;           /* PAW_ALERT_ANXIETY if episode detected */
    uint8_t  behavior_class;  /* 0=resting 1=pacing 2=vocalizing 3=destructive 4=elimination 5=playing */
    uint8_t  vocalization;    /* 0=none 1=pain 2=anxiety 3=alert 4=play 5=attention 6=distress */
    uint8_t  confidence;     /* 0-100 */
    uint16_t duration_s;     /* episode duration */
    uint32_t clip_ref;       /* SD card clip reference (0 = none) */
    uint16_t crc16;
} paw_behavior_payload_t;

/* ---- Feeding payload (feeder -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* PAW_MSG_FEEDING */
    uint8_t  node_id;         /* PAW_NODE_ID_FEEDER */
    uint8_t  seq;
    uint8_t  flags;           /* PAW_ALERT_APPETITE_LOSS if uneaten */
    uint8_t  pet_id;         /* RFID pet ID (0-255) */
    uint16_t dispensed_g;     /* grams dispensed */
    uint16_t consumed_g;      /* grams consumed (weight diff) */
    uint16_t water_ml;        /* water consumed since last report */
    uint8_t  hopper_pct;     /* hopper fill level 0-100 */
    uint16_t crc16;
} paw_feeding_payload_t;

/* ---- Alert payload (any -> hub, mesh-flood) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* PAW_MSG_ALERT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;           /* which alert(s) */
    uint16_t value;           /* e.g. HRV decline %, or 0 */
    uint16_t crc16;
} paw_alert_payload_t;

/* ---- Wellness score payload (hub -> mesh broadcast) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* PAW_MSG_WELLNESS */
    uint8_t  node_id;         /* PAW_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  wellness_score;  /* 0-100 */
    uint8_t  illness_risk;    /* 0-100 (7-day forecast) */
    uint8_t  anxiety_level;   /* 0-100 */
    uint16_t crc16;
} paw_wellness_payload_t;

/* ---- Enrichment command (hub -> feeder/camera) ---- */
typedef enum {
    PAW_ENRICHMENT_TREAT    = 0x01,  /* feeder: dispense small treat */
    PAW_ENRICHMENT_AUDIO    = 0x02,  /* hub: play calming audio */
    PAW_ENRICHMENT_VOICE    = 0x03,  /* hub: play owner voice recording */
} paw_enrichment_type_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;            /* PAW_MSG_ENRICHMENT */
    uint8_t  node_id;         /* PAW_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  enrichment_type; /* paw_enrichment_type_t */
    uint8_t  target_node;     /* PAW_NODE_ID_FEEDER or PAW_NODE_ID_CAMERA */
    uint8_t  intensity;       /* 0-100 (audio volume / treat size) */
    uint16_t crc16;
} paw_enrichment_payload_t;

/* ---- Heartbeat (any -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;            /* PAW_MSG_HEARTBEAT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  battery_pct;     /* 0-100 */
    uint16_t crc16;
} paw_heartbeat_payload_t;

/* ---- API ---- */
uint16_t paw_crc16(const uint8_t *data, size_t len);
uint16_t paw_pack_crc(void *payload, size_t struct_size_without_crc);
int      paw_verify_crc(const void *payload, size_t struct_size_without_crc, uint16_t crc);

/* Activity class names (for logging) */
static const char *const PAW_ACTIVITY_NAMES[] = {
    "resting", "walking", "running", "sleeping",
    "scratching", "head_shaking", "licking", "eating", "playing"
};

/* Behavior class names */
static const char *const PAW_BEHAVIOR_NAMES[] = {
    "resting", "pacing", "vocalizing", "destructive",
    "elimination", "playing"
};

/* Vocalization class names */
static const char *const PAW_VOCAL_NAMES[] = {
    "none", "pain", "anxiety", "alert", "play", "attention", "distress"
};

#endif /* PAW_PROTOCOL_H */