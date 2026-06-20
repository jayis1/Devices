/*
 * calm_protocol.h — CalmGrid shared mesh protocol definitions
 *
 * Defines the message types, payload structs, and vendor mesh model IDs
 * shared across all CalmGrid nodes (wrist band, room sentinel, hub, light).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef CALM_PROTOCOL_H
#define CALM_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---- Node IDs ---- */
#define CALM_NODE_ID_HUB        0x00
#define CALM_NODE_ID_WRIST      0x01
#define CALM_NODE_ID_SENTINEL   0x02
#define CALM_NODE_ID_LIGHT      0x03

/* ---- Mesh vendor model IDs (SIG Mesh vendor model range) ---- */
#define CALM_VENDOR_ID            0x05F2   /* placeholder OUI */
#define CALM_MODEL_ID_VITALS      0x0001
#define CALM_MODEL_ID_PROSODY     0x0002
#define CALM_MODEL_ID_ENVIRONMENT 0x0003
#define CALM_MODEL_ID_LIGHTING    0x0004
#define CALM_MODEL_ID_INTERVENTION 0x0005
#define CALM_MODEL_ID_ALERT       0x0006

/* ---- Message types ---- */
typedef enum {
    CALM_MSG_VITALS       = 0x10, /* wrist -> hub: HR, HRV, EDA, temp, activity, battery */
    CALM_MSG_PROSODY      = 0x11, /* sentinel -> hub/cloud: prosody stress + speech minutes */
    CALM_MSG_ENVIRONMENT  = 0x12, /* sentinel -> hub/cloud: light, temp, humidity, noise */
    CALM_MSG_LIGHTING     = 0x13, /* light -> hub: scene ack + ambient lux; hub -> light: scene cmd */
    CALM_MSG_INTERVENTION = 0x14, /* hub -> mesh: breathing/sound/light intervention command */
    CALM_MSG_ALERT        = 0x20, /* any -> hub: high-stress / burnout / env-stress (flood) */
    CALM_MSG_STRESS_SCORE = 0x30, /* hub -> mesh: broadcast current stress + burnout score */
    CALM_MSG_HEARTBEAT    = 0xF0, /* keep-alive + battery level */
} calm_msg_type_t;

/* ---- Alert flags ---- */
#define CALM_ALERT_HRV_DECLINE     0x01  /* HRV >20% below baseline */
#define CALM_ALERT_HR_ELEVATED     0x02  /* resting HR >10% above baseline */
#define CALM_ALERT_EDA_AROUSAL     0x04  /* EDA SCR rate >2x baseline */
#define CALM_ALERT_PROSODY_STRESS  0x08  /* voice prosody = high-stress sustained */
#define CALM_ALERT_ACUTE_STRESS    0x10  /* acute stress episode (EDA+HRV+HR) */
#define CALM_ALERT_POOR_SLEEP      0x20  /* sleep HRV suppressed / low efficiency */
#define CALM_ALERT_ENV_STRESS      0x40  /* environmental stressor (noise/heat/flicker) */
#define CALM_ALERT_LOW_BATT        0x80

/* ---- Lighting scenes ---- */
#define CALM_SCENE_CIRCADIAN       0x01  /* follow circadian schedule */
#define CALM_SCENE_WORK            0x02  /* cool focused work light */
#define CALM_SCENE_DESTRESS        0x03  /* warm low-CCT calming */
#define CALM_SCENE_BREATHING       0x04  /* gentle pulse synced to breathing pace */
#define CALM_SCENE_SUNSET          0x05  /* warm dimming for evening */
#define CALM_SCENE_SUNRISE         0x06  /* gradual warm wake-up */
#define CALM_SCENE_OFF             0x00

/* ---- Vitals payload (wrist -> hub) ---- */
#define CALM_GAIT_FEATURES 4  /* stride, activity_count, sedentary_min, step_count */

typedef struct __attribute__((packed)) {
    uint8_t  type;             /* CALM_MSG_VITALS */
    uint8_t  node_id;          /* CALM_NODE_ID_WRIST */
    uint8_t  seq;
    uint8_t  flags;            /* alert flags bitmask */
    uint8_t  hr_bpm;           /* heart rate, bpm (0-255) */
    uint16_t hrv_rmssd;        /* RMSSD, centi-ms (value/100 = ms) */
    uint16_t eda_scl;          /* tonic skin conductance level, microsiemens * 100 */
    uint16_t eda_scr_rate;     /* phasic SCR events per minute * 100 */
    int16_t  skin_temp_centic; /* skin temp, centi-degC */
    uint8_t  activity_class;   /* 0=sit 1=walk 2=run 3=rest 4=sleep 5=work 6=commute 7=exercise */
    uint8_t  confidence;       /* 0-100 */
    uint16_t step_count;       /* steps since last report */
    uint8_t  battery_pct;      /* 0-100 */
    uint16_t crc16;
} calm_vitals_payload_t;

