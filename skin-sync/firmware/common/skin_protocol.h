/*
 * skin_protocol.h — SkinSync shared Sub-GHz mesh protocol definitions
 *
 * Defines the message types, payload structs, and mesh model IDs
 * shared across all SkinSync nodes (UV patch, skin scanner, hub, dispenser).
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef SKIN_PROTOCOL_H
#define SKIN_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ---- Node IDs ---- */
#define SS_NODE_ID_HUB         0x00
#define SS_NODE_ID_UV_PATCH    0x01  /* range 0x01-0x0F (15 patches max) */
#define SS_NODE_ID_SCANNER     0x40
#define SS_NODE_ID_DISPENSER   0x50  /* range 0x50-0x57 (8 dispensers max) */

/* ---- Mesh vendor model IDs ---- */
#define SS_VENDOR_ID             0x05F4   /* placeholder OUI */
#define SS_MODEL_ID_TELEMETRY    0x0001
#define SS_MODEL_ID_DISPENSE     0x0002
#define SS_MODEL_ID_SCAN_RESULT  0x0003
#define SS_MODEL_ID_ALERT        0x0004
#define SS_MODEL_ID_HEARTBEAT    0x0005
#define SS_MODEL_ID_COMMAND      0x0006

/* ---- Message types ---- */
typedef enum {
    SS_MSG_TELEMETRY    = 0x10, /* UV patch -> hub: UVA/UVB dose, skin temp, battery */
    SS_MSG_DISPENSE_CMD = 0x20, /* hub -> dispenser: dispense product + amount */
    SS_MSG_DISPENSE_ACK = 0x21, /* dispenser -> hub: dispense result */
    SS_MSG_SCAN_RESULT  = 0x30, /* scanner -> hub/cloud: condition + ABCDE pre-screen */
    SS_MSG_ALERT        = 0x40, /* any -> hub: UV warning / lesion change / low product */
    SS_MSG_RISK_SCORE   = 0x50, /* hub -> mesh: broadcast UV + skin cancer risk per user */
    SS_MSG_HEARTBEAT    = 0xF0, /* keep-alive + battery level */
} ss_msg_type_t;

/* ---- Alert flags ---- */
#define SS_ALERT_MED_50     0x01  /* UV dose reached 50% of personal MED */
#define SS_ALERT_MED_70     0x02  /* UV dose reached 70% of personal MED — seek shade */
#define SS_ALERT_MED_90     0x04  /* UV dose reached 90% of personal MED — burning imminent */
#define SS_ALERT_FLUSH      0x08  /* skin temp rise >2°C (possible burn onset) */
#define SS_ALERT_LOW_BATT   0x10  /* patch battery < 15% */
#define SS_ALERT_LOW_PRODUCT 0x20 /* dispenser cartridge < 15% remaining */
#define SS_ALERT_LESION     0x40  /* scanner detected lesion change (ABCDE) */
#define SS_ALERT_CONDITION  0x80  /* scanner detected skin condition requiring attention */

/* ---- UV risk status (computed by hub) ---- */
#define SS_UV_SAFE          0  /* MED < 50% */
#define SS_UV_CAUTION       1  /* MED 50-70% */
#define SS_UV_WARNING       2  /* MED 70-90% */
#define SS_UV_DANGER        3  /* MED > 90% — burn imminent */
#define SS_UV_BURNED        4  /* MED exceeded — damage occurred */

/* ---- Skin condition status ---- */
#define SS_SKIN_NORMAL      0
#define SS_SKIN_MILD        1  /* mild condition detected (e.g. minor acne) */
#define SS_SKIN_ATTENTION   2  /* condition progressing — adjust routine */
#define SS_SKIN_SEE_DERMS   3  /* lesion change / ABCDE suspect — see dermatologist */

/* ---- Dispense status ---- */
#define SS_DISPENSE_OK      0
#define SS_DISPENSE_EMPTY   1  /* cartridge empty */
#define SS_DISPENSE_PARTIAL 2  /* less than requested amount dispensed */
#define SS_DISPENSE_TIMEOUT 3  /* max time exceeded (safety) */

/* ---- Fitzpatrick skin types ---- */
#define SS_FITZ_I    1   /* very fair, always burns, never tans */
#define SS_FITZ_II   2   /* fair, usually burns, tans minimally */
#define SS_FITZ_III  3   /* medium, sometimes burns, tans gradually */
#define SS_FITZ_IV   4   /* olive, rarely burns, tans easily */
#define SS_FITZ_V    5   /* brown, very rarely burns, tans darkly */
#define SS_FITZ_VI   6   /* dark, never burns, tans deeply */

