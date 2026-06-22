/*
 * skin_protocol.c — SkinSync shared Sub-GHz mesh protocol implementation
 *
 * CRC16-CCITT computation, payload CRC packing/verification, mesh send/recv
 * helpers, Fitzpatrick MED table, and erythema effectiveness calculation.
 *
 * SPDX-License-Identifier: MIT
 */
#include "skin_protocol.h"
#include <string.h>

/* ---- CRC16-CCITT (poly 0x1021, init 0xFFFF, no reflection) ---- */
uint16_t ss_crc16(const uint8_t *data, size_t len)
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

uint16_t ss_pack_crc(void *payload, size_t struct_size_without_crc)
{
    uint16_t crc = ss_crc16((const uint8_t *)payload, struct_size_without_crc);
    uint8_t *p = (uint8_t *)payload + struct_size_without_crc;
    p[0] = (uint8_t)(crc & 0xFF);
    p[1] = (uint8_t)((crc >> 8) & 0xFF);
    return crc;
}

int ss_verify_crc(const void *payload, size_t struct_size_without_crc,
                   uint16_t received_crc)
{
    uint16_t computed = ss_crc16((const uint8_t *)payload, struct_size_without_crc);
    return (computed == received_crc) ? 0 : -1;
}

/* ---- Mesh transport abstraction ---- */
static ss_mesh_tx_t     mesh_tx_func;
static ss_mesh_rx_cb_t  mesh_rx_cb;

void ss_mesh_set_tx(ss_mesh_tx_t tx_func)      { mesh_tx_func = tx_func; }
void ss_mesh_set_rx_callback(ss_mesh_rx_cb_t cb) { mesh_rx_cb = cb; }

int ss_mesh_send(uint8_t msg_type, uint8_t node_id,
                  const void *payload, size_t payload_len)
{
    (void)msg_type; (void)node_id;
    if (!mesh_tx_func) return -1;
    return mesh_tx_func((const uint8_t *)payload, payload_len);
}

void ss_mesh_on_rx(const uint8_t *data, size_t len)
{
    if (!mesh_rx_cb || len < 1) return;
    uint8_t type = data[0];
    mesh_rx_cb(type, data, len);
}

/* ---- Convenience senders ---- */

void ss_send_telemetry(uint8_t patch_id, uint16_t uva_delta, uint16_t uvb_delta,
                       uint16_t uva_total, uint16_t uvb_total,
                       int16_t skin_temp, uint8_t uv_index, uint8_t med_frac,
                       uint8_t batt, uint8_t flags)
{
    ss_telemetry_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = SS_MSG_TELEMETRY;
    p.node_id = patch_id;
    p.seq = 0;
    p.flags = flags;
    p.uva_dose_delta = uva_delta;
    p.uvb_dose_delta = uvb_delta;
    p.uva_total = uva_total;
    p.uvb_total = uvb_total;
    p.skin_temp_centic = skin_temp;
    p.uv_index = uv_index;
    p.med_fraction = med_frac;
    p.battery_pct = batt;
    ss_pack_crc(&p, sizeof(p) - 2);
    ss_mesh_send(SS_MSG_TELEMETRY, patch_id, &p, sizeof(p));
}

void ss_send_dispense_cmd(uint8_t slot, uint16_t amount_mg, uint8_t product_id)
{
    ss_dispense_cmd_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = SS_MSG_DISPENSE_CMD;
    p.node_id = SS_NODE_ID_HUB;
    p.slot = slot;
    p.amount_mg = amount_mg;
    p.product_id = product_id;
    ss_pack_crc(&p, sizeof(p) - 2);
    ss_mesh_send(SS_MSG_DISPENSE_CMD, SS_NODE_ID_DISPENSER | slot, &p, sizeof(p));
}

void ss_send_dispense_ack(uint8_t slot, uint8_t status, uint16_t mg_dispensed,
                          uint16_t mg_remaining, uint8_t flags)
{
    ss_dispense_ack_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = SS_MSG_DISPENSE_ACK;
    p.node_id = SS_NODE_ID_DISPENSER | slot;
    p.flags = flags;
    p.status = status;
    p.slot = slot;
    p.mg_dispensed = mg_dispensed;
    p.mg_remaining = mg_remaining;
    ss_pack_crc(&p, sizeof(p) - 2);
    ss_mesh_send(SS_MSG_DISPENSE_ACK, SS_NODE_ID_DISPENSER | slot, &p, sizeof(p));
}

