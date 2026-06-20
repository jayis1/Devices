/*
 * green_protocol.h — GreenPulse shared Sub-GHz mesh protocol definitions
 *
 * Defines the message types, payload structs, and mesh model IDs
 * shared across all GreenPulse nodes (plant tag, leaf scanner, hub, valve).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef GREEN_PROTOCOL_H
#define GREEN_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---- Node IDs ---- */
#define GP_NODE_ID_HUB         0x00
#define GP_NODE_ID_PLANT_TAG   0x01  /* range 0x01-0x3F (63 tags max) */
#define GP_NODE_ID_LEAF_SCANNER 0x40
#define GP_NODE_ID_WATER_VALVE 0x50  /* range 0x50-0x57 (8 zones max) */

/* ---- Mesh vendor model IDs (SIG Mesh vendor model range) ---- */
#define GP_VENDOR_ID             0x05F3   /* placeholder OUI */
#define GP_MODEL_ID_TELEMETRY    0x0001
#define GP_MODEL_ID_WATERING     0x0002
#define GP_MODEL_ID_SCAN_RESULT  0x0003
#define GP_MODEL_ID_ALERT        0x0004
#define GP_MODEL_ID_HEARTBEAT    0x0005
#define GP_MODEL_ID_COMMAND      0x0006

/* ---- Message types ---- */
typedef enum {
    GP_MSG_TELEMETRY    = 0x10, /* plant tag -> hub: soil, light, temp, humidity, battery */
    GP_MSG_WATERING_CMD = 0x20, /* hub -> valve: open valve for duration */
    GP_MSG_WATERING_ACK = 0x21, /* valve -> hub: watering result (liters, status) */
    GP_MSG_SCAN_RESULT  = 0x30, /* scanner -> hub/cloud: species + disease pre-screen */
    GP_MSG_ALERT        = 0x40, /* any -> hub: disease/pest/low-batt/flood (flood) */
    GP_MSG_STRESS_SCORE = 0x50, /* hub -> mesh: broadcast disease risk per plant */
    GP_MSG_HEARTBEAT    = 0xF0, /* keep-alive + battery level */
} gp_msg_type_t;

/* ---- Alert flags ---- */
#define GP_ALERT_LOW_MOISTURE    0x01  /* soil moisture below species threshold */
#define GP_ALERT_LOW_LIGHT       0x02  /* daily light integral below species minimum */
#define GP_ALERT_HIGH_TEMP       0x04  /* temp above species max */
#define GP_ALERT_LOW_TEMP        0x08  /* temp below species min */
#define GP_ALERT_DISEASE_SUSPECT 0x10  /* scanner flagged leaf as suspect */
#define GP_ALERT_PEST_DETECTED   0x20  /* scanner detected pests */
#define GP_ALERT_LOW_BATT        0x40  /* tag battery < 15% */
#define GP_ALERT_LEAK            0x80  /* valve flow sensor: flow after close */

/* ---- Plant status (computed by hub) ---- */
#define GP_PLANT_OK         0  /* all good */
#define GP_PLANT_WATER_SOON 1  /* moisture declining, water in <48h */
#define GP_PLANT_WATER_NOW  2  /* moisture at threshold, water now */
#define GP_PLANT_LOW_LIGHT 3  /* needs more light */
#define GP_PLANT_DISEASE   4  /* disease/pest detected by scanner */
#define GP_PLANT_STRESS    5  /* temp/light out of species range */

/* ---- Watering status ---- */
#define GP_WATER_OK         0
#define GP_WATER_NO_FLOW    1  /* empty reservoir or blocked */
#define GP_WATER_LEAK       2  /* flow detected after close */
#define GP_WATER_TIMEOUT    3  /* max duration exceeded (safety) */

/* ---- Telemetry payload (plant tag -> hub) ---- */
#define GP_TELEM_INTERVAL_S 900  /* 15 minutes */