/* ---- Telemetry payload (UV patch -> hub) ---- */
#define SS_TELEM_INTERVAL_ACTIVE_S   60   /* 1 min when outdoor/active */
#define SS_TELEM_INTERVAL_SLEEP_S    300  /* 5 min when indoor/dark */

typedef struct __attribute__((packed)) {
    uint8_t  type;             /* SS_MSG_TELEMETRY */
    uint8_t  node_id;          /* SS_NODE_ID_UV_PATCH | patch index */
    uint8_t  seq;
    uint8_t  flags;            /* alert flags bitmask */
    uint16_t uva_dose_delta;   /* UVA dose since last report (J/m² * 10) */
    uint16_t uvb_dose_delta;   /* UVB dose since last report (J/m² * 10) */
    uint16_t uva_total;        /* cumulative UVA dose today (J/m² * 10) */
    uint16_t uvb_total;        /* cumulative UVB dose today (J/m² * 10) */
    int16_t  skin_temp_centic; /* skin temperature (centi-degC) */
    uint8_t  uv_index;         /* current UV index * 10 (0-30.0) */
    uint8_t  med_fraction;     /* MED fraction used today (0-100) */
    uint8_t  battery_pct;      /* 0-100 */
    uint16_t crc16;
} ss_telemetry_payload_t;  /* 18 bytes */

/* ---- Dispense command (hub -> dispenser) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* SS_MSG_DISPENSE_CMD */
    uint8_t  node_id;          /* SS_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  slot;             /* product slot (0-3) */
    uint16_t amount_mg;        /* amount to dispense in mg (0 = manual) */
    uint8_t  product_id;       /* RFID product ID (for logging) */
    uint16_t crc16;
} ss_dispense_cmd_payload_t;  /* 9 bytes */

/* ---- Dispense ack (dispenser -> hub) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* SS_MSG_DISPENSE_ACK */
    uint8_t  node_id;          /* SS_NODE_ID_DISPENSER | slot */
    uint8_t  seq;
    uint8_t  flags;            /* SS_ALERT_LOW_PRODUCT if < 15% */
    uint8_t  status;           /* SS_DISPENSE_* */
    uint8_t  slot;             /* which slot */
    uint16_t mg_dispensed;     /* actual amount dispensed (mg) */
    uint16_t mg_remaining;     /* product remaining in cartridge (mg) */
    uint16_t crc16;
} ss_dispense_ack_payload_t;  /* 11 bytes */

/* ---- Scan result (scanner -> hub/cloud) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* SS_MSG_SCAN_RESULT */
    uint8_t  node_id;          /* SS_NODE_ID_SCANNER */
    uint8_t  seq;
    uint8_t  flags;            /* SS_ALERT_LESION / SS_ALERT_CONDITION */
    uint8_t  body_location;    /* 0=face 1=left-arm 2=right-arm 3=chest ... */
    uint8_t  condition_class;  /* 0=normal 1=acne 2=hyperpig 3=rosacea 4=eczema ... */
    uint8_t  condition_conf;   /* 0-100 */
    uint8_t  abcde_score;      /* 0-100 (lesion risk; 0 = no lesion) */
    uint8_t  skin_age;         /* estimated skin age (years) */
    uint16_t lesion_id;        /* tracked lesion ID (0 = untracked) */
    uint16_t crc16;
} ss_scan_result_payload_t;  /* 14 bytes */

/* ---- Alert payload (any -> hub, mesh-flood) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* SS_MSG_ALERT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  flags;            /* which alert(s) */
    uint8_t  patch_id;        /* affected patch (0 = system-wide) */
    uint16_t value;           /* e.g. MED fraction, lesion ID, 0 */
    uint16_t crc16;
} ss_alert_payload_t;  /* 9 bytes */

/* ---- Risk score (hub -> mesh broadcast) ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* SS_MSG_RISK_SCORE */
    uint8_t  node_id;          /* SS_NODE_ID_HUB */
    uint8_t  seq;
    uint8_t  flags;
    uint8_t  patch_id;        /* which patch */
    uint8_t  uv_status;       /* SS_UV_* */
    uint8_t  med_fraction;    /* 0-100 */
    uint8_t  skin_cancer_risk;/* 0-100 (annual cumulative risk) */
    uint8_t  skin_status;     /* SS_SKIN_* */
    uint16_t hours_to_burn;   /* hours until burn at current UV (0xFFFF = safe) */
    uint16_t crc16;
} ss_risk_score_payload_t;  /* 14 bytes */

/* ---- Heartbeat payload ---- */
typedef struct __attribute__((packed)) {
    uint8_t  type;             /* SS_MSG_HEARTBEAT */
    uint8_t  node_id;
    uint8_t  seq;
    uint8_t  battery_pct;
    uint8_t  state;            /* 0=normal 1=charging 2=low_batt 3=error */
    uint16_t crc16;
} ss_heartbeat_payload_t;  /* 8 bytes */

