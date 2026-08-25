#ifndef MOLDSYNC_PROTOCOL_H
#define MOLDSYNC_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define MS_MAX_PAYLOAD 64
#define MS_PREAMBLE 0xB7
#define MS_VERSION 0x01

typedef enum {
    MS_MSG_TELEMETRY = 0x01,
    MS_MSG_EVENT = 0x02,
    MS_MSG_ALERT = 0x03,
    MS_MSG_COMMAND = 0x04,
    MS_MSG_ACK = 0x05,
    MS_MSG_CONFIG = 0x06,
    MS_MSG_OTA_CHUNK = 0x07
} ms_message_type_t;

typedef enum {
    MS_NODE_HUB = 0x10,
    MS_NODE_ROOM_SENTINEL = 0x11,
    MS_NODE_VENT_CONTROLLER = 0x12,
    MS_NODE_PLUMBING_INTERLOCK = 0x13,
    MS_NODE_INSPECTION_WAND = 0x14
} ms_node_type_t;

typedef struct {
    uint8_t preamble;
    uint8_t version;
    uint8_t message_type;
    uint8_t node_type;
    uint16_t source_id;
    uint16_t destination_id;
    uint8_t flags;
    uint8_t sequence;
    uint8_t payload_length;
    uint8_t payload[MS_MAX_PAYLOAD];
    uint16_t crc16;
} ms_frame_t;

uint16_t ms_crc16_ccitt(const uint8_t *data, size_t length);
int ms_encode_frame(ms_frame_t *frame, uint8_t *buffer, size_t buffer_size);
int ms_decode_frame(ms_frame_t *frame, const uint8_t *buffer, size_t length);

#endif
