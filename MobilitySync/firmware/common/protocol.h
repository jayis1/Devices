#ifndef MOBILITYSYNC_PROTOCOL_H
#define MOBILITYSYNC_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define MB_MAX_PAYLOAD 96
#define MB_PREAMBLE 0xC3
#define MB_VERSION 0x01

typedef enum {
    MB_MSG_TELEMETRY = 0x01,
    MB_MSG_EVENT = 0x02,
    MB_MSG_ALERT = 0x03,
    MB_MSG_COMMAND = 0x04,
    MB_MSG_ACK = 0x05,
    MB_MSG_CONFIG = 0x06
} mb_message_type_t;

typedef enum {
    MB_NODE_HUB = 0x10,
    MB_NODE_WALKER = 0x11,
    MB_NODE_TRANSFER_MAT = 0x12,
    MB_NODE_DOORWAY = 0x13,
    MB_NODE_BAND = 0x14
} mb_node_type_t;

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
    uint8_t payload[MB_MAX_PAYLOAD];
    uint16_t crc16;
} mb_frame_t;

uint16_t mb_crc16_ccitt(const uint8_t *data, size_t length);
int mb_encode_frame(mb_frame_t *frame, uint8_t *buffer, size_t buffer_size);
int mb_decode_frame(mb_frame_t *frame, const uint8_t *buffer, size_t length);

#endif