typedef struct __attribute__((packed)) {
    uint8_t  type;             /* GP_MSG_TELEMETRY */
    uint8_t  node_id;          /* GP_NODE_ID_PLANT_TAG | tag index */
    uint8_t  seq;
    uint8_t  flags;            /* alert flags bitmask */
    uint8_t  soil_moisture;    /* volumetric water content % (0-100) */
    uint16_t ambient_lux;      /* lux * 10 (0-6553.5) */
    int16_t  temp_centic;      /* soil/air temp centi-degC */
    uint16_t humidity_centi;   /* relative humidity * 100 (%) */
    uint8_t  battery_pct;      /* 0-100 */
    uint8_t  plant_profile_id; /* species profile index (0-255) */
    uint16_t crc16;
} gp_telemetry_payload_t;  /* 14 bytes */

/* ---- Watering command (hub -> valve) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* GP_MSG_WATERING_CMD */
    uint8_t  node_id;          /* GP_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  zone;             /* valve zone (0-7) */
    uint8_t  emitter_id;       /* drip emitter (0=zone-level, 1-N per plant) */
    uint16_t duration_s;       /* watering duration (seconds) */
    uint16_t target_ml;        /* target volume (ml) — 0 = duration only */
    uint16_t crc16;
} gp_watering_cmd_payload_t;  /* 11 bytes */

/* ---- Watering ack (valve -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* GP_MSG_WATERING_ACK */
    uint8_t  node_id;          /* GP_NODE_ID_WATER_VALVE | zone */
    uint8_t  seq;
    uint8_t  flags;            /* GP_ALERT_LEAK if flow after close */
    uint8_t  status;           /* GP_WATER_* */
    uint16_t ml_delivered;     /* actual liters delivered (ml) */
    uint16_t duration_s;       /* actual duration */
    uint16_t crc16;
} gp_watering_ack_payload_t;  /* 10 bytes */

/* ---- Scan result (scanner -> hub/cloud) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* GP_MSG_SCAN_RESULT */
    uint8_t  node_id;          /* GP_NODE_ID_LEAF_SCANNER */
    uint8_t  seq;
    uint8_t  flags;            /* GP_ALERT_DISEASE_SUSPECT / GP_ALERT_PEST_DETECTED */
    uint8_t  plant_tag_id;     /* which tag the scan is for (0 = unpaired) */
    uint8_t  species_id_lo;    /* species ID (16-bit, low byte) */
    uint8_t  species_id_hi;    /* species ID (16-bit, high byte) */
    uint8_t  species_conf;     /* 0-100 */
    uint8_t  disease_class;    /* 0=healthy 1=mildew 2=leaf_spot 3=rust 4=root_rot 5=pest */
    uint8_t  disease_conf;     /* 0-100 */
    uint8_t  pest_count;       /* detected pest count (0=none) */
    uint16_t crc16;
} gp_scan_result_payload_t;  /* 13 bytes */

/* ---- Alert payload (any -> hub, mesh-flood) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* GP_MSG_ALERT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;            /* which alert(s) */
    uint8_t  plant_tag_id;    /* affected plant (0 = system-wide) */
    uint16_t value;           /* e.g. moisture %, pest count, 0 */
    uint16_t crc16;
} gp_alert_payload_t;  /* 9 bytes */

/* ---- Stress/disease risk score (hub -> mesh broadcast) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* GP_MSG_STRESS_SCORE */
    uint8_t  node_id;          /* GP_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  plant_tag_id;    /* which plant */
    uint8_t  disease_risk;     /* 0-100 (3-day forecast) */
    uint8_t  water_risk;      /* 0-100 (wilt risk if not watered) */
    uint8_t  light_risk;      /* 0-100 (light deficiency risk) */
    uint8_t  status;          /* GP_PLANT_* */
    uint16_t hours_to_water;  /* hours until watering needed (0xFFFF = not needed) */
    uint16_t crc16;
} gp_stress_score_payload_t;  /* 14 bytes */

