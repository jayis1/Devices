/*
 * green_protocol.c — GreenPulse shared Sub-GHz mesh protocol implementation
 *
 * CRC16-CCITT computation, payload CRC packing/verification, mesh send/recv
 * helpers, and the built-in houseplant care-profile table.
 *
 * SPDX-License-Identifier: MIT
 */
#include "green_protocol.h"
#include <string.h>

/* ---- CRC16-CCITT (poly 0x1021, init 0xFFFF, no reflection) ---- */
uint16_t gp_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/*
 * Pack CRC into the last 2 bytes of a payload struct.
 * `struct_size_without_crc` = sizeof(struct) - 2
 * Returns the computed CRC value.
 */
uint16_t gp_pack_crc(void *payload, size_t struct_size_without_crc)
{
    uint16_t crc = gp_crc16((const uint8_t *)payload, struct_size_without_crc);
    uint8_t *p = (uint8_t *)payload + struct_size_without_crc;
    p[0] = (uint8_t)(crc & 0xFF);
    p[1] = (uint8_t)((crc >> 8) & 0xFF);
    return crc;
}

/*
 * Verify the CRC of a received payload.
 * Returns 0 if valid, -1 if mismatch.
 */
int gp_verify_crc(const void *payload, size_t struct_size_without_crc,
                   uint16_t received_crc)
{
    uint16_t computed = gp_crc16((const uint8_t *)payload, struct_size_without_crc);
    return (computed == received_crc) ? 0 : -1;
}

/* ---- Mesh transport abstraction ---- */
static gp_mesh_tx_t     mesh_tx_func;
static gp_mesh_rx_cb_t  mesh_rx_cb;

void gp_mesh_set_tx(gp_mesh_tx_t tx_func)      { mesh_tx_func = tx_func; }
void gp_mesh_set_rx_callback(gp_mesh_rx_cb_t cb) { mesh_rx_cb = cb; }

int gp_mesh_send(uint8_t msg_type, uint8_t node_id,
                  const void *payload, size_t payload_len)
{
    (void)msg_type; (void)node_id;
    if (!mesh_tx_func) return -1;
    return mesh_tx_func((const uint8_t *)payload, payload_len);
}

void gp_mesh_on_rx(const uint8_t *data, size_t len)
{
    if (!mesh_rx_cb || len < 1) return;
    uint8_t type = data[0];
    mesh_rx_cb(type, data, len);
}

/* ---- Convenience senders ---- */

void gp_send_telemetry(uint8_t tag_id, uint8_t soil, uint16_t lux,
                       int16_t temp, uint16_t humidity, uint8_t batt,
                       uint8_t profile, uint8_t flags)
{
    gp_telemetry_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = GP_MSG_TELEMETRY;
    p.node_id = tag_id;
    p.seq = 0; /* caller sets via state */
    p.flags = flags;
    p.soil_moisture = soil;
    p.ambient_lux = lux;
    p.temp_centic = temp;
    p.humidity_centic = humidity;
    p.battery_pct = batt;
    p.plant_profile_id = profile;
    gp_pack_crc(&p, sizeof(p) - 2);
    gp_mesh_send(GP_MSG_TELEMETRY, tag_id, &p, sizeof(p));
}

void gp_send_watering_cmd(uint8_t zone, uint8_t emitter, uint16_t duration,
                          uint16_t target_ml)
{
    gp_watering_cmd_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = GP_MSG_WATERING_CMD;
    p.node_id = GP_NODE_ID_HUB;
    p.zone = zone;
    p.emitter_id = emitter;
    p.duration_s = duration;
    p.target_ml = target_ml;
    gp_pack_crc(&p, sizeof(p) - 2);
    gp_mesh_send(GP_MSG_WATERING_CMD, GP_NODE_ID_WATER_VALVE | zone, &p, sizeof(p));
}

void gp_send_watering_ack(uint8_t zone, uint8_t status, uint16_t ml,
                          uint16_t duration, uint8_t flags)
{
    gp_watering_ack_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = GP_MSG_WATERING_ACK;
    p.node_id = GP_NODE_ID_WATER_VALVE | zone;
    p.flags = flags;
    p.status = status;
    p.ml_delivered = ml;
    p.duration_s = duration;
    gp_pack_crc(&p, sizeof(p) - 2);
    gp_mesh_send(GP_MSG_WATERING_ACK, GP_NODE_ID_WATER_VALVE | zone, &p, sizeof(p));
}

void gp_send_scan_result(uint8_t tag_id, uint16_t species, uint8_t spec_conf,
                         uint8_t disease, uint8_t dis_conf, uint8_t pests,
                         uint8_t flags)
{
    gp_scan_result_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = GP_MSG_SCAN_RESULT;
    p.node_id = GP_NODE_ID_LEAF_SCANNER;
    p.flags = flags;
    p.plant_tag_id = tag_id;
    p.species_id_lo = (uint8_t)(species & 0xFF);
    p.species_id_hi = (uint8_t)((species >> 8) & 0xFF);
    p.species_conf = spec_conf;
    p.disease_class = disease;
    p.disease_conf = dis_conf;
    p.pest_count = pests;
    gp_pack_crc(&p, sizeof(p) - 2);
    gp_mesh_send(GP_MSG_SCAN_RESULT, GP_NODE_ID_LEAF_SCANNER, &p, sizeof(p));
}