/* ---- Prosody payload (sentinel -> hub/cloud) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* CALM_MSG_PROSODY */
    uint8_t  node_id;          /* CALM_NODE_ID_SENTINEL */
    uint8_t  seq;
    uint8_t  flags;            /* CALM_ALERT_PROSODY_STRESS if elevated */
    uint8_t  prosody_class;    /* 0=calm 1=neutral 2=elevated 3=high-stress */
    uint8_t  confidence;       /* 0-100 */
    uint16_t speech_minutes;   /* minutes of speech in last interval */
    int16_t  f0_deviation;     /* F0 mean deviation from baseline (cents * 10) */
    uint16_t crc16;
} calm_prosody_payload_t;

/* ---- Environment payload (sentinel -> hub/cloud) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* CALM_MSG_ENVIRONMENT */
    uint8_t  node_id;          /* CALM_NODE_ID_SENTINEL */
    uint8_t  seq;
    uint8_t  flags;            /* CALM_ALERT_ENV_STRESS if threshold breached */
    uint16_t ambient_lux;      /* lux * 10 (0-6553.5) */
    uint16_t cct_kelvin;       /* correlated color temperature (K) */
    int16_t  temp_centic;      /* room temp centi-degC */
    uint16_t humidity_centi;   /* relative humidity * 100 (%) */
    uint16_t noise_db_tenth;   /* noise level dB * 10 */
    uint16_t crc16;
} calm_environment_payload_t;

/* ---- Lighting payload (hub -> light / light -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* CALM_MSG_LIGHTING */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  scene;            /* CALM_SCENE_* */
    uint8_t  brightness;       /* 0-100 % */
    uint16_t warm_kelvin;      /* target warm CCT (0 = use scene default) */
    uint16_t cool_kelvin;      /* target cool CCT (0 = use scene default) */
    uint16_t ambient_lux;      /* light node ambient feedback (light -> hub only) */
    uint16_t crc16;
} calm_lighting_payload_t;

/* ---- Intervention payload (hub -> mesh) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* CALM_MSG_INTERVENTION */
    uint8_t  node_id;          /* CALM_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  intervention_id;  /* 0=breathing 1=soundscape 2=lighting 3=notification 4=combined */
    uint8_t  param1;           /* e.g. breathing pattern (0=4-7-8, 1=box, 2=coherent) */
    uint8_t  param2;           /* e.g. soundscape track id */
    uint16_t duration_s;       /* intervention duration */
    uint16_t crc16;
} calm_intervention_payload_t;

/* ---- Alert payload (any -> hub, mesh-flood) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* CALM_MSG_ALERT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;            /* which alert(s) */
    uint16_t value;            /* e.g. HRV decline %, or 0 */
    uint16_t crc16;
} calm_alert_payload_t;

/* ---- Stress score payload (hub -> mesh broadcast) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* CALM_MSG_STRESS_SCORE */
    uint8_t  node_id;          /* CALM_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  stress_score;     /* 0-100 (100 = max stress) */
    uint8_t  burnout_risk;     /* 0-100 (14-day forecast) */
    uint8_t  recovery_score;   /* 0-100 (sleep + HRV recovery) */
    uint16_t crc16;
} calm_stress_score_payload_t;

/* ---- Heartbeat payload ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* CALM_MSG_HEARTBEAT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  battery_pct;
    uint8_t  state;            /* 0=normal 1=charging 2=low_batt 3=error */
    uint16_t crc16;
} calm_heartbeat_payload_t;

/* ---- Function prototypes ---- */
uint16_t calm_crc16(const uint8_t *data, size_t len);
uint16_t calm_pack_crc(void *payload, size_t struct_size_without_crc);
int      calm_verify_crc(const void *payload, size_t struct_size_without_crc,
                         uint16_t received_crc);

/* ---- Mesh transport abstraction ---- */
typedef void (*calm_mesh_rx_cb_t)(uint8_t type, const uint8_t *data, size_t len);
typedef int  (*calm_mesh_tx_t)(const uint8_t *data, size_t len);

void calm_mesh_set_tx(calm_mesh_tx_t tx_func);
void calm_mesh_set_rx_callback(calm_mesh_rx_cb_t cb);
int  calm_mesh_send(uint8_t msg_type, uint8_t node_id,
                    const void *payload, size_t payload_len);
void calm_mesh_on_rx(const uint8_t *data, size_t len);

#endif /* CALM_PROTOCOL_H */