void ss_send_scan_result(uint8_t body_loc, uint8_t condition, uint8_t cond_conf,
                         uint8_t abcde, uint8_t skin_age, uint16_t lesion_id,
                         uint8_t flags)
{
    ss_scan_result_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = SS_MSG_SCAN_RESULT;
    p.node_id = SS_NODE_ID_SCANNER;
    p.flags = flags;
    p.body_location = body_loc;
    p.condition_class = condition;
    p.condition_conf = cond_conf;
    p.abcde_score = abcde;
    p.skin_age = skin_age;
    p.lesion_id = lesion_id;
    ss_pack_crc(&p, sizeof(p) - 2);
    ss_mesh_send(SS_MSG_SCAN_RESULT, SS_NODE_ID_SCANNER, &p, sizeof(p));
}

void ss_send_risk_score(uint8_t patch_id, uint8_t uv_status, uint8_t med_frac,
                        uint8_t cancer_risk, uint8_t skin_status, uint16_t hours)
{
    ss_risk_score_payload_t p;
    memset(&p, 0, sizeof(p));
    p.type = SS_MSG_RISK_SCORE;
    p.node_id = SS_NODE_ID_HUB;
    p.patch_id = patch_id;
    p.uv_status = uv_status;
    p.med_fraction = med_frac;
    p.skin_cancer_risk = cancer_risk;
    p.skin_status = skin_status;
    p.hours_to_burn = hours;
    ss_pack_crc(&p, sizeof(p) - 2);
    ss_mesh_send(SS_MSG_RISK_SCORE, SS_NODE_ID_HUB, &p, sizeof(p));
}

/* ---- Fitzpatrick MED reference table ---- */
/* Based on ISO 17166 / dermatological literature.
 * MED values are typical; individual variation is ±30%.
 * Cloud ML personalizes from actual burn data. */
static const ss_fitz_profile_t fitz_table[] = {
    { 0, 0, 0 },               /* unused */
    { SS_FITZ_I,   200,  10 },  /* 200 J/m², ~10 min to burn at UV index 10 */
    { SS_FITZ_II,  250,  15 },  /* 250 J/m², ~15 min */
    { SS_FITZ_III, 350,  25 },  /* 350 J/m², ~25 min */
    { SS_FITZ_IV,  500,  40 },  /* 500 J/m², ~40 min */
    { SS_FITZ_V,   800,  60 },  /* 800 J/m², ~60 min */
    { SS_FITZ_VI,  1200, 90 },  /* 1200 J/m², ~90 min */
};

const ss_fitz_profile_t *ss_get_fitz_profile(uint8_t fitz_type)
{
    if (fitz_type == 0 || fitz_type > SS_FITZ_VI) return NULL;
    if (fitz_type < (uint8_t)(sizeof(fitz_table) / sizeof(fitz_table[0])))
        return &fitz_table[fitz_type];
    return NULL;
}

/* ---- Erythema effectiveness (ISO 17166 / CIE S 007) ---- */
/* The erythema action spectrum peaks at 298nm (UVB) and drops
 * sharply in UVA. Simplified weighting:
 *   UVB (280-320nm): ~1.0 effectiveness (primary burn risk)
 *   UVA (320-400nm): ~0.05 effectiveness (aging, contributes to burn at high dose)
 *
 * In production: use the full CIE S 007 action spectrum table with
 * per-wavelength weighting. This simplified version is for edge compute. */
float ss_erythema_weighted_dose(float uva_wm2, float uvb_wm2, float seconds)
{
    /* Effective erythemal irradiance */
    float eff_uvb = uvb_wm2 * 1.0f;     /* UVB: full weight */
    float eff_uva = uva_wm2 * 0.05f;    /* UVA: 5% weight */
    /* Effective dose in J/m² = W/m² * seconds */
    return (eff_uvb + eff_uva) * seconds;
}

/* ---- Status name helpers (for logging/display) ---- */
static const char *const uv_status_names[] = {
    "safe", "caution", "warning", "danger", "burned"
};

const char *ss_uv_status_name(uint8_t status)
{
    if (status < 5) return uv_status_names[status];
    return "unknown";
}

static const char *const skin_status_names[] = {
    "normal", "mild", "attention", "see_dermatologist"
};

const char *ss_skin_status_name(uint8_t status)
{
    if (status < 4) return skin_status_names[status];
    return "unknown";
}

static const char *const condition_names[] = {
    "normal", "acne_comedonal", "acne_inflammatory", "acne_cystic",
    "melasma", "PIH", "solar_lentigines", "rosacea_erythematous",
    "rosacea_papulopustular", "eczema", "seborrheic_dermatitis",
    "actinic_keratosis", "BCC_sign", "SCC_sign", "melanoma_sign",
    "vitiligo", "fungal_acne", "dermatitis", "psoriasis_facial",
    "perioral_dermatitis", "folliculitis", "milia", "xerosis",
    "keratosis_pilaris", "barrier_damage", "seborrheic_keratosis"
};

const char *ss_condition_name(uint8_t cls)
{
    if (cls < 26) return condition_names[cls];
    return "unknown";
}