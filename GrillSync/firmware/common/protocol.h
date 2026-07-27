/*
 * GrillSync — Protocol Header
 * Binary message encoding/decoding for Sub-GHz mesh network
 */
#ifndef GRILLSYNC_PROTOCOL_H
#define GRILLSYNC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Network constants */
#define GS_SYNC0            0x47  /* 'G' */
#define GS_SYNC1            0x53  /* 'S' */
#define GS_BROADCAST        0xFF
#define GS_MAX_PAYLOAD      240
#define GS_MAX_MSG          256
#define GS_MAX_NODES        16
#define GS_AES_KEY_LEN      16
#define GS_CRC_POLY         0x1021  /* CRC-16-CCITT */

/* Message types */
enum gs_msg_type {
    GS_MSG_JOIN_REQ         = 0x01,
    GS_MSG_JOIN_ACK         = 0x02,
    GS_MSG_TELEMETRY        = 0x03,
    GS_MSG_COMMAND          = 0x04,
    GS_MSG_CMD_ACK          = 0x05,
    GS_MSG_ALERT            = 0x06,
    GS_MSG_OTA_BLOCK        = 0x07,
    GS_MSG_OTA_ACK          = 0x08,
    GS_MSG_HEARTBEAT        = 0x09,
    GS_MSG_MESH_RELAY       = 0x0A,
    GS_MSG_DONENESS_UPDATE  = 0x0B,
    GS_MSG_TIME_SYNC        = 0x0C,
    GS_MSG_CONFIG           = 0x0D,
    GS_MSG_CONFIG_ACK       = 0x0E,
    GS_MSG_COOK_SESSION     = 0x0F,
    GS_MSG_THERMAL_FRAME    = 0x10,
    GS_MSG_SMOKE_QUALITY    = 0x11,
};

/* Telemetry sub-types */
enum gs_telem_subtype {
    GS_TELEM_SENTINEL  = 0x01,
    GS_TELEM_PROBE     = 0x02,
    GS_TELEM_SMOKE     = 0x03,
};

/* Alert types */
enum gs_alert_type {
    GS_ALERT_GAS_LEAK          = 0x01,
    GS_ALERT_FLARE_UP_WARNING  = 0x02,
    GS_ALERT_FLARE_UP_ACTIVE   = 0x03,
    GS_ALERT_GRILL_FIRE        = 0x04,
    GS_ALERT_FOOD_UNDERCOOKED  = 0x05,
    GS_ALERT_FOOD_OVERCOOKED   = 0x06,
    GS_ALERT_PROBE_DISCONNECT  = 0x07,
    GS_ALERT_PROBE_LOW_BATT    = 0x08,
    GS_ALERT_CHILD_IN_ZONE     = 0x09,
    GS_ALERT_PROBE_OVERTEMP    = 0x0A,
    GS_ALERT_SMOKE_CREOSOTE    = 0x0B,
    GS_ALERT_NODE_OFFLINE      = 0x0C,
};

/* Command sub-types */
enum gs_cmd_type {
    GS_CMD_GAS_SHUTOFF    = 0x01,
    GS_CMD_GAS_RESUME     = 0x02,
    GS_CMD_BUZZER_ON      = 0x03,
    GS_CMD_BUZZER_OFF     = 0x04,
    GS_CMD_EMERGENCY_MODE = 0x05,
    GS_CMD_NORMAL_MODE    = 0x06,
    GS_CMD_SET_CONFIG     = 0x07,
    GS_CMD_REBOOT         = 0x08,
    GS_CMD_CALIBRATE      = 0x09,
    GS_CMD_START_COOK     = 0x0A,
    GS_CMD_STOP_COOK      = 0x0B,
    GS_CMD_SET_MEAT_PROF  = 0x0C,
    GS_CMD_SET_TARGET    = 0x0D,
    GS_CMD_LED_RING       = 0x0E,
    GS_CMD_SILENCE        = 0x0F,
};

/* Meat type names for reference */
#define GS_MEAT_NAMES \
    { "Beef","Pork","Chicken","Fish","Lamb","Veal","Game","Custom" }

/* Doneness level names */
#define GS_DONENESS_NAMES \
    { "Raw","Rare","MediumRare","Medium","MediumWell","Well" }

/* USDA minimum safe temperatures (×0.1°C) */
static const int16_t gs_usda_min_temp[8] = {
    628,  /* Beef: 62.8°C / 145°F */
    628,  /* Pork: 62.8°C / 145°F */
    739,  /* Chicken: 73.9°C / 165°F */
    628,  /* Fish: 62.8°C / 145°F */
    628,  /* Lamb: 62.8°C / 145°F */
    628,  /* Veal: 62.8°C / 145°F */
    628,  /* Game: 62.8°C / 145°F */
    0,    /* Custom: user-set */
};

/* Default doneness target temps per meat type (×0.1°C) */
/* [meat_type][doneness_level] — 0 = no target for that level */
static const int16_t gs_doneness_temp[8][6] = {
    /* Raw  Rare   MR     Med    MW     Well */
    {   0,  520,  540,   600,   650,   710 },  /* Beef */
    {   0,    0,  600,   650,   700,   770 },  /* Pork */
    {   0,    0,    0,     0,     0,   740 },  /* Chicken (done only) */
    {   0,  450,  550,   600,     0,     0 },  /* Fish */
    {   0,  520,  570,   630,     0,   710 },  /* Lamb */
    {   0,  540,  570,   630,     0,   710 },  /* Veal */
    {   0,  500,  540,   600,     0,   680 },  /* Game */
    {   0,    0,    0,     0,     0,     0 },  /* Custom */
};