void gp_send_stress_score(uint8_t tag_id, uint8_t disease_risk, uint8_t water_risk,
                          uint8_t light_risk, uint8_t status, uint16_t hours)
{
    gp_stress_score_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = GP_MSG_STRESS_SCORE;
    p.node_id = GP_NODE_ID_HUB;
    p.plant_tag_id = tag_id;
    p.disease_risk = disease_risk;
    p.water_risk = water_risk;
    p.light_risk = light_risk;
    p.status = status;
    p.hours_to_water = hours;
    gp_pack_crc(&p, sizeof(p) - 2);
    gp_mesh_send(GP_MSG_STRESS_SCORE, GP_NODE_ID_HUB, &p, sizeof(p));
}

/* ---- Plant species care profile table ---- */
static const gp_plant_profile_t profile_table[] = {
    { 0, 0, 0, 0, 0, 0, 0, 0 }, /* unused profile 0 */
    /* Monstera deliciosa — water when top 50% dry, bright indirect */
    { GP_PROFILE_MONSTERA, 35, 80, 3000, 1500, 3500, 40, 168 },
    /* Calathea — keep moist (50%+), high humidity, low-medium light */
    { GP_PROFILE_CALATHEA, 50, 85, 1500, 1600, 3000, 60, 96 },
    /* Fiddle Leaf Fig — water when top dry, very bright light */
    { GP_PROFILE_FIDDLE_LEAF, 30, 75, 5000, 1200, 3000, 30, 168 },
    /* Snake Plant — drought tolerant, low to bright light */
    { GP_PROFILE_SNAKE_PLANT, 15, 60, 800, 1000, 3500, 20, 360 },
    /* Pothos — water when top 50% dry, low to medium light */
    { GP_PROFILE_POTHOS, 30, 80, 1200, 1500, 3200, 30, 168 },
    /* Cactus — very dry, very bright light */
    { GP_PROFILE_CACTUS, 10, 40, 4000, 500, 4000, 10, 504 },
    /* Fern — keep moist, high humidity, indirect light */
    { GP_PROFILE_FERN, 55, 90, 1500, 1500, 2800, 60, 72 },
    /* Orchid — water weekly, bright indirect, 50-70% humidity */
    { GP_PROFILE_ORCHID, 40, 75, 3000, 1500, 3000, 50, 168 },
    /* Philodendron — water when top 50% dry, medium indirect */
    { GP_PROFILE_PHILODENDRON, 35, 80, 2000, 1500, 3200, 40, 168 },
    /* ZZ Plant — drought tolerant, low to bright light */
    { GP_PROFILE_ZZ_PLANT, 20, 65, 1000, 1500, 3500, 25, 288 },
    /* Spider Plant — water when top dry, bright indirect */
    { GP_PROFILE_SPIDER_PLANT, 35, 80, 2500, 1000, 3200, 40, 144 },
    /* Rubber Tree — water when top dry, bright indirect */
    { GP_PROFILE_RUBBER_TREE, 35, 80, 3000, 1500, 3200, 40, 168 },
    /* Peperomia — water when mostly dry, medium light */
    { GP_PROFILE_PEPEROMIA, 25, 70, 2000, 1500, 3000, 40, 216 },
    /* Aglaonema — water when top dry, low to medium light */
    { GP_PROFILE_AGLAONEMA, 35, 80, 1500, 1500, 3200, 40, 168 },
    /* Dieffenbachia — water when top dry, medium indirect */
    { GP_PROFILE_DIEFFENBACHIA, 35, 80, 2000, 1500, 3200, 40, 168 },
    /* Anthurium — keep moist, bright indirect, high humidity */
    { GP_PROFILE_ANTHURIUM, 45, 85, 3000, 1600, 3200, 55, 120 },
};

const gp_plant_profile_t *gp_get_profile(uint8_t profile_id)
{
    if (profile_id == 0 || profile_id > GP_PROFILE_ANTHURIUM) return NULL;
    /* profile_id maps to array index (table[0] is unused) */
    if (profile_id < (uint8_t)(sizeof(profile_table) / sizeof(profile_table[0])))
        return &profile_table[profile_id];
    return NULL;
}

/* ---- Status name helpers (for logging/display) ---- */
static const char *const status_names[] = {
    "ok", "water_soon", "water_now", "low_light",
    "disease", "stress"
};

const char *gp_status_name(uint8_t status)
{
    if (status < 6) return status_names[status];
    return "unknown";
}

static const char *const disease_names[] = {
    "healthy", "powdery_mildew", "leaf_spot", "rust",
    "root_rot_sign", "pest_infestation"
};

const char *gp_disease_name(uint8_t cls)
{
    if (cls < 6) return disease_names[cls];
    return "unknown";
}