#ifndef CLEANSYNC_PROTOCOL_H
#define CLEANSYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CS_PROTO_VERSION 1u
#define CS_MAX_PAYLOAD 48u
#define CS_NODE_HUB 0x0001u
#define CS_NODE_DOCK 0x0020u
#define CS_NODE_WAND 0x0030u
#define CS_NODE_DIRT_BASE 0x0100u

typedef enum {
    CS_MSG_JOIN_REQ = 0x01,
    CS_MSG_JOIN_ACK = 0x02,
    CS_MSG_DIRT_TELEMETRY = 0x10,
    CS_MSG_DOCK_STATE = 0x11,
    CS_MSG_WAND_SCAN = 0x12,
    CS_MSG_ALERT = 0x20,
    CS_MSG_PREPARE = 0x30,
    CS_MSG_SANITIZE = 0x31,
    CS_MSG_IDENTIFY = 0x32
} cs_msg_type_t;

typedef struct {
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t msg_type;
    uint16_t seq;
    uint32_t session_nonce;
    uint8_t payload_len;
    uint8_t payload[CS_MAX_PAYLOAD];
    uint16_t crc;
} cs_frame_t;

typedef struct {
    uint8_t dust_index;
    uint8_t traffic_score;
    uint8_t wet_floor_pct;
    uint8_t odor_index;
    int16_t temp_c_x10;
    uint16_t humidity_x10;
    uint16_t battery_mv;
} cs_dirt_telemetry_t;

typedef struct {
    uint16_t clean_tank_ml;
    uint16_t dirty_tank_ml;
    uint16_t detergent_ml;
    uint8_t pump_state;
    uint8_t leak_detected;
    uint16_t current_ma;
} cs_dock_state_t;

typedef struct {
    uint8_t mode;
    uint8_t residue_class;
    uint8_t confidence_pct;
    uint8_t fluorescence_score;
    uint16_t battery_mv;
    uint16_t image_tag;
} cs_wand_scan_t;

void cs_build_frame(cs_frame_t *frame,
                    uint16_t src,
                    uint16_t dst,
                    uint8_t msg_type,
                    uint16_t seq,
                    uint32_t session_nonce,
                    const uint8_t *payload,
                    uint8_t payload_len);

bool cs_validate_frame(const cs_frame_t *frame);

#endif