/* ---- Heartbeat payload ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* GP_MSG_HEARTBEAT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  battery_pct;
    uint8_t  state;            /* 0=normal 1=charging 2=low_batt 3=error */
    uint16_t crc16;
} gp_heartbeat_payload_t;  /* 8 bytes */

/* ---- Function prototypes ---- */
uint16_t gp_crc16(const uint8_t *data, size_t len);
uint16_t gp_pack_crc(void *payload, size_t struct_size_without_crc);
int      gp_verify_crc(const void *payload, size_t struct_size_without_crc,
                       uint16_t received_crc);

/* ---- Mesh transport abstraction ---- */
typedef void (*gp_mesh_rx_cb_t)(uint8_t type, const uint8_t *data, size_t len);
typedef int  (*gp_mesh_tx_t)(const uint8_t *data, size_t len);

void gp_mesh_set_tx(gp_mesh_tx_t tx_func);
void gp_mesh_set_rx_callback(gp_mesh_rx_cb_t cb);
int  gp_mesh_send(uint8_t msg_type, uint8_t node_id,
                  const void *payload, size_t payload_len);
void gp_mesh_on_rx(const uint8_t *data, size_t len);

/* ---- Convenience senders ---- */
void gp_send_telemetry(uint8_t tag_id, uint8_t soil, uint16_t lux,
                       int16_t temp, uint16_t humidity, uint8_t batt,
                       uint8_t profile, uint8_t flags);
void gp_send_watering_cmd(uint8_t zone, uint8_t emitter, uint16_t duration,
                          uint16_t target_ml);
void gp_send_watering_ack(uint8_t zone, uint8_t status, uint16_t ml,
                          uint16_t duration, uint8_t flags);
void gp_send_scan_result(uint8_t tag_id, uint16_t species, uint8_t spec_conf,
                         uint8_t disease, uint8_t dis_conf, uint8_t pests,
                         uint8_t flags);
void gp_send_stress_score(uint8_t tag_id, uint8_t disease_risk, uint8_t water_risk,
                          uint8_t light_risk, uint8_t status, uint16_t hours);

/* ---- Plant species care profile (edge-resident subset) ---- */
typedef struct {
    uint8_t  profile_id;
    uint8_t  min_moisture;     /* % — water when soil drops below this */
    uint8_t  max_moisture;     /* % — overwatering risk above this */
    uint16_t min_dli_lux_hr;   /* minimum daily light integral (lux·hr/10) */
    int16_t  temp_min_centic;  /* minimum temp (centi-degC) */
    int16_t  temp_max_centic;  /* maximum temp (centi-degC) */
    uint8_t  humidity_min;     /* % minimum humidity */
    uint8_t  watering_interval_h; /* suggested watering interval (hours) */
} gp_plant_profile_t;

/* A small built-in table of common houseplant care profiles.
 * The full 4,000-species DB lives in the cloud; the hub caches the
 * profiles for currently-paired plants. */
#define GP_PROFILE_MONSTERA      1
#define GP_PROFILE_CALATHEA     2
#define GP_PROFILE_FIDDLE_LEAF  3
#define GP_PROFILE_SNAKE_PLANT  4
#define GP_PROFILE_POTHOS        5
#define GP_PROFILE_CACTUS        6
#define GP_PROFILE_FERN          7
#define GP_PROFILE_ORCHID        8
#define GP_PROFILE_PHILODENDRON 9
#define GP_PROFILE_ZZ_PLANT     10
#define GP_PROFILE_SPIDER_PLANT 11
#define GP_PROFILE_RUBBER_TREE  12
#define GP_PROFILE_PEPEROMIA    13
#define GP_PROFILE_AGLAONEMA    14
#define GP_PROFILE_DIEFFENBACHIA 15
#define GP_PROFILE_ANTHURIUM    16

const gp_plant_profile_t *gp_get_profile(uint8_t profile_id);

#endif /* GREEN_PROTOCOL_H */