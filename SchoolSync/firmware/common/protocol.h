#ifndef SCHOOLSYNC_PROTOCOL_H
#define SCHOOLSYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SS_NODE_HUB                0x0001
#define SS_NODE_BACKPACK           0x0002
#define SS_NODE_LUNCH_DOCK         0x0003
#define SS_NODE_DOORWAY            0x0004
#define SS_NODE_TRANSIT            0x0005
#define SS_NODE_BROADCAST          0xFFFF

#define SS_MSG_JOIN_REQ            0x01
#define SS_MSG_JOIN_ACK            0x02
#define SS_MSG_HEARTBEAT           0x03
#define SS_MSG_BACKPACK_STATE      0x04
#define SS_MSG_LUNCH_STATUS        0x05
#define SS_MSG_DOOR_EVENT          0x06
#define SS_MSG_TRANSIT_EVENT       0x07
#define SS_MSG_COMMAND             0x08
#define SS_MSG_CONFIG              0x09
#define SS_MSG_ALERT               0x0A

#define SS_STATE_STATIONARY        0
#define SS_STATE_LIFTED            1
#define SS_STATE_WORN              2
#define SS_STATE_VEHICLE           3

#define SS_FRAME_PAYLOAD_MAX       48
#define SS_SYNC_0                  0x53
#define SS_SYNC_1                  0x53

#pragma pack(push, 1)
typedef struct {
    uint8_t preamble[4];
    uint8_t sync[2];
    uint8_t length;
    uint16_t src_id;
    uint16_t dst_id;
    uint8_t msg_type;
    uint16_t seq;
    uint32_t session_nonce;
    uint8_t payload[SS_FRAME_PAYLOAD_MAX];
    uint16_t crc;
} ss_frame_t;

typedef struct {
    uint8_t carry_state;
    uint8_t pocket_homework_open;
    uint8_t pocket_meds_open;
    uint8_t ack_pressed;
    int16_t range_cm;
    int16_t accel_peak_mg;
    int16_t bag_temp_c_x100;
    uint16_t battery_mv;
} ss_backpack_state_t;

typedef struct {
    uint8_t packed;
    uint8_t ice_pack_present;
    uint8_t taken;
    uint8_t risk_level;
    uint16_t mass_grams;
    int16_t plate_temp_c_x100;
    int16_t ambient_temp_c_x100;
    uint16_t safe_minutes_remaining;
} ss_lunch_status_t;

typedef struct {
    uint8_t door_open;
    uint8_t bag_present;
    uint8_t lunch_present;
    uint8_t missing_items;
    int16_t uwb_range_cm;
    uint16_t eta_minutes;
    uint8_t readiness_score;
    uint8_t reserved;
} ss_door_event_t;

typedef struct {
    uint8_t boarded;
    uint8_t route_ok;
    uint8_t child_present;
    uint8_t backpack_present;
    uint16_t eta_minutes;
    uint16_t speed_dmps;
    uint16_t battery_mv;
    uint16_t alert_code;
} ss_transit_event_t;
#pragma pack(pop)

uint16_t ss_crc16(const uint8_t *data, size_t len);
void ss_build_frame(ss_frame_t *frame,
                    uint16_t src,
                    uint16_t dst,
                    uint8_t msg_type,
                    uint16_t seq,
                    uint32_t nonce,
                    const uint8_t *payload,
                    uint8_t payload_len);
bool ss_validate_frame(const ss_frame_t *frame, uint8_t payload_len);

#endif
