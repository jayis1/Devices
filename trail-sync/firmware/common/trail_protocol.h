/*
 * trail_protocol.h — TrailSync shared Sub-GHz mesh + LoRa protocol definitions
 *
 * Defines the message types, payload structs, and mesh model IDs
 * shared across all TrailSync nodes (wrist unit, shoe pod, trail beacon, hub).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef TRAIL_PROTOCOL_H
#define TRAIL_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---- Node IDs ---- */
#define TS_NODE_ID_HUB           0x00
#define TS_NODE_ID_WRIST         0x01  /* range 0x01-0x0F (15 wrist units per group) */
#define TS_NODE_ID_SHOE_POD      0x10  /* range 0x10-0x2F (left/right per runner) */
#define TS_NODE_ID_BEACON        0x40  /* range 0x40-0xFF (up to 192 beacons) */

/* ---- Shoe pod side identifiers ---- */
#define TS_POD_LEFT              0x00
#define TS_POD_RIGHT             0x01

/* ---- Mesh vendor model IDs ---- */
#define TS_VENDOR_ID               0x06F5   /* placeholder OUI */
#define TS_MODEL_ID_GAIT          0x0001
#define TS_MODEL_ID_NAV           0x0002
#define TS_MODEL_ID_SOS           0x0003
#define TS_MODEL_ID_TELEMETRY     0x0004
#define TS_MODEL_ID_BEACON_DATA   0x0005
#define TS_MODEL_ID_TRAIL_COND    0x0006
#define TS_MODEL_ID_COMMAND       0x0007

/* ---- Message types ---- */
typedef enum {
    TS_MSG_GAIT          = 0x10, /* shoe pod -> wrist: gait summary */
    TS_MSG_TELEMETRY     = 0x20, /* wrist -> hub: GPS, HR, SpO2, altitude */
    TS_MSG_NAV           = 0x30, /* beacon -> wrist: trail junction data */
    TS_MSG_SOS           = 0x40, /* wrist/hub -> LoRa mesh: emergency distress */
    TS_MSG_SOS_ACK       = 0x41, /* hub -> LoRa mesh: SOS received, relaying */
    TS_MSG_BEACON_DATA   = 0x50, /* beacon -> mesh: conditions broadcast */
    TS_MSG_TRAIL_COND    = 0x60, /* cloud -> beacon: trail condition update */
    TS_MSG_INJURY_ALERT  = 0x70, /* wrist -> hub: injury risk threshold crossed */
    TS_MSG_STORM_ALERT   = 0x80, /* wrist/hub -> mesh: storm prediction */
    TS_MSG_HEARTBEAT     = 0xF0, /* keep-alive + battery level */
} ts_msg_type_t;

/* ---- Alert flags ---- */
#define TS_ALERT_FALL_DETECTED    0x01  /* acceleration spike > 8G + stillness */
#define TS_ALERT_ALTITUDE_SICK    0x02  /* SpO2 < 94% + HRV drop + fast ascent */
#define TS_ALERT_STORM_INCOMING   0x04  /* pressure drop > 4 hPa/3hr */
#define TS_ALERT_OFF_TRAIL        0x08  /* GPS position outside trail corridor */
#define TS_ALERT_LOW_BATT         0x10  /* battery < 15% */
#define TS_ALERT_INJURY_RISK      0x20  /* gait asymmetry or impact threshold */
#define TS_ALERT_OVERTRAINING     0x40  /* HRV < 60% of baseline */
#define TS_ALERT_BEACON_NEAR      0x80  /* approaching a trail beacon */

/* ---- Injury risk classes ---- */
#define TS_INJURY_NONE            0
#define TS_INJURY_IT_BAND         1   /* IT band syndrome */
#define TS_INJURY_PLANTAR         2   /* plantar fasciitis */
#define TS_INJURY_ACHILLES        3   /* Achilles tendinopathy */
#define TS_INJURY_STRESS_FX       4   /* stress fracture */
#define TS_INJURY_SHIN_SPLINT     5   /* medial tibial stress syndrome */
#define TS_INJURY_RUNNERS_KNEE    6   /* patellofemoral pain syndrome */
#define TS_INJURY_ANKLE_SPRAIN    7   /* lateral ankle sprain */
#define TS_INJURY_HAMSTRING       8   /* hamstring strain */
#define TS_INJURY_HIP_FLEXOR      9   /* hip flexor strain */
#define TS_INJURY_CALF_STRAIN     10  /* calf/gastrocnemius strain */
#define TS_INJURY_PATELLAR        11  /* patellar tendinopathy */