/* ---- Function prototypes ---- */
uint16_t ss_crc16(const uint8_t *data, size_t len);
uint16_t ss_pack_crc(void *payload, size_t struct_size_without_crc);
int      ss_verify_crc(const void *payload, size_t struct_size_without_crc,
                       uint16_t received_crc);

/* ---- Mesh transport abstraction ---- */
typedef void (*ss_mesh_rx_cb_t)(uint8_t type, const uint8_t *data, size_t len);
typedef int  (*ss_mesh_tx_t)(const uint8_t *data, size_t len);

void ss_mesh_set_tx(ss_mesh_tx_t tx_func);
void ss_mesh_set_rx_callback(ss_mesh_rx_cb_t cb);
int  ss_mesh_send(uint8_t msg_type, uint8_t node_id,
                  const void *payload, size_t payload_len);
void ss_mesh_on_rx(const uint8_t *data, size_t len);

/* ---- Convenience senders ---- */
void ss_send_telemetry(uint8_t patch_id, uint16_t uva_delta, uint16_t uvb_delta,
                       uint16_t uva_total, uint16_t uvb_total,
                       int16_t skin_temp, uint8_t uv_index, uint8_t med_frac,
                       uint8_t batt, uint8_t flags);
void ss_send_dispense_cmd(uint8_t slot, uint16_t amount_mg, uint8_t product_id);
void ss_send_dispense_ack(uint8_t slot, uint8_t status, uint16_t mg_dispensed,
                          uint16_t mg_remaining, uint8_t flags);
void ss_send_scan_result(uint8_t body_loc, uint8_t condition, uint8_t cond_conf,
                         uint8_t abcde, uint8_t skin_age, uint16_t lesion_id,
                         uint8_t flags);
void ss_send_risk_score(uint8_t patch_id, uint8_t uv_status, uint8_t med_frac,
                        uint8_t cancer_risk, uint8_t skin_status, uint16_t hours);

/* ---- Skin type MED reference (ISO 17166 erythema effectiveness) ---- */
/* Minimal Erythema Dose (SED) per Fitzpatrick type in J/m²
 * 1 SED = 100 J/m² effective erythemal dose
 * These are typical MED values; personalized in cloud from burn data */
typedef struct {
    uint8_t  fitz_type;
    uint16_t med_jm2;        /* minimal erythema dose in J/m² */
    uint16_t typical_burn_min_july; /* minutes to burn in July noon UV index 10 */
} ss_fitz_profile_t;

const ss_fitz_profile_t *ss_get_fitz_profile(uint8_t fitz_type);

/* ---- Erythema effectiveness function (ISO 17166 / CIE S 007) ---- */
/* Converts UV irradiance to effective erythemal dose */
float ss_erythema_weighted_dose(float uva_wm2, float uvb_wm2, float seconds);

/* ---- Condition class names ---- */
#define SS_COND_NORMAL           0
#define SS_COND_ACNE_COMEDONAL   1
#define SS_COND_ACNE_INFLAM      2
#define SS_COND_ACNE_CYSTIC      3
#define SS_COND_MELASMA          4
#define SS_COND_PIH              5   /* post-inflammatory hyperpigmentation */
#define SS_COND_SOLAR_LENTIGINES 6   /* sun spots */
#define SS_COND_ROSACEA_ERYTH    7   /* erythematotelangiectatic */
#define SS_COND_ROSACEA_PAPUL    8   /* papulopustular */
#define SS_COND_ECZEMA           9
#define SS_COND_SEBORRHEIC_DERM  10
#define SS_COND_ACTINIC_KERATOSIS 11  /* pre-cancer */
#define SS_COND_BCC_SIGN         12  /* basal cell carcinoma signs */
#define SS_COND_SCC_SIGN         13  /* squamous cell carcinoma signs */
#define SS_COND_MELANOMA_SIGN    14
#define SS_COND_VITILIGO         15
#define SS_COND_FUNGAL_ACNE      16
#define SS_COND_DERMATITIS       17
#define SS_COND_PSORIASIS_FACIAL 18
#define SS_COND_PERIORAL_DERM    19
#define SS_COND_FOLLICULITIS     20
#define SS_COND_MILIA            21
#define SS_COND_XEROSIS          22  /* dry skin */
#define SS_COND_KERATOSIS_PILARIS 23
#define SS_COND_BARRIER_DAMAGE   24
#define SS_COND_SEBORRHEIC_KERAT 25  /* benign growth */

#endif /* SKIN_PROTOCOL_H */