/* Alert type → priority mapping */
static const uint8_t gs_alert_priority[12] = {
    GS_PRIORITY_CRITICAL,  /* Gas leak */
    GS_PRIORITY_HIGH,      /* Flare-up warning */
    GS_PRIORITY_CRITICAL,  /* Flare-up active */
    GS_PRIORITY_CRITICAL,  /* Grill fire */
    GS_PRIORITY_HIGH,      /* Food undercooked */
    GS_PRIORITY_MEDIUM,    /* Food overcooked */
    GS_PRIORITY_MEDIUM,    /* Probe disconnect */
    GS_PRIORITY_LOW,       /* Probe low battery */
    GS_PRIORITY_HIGH,      /* Child in zone */
    GS_PRIORITY_CRITICAL,  /* Probe overtemp */
    GS_PRIORITY_MEDIUM,    /* Smoke creosote */
    GS_PRIORITY_LOW,       /* Node offline */
};

#define IS_CRITICAL_ALERT(a) (gs_alert_priority[(a)-1] == GS_PRIORITY_CRITICAL)
#define IS_HIGH_ALERT(a) (gs_alert_priority[(a)-1] >= GS_PRIORITY_HIGH)

/* Message header (7 bytes) */
typedef struct {
    uint8_t  sync[2];       /* 0x47, 0x53 */
    uint8_t  src;           /* Source node ID */
    uint8_t  dst;           /* Destination (0xFF = broadcast) */
    uint8_t  type;          /* Message type */
    uint16_t msg_id;        /* Sequence number */
} gs_header_t;

/* Full message with CRC */
typedef struct {
    gs_header_t header;
    uint8_t  payload[GS_MAX_PAYLOAD];
    uint8_t  payload_len;
    uint16_t crc;
} gs_message_t;

/* CRC-16-CCITT */
uint16_t gs_crc16(const uint8_t *data, size_t len);

/* Encode message into buffer, returns total length (0 on error) */
size_t gs_encode(const gs_message_t *msg, uint8_t *buf, size_t buf_len);

/* Decode buffer into message, returns 0 on success, -1 on error */
int gs_decode(gs_message_t *msg, const uint8_t *buf, size_t len);

/* Build a telemetry message for grill sentinel (24 bytes payload) */
int gs_build_sentinel_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                             uint8_t battery_v, int16_t surface_max_deci,
                             int16_t surface_avg_deci, uint8_t hot_zone_count,
                             uint16_t gas_ppm, uint8_t gas_lel_pct,
                             uint8_t flame_intensity, uint8_t flame_detected,
                             int16_t ambient_temp_deci, uint16_t humidity_deci,
                             uint16_t acoustic_energy, uint8_t flareup_risk,
                             uint16_t flareup_eta_100ms, uint16_t event_id,
                             int8_t rssi);

/* Build a telemetry message for meat probe (18 bytes payload) */
int gs_build_probe_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                          uint8_t battery_v, uint8_t probe_id, uint8_t meat_type,
                          int16_t temp_tip_deci, int16_t temp_mid_deci,
                          int16_t temp_surface_deci, int16_t temp_ambient_deci,
                          int16_t target_temp_deci, uint8_t doneness_level,
                          uint16_t doneness_eta_10s, int8_t rssi);

/* Build a telemetry message for smoke node (22 bytes payload) */
int gs_build_smoke_telem(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                         uint8_t battery_v, uint16_t pm1_0_deci, uint16_t pm2_5_deci,
                         uint16_t pm10_deci, uint16_t voc_index,
                         uint16_t gas_resistance_100ohm, uint16_t co2eq_ppm,
                         uint8_t smoke_quality, uint8_t flame_intensity,
                         int16_t temp_deci, uint16_t humidity_deci, int8_t rssi);

/* Build a doneness update broadcast (Hub→App/probe) */
int gs_build_doneness_update(gs_message_t *msg, uint8_t src, uint16_t msg_seq,
                              uint8_t probe_id, uint8_t meat_type,
                              uint8_t doneness_level, uint16_t eta_10s,
                              int16_t current_temp_deci, int16_t target_temp_deci);

/* Build a command message */
int gs_build_command(gs_message_t *msg, uint8_t src, uint8_t dst,
                     uint16_t msg_seq, uint8_t cmd_type,
                     const uint8_t *cmd_data, uint8_t cmd_len);

/* Build an alert message */
int gs_build_alert(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                   uint8_t alert_type, uint8_t severity,
                   const uint8_t *alert_data, uint8_t data_len);

/* Build a thermal frame message (compressed) */
int gs_build_thermal_frame(gs_message_t *msg, uint8_t node_id, uint16_t msg_seq,
                            uint8_t frame_seq, const uint8_t *compressed,
                            uint8_t compressed_len, int16_t max_deci,
                            int16_t avg_deci, uint8_t hot_zones);

#endif /* GRILLSYNC_PROTOCOL_H */