/* ---- Terrain types (from shoe pod CNN) ---- */
#define TS_TERRAIN_ROAD       0
#define TS_TERRAIN_GRAVEL     1
#define TS_TERRAIN_DIRT       2
#define TS_TERRAIN_MUD        3
#define TS_TERRAIN_SNOW        4
#define TS_TERRAIN_ICE         5
#define TS_TERRAIN_ROCK        6
#define TS_TERRAIN_SAND        7

/* ---- Trail difficulty ---- */
#define TS_TRAIL_EASY         1   /* flat, well-maintained */
#define TS_TRAIL_MODERATE     2   /* some elevation, moderate terrain */
#define TS_TRAIL_DIFFICULT    3   /* significant elevation, technical terrain */
#define TS_TRAIL_EXPERT       4   /* extreme elevation gain, scrambling */
#define TS_TRAIL_HAZARDOUS    5   /* current hazard (washout, ice, wildlife) */

/* ---- SOS types ---- */
#define TS_SOS_MANUAL         0x01  /* user pressed SOS button */
#define TS_SOS_FALL_AUTO      0x02  /* auto-detected fall + stillness */
#define TS_SOS_ALTITUDE       0x04  /* altitude sickness emergency */
#define TS_SOS_MEDICAL        0x08  /* HR/SpO2 critical */

/* ---- Gait summary payload (shoe pod -> wrist, every 5s) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_GAIT */
    uint8_t  node_id;          /* TS_NODE_ID_SHOE_POD | pod_index */
    uint8_t  side;              /* TS_POD_LEFT or TS_POD_RIGHT */
    uint8_t  seq;
    uint8_t  flags;             /* TS_ALERT_INJURY_RISK if gait anomaly */
    uint8_t  terrain;           /* TS_TERRAIN_* from on-device CNN */
    uint8_t  gait_class;        /* 0=normal 1=asymmetric 2=overpronating 3=high-impact */
    uint8_t  gait_conf;        /* 0-100 confidence */
    int16_t  cadence;           /* steps/min * 10 (e.g. 1800 = 180 spm) */
    int16_t  ground_contact_ms;/* ground contact time in ms */
    int16_t  vertical_osc_mm;  /* vertical oscillation in mm */
    int16_t  impact_load_pct;  /* impact load as % of body weight * 10 */
    int16_t  pronation_deg;    /* pronation angle in degrees * 10 */
    int16_t  asymmetry_pct;    /* L/R asymmetry % * 10 */
    int16_t  stride_length_cm; /* stride length in cm * 10 */
    uint8_t  battery_pct;
    uint16_t crc16;
} ts_gait_payload_t;  /* 24 bytes */

/* ---- Telemetry payload (wrist -> hub, every 60s) ---- */
#define TS_TELEM_INTERVAL_S     60    /* 1 min during activity */
#define TS_TELEM_INTERVAL_IDLE  300   /* 5 min when idle */

typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_TELEMETRY */
    uint8_t  node_id;           /* TS_NODE_ID_WRIST */
    uint8_t  seq;
    uint8_t  flags;             /* alert flags bitmask */
    int32_t  lat_deg1e5;        /* latitude * 1e5 (±180.00000°) */
    int32_t  lon_deg1e5;        /* longitude * 1e5 */
    int16_t  altitude_dm;       /* altitude in decimeters (±3276.7 m) */
    int16_t  speed_cm_s;        /* speed in cm/s */
    uint16_t distance_dm;       /* cumulative distance in decimeters */
    uint8_t  hr;                /* heart rate bpm */
    uint8_t  spo2;              /* SpO2 % (0-100) */
    int16_t  hrv_rmssd;        /* HRV RMSSD in ms * 10 */
    int16_t  skin_temp_centic;  /* skin temperature (centi-degC) */
    int16_t  pressure_hpa;     /* barometric pressure (hPa * 10) */
    uint8_t  battery_pct;
    uint8_t  num_satellites;    /* GPS satellite count */
    uint16_t crc16;
} ts_telemetry_payload_t;  /* 28 bytes */

