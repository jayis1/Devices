/*
 * trail_protocol.c — TrailSync shared protocol implementation
 *
 * CRC16, packing, mesh transport, and convenience functions.
 *
 * SPDX-License-Identifier: MIT
 */
#include "trail_protocol.h"
#include <string.h>

/* ---- CRC16-CCITT (poly 0x1021, init 0xFFFF) ---- */
uint16_t ts_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

uint16_t ts_pack_crc(void *payload, size_t struct_size_without_crc)
{
    uint8_t *p = (uint8_t *)payload;
    uint16_t crc = ts_crc16(p, struct_size_without_crc);
    /* CRC goes at the end of the struct */
    memcpy(p + struct_size_without_crc, &crc, sizeof(crc));
    return crc;
}

int ts_verify_crc(const void *payload, size_t struct_size_without_crc,
                  uint16_t received_crc)
{
    uint16_t computed = ts_crc16((const uint8_t *)payload, struct_size_without_crc);
    return (computed == received_crc) ? 0 : -1;
}

/* ---- Mesh transport state ---- */
static ts_mesh_tx_t mesh_tx_func = NULL;
static ts_mesh_rx_cb_t mesh_rx_cb = NULL;

void ts_mesh_set_tx(ts_mesh_tx_t tx_func)
{
    mesh_tx_func = tx_func;
}

void ts_mesh_set_rx_callback(ts_mesh_rx_cb_t cb)
{
    mesh_rx_cb = cb;
}

int ts_mesh_send(uint8_t msg_type, uint8_t node_id,
                 const void *payload, size_t payload_len)
{
    if (!mesh_tx_func) return -1;
    /* Prepend type + node_id header */
    uint8_t buf[256];
    buf[0] = msg_type;
    buf[1] = node_id;
    if (payload_len > 0 && payload != NULL) {
        memcpy(buf + 2, payload, payload_len);
    }
    return mesh_tx_func(buf, 2 + payload_len);
}

void ts_mesh_on_rx(const uint8_t *data, size_t len)
{
    if (!mesh_rx_cb || len < 2) return;
    uint8_t msg_type = data[0];
    /* node_id = data[1]; */
    mesh_rx_cb(msg_type, data + 2, len - 2);
}

/* ---- LoRa transport state ---- */
static ts_lora_tx_t lora_tx_func = NULL;

void ts_lora_set_tx(ts_lora_tx_t tx_func)
{
    lora_tx_func = tx_func;
}

int ts_lora_send(uint8_t msg_type, uint8_t node_id,
                 const void *payload, size_t payload_len)
{
    if (!lora_tx_func) return -1;
    uint8_t buf[64]; /* LoRa frames are shorter */
    buf[0] = msg_type;
    buf[1] = node_id;
    if (payload_len > 0 && payload != NULL && payload_len <= 60) {
        memcpy(buf + 2, payload, payload_len);
    }
    return lora_tx_func(buf, 2 + payload_len);
}

/* ---- Convenience senders ---- */
void ts_send_gait(uint8_t pod_side, uint8_t terrain, uint8_t gait_class,
                  uint8_t gait_conf, int16_t cadence, int16_t contact_ms,
                  int16_t vert_osc, int16_t impact, int16_t pronation,
                  int16_t asymmetry, int16_t stride_cm, uint8_t batt,
                  uint8_t flags)
{
    ts_gait_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = TS_MSG_GAIT;
    p.node_id = TS_NODE_ID_SHOE_POD;
    p.side = pod_side;
    p.seq = 0; /* filled by caller or auto-increment */
    p.flags = flags;
    p.terrain = terrain;
    p.gait_class = gait_class;
    p.gait_conf = gait_conf;
    p.cadence = cadence;
    p.ground_contact_ms = contact_ms;
    p.vertical_osc_mm = vert_osc;
    p.impact_load_pct = impact;
    p.pronation_deg = pronation;
    p.asymmetry_pct = asymmetry;
    p.stride_length_cm = stride_cm;
    p.battery_pct = batt;
    ts_pack_crc(&p, sizeof(p) - sizeof(uint16_t));
    ts_mesh_send(TS_MSG_GAIT, TS_NODE_ID_SHOE_POD, &p, sizeof(p));
}

void ts_send_telemetry(int32_t lat, int32_t lon, int16_t alt, int16_t speed,
                       uint16_t dist, uint8_t hr, uint8_t spo2, int16_t hrv,
                       int16_t skin_temp, int16_t pressure, uint8_t batt,
                       uint8_t sats, uint8_t flags)
{
    ts_telemetry_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = TS_MSG_TELEMETRY;
    p.node_id = TS_NODE_ID_WRIST;
    p.flags = flags;
    p.lat_deg1e5 = lat;
    p.lon_deg1e5 = lon;
    p.altitude_dm = alt;
    p.speed_cm_s = speed;
    p.distance_dm = dist;
    p.hr = hr;
    p.spo2 = spo2;
    p.hrv_rmssd = hrv;
    p.skin_temp_centic = skin_temp;
    p.pressure_hpa = pressure;
    p.battery_pct = batt;
    p.num_satellites = sats;
    ts_pack_crc(&p, sizeof(p) - sizeof(uint16_t));
    ts_mesh_send(TS_MSG_TELEMETRY, TS_NODE_ID_WRIST, &p, sizeof(p));
}

