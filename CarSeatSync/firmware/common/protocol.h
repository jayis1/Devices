#ifndef CARSEATSYNC_PROTOCOL_H
#define CARSEATSYNC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CSYNC_PROTO_VERSION 1U
#define CSYNC_MAX_PAYLOAD 48U
#define CSYNC_SYNC_WORD 0xA5U

typedef enum {
    MSG_SEAT_STATUS = 0x10,
    MSG_CHILD_STATUS = 0x11,
    MSG_CABIN_STATUS = 0x12,
    MSG_HANDOFF_EVENT = 0x13,
    MSG_TRIP_STATE = 0x14,
    MSG_ALERT = 0x15,
    MSG_ACK = 0x16
} csync_message_type_t;

typedef enum {
    ALERT_NONE = 0,
    ALERT_HARNESS = 1,
    ALERT_HEAT = 2,
    ALERT_MISSED_UNLOAD = 3,
    ALERT_CHILD_DISTRESS = 4
} csync_alert_t;

typedef struct {
    uint8_t sync;
    uint8_t version;
    uint8_t source_id;
    uint8_t msg_type;
    uint32_t trip_id;
    uint8_t payload_len;
    uint8_t payload[CSYNC_MAX_PAYLOAD];
    uint16_t crc16;
} csync_frame_t;

uint16_t csync_crc16(const uint8_t *data, size_t length);
bool csync_encode(csync_frame_t *frame, uint8_t source_id, csync_message_type_t type, uint32_t trip_id, const uint8_t *payload, uint8_t payload_len);
bool csync_validate(const csync_frame_t *frame);

#endif