/* ---- Navigation payload (beacon -> wrist, on approach) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_NAV */
    uint8_t  node_id;           /* TS_NODE_ID_BEACON */
    uint8_t  seq;
    uint8_t  trail_difficulty;  /* TS_TRAIL_* */
    int32_t  lat_deg1e5;        /* beacon latitude */
    int32_t  lon_deg1e5;        /* beacon longitude */
    int16_t  altitude_dm;       /* beacon altitude */
    uint8_t  water_available;   /* 0=no 1=seasonal 2=reliable */
    uint8_t  hazard_flags;      /* bitmask: 0x01=washout 0x02=ice 0x04=wildlife 0x08=fire */
    uint16_t dist_trailhead_m;  /* distance to trailhead in meters */
    uint16_t dist_next_water_m; /* distance to next water source in meters */
    uint8_t  cell_signal;       /* 0=none 1=weak 2=moderate 3=strong */
    uint16_t crc16;
} ts_nav_payload_t;  /* 22 bytes */

/* ---- SOS payload (wrist/hub -> LoRa mesh) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_SOS */
    uint8_t  node_id;           /* who is in distress */
    uint8_t  sos_type;          /* TS_SOS_* bitmask */
    uint8_t  severity;          /* 0=minor 1=moderate 2=serious 3=critical */
    int32_t  lat_deg1e5;        /* GPS latitude */
    int32_t  lon_deg1e5;        /* GPS longitude */
    int16_t  altitude_dm;       /* altitude */
    uint8_t  hr;                /* heart rate */
    uint8_t  spo2;              /* SpO2 */
    int16_t  hrv_rmssd;        /* HRV */
    uint8_t  injury_class;      /* TS_INJURY_* if gait detected, 0=unknown */
    uint8_t  num_people;        /* number of people in group (1=solo) */
    uint16_t bearing_trail_m;   /* bearing + distance to nearest trail beacon */
    uint16_t crc16;
} ts_sos_payload_t;  /* 20 bytes */

/* ---- SOS ACK (hub -> mesh) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_SOS_ACK */
    uint8_t  hub_id;            /* TS_NODE_ID_HUB */
    uint8_t  sos_node_id;       /* who the SOS is from */
    uint8_t  status;            /* 0=received 1=relaying 2=rescue_dispatched */
    uint8_t  eta_minutes;       /* estimated rescue time */
    uint16_t crc16;
} ts_sos_ack_payload_t;  /* 8 bytes */

/* ---- Beacon conditions broadcast (beacon -> mesh, every 5 min) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_BEACON_DATA */
    uint8_t  node_id;           /* TS_NODE_ID_BEACON */
    uint8_t  seq;
    int32_t  lat_deg1e5;        /* beacon latitude */
    int32_t  lon_deg1e5;        /* beacon longitude */
    int16_t  altitude_dm;       /* beacon altitude */
    int16_t  temp_centic;       /* temperature (centi-degC) */
    uint16_t humidity_pct;     /* humidity % */
    uint16_t pressure_hpa;     /* pressure hPa * 10 */
    uint8_t  battery_pct;
    uint8_t  pir_events;        /* PIR motion events since last broadcast */
    uint8_t  hazard_flags;      /* same as nav payload */
    uint8_t  trail_difficulty;  /* current trail difficulty rating */
    uint16_t crc16;
} ts_beacon_data_payload_t;  /* 22 bytes */

/* ---- Trail condition update (cloud -> beacon) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_TRAIL_COND */
    uint8_t  node_id;           /* TS_NODE_ID_HUB */
    uint8_t  beacon_id;         /* target beacon (0xFF = all) */
    uint8_t  trail_difficulty;  /* updated difficulty */
    uint8_t  hazard_flags;      /* updated hazards */
    uint8_t  water_available;   /* updated water status */
    uint8_t  closure;           /* 0=open 1=partially closed 2=closed */
    uint16_t crc16;
} ts_trail_cond_payload_t;  /* 10 bytes */

/* ---- Injury alert (wrist -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_INJURY_ALERT */
    uint8_t  node_id;           /* TS_NODE_ID_WRIST */
    uint8_t  seq;
    uint8_t  flags;             /* TS_ALERT_INJURY_RISK */
    uint8_t  injury_class;      /* TS_INJURY_* */
    uint8_t  risk_pct;          /* injury risk 0-100 */
    int16_t  asymmetry_pct;     /* current L/R asymmetry % * 10 */
    int16_t  impact_load_pct;  /* current impact % of BW * 10 */
    uint8_t  terrain;           /* current terrain type */
    uint16_t crc16;
} ts_injury_alert_payload_t;  /* 13 bytes */