void ts_send_sos(uint8_t sos_type, uint8_t severity, int32_t lat, int32_t lon,
                 int16_t alt, uint8_t hr, uint8_t spo2, int16_t hrv,
                 uint8_t injury, uint8_t num_people, uint16_t bearing)
{
    ts_sos_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = TS_MSG_SOS;
    p.node_id = TS_NODE_ID_WRIST;
    p.sos_type = sos_type;
    p.severity = severity;
    p.lat_deg1e5 = lat;
    p.lon_deg1e5 = lon;
    p.altitude_dm = alt;
    p.hr = hr;
    p.spo2 = spo2;
    p.hrv_rmssd = hrv;
    p.injury_class = injury;
    p.num_people = num_people;
    p.bearing_trail_m = bearing;
    ts_pack_crc(&p, sizeof(p) - sizeof(uint16_t));
    /* SOS goes on both Sub-GHz mesh AND LoRa for maximum reach */
    ts_mesh_send(TS_MSG_SOS, TS_NODE_ID_WRIST, &p, sizeof(p));
    ts_lora_send(TS_MSG_SOS, TS_NODE_ID_WRIST, &p, sizeof(p));
}

void ts_send_injury_alert(uint8_t injury_class, uint8_t risk_pct,
                          int16_t asymmetry, int16_t impact, uint8_t terrain)
{
    ts_injury_alert_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = TS_MSG_INJURY_ALERT;
    p.node_id = TS_NODE_ID_WRIST;
    p.flags = TS_ALERT_INJURY_RISK;
    p.injury_class = injury_class;
    p.risk_pct = risk_pct;
    p.asymmetry_pct = asymmetry;
    p.impact_load_pct = impact;
    p.terrain = terrain;
    ts_pack_crc(&p, sizeof(p) - sizeof(uint16_t));
    ts_mesh_send(TS_MSG_INJURY_ALERT, TS_NODE_ID_WRIST, &p, sizeof(p));
}

void ts_send_storm_alert(uint8_t severity, int16_t pressure, int16_t delta,
                         uint8_t hours)
{
    ts_storm_alert_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = TS_MSG_STORM_ALERT;
    p.node_id = TS_NODE_ID_WRIST;
    p.severity = severity;
    p.pressure_hpa = pressure;
    p.pressure_delta = delta;
    p.hours_to_storm = hours;
    ts_pack_crc(&p, sizeof(p) - sizeof(uint16_t));
    ts_mesh_send(TS_MSG_STORM_ALERT, TS_NODE_ID_WRIST, &p, sizeof(p));
    ts_lora_send(TS_MSG_STORM_ALERT, TS_NODE_ID_WRIST, &p, sizeof(p));
}

/* ---- Name lookups ---- */
const char *ts_gait_class_name(uint8_t gait_class)
{
    switch (gait_class) {
    case 0: return "normal";
    case 1: return "asymmetric";
    case 2: return "overpronating";
    case 3: return "high-impact";
    default: return "unknown";
    }
}

const char *ts_injury_name(uint8_t injury_class)
{
    switch (injury_class) {
    case TS_INJURY_NONE:         return "none";
    case TS_INJURY_IT_BAND:      return "IT band syndrome";
    case TS_INJURY_PLANTAR:      return "plantar fasciitis";
    case TS_INJURY_ACHILLES:     return "Achilles tendinopathy";
    case TS_INJURY_STRESS_FX:   return "stress fracture";
    case TS_INJURY_SHIN_SPLINT: return "shin splints";
    case TS_INJURY_RUNNERS_KNEE: return "runner's knee";
    case TS_INJURY_ANKLE_SPRAIN: return "ankle sprain";
    case TS_INJURY_HAMSTRING:   return "hamstring strain";
    case TS_INJURY_HIP_FLEXOR:  return "hip flexor strain";
    case TS_INJURY_CALF_STRAIN:  return "calf strain";
    case TS_INJURY_PATELLAR:    return "patellar tendinopathy";
    default: return "unknown";
    }
}

const char *ts_terrain_name(uint8_t terrain)
{
    switch (terrain) {
    case TS_TERRAIN_ROAD:   return "road";
    case TS_TERRAIN_GRAVEL: return "gravel";
    case TS_TERRAIN_DIRT:   return "dirt";
    case TS_TERRAIN_MUD:    return "mud";
    case TS_TERRAIN_SNOW:   return "snow";
    case TS_TERRAIN_ICE:    return "ice";
    case TS_TERRAIN_ROCK:   return "rock";
    case TS_TERRAIN_SAND:   return "sand";
    default: return "unknown";
    }
}

const char *ts_trail_diff_name(uint8_t difficulty)
{
    switch (difficulty) {
    case TS_TRAIL_EASY:      return "easy";
    case TS_TRAIL_MODERATE:  return "moderate";
    case TS_TRAIL_DIFFICULT: return "difficult";
    case TS_TRAIL_EXPERT:    return "expert";
    case TS_TRAIL_HAZARDOUS: return "hazardous";
    default: return "unknown";
    }
}