#ifndef ROUTINESYNC_PROTOCOL_H
#define ROUTINESYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RS_NODE_HUB                0x0001
#define RS_NODE_DOORWAY_DOCK       0x0002
#define RS_NODE_FOCUS_BEACON       0x0003
#define RS_NODE_TAG_BASE           0x0100
#define RS_NODE_BROADCAST          0xFFFF

#define RS_MSG_JOIN_REQ            0x01
#define RS_MSG_JOIN_ACK            0x02
#define RS_MSG_HEARTBEAT           0x03
#define RS_MSG_ITEM_TELEMETRY      0x04
#define RS_MSG_DOORWAY_STATUS      0x05
#define RS_MSG_FOCUS_STATE         0x06
#define RS_MSG_FIND_REQUEST        0x07
#define RS_MSG_FIND_RESPONSE       0x08
#define RS_MSG_COMMAND             0x09
#define RS_MSG_CONFIG              0x0A
#define RS_MSG_ALERT               0x0B

#define RS_FOCUS_UNKNOWN           0
#define RS_FOCUS_DISTRACTED        1
#define RS_FOCUS_FOCUSED           2
#define RS_FOCUS_TRANSITION        3
#define RS_FOCUS_HYPERFOCUS        4

#define RS_SYNC_0                  0x52
#define RS_SYNC_1                  0x53
#define RS_FRAME_PAYLOAD_MAX       48

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
    uint8_t payload[RS_FRAME_PAYLOAD_MAX];
    uint16_t crc;
} rs_frame_t;

typedef struct {
    uint8_t item_class;
    uint8_t motion_state;
    uint16_t battery_mv;
    uint16_t uwb_range_cm;
    uint8_t room_anchor_id;
    uint8_t critical_for_departure;
    uint32_t last_seen_epoch;
} rs_item_telemetry_t;

typedef struct {
    uint8_t routine_id;
    uint8_t missing_items;
    uint8_t checklist_complete_pct;
    uint8_t door_open;
    uint16_t tray_mass_delta_g;
    uint16_t nfc_flags;
    uint16_t nearest_tag_range_cm;
    uint16_t minutes_to_deadline;
} rs_doorway_status_t;

typedef struct {
    uint8_t focus_state;
    uint8_t occupancy;
    uint16_t voc_index;
    uint16_t light_lux;
    uint16_t noise_db_x10;
    uint16_t seat_exit_count;
    uint16_t session_minutes;
    uint8_t cue_level;
} rs_focus_state_t;
#pragma pack(pop)

uint16_t rs_crc16(const uint8_t *data, size_t len);
void rs_build_frame(rs_frame_t *frame,
                    uint16_t src,
                    uint16_t dst,
                    uint8_t msg_type,
                    uint16_t seq,
                    uint32_t nonce,
                    const uint8_t *payload,
                    uint8_t payload_len);
bool rs_validate_frame(const rs_frame_t *frame, uint8_t payload_len);

#endif