/* ---- Storm alert (wrist/hub -> mesh) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;              /* TS_MSG_STORM_ALERT */
    uint8_t  node_id;           /* TS_NODE_ID_WRIST or TS_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  severity;          /* 0=watch 1=warning 2=severe */
    int16_t  pressure_hpa;     /* current pressure hPa * 10 */
    int16_t  pressure_delta;   /* pressure change in last 3hr hPa * 10 */
    uint8_t  hours_to_storm;   /* estimated hours until storm */
    uint16_t crc16;
} ts_storm_alert_payload_t;  /* 11 bytes */

/* ---- Heartbeat payload ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* TS_MSG_HEARTBEAT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  battery_pct;
    uint8_t  state;            /* 0=normal 1=charging 2=low_batt 3=error */
    uint16_t crc16;
} ts_heartbeat_payload_t;  /* 8 bytes */

/* ---- Function prototypes ---- */
uint16_t ts_crc16(const uint8_t *data, size_t len);
uint16_t ts_pack_crc(void *payload, size_t struct_size_without_crc);
int      ts_verify_crc(const void *payload, size_t struct_size_without_crc,
                       uint16_t received_crc);

/* ---- Mesh transport abstraction ---- */
typedef void (*ts_mesh_rx_cb_t)(uint8_t type, const uint8_t *data, size_t len);
typedef int  (*ts_mesh_tx_t)(const uint8_t *data, size_t len);

void ts_mesh_set_tx(ts_mesh_tx_t tx_func);
void ts_mesh_set_rx_callback(ts_mesh_rx_cb_t cb);
int  ts_mesh_send(uint8_t msg_type, uint8_t node_id,
                  const void *payload, size_t payload_len);
void ts_mesh_on_rx(const uint8_t *data, size_t len);

/* ---- LoRa transport abstraction ---- */
typedef int (*ts_lora_tx_t)(const uint8_t *data, size_t len);

void ts_lora_set_tx(ts_lora_tx_t tx_func);
int  ts_lora_send(uint8_t msg_type, uint8_t node_id,
                  const void *payload, size_t payload_len);

/* ---- Convenience senders ---- */
void ts_send_gait(uint8_t pod_side, uint8_t terrain, uint8_t gait_class,
                  uint8_t gait_conf, int16_t cadence, int16_t contact_ms,
                  int16_t vert_osc, int16_t impact, int16_t pronation,
                  int16_t asymmetry, int16_t stride_cm, uint8_t batt,
                  uint8_t flags);
void ts_send_telemetry(int32_t lat, int32_t lon, int16_t alt, int16_t speed,
                       uint16_t dist, uint8_t hr, uint8_t spo2, int16_t hrv,
                       int16_t skin_temp, int16_t pressure, uint8_t batt,
                       uint8_t sats, uint8_t flags);
void ts_send_sos(uint8_t sos_type, uint8_t severity, int32_t lat, int32_t lon,
                 int16_t alt, uint8_t hr, uint8_t spo2, int16_t hrv,
                 uint8_t injury, uint8_t num_people, uint16_t bearing);
void ts_send_injury_alert(uint8_t injury_class, uint8_t risk_pct,
                          int16_t asymmetry, int16_t impact, uint8_t terrain);
void ts_send_storm_alert(uint8_t severity, int16_t pressure, int16_t delta,
                         uint8_t hours);

/* ---- Gait class names ---- */
const char *ts_gait_class_name(uint8_t gait_class);
const char *ts_injury_name(uint8_t injury_class);
const char *ts_terrain_name(uint8_t terrain);
const char *ts_trail_diff_name(uint8_t difficulty);

/* ---- Altitude sickness thresholds ---- */
#define TS_SPO2_NORMAL_MIN      95   /* normal SpO2 at sea level */
#define TS_SPO2_AMS_THRESHOLD  94   /* SpO2 below this = AMS risk */
#define TS_SPO2_HACE_THRESHOLD 88   /* SpO2 below this = HACE/HAPE risk */
#define TS_HRV_DROP_AMS        20    /* HRV drop > 20% from baseline */
#define TS_ASCENT_RATE_AMS     500   /* > 500m/hr = fast ascent risk */
#define TS_PRESSURE_DROP_STORM  40   /* > 4 hPa drop in 3 hr = storm risk */

#endif /* TRAIL_PROTOCOL_H */